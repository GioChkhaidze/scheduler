#include <SharedLinkScheduler.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

template <typename... Visitors>
struct LinkOverloaded : Visitors... {
  using Visitors::operator()...;
};

template <typename... Visitors>
LinkOverloaded(Visitors...) -> LinkOverloaded<Visitors...>;

bool sameServer(const ServerId& left, const ServerId& right) {
  return left.type == right.type && left.cloud_index == right.cloud_index;
}

bool sameTask(const TaskSpec& left, const TaskSpec& right) {
  if (left.index() != right.index()) {
    return false;
  }
  return std::visit(LinkOverloaded{
    [](const PrefillPreTask& a, const PrefillPreTask& b) {
      return a.remote == b.remote && a.rid == b.rid;
    },
    [](const PrefillProcTask& a, const PrefillProcTask& b) {
      return a.layer_begin == b.layer_begin && a.layer_end == b.layer_end
        && a.remote == b.remote && a.rid == b.rid;
    },
    [](const PrefillPostTask& a, const PrefillPostTask& b) {
      return a.remote == b.remote && a.rid == b.rid;
    },
    [](const DecodePreTask& a, const DecodePreTask& b) {
      return a.rids == b.rids;
    },
    [](const DecodeProcTask& a, const DecodeProcTask& b) {
      return a.remote == b.remote && a.rids == b.rids;
    },
    [](const DecodePostTask& a, const DecodePostTask& b) {
      return a.rids == b.rids;
    },
    [](const auto&, const auto&) {
      return false;
    },
  }, left, right);
}

const Request& getRequest(const WorldState& world, int rid) {
  assert(rid >= 0);
  return world.requests.at(static_cast<std::size_t>(rid));
}

LinkDirectionState& directionState(SharedLinkState& links, TransferDirection direction) {
  return direction == TransferDirection::Up ? links.up : links.down;
}

const LinkDirectionState& directionState(
  const SharedLinkState& links, TransferDirection direction) {
  return direction == TransferDirection::Up ? links.up : links.down;
}

std::vector<LinkTransferSpec> triggeredTransfers(
  const WorldState& world, const TaskSpec& task, int num_layers) {
  return std::visit(LinkOverloaded{
    [&](const PrefillPreTask& value) {
      const Request& request = getRequest(world, value.rid);
      return std::vector<LinkTransferSpec>{
        {TransferDirection::Up, value.remote, TransferStage::Prefill, request.input_length, {value.rid}},
      };
    },
    [&](const PrefillProcTask& value) {
      if (value.layer_end != num_layers) {
        return std::vector<LinkTransferSpec>{};
      }
      const Request& request = getRequest(world, value.rid);
      return std::vector<LinkTransferSpec>{
        {TransferDirection::Down, value.remote, TransferStage::Prefill, request.input_length, {value.rid}},
      };
    },
    [](const PrefillPostTask&) {
      return std::vector<LinkTransferSpec>{};
    },
    [&](const DecodePreTask& value) {
      std::vector<std::vector<int>> by_remote(world.clouds.size());
      for (const int rid : value.rids) {
        const Request& request = getRequest(world, rid);
        assert(request.remote.has_value());
        by_remote.at(static_cast<std::size_t>(*request.remote)).push_back(rid);
      }

      std::vector<LinkTransferSpec> transfers;
      for (std::size_t remote = 0; remote < by_remote.size(); ++remote) {
        if (!by_remote[remote].empty()) {
          transfers.push_back({
            TransferDirection::Up,
            static_cast<int>(remote),
            TransferStage::Decode,
            static_cast<std::int64_t>(by_remote[remote].size()),
            std::move(by_remote[remote]),
          });
        }
      }
      return transfers;
    },
    [](const DecodeProcTask& value) {
      return std::vector<LinkTransferSpec>{
        {TransferDirection::Down, value.remote, TransferStage::Decode,
          static_cast<std::int64_t>(value.rids.size()), value.rids},
      };
    },
    [](const DecodePostTask&) {
      return std::vector<LinkTransferSpec>{};
    },
  }, task);
}

void commitTransfer(
  SharedLinkState& links,
  const LinkTransferSpec& transfer,
  double queued_at,
  const SystemConfig& system) {
  LinkDirectionState& direction = directionState(links, transfer.direction);
  const double starts_at = std::max(queued_at, direction.committed_tail);
  const double completes_at = starts_at + transferTime(system, transfer.length);
  direction.committed_tail = completes_at;
  direction.committed.push_back({transfer, queued_at, starts_at, completes_at});
}

bool sameTransfer(const LinkTransferSpec& expected, const TransferDoneEvent& actual) {
  return expected.direction == actual.direction
    && expected.remote == actual.remote
    && expected.stage == actual.stage
    && expected.rids == actual.rids;
}

void reconcileTransfer(
  SharedLinkState& links,
  const TransferDoneEvent& event,
  double timestamp,
  const SystemConfig& system) {
  LinkDirectionState& direction = directionState(links, event.direction);
  assert(!direction.committed.empty());
  const CommittedLinkTransfer& expected = direction.committed.front();
  assert(sameTransfer(expected.transfer, event));
  assert(event.size_bytes == expected.transfer.length * system.bytes_per_token);

  const double error = std::abs(timestamp - expected.completes_at);
  links.maximum_reconciliation_error = std::max(links.maximum_reconciliation_error, error);
  ++links.reconciliation_count;
  direction.committed.pop_front();
}

struct FutureTransfer {
  LinkTransferSpec transfer;
  double enqueue_at;
  std::uint64_t trigger_order;
  std::size_t within_trigger_order;
};

std::vector<FutureTransfer> futureTransfers(
  const SharedLinkState& links, TransferDirection direction) {
  std::vector<FutureTransfer> result;
  for (const PendingLinkTrigger& trigger : links.pending_triggers) {
    for (std::size_t index = 0; index < trigger.transfers.size(); ++index) {
      if (trigger.transfers[index].direction == direction) {
        result.push_back({trigger.transfers[index], trigger.expected_completion, trigger.order, index});
      }
    }
  }
  std::sort(result.begin(), result.end(), [](const FutureTransfer& left, const FutureTransfer& right) {
    return std::tie(left.enqueue_at, left.trigger_order, left.within_trigger_order)
      < std::tie(right.enqueue_at, right.trigger_order, right.within_trigger_order);
  });
  return result;
}

double tailBefore(
  const SharedLinkState& links,
  TransferDirection direction,
  double enqueue_at,
  const SystemConfig& system) {
  double tail = directionState(links, direction).committed_tail;
  for (const FutureTransfer& future : futureTransfers(links, direction)) {
    if (future.enqueue_at > enqueue_at) {
      break;
    }
    tail = std::max(tail, future.enqueue_at) + transferTime(system, future.transfer.length);
  }
  return tail;
}

struct RequestPriority {
  RequestClass request_class;
  double age;
  int rid;
};

RequestPriority priorityOf(const WorldState& world, int rid) {
  const Request& request = getRequest(world, rid);
  const RequestClass request_class = classifyRequest(request);
  assert(request_class == RequestClass::Cold || request_class == RequestClass::Hot);
  const double origin = request_class == RequestClass::Hot
    ? request.last_token_time.value_or(request.arrival_time)
    : request.arrival_time;
  return {request_class, world.current_time - origin, rid};
}

int classRank(RequestClass request_class) {
  return request_class == RequestClass::Hot ? 1 : 0;
}

bool higherPriority(const RequestPriority& left, const RequestPriority& right) {
  if (left.request_class != right.request_class) {
    return classRank(left.request_class) > classRank(right.request_class);
  }
  if (left.age != right.age) {
    return left.age > right.age;
  }
  return left.rid < right.rid;
}

bool compatibleReplacement(
  const RequestPriority& candidate,
  const RequestPriority& original,
  const SharedLinkSchedulerConfig& config) {
  if (classRank(candidate.request_class) > classRank(original.request_class)) {
    return true;
  }
  if (candidate.request_class != original.request_class) {
    return false;
  }
  const double permitted_age_loss = config.system.latency_in_ms;
  return original.age - candidate.age <= permitted_age_loss;
}

bool hasGuaranteedFutureEvent(const WorldState& world) {
  if (world.edge.busy
      || std::any_of(world.clouds.begin(), world.clouds.end(), [](const ServerState& server) {
        return server.busy;
      })) {
    return true;
  }
  return !world.links.up.committed.empty()
    || !world.links.down.committed.empty()
    || !world.links.pending_triggers.empty();
}

std::optional<LinkTransferSpec> prefillTransferForAssignment(
  const WorldState& world, const Assignment& assignment, int num_layers) {
  const std::vector<LinkTransferSpec> transfers = triggeredTransfers(world, assignment.task, num_layers);
  if (transfers.size() != 1 || transfers.front().stage != TransferStage::Prefill) {
    return std::nullopt;
  }
  return transfers.front();
}

struct ProjectedTransfer {
  LinkTransferSpec transfer;
  double enqueue_at;
  std::uint64_t order;
  std::size_t within_order;
  std::optional<std::size_t> target;
  bool candidate = false;
};

std::vector<ProjectedTransfer> pendingProjection(
  const WorldState& world, TransferDirection direction) {
  std::vector<ProjectedTransfer> result;
  for (const FutureTransfer& future : futureTransfers(world.links, direction)) {
    result.push_back({
      future.transfer, future.enqueue_at, future.trigger_order, future.within_trigger_order, std::nullopt, false,
    });
  }
  return result;
}

std::vector<double> simulateProjection(
  const WorldState& world,
  TransferDirection direction,
  std::vector<ProjectedTransfer> transfers,
  std::size_t target_count,
  bool include_candidate,
  const SystemConfig& system) {
  std::sort(transfers.begin(), transfers.end(), [](const ProjectedTransfer& left, const ProjectedTransfer& right) {
    return std::tie(left.enqueue_at, left.order, left.within_order)
      < std::tie(right.enqueue_at, right.order, right.within_order);
  });

  std::vector<double> completions(target_count, -1.0);
  double tail = directionState(world.links, direction).committed_tail;
  for (const ProjectedTransfer& projected : transfers) {
    if (projected.candidate && !include_candidate) {
      continue;
    }
    tail = std::max(tail, projected.enqueue_at) + transferTime(system, projected.transfer.length);
    if (projected.target.has_value()) {
      completions.at(*projected.target) = tail;
    }
  }
  return completions;
}

std::optional<double> hotDeadline(
  const WorldState& world, const LinkTransferSpec& transfer, const SharedLinkSchedulerConfig& config) {
  std::optional<double> deadline;
  for (const int rid : transfer.rids) {
    const Request& request = getRequest(world, rid);
    if (classifyRequest(request) != RequestClass::Hot || !request.last_token_time.has_value()) {
      continue;
    }
    const double request_deadline = *request.last_token_time + config.system.SLO2;
    deadline = std::min(deadline.value_or(request_deadline), request_deadline);
  }
  return deadline;
}

double maximumBlockedHotUrgency(
  const std::vector<double>& without_candidate,
  const std::vector<double>& with_candidate,
  const std::vector<double>& deadlines,
  const SharedLinkSchedulerConfig& config) {
  assert(without_candidate.size() == with_candidate.size());
  assert(with_candidate.size() == deadlines.size());
  double maximum = 0.0;
  for (std::size_t index = 0; index < deadlines.size(); ++index) {
    const bool candidate_causes_delay = with_candidate[index] > without_candidate[index] + 1e-9;
    if (candidate_causes_delay && with_candidate[index] > deadlines[index]) {
      const double last_token_time = deadlines[index] - config.system.SLO2;
      maximum = std::max(maximum, (with_candidate[index] - last_token_time) / config.system.SLO2);
    }
  }
  return maximum;
}

std::optional<Assignment> protectedDecodePreAssignment(
  const WorldState& world, const SharedLinkSchedulerConfig& config) {
  std::vector<int> ready;
  for (std::size_t rid = 0; rid < world.requests.size(); ++rid) {
    const Request& request = world.requests[rid];
    if (request.state == RequestState::ReadyDecodePre && classifyRequest(request) == RequestClass::Hot) {
      ready.push_back(static_cast<int>(rid));
    }
  }
  if (ready.empty()) {
    return std::nullopt;
  }
  std::sort(ready.begin(), ready.end(), [&](int left, int right) {
    return higherPriority(priorityOf(world, left), priorityOf(world, right));
  });
  const DecodeBatchPolicy& fallback = config.multiprocessor.adaptive.baseline_decode_batches;
  ready.resize(static_cast<std::size_t>(
    selectDecodeBatchSize(fallback, RequestState::ReadyDecodePre, ready.size())));
  const Assignment baseline{ServerId{ServerType::Edge, -1}, DecodePreTask{std::move(ready)}};
  return localizeDecodePreAssignment(world, baseline, config);
}

double blockedUpUrgency(
  const WorldState& world,
  const Assignment& candidate,
  const Assignment& protected_decode,
  const SharedLinkSchedulerConfig& config) {
  const LinkTransferSpec candidate_transfer = *prefillTransferForAssignment(
    world, candidate, config.system.num_layers);
  const std::vector<LinkTransferSpec> decode_transfers =
    triggeredTransfers(world, protected_decode.task, config.system.num_layers);
  const double candidate_enqueue = world.current_time + config.system.S
    + estimateTaskDuration(world, candidate.task, config.curves, config.system.num_layers);
  const double decode_duration = config.system.S
    + estimateTaskDuration(world, protected_decode.task, config.curves, config.system.num_layers);

  std::vector<ProjectedTransfer> without = pendingProjection(world, TransferDirection::Up);
  std::vector<ProjectedTransfer> with = without;
  with.push_back({
    candidate_transfer, candidate_enqueue, world.links.next_trigger_order, 0, std::nullopt, true,
  });
  std::vector<double> deadlines;
  deadlines.reserve(decode_transfers.size());
  for (std::size_t index = 0; index < decode_transfers.size(); ++index) {
    const LinkTransferSpec& transfer = decode_transfers[index];
    deadlines.push_back(*hotDeadline(world, transfer, config));
    without.push_back({
      transfer, world.current_time + decode_duration, world.links.next_trigger_order, index, index, false,
    });
    with.push_back({
      transfer, candidate_enqueue + decode_duration, world.links.next_trigger_order + 1, index, index, false,
    });
  }

  const std::vector<double> without_completion = simulateProjection(
    world, TransferDirection::Up, std::move(without), deadlines.size(), true, config.system);
  const std::vector<double> with_completion = simulateProjection(
    world, TransferDirection::Up, std::move(with), deadlines.size(), true, config.system);
  return maximumBlockedHotUrgency(without_completion, with_completion, deadlines, config);
}

double blockedDownUrgency(
  const WorldState& world,
  std::size_t candidate_index,
  const std::vector<Assignment>& candidates,
  const SharedLinkSchedulerConfig& config) {
  std::vector<ProjectedTransfer> projection = pendingProjection(world, TransferDirection::Down);
  std::vector<double> deadlines;
  for (std::size_t assignment_index = 0; assignment_index < candidates.size(); ++assignment_index) {
    const Assignment& assignment = candidates[assignment_index];
    const std::vector<LinkTransferSpec> transfers =
      triggeredTransfers(world, assignment.task, config.system.num_layers);
    const double enqueue_at = world.current_time + config.system.S
      + estimateTaskDuration(world, assignment.task, config.curves, config.system.num_layers);
    for (std::size_t within = 0; within < transfers.size(); ++within) {
      const LinkTransferSpec& transfer = transfers[within];
      if (transfer.direction != TransferDirection::Down) {
        continue;
      }
      const bool is_candidate = assignment_index == candidate_index;
      std::optional<std::size_t> target;
      if (!is_candidate && transfer.stage == TransferStage::Decode) {
        const std::optional<double> deadline = hotDeadline(world, transfer, config);
        if (deadline.has_value()) {
          target = deadlines.size();
          deadlines.push_back(*deadline);
        }
      }
      projection.push_back({
        transfer,
        enqueue_at,
        world.links.next_trigger_order + assignment_index,
        within,
        target,
        is_candidate,
      });
    }
  }
  if (deadlines.empty()) {
    return 0.0;
  }
  const std::vector<double> without_candidate = simulateProjection(
    world, TransferDirection::Down, projection, deadlines.size(), false, config.system);
  const std::vector<double> with_candidate = simulateProjection(
    world, TransferDirection::Down, std::move(projection), deadlines.size(), true, config.system);
  return maximumBlockedHotUrgency(without_candidate, with_candidate, deadlines, config);
}

int prefillRid(const Assignment& assignment) {
  if (const PrefillPreTask* task = std::get_if<PrefillPreTask>(&assignment.task)) {
    return task->rid;
  }
  const PrefillProcTask* task = std::get_if<PrefillProcTask>(&assignment.task);
  assert(task != nullptr);
  return task->rid;
}

bool shouldDeferPrefill(
  const WorldState& world,
  std::size_t candidate_index,
  const std::vector<Assignment>& candidates,
  const std::optional<Assignment>& protected_decode,
  const SharedLinkSchedulerConfig& config) {
  const Assignment& candidate = candidates[candidate_index];
  const std::optional<LinkTransferSpec> transfer =
    prefillTransferForAssignment(world, candidate, config.system.num_layers);
  if (!transfer.has_value()) {
    return false;
  }

  double hot_urgency = 0.0;
  if (transfer->direction == TransferDirection::Up) {
    if (!config.features.prefill_up_admission || !protected_decode.has_value()) {
      return false;
    }
    hot_urgency = blockedUpUrgency(world, candidate, *protected_decode, config);
  } else {
    if (!config.features.prefill_down_admission) {
      return false;
    }
    hot_urgency = blockedDownUrgency(world, candidate_index, candidates, config);
  }

  const Request& prefill = getRequest(world, prefillRid(candidate));
  const double prefill_urgency = (world.current_time - prefill.arrival_time) / config.system.SLO1;
  return hot_urgency > 1.0 && hot_urgency > prefill_urgency;
}

} // namespace

SharedLinkSchedulerConfig buildSharedLinkSchedulerConfig(
  const SystemConfig& system,
  const TimingCurves& curves,
  SharedLinkFeatures features) {
  return {
    .multiprocessor = buildMultiprocessorSchedulerConfig(system, curves),
    .system = system,
    .curves = curves,
    .features = features,
  };
}

std::vector<LinkTransferSpec> deriveTriggeredTransfers(
  const WorldState& world, const TaskSpec& task, int num_layers) {
  return triggeredTransfers(world, task, num_layers);
}

double estimateTaskDuration(
  const WorldState& world, const TaskSpec& task, const TimingCurves& curves, int num_layers) {
  assert(num_layers > 0);
  return std::visit(LinkOverloaded{
    [&](const PrefillPreTask& value) {
      return interpolate(curves.prefill_pre, getRequest(world, value.rid).input_length);
    },
    [&](const PrefillProcTask& value) {
      const double full = interpolate(curves.prefill_proc, getRequest(world, value.rid).input_length);
      return full * (value.layer_end - value.layer_begin) / num_layers;
    },
    [&](const PrefillPostTask& value) {
      return interpolate(curves.prefill_post, getRequest(world, value.rid).input_length);
    },
    [&](const DecodePreTask& value) {
      return interpolate(curves.decode_pre, static_cast<int>(value.rids.size()));
    },
    [&](const DecodeProcTask& value) {
      return interpolate(curves.decode_proc, static_cast<int>(value.rids.size()));
    },
    [&](const DecodePostTask& value) {
      return interpolate(curves.decode_post, static_cast<int>(value.rids.size()));
    },
  }, task);
}

void recordLinkAssignment(
  WorldState& world,
  const Assignment& assignment,
  const SystemConfig& system,
  const TimingCurves& curves) {
  std::vector<LinkTransferSpec> transfers =
    triggeredTransfers(world, assignment.task, system.num_layers);
  if (transfers.empty()) {
    return;
  }
  const double expected_completion = world.current_time + system.S
    + estimateTaskDuration(world, assignment.task, curves, system.num_layers);
  world.links.pending_triggers.push_back({
    assignment.server,
    assignment.task,
    expected_completion,
    world.links.next_trigger_order++,
    std::move(transfers),
  });
}

void observeLinkFrame(WorldState& world, const Frame& frame, const SystemConfig& system) {
  for (const Event& event : frame.events) {
    if (const TaskDoneEvent* done = std::get_if<TaskDoneEvent>(&event)) {
      const auto pending = std::find_if(
        world.links.pending_triggers.begin(),
        world.links.pending_triggers.end(),
        [&](const PendingLinkTrigger& trigger) {
          return sameServer(trigger.server, done->server) && sameTask(trigger.task, done->task);
        });

      std::vector<LinkTransferSpec> transfers;
      if (pending != world.links.pending_triggers.end()) {
        transfers = std::move(pending->transfers);
        world.links.pending_triggers.erase(pending);
      } else {
        transfers = triggeredTransfers(world, done->task, system.num_layers);
      }
      for (const LinkTransferSpec& transfer : transfers) {
        commitTransfer(world.links, transfer, frame.timestamp, system);
      }
    } else if (const TransferDoneEvent* transfer_done = std::get_if<TransferDoneEvent>(&event)) {
      reconcileTransfer(world.links, *transfer_done, frame.timestamp, system);
    }
  }
}

double estimateDecodePreLinkCost(
  const WorldState& world,
  const std::vector<int>& rids,
  const SharedLinkSchedulerConfig& config) {
  assert(!rids.empty());
  std::vector<int> counts(world.clouds.size(), 0);
  for (const int rid : rids) {
    const Request& request = getRequest(world, rid);
    assert(request.remote.has_value());
    ++counts.at(static_cast<std::size_t>(*request.remote));
  }

  const double enqueue_at = world.current_time + config.system.S
    + interpolate(config.curves.decode_pre, static_cast<int>(rids.size()));
  const double tail = tailBefore(
    world.links, TransferDirection::Up, enqueue_at, config.system);
  double cost = std::max(0.0, tail - enqueue_at);
  for (const int count : counts) {
    if (count > 0) {
      cost += transferTime(config.system, count);
    }
  }
  return cost;
}

Assignment localizeDecodePreAssignment(
  const WorldState& world,
  const Assignment& baseline,
  const SharedLinkSchedulerConfig& config) {
  const DecodePreTask* baseline_task = std::get_if<DecodePreTask>(&baseline.task);
  if (!config.features.decode_locality || baseline_task == nullptr || baseline_task->rids.size() < 2) {
    return baseline;
  }

  std::vector<bool> in_baseline(world.requests.size(), false);
  for (const int rid : baseline_task->rids) {
    in_baseline.at(static_cast<std::size_t>(rid)) = true;
  }
  std::vector<int> fillers;
  for (std::size_t rid = 0; rid < world.requests.size(); ++rid) {
    if (!in_baseline[rid] && world.requests[rid].state == RequestState::ReadyDecodePre) {
      fillers.push_back(static_cast<int>(rid));
    }
  }
  std::sort(fillers.begin(), fillers.end(), [&](int left, int right) {
    return higherPriority(priorityOf(world, left), priorityOf(world, right));
  });

  std::vector<int> localized;
  localized.reserve(baseline_task->rids.size());
  std::vector<bool> represented(world.clouds.size(), false);
  std::vector<bool> used(world.requests.size(), false);

  for (const int original_rid : baseline_task->rids) {
    const Request& original = getRequest(world, original_rid);
    assert(original.remote.has_value());
    const std::size_t original_remote = static_cast<std::size_t>(*original.remote);
    int selected_rid = original_rid;

    if (!represented[original_remote] && !localized.empty()) {
      const RequestPriority original_priority = priorityOf(world, original_rid);
      const auto replacement = std::find_if(fillers.begin(), fillers.end(), [&](int candidate_rid) {
        if (used.at(static_cast<std::size_t>(candidate_rid))) {
          return false;
        }
        const Request& candidate = getRequest(world, candidate_rid);
        assert(candidate.remote.has_value());
        return represented.at(static_cast<std::size_t>(*candidate.remote))
          && compatibleReplacement(priorityOf(world, candidate_rid), original_priority, config);
      });
      if (replacement != fillers.end()) {
        selected_rid = *replacement;
      }
    }

    const Request& selected = getRequest(world, selected_rid);
    represented.at(static_cast<std::size_t>(*selected.remote)) = true;
    used.at(static_cast<std::size_t>(selected_rid)) = true;
    localized.push_back(selected_rid);
  }

  if (estimateDecodePreLinkCost(world, localized, config)
      >= estimateDecodePreLinkCost(world, baseline_task->rids, config)) {
    return baseline;
  }
  return {baseline.server, DecodePreTask{std::move(localized)}};
}

std::vector<Assignment> chooseSharedLinkAssignments(
  const WorldState& world, int num_layers, const SharedLinkSchedulerConfig& config) {
  const std::vector<Assignment> baseline =
    chooseMultiprocessorAssignments(world, num_layers, config.multiprocessor);
  if (!config.features.decode_locality
      && !config.features.prefill_up_admission
      && !config.features.prefill_down_admission) {
    return baseline;
  }

  std::vector<Assignment> candidates = baseline;
  for (Assignment& assignment : candidates) {
    assignment = localizeDecodePreAssignment(world, assignment, config);
  }

  std::vector<Assignment> accepted;
  accepted.reserve(candidates.size());
  const std::optional<Assignment> protected_decode = protectedDecodePreAssignment(world, config);
  bool added_protected_decode = false;
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const Assignment& assignment = candidates[index];
    if (!shouldDeferPrefill(world, index, candidates, protected_decode, config)) {
      accepted.push_back(assignment);
      continue;
    }
    const PrefillPreTask* prefill_pre = std::get_if<PrefillPreTask>(&assignment.task);
    if (prefill_pre != nullptr && protected_decode.has_value() && !added_protected_decode) {
      accepted.push_back(*protected_decode);
      added_protected_decode = true;
    }
  }
  if (accepted.empty() && !baseline.empty() && !hasGuaranteedFutureEvent(world)) {
    return baseline;
  }
  return accepted;
}
