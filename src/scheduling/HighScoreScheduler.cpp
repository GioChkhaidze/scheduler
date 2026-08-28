#include <scheduler/scheduling/HighScoreScheduler.hpp>

#include "SchedulerCore.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace {

enum class RuntimePolicy {
  Waiting,
  Throughput
};

constexpr std::size_t bottleneckIndex(ResourceBottleneck bottleneck) {
  return static_cast<std::size_t>(bottleneck);
}

int decodePopulation(const WorldState& world) {
  return static_cast<int>(std::count_if(
    world.requests.begin(), world.requests.end(), [](const Request& request) {
      const RequestClass request_class = classifyRequest(request);
      return request_class == RequestClass::Cold || request_class == RequestClass::Hot;
    }));
}

int cloudDecodeSupply(const WorldState& world) {
  return static_cast<int>(std::count_if(
    world.requests.begin(), world.requests.end(), [](const Request& request) {
      switch (request.state) {
        case RequestState::WaitingDecodePreDone:
        case RequestState::WaitingDecodeUpload:
        case RequestState::ReadyDecodeProc:
        case RequestState::WaitingDecodeProcDone:
          return true;
        default:
          return false;
      }
    }));
}

bool hasObservedArrivalBurst(const WorldState& world) {
  for (std::size_t index = 1; index < world.requests.size(); ++index) {
    if (world.requests[index - 1].arrival_time == world.requests[index].arrival_time) {
      return true;
    }
  }
  return false;
}

double currentThroughput(const WorldState& world) {
  if (world.requests.empty()) {
    return 0.0;
  }
  int tokens = 0;
  double earliest = world.current_time;
  for (const Request& request : world.requests) {
    tokens += request.tokens_produced;
    earliest = std::min(earliest, request.arrival_time);
  }
  const double elapsed = world.current_time - earliest;
  return elapsed > 0.0 ? tokens / elapsed : 0.0;
}

bool waitingComponentSaturated(const WorldState& world, const SystemConfig& system, const ScorePressure& pressure) {
  if (pressure.maximum_tdr > 1.0 || pressure.maximum_tpot > 1.0) {
    return false;
  }
  if (world.observed_tdr_count > 0
      && world.observed_tdr_sum / world.observed_tdr_count > system.SLO1) {
    return false;
  }
  if (world.observed_tpot_count > 0
      && world.observed_tpot_sum / world.observed_tpot_count > system.SLO2) {
    return false;
  }
  return world.observed_tdr_count > 0;
}

double observedWaitingDistance(const WorldState& world, const SystemConfig& system) {
  const double mean_tdr = world.observed_tdr_count > 0
    ? world.observed_tdr_sum / world.observed_tdr_count
    : 0.0;
  const double mean_tpot = world.observed_tpot_count > 0
    ? world.observed_tpot_sum / world.observed_tpot_count
    : 0.0;
  const double excess_tdr = std::max(0.0, mean_tdr / system.SLO1 - 1.0);
  const double excess_tpot = std::max(0.0, mean_tpot / system.SLO2 - 1.0);
  return std::hypot(excess_tdr, excess_tpot);
}

bool intrinsicHardSloFeasible(const WorldState& world, const HighScoreSchedulerConfig& config) {
  const double decode_floor = 3.0 * config.system.S
    + interpolate(config.curves.decode_pre, 1)
    + interpolate(config.curves.decode_proc, 1)
    + interpolate(config.curves.decode_post, 1)
    + 2.0 * transferTime(config.system, 1);
  if (decode_floor > config.system.SLO2) {
    return false;
  }
  for (const Request& request : world.requests) {
    if (classifyRequest(request) != RequestClass::Prefill) {
      continue;
    }
    const double prefill_floor = 3.0 * config.system.S
      + interpolate(config.curves.prefill_pre, request.input_length)
      + interpolate(config.curves.prefill_proc, request.input_length)
      + interpolate(config.curves.prefill_post, request.input_length)
      + 2.0 * transferTime(config.system, request.input_length);
    if (prefill_floor > config.system.SLO1) {
      return false;
    }
  }
  return true;
}

bool waitingEvidenceRequiresSpecialization(
  const WorldState& world, HighScoreSchedulerConfig& config) {
  if (config.controller.waiting_specialization_latched) {
    return true;
  }
  if (config.score_regime == HighScoreRegime::HardSlo) {
    const bool intrinsically_tight = !intrinsicHardSloFeasible(world, config);
    config.controller.waiting_specialization_latched = intrinsically_tight;
    return intrinsically_tight;
  }
  if (world.observed_tdr_count == 0 && world.observed_tpot_count == 0) {
    return false;
  }
  const double distance = observedWaitingDistance(world, config.system);
  const bool material = config.system.dist_base > 0.0
    && distance >= 0.25 * config.system.dist_base;
  config.controller.waiting_specialization_latched = material;
  return material;
}

RuntimePolicy selectRuntimePolicy(
  const WorldState& world, const ScorePressure& pressure, HighScoreSchedulerConfig& config) {
  if (config.mode == HighScoreMode::Waiting) {
    return RuntimePolicy::Waiting;
  }
  if (config.mode == HighScoreMode::Throughput) {
    return RuntimePolicy::Throughput;
  }
  if (config.score_regime == HighScoreRegime::ThroughputOnly) {
    return RuntimePolicy::Throughput;
  }
  if (config.score_regime == HighScoreRegime::WaitingOnly
      || config.score_regime == HighScoreRegime::HardSlo) {
    return RuntimePolicy::Waiting;
  }
  const bool throughput_saturated = currentThroughput(world) >= config.system.tp_UB;
  const bool waiting_saturated = waitingComponentSaturated(world, config.system, pressure);
  if (throughput_saturated && !waiting_saturated) {
    return RuntimePolicy::Waiting;
  }
  if (waiting_saturated && !throughput_saturated) {
    return RuntimePolicy::Throughput;
  }

  const double throughput_gap = std::clamp(
    (config.system.tp_UB - currentThroughput(world))
      / (config.system.tp_UB - config.system.tp_base),
    0.0,
    1.0);
  const double waiting_excess = std::max(0.0, pressure.maximum_tdr - 1.0)
    + std::max(0.0, pressure.maximum_tpot - 1.0);
  return config.system.w_c * waiting_excess > config.system.w_tp * throughput_gap
    ? RuntimePolicy::Waiting
    : RuntimePolicy::Throughput;
}

RuntimePolicy stabilizeRuntimePolicy(RuntimePolicy requested, HighScoreSchedulerConfig& config) {
  if (!config.features.runtime_policy_hysteresis
      || config.score_regime != HighScoreRegime::Balanced) {
    return requested;
  }
  BottleneckControllerState& state = config.controller;
  const bool requested_waiting = requested == RuntimePolicy::Waiting;
  if (!state.runtime_policy_initialized) {
    state.runtime_policy_initialized = true;
    state.active_runtime_policy_waiting = requested_waiting;
    state.candidate_runtime_policy_waiting = requested_waiting;
    state.runtime_policy_candidate_streak = 1;
  } else if (state.candidate_runtime_policy_waiting == requested_waiting) {
    ++state.runtime_policy_candidate_streak;
  } else {
    state.candidate_runtime_policy_waiting = requested_waiting;
    state.runtime_policy_candidate_streak = 1;
  }
  if (state.runtime_policy_candidate_streak >= 3
      && state.active_runtime_policy_waiting != state.candidate_runtime_policy_waiting) {
    state.active_runtime_policy_waiting = state.candidate_runtime_policy_waiting;
    ++config.telemetry.runtime_policy_switches;
  }
  return state.active_runtime_policy_waiting ? RuntimePolicy::Waiting : RuntimePolicy::Throughput;
}

void updateController(
  const SchedulerBacklog& backlog, RuntimePolicy policy, HighScoreSchedulerConfig& config) {
  HighScoreTelemetry& telemetry = config.telemetry;
  ++telemetry.bottleneck_observations[bottleneckIndex(backlog.bottleneck)];
  BottleneckControllerState& state = config.controller;
  if (!config.features.bottleneck_controller) {
    state.active = ResourceBottleneck::None;
  } else if (state.candidate == backlog.bottleneck) {
    ++state.candidate_streak;
  } else {
    state.candidate = backlog.bottleneck;
    state.candidate_streak = 1;
  }
  if (config.features.bottleneck_controller && state.candidate_streak >= 2
      && state.active != state.candidate) {
    state.active = state.candidate;
    ++telemetry.bottleneck_switches;
  }

  const int base_target = policy == RuntimePolicy::Waiting
    ? config.waiting_hot_target
    : config.throughput_hot_target;
  state.target_hot_set_size = base_target;
  state.active_cloud_count = config.system.K;
  if (config.features.link_cloud_limit
      && (state.active == ResourceBottleneck::Up || state.active == ResourceBottleneck::Down)) {
    state.active_cloud_count = config.throughput_active_clouds;
    const bool expand_for_waiting_prefill = config.features.waiting_prefill_cloud_expansion
      && config.score_regime == HighScoreRegime::WaitingOnly;
    if (config.features.prefill_aware_cloud_limit || expand_for_waiting_prefill) {
      const double total_demand = backlog.prefill_demand_ms + backlog.hot_demand_ms;
      if (total_demand > 0.0) {
        const int prefill_clouds = static_cast<int>(std::ceil(
          config.system.K * backlog.prefill_demand_ms / total_demand));
        state.active_cloud_count = std::max(state.active_cloud_count, prefill_clouds);
      }
    }
    state.target_hot_set_size = std::max(
      state.active_cloud_count,
      std::min(base_target, config.throughput_active_clouds * config.v7.multiprocessor.adaptive
        .preferred_cloud_batch_size));
  } else if (state.active == ResourceBottleneck::Cloud) {
    state.target_hot_set_size = std::max(base_target, config.system.K);
  }
}

double starvationHorizon(const HighScoreSchedulerConfig& config) {
  return config.system.SLO1
    + static_cast<double>(std::max(1, config.controller.target_hot_set_size)) * config.system.SLO2;
}

bool isAged(const WorldState& world, const Request& request, const HighScoreSchedulerConfig& config) {
  return world.current_time - request.arrival_time >= starvationHorizon(config);
}

bool hasReadyHot(const WorldState& world, RequestState state, std::optional<int> remote) {
  return std::any_of(world.requests.begin(), world.requests.end(), [&](const Request& request) {
    return request.state == state && (!remote.has_value() || request.remote == remote)
      && classifyRequest(request) == RequestClass::Hot;
  });
}

struct HotCoalescingOpportunity {
  double download_completion;
  int cold_count;
  int hot_count;
  int post_count;
};

std::optional<HotCoalescingOpportunity> findHotCoalescingOpportunity(const WorldState& world) {
  const int all_cold = static_cast<int>(std::count_if(
    world.requests.begin(), world.requests.end(), [](const Request& request) {
      return request.state == RequestState::ReadyDecodePre
        && classifyRequest(request) == RequestClass::Cold;
    }));
  std::optional<HotCoalescingOpportunity> result;
  for (const CommittedLinkTransfer& transfer : world.links.down.committed) {
    if (transfer.transfer.stage != TransferStage::Decode) {
      continue;
    }
    const int local_cold = static_cast<int>(std::count_if(
      world.requests.begin(), world.requests.end(), [&](const Request& request) {
        return request.state == RequestState::ReadyDecodePre
          && request.remote == transfer.transfer.remote
          && classifyRequest(request) == RequestClass::Cold;
      }));
    const int hot = static_cast<int>(std::count_if(
      transfer.transfer.rids.begin(), transfer.transfer.rids.end(), [&](int rid) {
        const Request& request = world.requests.at(static_cast<std::size_t>(rid));
        return request.state == RequestState::WaitingDecodeDownload
          && classifyRequest(request) == RequestClass::Hot;
      }));
    if (local_cold == 0 || local_cold != all_cold || hot == 0) {
      continue;
    }
    HotCoalescingOpportunity candidate{
      transfer.completes_at, local_cold, hot, static_cast<int>(transfer.transfer.rids.size())};
    if (!result.has_value() || candidate.download_completion < result->download_completion) {
      result = candidate;
    }
  }
  return result;
}

bool profitableHotCoalescing(const WorldState& world, const HighScoreSchedulerConfig& config) {
  const std::optional<HotCoalescingOpportunity> opportunity = findHotCoalescingOpportunity(world);
  if (!opportunity.has_value()) {
    return false;
  }
  const double cold_task_end = world.current_time + config.system.S
    + interpolate(config.curves.decode_pre, opportunity->cold_count);
  const double separate_hot_ready = std::max(opportunity->download_completion, cold_task_end)
    + config.system.S + interpolate(config.curves.decode_post, opportunity->post_count);
  const double separate_hot_task_end = separate_hot_ready + config.system.S
    + interpolate(config.curves.decode_pre, opportunity->hot_count);
  const double cold_upload_end = std::max(world.links.up.committed_tail, cold_task_end)
    + transferTime(config.system, opportunity->cold_count);
  const double separate_end = std::max(cold_upload_end, separate_hot_task_end)
    + transferTime(config.system, opportunity->hot_count);

  const double batch_hot_ready = opportunity->download_completion + config.system.S
    + interpolate(config.curves.decode_post, opportunity->post_count);
  const int batch_count = opportunity->cold_count + opportunity->hot_count;
  const double local_decode_service = 3.0 * config.system.S
    + interpolate(config.curves.decode_pre, 1)
    + interpolate(config.curves.decode_proc, 1)
    + interpolate(config.curves.decode_post, 1);
  if (batch_hot_ready - world.current_time > batch_count * local_decode_service) {
    return false;
  }
  const double batch_task_end = batch_hot_ready + config.system.S
    + interpolate(config.curves.decode_pre, batch_count);
  const double batch_end = std::max(world.links.up.committed_tail, batch_task_end)
    + transferTime(config.system, batch_count);
  const double safety_margin = 0.05 * config.system.latency_in_ms;
  return separate_end - batch_end > safety_margin;
}

const TimingCurve& decodeCurve(const TimingCurves& curves, RequestState state) {
  if (state == RequestState::ReadyDecodePre) {
    return curves.decode_pre;
  }
  if (state == RequestState::ReadyDecodeProc) {
    return curves.decode_proc;
  }
  assert(state == RequestState::ReadyDecodePost);
  return curves.decode_post;
}

int waitingBatchSize(
  const WorldState& world,
  RequestState state,
  const std::vector<int>& selected,
  const HighScoreSchedulerConfig& config) {
  assert(!selected.empty());
  double earliest_deadline = std::numeric_limits<double>::infinity();
  for (const int rid : selected) {
    const Request& request = world.requests.at(static_cast<std::size_t>(rid));
    if (classifyRequest(request) == RequestClass::Hot && request.last_token_time.has_value()) {
      earliest_deadline = std::min(earliest_deadline, *request.last_token_time + config.system.SLO2);
    }
  }
  if (!std::isfinite(earliest_deadline) || earliest_deadline <= world.current_time) {
    return selectDecodeBatchSize(config.decode_batches, state, selected.size());
  }

  int result = 1;
  const double slack = earliest_deadline - world.current_time;
  const TimingCurve& curve = decodeCurve(config.curves, state);
  for (std::size_t batch_size = 1; batch_size <= selected.size(); ++batch_size) {
    if (config.system.S + interpolate(curve, static_cast<int>(batch_size)) <= slack) {
      result = static_cast<int>(batch_size);
    }
  }
  return result;
}

struct DecodeCandidate {
  int rid;
  bool hot;
  bool aged;
  double urgency;
};

class HighScoreDecodeSelector {
public:
  HighScoreDecodeSelector(
    HighScoreSchedulerConfig& config, RuntimePolicy policy, ScorePressure pressure)
    : config_(config), policy_(policy), pressure_(pressure) {}

  std::vector<int> select(
    const WorldState& world, RequestState state, std::optional<int> remote) const {
    std::vector<int> ready = scheduler_detail::findRequests(
      world, state, remote, std::numeric_limits<std::size_t>::max());
    std::vector<DecodeCandidate> candidates;
    candidates.reserve(ready.size());
    for (const int rid : ready) {
      const Request& request = world.requests.at(static_cast<std::size_t>(rid));
      const bool hot = classifyRequest(request) == RequestClass::Hot;
      const bool aged = isAged(world, request, config_);
      const double origin = hot
        ? request.last_token_time.value_or(request.arrival_time)
        : request.arrival_time;
      const double target = hot ? config_.system.SLO2 : starvationHorizon(config_);
      candidates.push_back({rid, hot, aged, (world.current_time - origin) / target});
    }
    std::sort(candidates.begin(), candidates.end(), [](const DecodeCandidate& left, const DecodeCandidate& right) {
      if (left.aged != right.aged) {
        return left.aged;
      }
      if (left.hot != right.hot) {
        return left.hot;
      }
      return left.urgency != right.urgency ? left.urgency > right.urgency : left.rid < right.rid;
    });

    int cold_admissions = std::numeric_limits<int>::max();
    if (config_.features.cold_admission && state == RequestState::ReadyDecodePre) {
      cold_admissions = std::max(0, config_.controller.target_hot_set_size - countActiveHotSet(world));
      if (policy_ == RuntimePolicy::Waiting && pressure_.maximum_tpot > 1.0
          && pressure_.tpot >= pressure_.tdr) {
        cold_admissions = 0;
      }
      if (countActiveHotSet(world) < config_.controller.active_cloud_count) {
        cold_admissions = std::max(cold_admissions, 1);
      }
      if (config_.features.throughput_idle_admission
          && config_.score_regime == HighScoreRegime::ThroughputOnly
          && decodePopulation(world) <= 2 * config_.system.K
          && hasObservedArrivalBurst(world)) {
        const int cloud_deficit = config_.controller.active_cloud_count - cloudDecodeSupply(world);
        cold_admissions = std::max(cold_admissions, cloud_deficit);
      }
      if (config_.features.throughput_hot_coalescing && policy_ == RuntimePolicy::Throughput
          && config_.score_regime == HighScoreRegime::ThroughputOnly
          && !hasReadyHot(world, RequestState::ReadyDecodePre, std::nullopt)) {
        if (profitableHotCoalescing(world, config_)) {
          cold_admissions = 0;
          ++config_.telemetry.hot_coalescing_waits;
        }
      }
    }

    std::vector<int> selected;
    selected.reserve(candidates.size());
    bool admitted_aged = false;
    for (const DecodeCandidate& candidate : candidates) {
      if (state == RequestState::ReadyDecodePre && !candidate.hot) {
        if (cold_admissions == 0 && !(candidate.aged && !admitted_aged)) {
          ++config_.telemetry.cold_throttles;
          continue;
        }
        if (candidate.aged && cold_admissions == 0) {
          admitted_aged = true;
          ++config_.telemetry.aged_promotions;
        } else {
          --cold_admissions;
        }
        ++config_.telemetry.cold_admissions;
      }
      selected.push_back(candidate.rid);
    }
    if (selected.empty()) {
      return selected;
    }
    const int batch_size = policy_ == RuntimePolicy::Throughput || !config_.features.deadline_batching
      ? selectDecodeBatchSize(config_.decode_batches, state, selected.size())
      : waitingBatchSize(world, state, selected, config_);
    selected.resize(static_cast<std::size_t>(batch_size));
    return selected;
  }

  bool preferPrefillPostBeforeDecodePre(const WorldState& world) const {
    if (policy_ == RuntimePolicy::Throughput) {
      return true;
    }
    if (hasReadyHot(world, RequestState::ReadyDecodePre, std::nullopt)) {
      return pressure_.tdr >= pressure_.tpot;
    }
    return pressure_.tdr > 0.0 || countActiveHotSet(world) >= config_.controller.target_hot_set_size;
  }

  bool preferPrefillPreBeforeDecodePre(const WorldState& world) const {
    if (hasReadyHot(world, RequestState::ReadyDecodePre, std::nullopt)) {
      return false;
    }
    if (hasAgedPrefill(world, std::nullopt)) {
      ++config_.telemetry.aged_promotions;
      return true;
    }
    return policy_ == RuntimePolicy::Throughput
      ? decodePopulation(world) < config_.controller.target_hot_set_size
      : pressure_.tdr >= pressure_.tpot;
  }

  bool preferPrefillProcBeforeDecodeProc(const WorldState& world, int remote) const {
    if (hasReadyHot(world, RequestState::ReadyDecodeProc, remote)) {
      return false;
    }
    if (hasAgedPrefill(world, remote)) {
      ++config_.telemetry.aged_promotions;
      return true;
    }
    return policy_ == RuntimePolicy::Throughput
      ? decodePopulation(world) < config_.controller.target_hot_set_size
      : pressure_.tdr > pressure_.tpot;
  }

private:
  bool hasAgedPrefill(const WorldState& world, std::optional<int> remote) const {
    return std::any_of(world.requests.begin(), world.requests.end(), [&](const Request& request) {
      if (classifyRequest(request) != RequestClass::Prefill || !isAged(world, request, config_)) {
        return false;
      }
      if (!remote.has_value()) {
        return request.state == RequestState::ReadyPrefillPre;
      }
      return request.remote == remote && request.state == RequestState::ReadyPrefillProc;
    });
  }

  HighScoreSchedulerConfig& config_;
  RuntimePolicy policy_;
  ScorePressure pressure_;
};

class ActiveCloudRemoteSelector {
public:
  explicit ActiveCloudRemoteSelector(const HighScoreSchedulerConfig& config)
    : config_(config) {}

  int operator()(const WorldState& world, int rid) const {
    const std::vector<double> scores = estimatePlacementScores(world, rid, config_.v7.multiprocessor);
    const int requested = config_.features.link_cloud_limit
      ? config_.controller.active_cloud_count
      : static_cast<int>(scores.size());
    const int active = std::clamp(requested, 1, static_cast<int>(scores.size()));
    int best = rid % active;
    for (int remote = 0; remote < active; ++remote) {
      if (scores[static_cast<std::size_t>(remote)] < scores[static_cast<std::size_t>(best)]) {
        best = remote;
      }
    }
    return best;
  }

private:
  const HighScoreSchedulerConfig& config_;
};

} // namespace

HighScoreRegime classifyHighScoreRegime(const SystemConfig& system) {
  if (system.w_c == 0.0) {
    return HighScoreRegime::ThroughputOnly;
  }
  if (system.dist_base == 0.0) {
    return HighScoreRegime::HardSlo;
  }
  if (system.w_tp == 0.0) {
    return HighScoreRegime::WaitingOnly;
  }
  return HighScoreRegime::Balanced;
}

int selectThroughputActiveCloudCount(
  const SystemConfig& system, const TimingCurves& curves, int batch_size) {
  assert(batch_size > 0);
  int best_clouds = 1;
  double best_capacity = -1.0;
  for (int clouds = 1; clouds <= std::min(system.K, batch_size); ++clouds) {
    const int cloud_batch = (batch_size + clouds - 1) / clouds;
    const double edge = 2.0 * system.S
      + interpolate(curves.decode_pre, batch_size)
      + interpolate(curves.decode_post, batch_size);
    const double cloud = system.S + interpolate(curves.decode_proc, cloud_batch);
    const double link = clouds * system.latency_in_ms
      + 8.0 * static_cast<double>(batch_size) * system.bytes_per_token
        / (system.bandwidth_gbps * 1'000'000.0);
    const double capacity = batch_size / std::max({edge, cloud, link});
    if (capacity > best_capacity + 1e-12) {
      best_capacity = capacity;
      best_clouds = clouds;
    }
  }
  return best_clouds;
}

ResourceBottleneck estimateStaticDecodeBottleneck(
  const SystemConfig& system, const TimingCurves& curves, int batch_size, int active_clouds) {
  assert(batch_size > 0);
  assert(active_clouds > 0 && active_clouds <= system.K);
  const int cloud_batch = (batch_size + active_clouds - 1) / active_clouds;
  const double edge = 2.0 * system.S
    + interpolate(curves.decode_pre, batch_size)
    + interpolate(curves.decode_post, batch_size);
  const double cloud = system.S + interpolate(curves.decode_proc, cloud_batch);
  const double link = active_clouds * system.latency_in_ms
    + 8.0 * static_cast<double>(batch_size) * system.bytes_per_token
      / (system.bandwidth_gbps * 1'000'000.0);
  if (link >= edge && link >= cloud) {
    return ResourceBottleneck::Up;
  }
  return edge >= cloud ? ResourceBottleneck::Edge : ResourceBottleneck::Cloud;
}

HighScoreSchedulerConfig buildHighScoreSchedulerConfig(
  const SystemConfig& system,
  const TimingCurves& curves,
  HighScoreMode mode,
  HighScoreFeatures features) {
  const SharedLinkSchedulerConfig v7 = buildSharedLinkSchedulerConfig(system, curves);
  const AdaptiveSchedulerConfig& adaptive = v7.multiprocessor.adaptive;
  const int capacity_target = std::clamp(
    static_cast<int>(std::floor(adaptive.estimated_decode_capacity * system.SLO2)),
    1,
    4096);
  const int waiting_target = std::max(system.K, capacity_target);
  const int throughput_clouds = selectThroughputActiveCloudCount(
    system, curves, adaptive.preferred_decode_batch_size);
  HighScoreSchedulerConfig result{
    .v7 = v7,
    .system = system,
    .curves = curves,
    .decode_batches = adaptive.baseline_decode_batches,
    .mode = mode,
    .score_regime = classifyHighScoreRegime(system),
    .features = features,
    .waiting_hot_target = waiting_target,
    .throughput_hot_target = adaptive.target_hot_set_size,
    .throughput_active_clouds = throughput_clouds,
    .estimated_decode_capacity = adaptive.estimated_decode_capacity,
    .static_decode_bottleneck = estimateStaticDecodeBottleneck(
      system, curves, adaptive.preferred_decode_batch_size, throughput_clouds),
    .controller = {},
    .telemetry = {},
  };
  result.controller.target_hot_set_size = mode == HighScoreMode::Waiting
    ? result.waiting_hot_target
    : result.throughput_hot_target;
  result.controller.active_cloud_count = system.K;
  return result;
}

HighScoreSchedulerConfig buildFinalSchedulerConfig(const SystemConfig& system, const TimingCurves& curves) {
  HighScoreFeatures features;
  features.evidence_gated_waiting = true;
  features.proactive_waiting = true;
  features.waiting_prefill_cloud_expansion = true;
  features.runtime_policy_hysteresis = true;
  features.single_cloud_specialization = true;
  features.throughput_idle_admission = true;
  features.throughput_hot_coalescing = true;
  return buildHighScoreSchedulerConfig(system, curves, HighScoreMode::Adaptive, features);
}

std::vector<Assignment> chooseHighScoreAssignments(
  const WorldState& world, int num_layers, HighScoreSchedulerConfig& config) {
  assert(num_layers == config.system.num_layers);
  if (config.mode == HighScoreMode::Control
      || (!config.features.waiting_policy && !config.features.throughput_policy)) {
    ++config.telemetry.control_decisions;
    return chooseSharedLinkAssignments(world, num_layers, config.v7);
  }

  const ScorePressure pressure = estimateScorePressure(world, config.system);
  RuntimePolicy policy = stabilizeRuntimePolicy(selectRuntimePolicy(world, pressure, config), config);
  const bool gated_waiting_only = config.score_regime == HighScoreRegime::WaitingOnly
    && !config.features.proactive_waiting;
  const bool gated_hard_slo = config.score_regime == HighScoreRegime::HardSlo;
  if (config.mode == HighScoreMode::Adaptive && config.features.evidence_gated_waiting
      && (gated_waiting_only || gated_hard_slo)
      && policy == RuntimePolicy::Waiting
      && !waitingEvidenceRequiresSpecialization(world, config)) {
    ++config.telemetry.control_decisions;
    return chooseSharedLinkAssignments(world, num_layers, config.v7);
  }
  if ((policy == RuntimePolicy::Waiting && !config.features.waiting_policy)
      || (policy == RuntimePolicy::Throughput && !config.features.throughput_policy)) {
    ++config.telemetry.policy_fallbacks;
    return chooseSharedLinkAssignments(world, num_layers, config.v7);
  }
  if (policy == RuntimePolicy::Waiting) {
    ++config.telemetry.waiting_decisions;
  } else {
    ++config.telemetry.throughput_decisions;
  }

  const SchedulerBacklog backlog = estimateSchedulerBacklog(world, config.system, config.curves);
  updateController(backlog, policy, config);
  const bool static_link_bottleneck = config.static_decode_bottleneck == ResourceBottleneck::Up
    || config.static_decode_bottleneck == ResourceBottleneck::Down;
  const bool specialize_single_cloud = config.features.single_cloud_specialization
    && config.system.K == 1;
  if (config.mode == HighScoreMode::Adaptive && policy == RuntimePolicy::Throughput
      && !static_link_bottleneck
      && !specialize_single_cloud) {
    ++config.telemetry.control_decisions;
    return chooseSharedLinkAssignments(world, num_layers, config.v7);
  }
  HighScoreDecodeSelector selector{config, policy, pressure};
  ActiveCloudRemoteSelector remote_selector{config};
  const std::vector<Assignment> baseline = scheduler_detail::chooseAssignments(
    world, num_layers, selector, remote_selector);
  std::vector<Assignment> result = applySharedLinkPolicy(world, num_layers, baseline, config.v7);
  if (result.empty() && !baseline.empty() && !hasGuaranteedSchedulerEvent(world)) {
    ++config.telemetry.policy_fallbacks;
    return chooseSharedLinkAssignments(world, num_layers, config.v7);
  }
  return result;
}
