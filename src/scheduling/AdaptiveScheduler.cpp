#include <scheduler/scheduling/AdaptiveScheduler.hpp>

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

constexpr int max_request_count = 2000;
constexpr int pipeline_batch_count = 4;

bool hasPrefillWork(const WorldState& world) {
  return std::any_of(world.requests.begin(), world.requests.end(), [](const Request& request) {
    return classifyRequest(request) == RequestClass::Prefill;
  });
}

int countDecodePopulation(const WorldState& world) {
  return static_cast<int>(std::count_if(
    world.requests.begin(), world.requests.end(), [](const Request& request) {
      const RequestClass request_class = classifyRequest(request);
      return request_class == RequestClass::Cold || request_class == RequestClass::Hot;
    }));
}

bool hasPendingDecodeUpload(const WorldState& world, std::optional<int> remote) {
  return std::any_of(world.requests.begin(), world.requests.end(), [&](const Request& request) {
    const bool matches_remote = !remote.has_value() || request.remote == remote;
    return matches_remote
      && (request.state == RequestState::WaitingDecodePreDone
        || request.state == RequestState::WaitingDecodeUpload);
  });
}

bool hasPendingDecodeDownload(const WorldState& world) {
  return std::any_of(world.requests.begin(), world.requests.end(), [](const Request& request) {
    return request.state == RequestState::ReadyDecodeProc
      || request.state == RequestState::WaitingDecodeProcDone
      || request.state == RequestState::WaitingDecodeDownload;
  });
}

bool waitingSlosComfortablyMet(
  const WorldState& world, const ScoreAwareSchedulerConfig& waiting_policy) {
  if (world.observed_tdr_count == 0 || world.observed_tpot_count == 0) {
    return false;
  }
  const double observed_tdr = world.observed_tdr_sum / world.observed_tdr_count;
  const double observed_tpot = world.observed_tpot_sum / world.observed_tpot_count;
  return observed_tdr <= 0.8 * waiting_policy.slo_tdr
    && observed_tpot <= 0.8 * waiting_policy.slo_tpot;
}

class ThroughputDecodeSelector {
public:
  explicit ThroughputDecodeSelector(const AdaptiveSchedulerConfig& config)
    : config_(config) {}

  std::vector<int> select(
    const WorldState& world, RequestState state, std::optional<int> remote) const {
    std::vector<int> ready = scheduler_detail::findRequests(
      world, state, remote, std::numeric_limits<std::size_t>::max());
    if (ready.empty() || shouldWaitForBatch(world, state, remote, ready.size())) {
      return {};
    }

    if (state == RequestState::ReadyDecodePre) {
      const int available_admissions =
        std::max(0, config_.target_hot_set_size - countActiveHotSet(world));
      int remaining_admissions = available_admissions;
      ready.erase(std::remove_if(ready.begin(), ready.end(), [&](int rid) {
        const Request& request = world.requests.at(static_cast<std::size_t>(rid));
        if (classifyRequest(request) != RequestClass::Cold) {
          return false;
        }
        if (remaining_admissions == 0) {
          return true;
        }
        --remaining_admissions;
        return false;
      }), ready.end());
    }

    if (!ready.empty()) {
      ready.resize(static_cast<std::size_t>(
        selectDecodeBatchSize(config_.throughput_decode_batches, state, ready.size())));
    }
    return ready;
  }

  bool preferPrefillPostBeforeDecodePre(const WorldState&) const {
    return true;
  }

  bool preferPrefillPreBeforeDecodePre(const WorldState& world) const {
    return countDecodePopulation(world) < config_.target_hot_set_size;
  }

  bool preferPrefillProcBeforeDecodeProc(const WorldState& world, int) const {
    return countDecodePopulation(world) < config_.target_hot_set_size;
  }

private:
  bool shouldWaitForBatch(
    const WorldState& world,
    RequestState state,
    std::optional<int> remote,
    std::size_t ready_count) const {
    const int preferred_size = state == RequestState::ReadyDecodeProc
      ? config_.preferred_cloud_batch_size
      : config_.preferred_decode_batch_size;
    if (ready_count >= static_cast<std::size_t>(preferred_size)) {
      return false;
    }
    if (state == RequestState::ReadyDecodePre) {
      return countDecodePopulation(world) < config_.target_hot_set_size && hasPrefillWork(world);
    }
    if (state == RequestState::ReadyDecodeProc) {
      return hasPendingDecodeUpload(world, remote);
    }
    assert(state == RequestState::ReadyDecodePost);
    return hasPendingDecodeDownload(world);
  }

  const AdaptiveSchedulerConfig& config_;
};

std::vector<Assignment> chooseAdaptiveAssignmentsImpl(
  const WorldState& world,
  int num_layers,
  const AdaptiveSchedulerConfig& config,
  const PrefillRemoteSelector* remote_selector) {
  const auto choose_batched = [&] {
    return remote_selector == nullptr
      ? chooseBatchedAssignments(world, num_layers, config.baseline_decode_batches)
      : chooseBatchedAssignments(world, num_layers, config.baseline_decode_batches, *remote_selector);
  };
  const auto choose_score_aware = [&] {
    return remote_selector == nullptr
      ? chooseScoreAwareAssignments(world, num_layers, config.waiting_policy)
      : chooseScoreAwareAssignments(world, num_layers, config.waiting_policy, *remote_selector);
  };

  if (config.regime == AdaptiveScoreRegime::Balanced) {
    return choose_batched();
  }
  if (config.regime == AdaptiveScoreRegime::HardSlo) {
    return choose_score_aware();
  }
  if (config.regime == AdaptiveScoreRegime::Waiting) {
    return waitingSlosComfortablyMet(world, config.waiting_policy)
      ? choose_batched()
      : choose_score_aware();
  }
  const ThroughputDecodeSelector decode_selector{config};
  return remote_selector == nullptr
    ? scheduler_detail::chooseAssignments(world, num_layers, decode_selector)
    : scheduler_detail::chooseAssignments(world, num_layers, decode_selector, *remote_selector);
}

} // namespace

AdaptiveScoreRegime classifyScoreRegime(const SystemConfig& system) {
  assert(system.w_tp >= 0.0);
  assert(system.w_c >= 0.0);
  if (system.dist_base == 0.0 && system.w_c >= 0.1) {
    return AdaptiveScoreRegime::HardSlo;
  }
  if (system.w_tp >= 0.8) {
    return AdaptiveScoreRegime::Throughput;
  }
  if (system.w_c >= 0.7) {
    return AdaptiveScoreRegime::Waiting;
  }
  return AdaptiveScoreRegime::Balanced;
}

double estimateDecodeCapacity(
  const SystemConfig& system, const TimingCurves& curves, int total_batch_size) {
  assert(total_batch_size >= 1);
  const int active_clouds = std::min(system.K, total_batch_size);
  const int cloud_batch_size = (total_batch_size + active_clouds - 1) / active_clouds;
  const double edge_service_time =
    2.0 * system.S
    + interpolate(curves.decode_pre, total_batch_size)
    + interpolate(curves.decode_post, total_batch_size);
  const double cloud_service_time = system.S + interpolate(curves.decode_proc, cloud_batch_size);
  const double serialized_bytes_time =
    8.0 * static_cast<double>(total_batch_size) * system.bytes_per_token
    / (system.bandwidth_gbps * 1'000'000.0);
  const double link_service_time = active_clouds * system.latency_in_ms + serialized_bytes_time;
  const double bottleneck_time = std::max({edge_service_time, cloud_service_time, link_service_time});
  return total_batch_size / bottleneck_time;
}

AdaptiveSchedulerConfig buildAdaptiveSchedulerConfig(
  const SystemConfig& system, const TimingCurves& curves) {
  DecodeBatchPolicy baseline = buildDecodeBatchPolicy(curves, system.S);
  const AdaptiveScoreRegime regime = classifyScoreRegime(system);
  const std::optional<int> waiting_target = regime == AdaptiveScoreRegime::HardSlo
    ? std::optional<int>{system.K}
    : std::nullopt;
  ScoreAwareSchedulerConfig waiting_policy =
    buildScoreAwareSchedulerConfig(system, curves, waiting_target);
  waiting_policy.prefill_warmup =
    regime == AdaptiveScoreRegime::HardSlo && system.w_tp <= 0.1;
  const double safety_margin = 1.05 + 0.1 * system.w_tp;
  const double throughput_target = system.tp_UB * safety_margin;

  int preferred_batch_size = 1;
  double preferred_capacity = estimateDecodeCapacity(system, curves, 1);
  bool reached_target = preferred_capacity >= throughput_target;
  for (int batch_size = 2; batch_size <= max_request_count; ++batch_size) {
    const double capacity = estimateDecodeCapacity(system, curves, batch_size);
    if (!reached_target && capacity > preferred_capacity) {
      preferred_batch_size = batch_size;
      preferred_capacity = capacity;
    }
    if (!reached_target && capacity >= throughput_target) {
      preferred_batch_size = batch_size;
      preferred_capacity = capacity;
      reached_target = true;
    }
  }

  const int cloud_batch_size =
    (preferred_batch_size + std::min(system.K, preferred_batch_size) - 1)
    / std::min(system.K, preferred_batch_size);
  const int throughput_hot_target = std::min(
    max_request_count,
    std::max(2 * system.K, pipeline_batch_count * preferred_batch_size));
  const int hot_target = regime == AdaptiveScoreRegime::Throughput
    ? throughput_hot_target
    : waiting_policy.target_hot_set_size;

  return {
    .baseline_decode_batches = baseline,
    .throughput_decode_batches = std::move(baseline),
    .waiting_policy = waiting_policy,
    .regime = regime,
    .preferred_decode_batch_size = preferred_batch_size,
    .preferred_cloud_batch_size = cloud_batch_size,
    .target_hot_set_size = hot_target,
    .estimated_decode_capacity = preferred_capacity,
    .throughput_target = throughput_target,
  };
}

std::vector<Assignment> chooseAdaptiveAssignments(
  const WorldState& world, int num_layers, const AdaptiveSchedulerConfig& config) {
  return chooseAdaptiveAssignmentsImpl(world, num_layers, config, nullptr);
}

std::vector<Assignment> chooseAdaptiveAssignments(
  const WorldState& world,
  int num_layers,
  const AdaptiveSchedulerConfig& config,
  const PrefillRemoteSelector& remote_selector) {
  return chooseAdaptiveAssignmentsImpl(world, num_layers, config, &remote_selector);
}
