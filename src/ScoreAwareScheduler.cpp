#include <ScoreAwareScheduler.hpp>

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

bool isPrefillState(RequestState state) {
  switch (state) {
    case RequestState::ReadyPrefillPre:
    case RequestState::WaitingPrefillPreDone:
    case RequestState::WaitingPrefillUpload:
    case RequestState::ReadyPrefillProc:
    case RequestState::WaitingPrefillProcDone:
    case RequestState::WaitingPrefillDownload:
    case RequestState::ReadyPrefillPost:
    case RequestState::WaitingPrefillPostDone:
      return true;
    default:
      return false;
  }
}

bool isColdAdmissionInFlight(const Request& request) {
  return classifyRequest(request) == RequestClass::Cold
    && request.state != RequestState::ReadyDecodePre;
}

double normalizedExcess(double elapsed, double target) {
  assert(target > 0.0);
  return std::max(0.0, elapsed / target - 1.0);
}

double hotUrgency(const WorldState& world, const Request& request, double slo_tpot) {
  assert(classifyRequest(request) == RequestClass::Hot);
  assert(request.last_token_time.has_value());
  const double last_token_time = request.last_token_time.value_or(request.arrival_time);
  return (world.current_time - last_token_time) / slo_tpot;
}

double coldUrgency(const WorldState& world, const Request& request, double slo_tdr) {
  return (world.current_time - request.arrival_time) / slo_tdr;
}

struct Candidate {
  int rid;
  RequestClass request_class;
  double urgency;
};

class ScoreAwareDecodeSelector {
public:
  explicit ScoreAwareDecodeSelector(const ScoreAwareSchedulerConfig& config)
    : config_(config) {}

  std::vector<int> select(
    const WorldState& world, RequestState state, std::optional<int> remote) const {
    if (state == RequestState::ReadyDecodePre && shouldWarmUpPrefills(world)) {
      return {};
    }
    const std::vector<int> ready = scheduler_detail::findRequests(
      world, state, remote, std::numeric_limits<std::size_t>::max());
    std::vector<Candidate> candidates;
    candidates.reserve(ready.size());

    for (const int rid : ready) {
      const Request& request = world.requests.at(static_cast<std::size_t>(rid));
      const RequestClass request_class = classifyRequest(request);
      assert(request_class == RequestClass::Cold || request_class == RequestClass::Hot);
      const double urgency = request_class == RequestClass::Hot
        ? hotUrgency(world, request, config_.slo_tpot)
        : coldUrgency(world, request, config_.slo_tdr);
      candidates.push_back({rid, request_class, urgency});
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& left, const Candidate& right) {
      if (left.request_class != right.request_class) {
        return left.request_class == RequestClass::Hot;
      }
      if (left.urgency != right.urgency) {
        return left.urgency > right.urgency;
      }
      return left.rid < right.rid;
    });

    int remaining_cold_admissions = state == RequestState::ReadyDecodePre
      ? std::max(0, config_.target_hot_set_size - countActiveHotSet(world))
      : std::numeric_limits<int>::max();
    std::vector<int> selected;
    selected.reserve(candidates.size());
    for (const Candidate& candidate : candidates) {
      if (state == RequestState::ReadyDecodePre && candidate.request_class == RequestClass::Cold) {
        if (remaining_cold_admissions == 0) {
          continue;
        }
        --remaining_cold_admissions;
      }
      selected.push_back(candidate.rid);
    }

    if (!selected.empty()) {
      selected.resize(static_cast<std::size_t>(
        selectDecodeBatchSize(config_.decode_batches, state, selected.size())));
    }
    return selected;
  }

  bool preferPrefillPostBeforeDecodePre(const WorldState& world) const {
    double tdr_pressure = 0.0;
    double tpot_pressure = 0.0;
    for (const Request& request : world.requests) {
      if (request.state == RequestState::ReadyPrefillPost) {
        tdr_pressure = std::max(
          tdr_pressure,
          normalizedExcess(world.current_time - request.arrival_time, config_.slo_tdr));
      }
      if (request.state == RequestState::ReadyDecodePre && classifyRequest(request) == RequestClass::Hot) {
        tpot_pressure = std::max(
          tpot_pressure,
          normalizedExcess(
            world.current_time - request.last_token_time.value_or(request.arrival_time),
            config_.slo_tpot));
      }
    }
    if (config_.waiting_weight > 0.0 && (tdr_pressure > 0.0 || tpot_pressure > 0.0)) {
      return tdr_pressure > 0.0 && tdr_pressure >= tpot_pressure;
    }
    return countActiveHotSet(world) < config_.target_hot_set_size;
  }

  bool preferPrefillPreBeforeDecodePre(const WorldState& world) const {
    return shouldWarmUpPrefills(world);
  }

  bool preferPrefillProcBeforeDecodeProc(const WorldState& world, int) const {
    return shouldWarmUpPrefills(world);
  }

private:
  bool shouldWarmUpPrefills(const WorldState& world) const {
    return config_.prefill_warmup
      && countActiveHotSet(world) == 0
      && std::any_of(world.requests.begin(), world.requests.end(), [](const Request& request) {
        return classifyRequest(request) == RequestClass::Prefill;
      });
  }

  const ScoreAwareSchedulerConfig& config_;
};

} // namespace

RequestClass classifyRequest(const Request& request) {
  if (request.state == RequestState::Finished) {
    return RequestClass::Finished;
  }
  if (isPrefillState(request.state)) {
    return RequestClass::Prefill;
  }
  return request.tokens_produced == 0 ? RequestClass::Cold : RequestClass::Hot;
}

int countActiveHotSet(const WorldState& world) {
  return static_cast<int>(std::count_if(
    world.requests.begin(), world.requests.end(), [](const Request& request) {
      return classifyRequest(request) == RequestClass::Hot || isColdAdmissionInFlight(request);
    }));
}

ScoreAwareSchedulerConfig buildScoreAwareSchedulerConfig(
  const SystemConfig& system,
  const TimingCurves& curves,
  std::optional<int> target_hot_set_size) {
  DecodeBatchPolicy decode_batches = buildDecodeBatchPolicy(curves, system.S);
  const double singleton_decode_cycle =
    3.0 * system.S
    + interpolate(curves.decode_pre, 1)
    + interpolate(curves.decode_proc, 1)
    + interpolate(curves.decode_post, 1)
    + 2.0 * transferTime(system, 1);
  const double tpot_slack = std::clamp(system.SLO2 / singleton_decode_cycle, 0.5, 4.0);
  const double concurrency_per_cloud = (4.0 + 28.0 * system.w_tp) * tpot_slack;
  const int horizon = std::clamp(
    static_cast<int>(std::ceil(system.K * concurrency_per_cloud)), 1, 4096);
  const int curve_target = std::max({
    selectDecodeBatchSize(decode_batches, RequestState::ReadyDecodePre, horizon),
    selectDecodeBatchSize(decode_batches, RequestState::ReadyDecodeProc, horizon),
    selectDecodeBatchSize(decode_batches, RequestState::ReadyDecodePost, horizon),
  });
  const int derived_target = std::max(2 * system.K, curve_target);
  const int target = target_hot_set_size.value_or(derived_target);
  assert(target >= 1);
  return {
    .decode_batches = std::move(decode_batches),
    .target_hot_set_size = target,
    .slo_tdr = system.SLO1,
    .slo_tpot = system.SLO2,
    .waiting_weight = system.w_c,
    .prefill_warmup = false,
  };
}

std::vector<Assignment> chooseScoreAwareAssignments(
  const WorldState& world, int num_layers, const ScoreAwareSchedulerConfig& config) {
  assert(config.target_hot_set_size >= 1);
  assert(config.slo_tdr > 0.0);
  assert(config.slo_tpot > 0.0);
  return scheduler_detail::chooseAssignments(world, num_layers, ScoreAwareDecodeSelector{config});
}
