#include <scheduler/scheduling/SchedulingAnalysis.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <variant>
#include <vector>

namespace {

template <typename... Visitors>
struct AnalysisOverloaded : Visitors... {
  using Visitors::operator()...;
};

template <typename... Visitors>
AnalysisOverloaded(Visitors...) -> AnalysisOverloaded<Visitors...>;

constexpr std::size_t taskIndex(ScheduledTaskKind kind) {
  return static_cast<std::size_t>(kind);
}

double taskWork(double schedule_cost, const TimingCurve& curve, int size) {
  return schedule_cost + interpolate(curve, size);
}

double remainingPrefillProcWork(
  const Request& request, const SystemConfig& system, const TimingCurves& curves) {
  const double fraction = static_cast<double>(system.num_layers - request.next_prefill_layer)
    / system.num_layers;
  return system.S + fraction * interpolate(curves.prefill_proc, request.input_length);
}

double projectedLinkBacklog(
  const WorldState& world, TransferDirection direction, const SystemConfig& system) {
  struct Future {
    double enqueue_at;
    std::uint64_t order;
    std::size_t within;
    std::int64_t length;
  };

  const LinkDirectionState& state = direction == TransferDirection::Up
    ? world.links.up
    : world.links.down;
  std::vector<Future> future;
  for (const PendingLinkTrigger& trigger : world.links.pending_triggers) {
    for (std::size_t within = 0; within < trigger.transfers.size(); ++within) {
      const LinkTransferSpec& transfer = trigger.transfers[within];
      if (transfer.direction == direction) {
        future.push_back({trigger.expected_completion, trigger.order, within, transfer.length});
      }
    }
  }
  std::sort(future.begin(), future.end(), [](const Future& left, const Future& right) {
    return std::tie(left.enqueue_at, left.order, left.within)
      < std::tie(right.enqueue_at, right.order, right.within);
  });

  double tail = std::max(world.current_time, state.committed_tail);
  for (const Future& transfer : future) {
    tail = std::max(tail, transfer.enqueue_at) + transferTime(system, transfer.length);
  }
  return std::max(0.0, tail - world.current_time);
}

void addReadyWork(
  SchedulerBacklog& backlog,
  const Request& request,
  const SystemConfig& system,
  const TimingCurves& curves) {
  switch (request.state) {
    case RequestState::ReadyPrefillPre:
      backlog.edge_ms += taskWork(system.S, curves.prefill_pre, request.input_length);
      break;
    case RequestState::ReadyPrefillProc:
      assert(request.remote.has_value());
      backlog.cloud_ms.at(static_cast<std::size_t>(*request.remote)) +=
        remainingPrefillProcWork(request, system, curves);
      break;
    case RequestState::ReadyPrefillPost:
      backlog.edge_ms += taskWork(system.S, curves.prefill_post, request.input_length);
      break;
    case RequestState::ReadyDecodePre:
      backlog.edge_ms += taskWork(system.S, curves.decode_pre, 1);
      break;
    case RequestState::ReadyDecodeProc:
      assert(request.remote.has_value());
      backlog.cloud_ms.at(static_cast<std::size_t>(*request.remote)) +=
        taskWork(system.S, curves.decode_proc, 1);
      break;
    case RequestState::ReadyDecodePost:
      backlog.edge_ms += taskWork(system.S, curves.decode_post, 1);
      break;
    default:
      break;
  }
}

double prefillDemand(const Request& request, const SystemConfig& system, const TimingCurves& curves) {
  switch (request.state) {
    case RequestState::ReadyPrefillPre:
    case RequestState::WaitingPrefillPreDone:
      return taskWork(system.S, curves.prefill_pre, request.input_length)
        + remainingPrefillProcWork(request, system, curves)
        + taskWork(system.S, curves.prefill_post, request.input_length);
    case RequestState::WaitingPrefillUpload:
    case RequestState::ReadyPrefillProc:
    case RequestState::WaitingPrefillProcDone:
      return remainingPrefillProcWork(request, system, curves)
        + taskWork(system.S, curves.prefill_post, request.input_length);
    case RequestState::WaitingPrefillDownload:
    case RequestState::ReadyPrefillPost:
    case RequestState::WaitingPrefillPostDone:
      return taskWork(system.S, curves.prefill_post, request.input_length);
    default:
      return 0.0;
  }
}

double hotDemand(const Request& request, const SystemConfig& system, const TimingCurves& curves) {
  if (classifyRequest(request) != RequestClass::Hot) {
    return 0.0;
  }
  return taskWork(system.S, curves.decode_pre, 1)
    + taskWork(system.S, curves.decode_proc, 1)
    + taskWork(system.S, curves.decode_post, 1)
    + 2.0 * transferTime(system, 1);
}

} // namespace

ScheduledTaskKind scheduledTaskKind(const TaskSpec& task) {
  return std::visit(AnalysisOverloaded{
    [](const PrefillPreTask&) { return ScheduledTaskKind::PrefillPre; },
    [](const PrefillProcTask&) { return ScheduledTaskKind::PrefillProc; },
    [](const PrefillPostTask&) { return ScheduledTaskKind::PrefillPost; },
    [](const DecodePreTask&) { return ScheduledTaskKind::DecodePre; },
    [](const DecodeProcTask&) { return ScheduledTaskKind::DecodeProc; },
    [](const DecodePostTask&) { return ScheduledTaskKind::DecodePost; },
  }, task);
}

int scheduledTaskBatchSize(const TaskSpec& task) {
  return std::visit(AnalysisOverloaded{
    [](const PrefillPreTask&) { return 1; },
    [](const PrefillProcTask&) { return 1; },
    [](const PrefillPostTask&) { return 1; },
    [](const DecodePreTask& value) { return static_cast<int>(value.rids.size()); },
    [](const DecodeProcTask& value) { return static_cast<int>(value.rids.size()); },
    [](const DecodePostTask& value) { return static_cast<int>(value.rids.size()); },
  }, task);
}

std::array<int, scheduled_task_kind_count> eligibleTaskCounts(
  const WorldState& world, const ServerId& server) {
  std::array<int, scheduled_task_kind_count> result{};
  for (const Request& request : world.requests) {
    if (server.type == ServerType::Edge) {
      if (request.state == RequestState::ReadyPrefillPre) {
        ++result[taskIndex(ScheduledTaskKind::PrefillPre)];
      } else if (request.state == RequestState::ReadyPrefillPost) {
        ++result[taskIndex(ScheduledTaskKind::PrefillPost)];
      } else if (request.state == RequestState::ReadyDecodePre) {
        ++result[taskIndex(ScheduledTaskKind::DecodePre)];
      } else if (request.state == RequestState::ReadyDecodePost) {
        ++result[taskIndex(ScheduledTaskKind::DecodePost)];
      }
      continue;
    }

    if (request.remote != server.cloud_index) {
      continue;
    }
    if (request.state == RequestState::ReadyPrefillProc) {
      ++result[taskIndex(ScheduledTaskKind::PrefillProc)];
    } else if (request.state == RequestState::ReadyDecodeProc) {
      ++result[taskIndex(ScheduledTaskKind::DecodeProc)];
    }
  }
  return result;
}

ScorePressure estimateScorePressure(const WorldState& world, const SystemConfig& system) {
  assert(system.SLO1 > 0.0);
  assert(system.SLO2 > 0.0);
  ScorePressure result;
  for (const Request& request : world.requests) {
    const RequestClass request_class = classifyRequest(request);
    if (request_class == RequestClass::Prefill) {
      const double pressure = std::max(0.0, (world.current_time - request.arrival_time) / system.SLO1);
      result.tdr += pressure;
      result.maximum_tdr = std::max(result.maximum_tdr, pressure);
      ++result.prefill_requests;
    } else if (request_class == RequestClass::Hot && request.last_token_time.has_value()) {
      const double pressure = std::max(0.0, (world.current_time - *request.last_token_time) / system.SLO2);
      result.tpot += pressure;
      result.maximum_tpot = std::max(result.maximum_tpot, pressure);
      ++result.hot_requests;
    }
  }
  return result;
}

SchedulerBacklog estimateSchedulerBacklog(
  const WorldState& world, const SystemConfig& system, const TimingCurves& curves) {
  SchedulerBacklog result;
  result.cloud_ms.resize(world.clouds.size(), 0.0);
  for (const Request& request : world.requests) {
    addReadyWork(result, request, system, curves);
    result.prefill_demand_ms += prefillDemand(request, system, curves);
    result.hot_demand_ms += hotDemand(request, system, curves);
  }
  result.up_ms = projectedLinkBacklog(world, TransferDirection::Up, system);
  result.down_ms = projectedLinkBacklog(world, TransferDirection::Down, system);

  if (!world.requests.empty()) {
    const double earliest = std::min_element(
      world.requests.begin(), world.requests.end(), [](const Request& left, const Request& right) {
        return left.arrival_time < right.arrival_time;
      })->arrival_time;
    const double elapsed = world.current_time - earliest;
    if (elapsed > 0.0) {
      result.observed_arrival_rate = world.requests.size() / elapsed;
    }
  }

  double largest = result.edge_ms;
  result.bottleneck = largest > 0.0 ? ResourceBottleneck::Edge : ResourceBottleneck::None;
  const double cloud = result.cloud_ms.empty()
    ? 0.0
    : *std::max_element(result.cloud_ms.begin(), result.cloud_ms.end());
  if (cloud > largest) {
    largest = cloud;
    result.bottleneck = ResourceBottleneck::Cloud;
  }
  if (result.up_ms > largest) {
    largest = result.up_ms;
    result.bottleneck = ResourceBottleneck::Up;
  }
  if (result.down_ms > largest) {
    result.bottleneck = ResourceBottleneck::Down;
  }
  return result;
}
