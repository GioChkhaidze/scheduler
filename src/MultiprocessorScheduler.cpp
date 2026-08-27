#include <MultiprocessorScheduler.hpp>

#include <RemotePlacement.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

namespace {

bool hasOutstandingPrefillCompute(RequestState state) {
  switch (state) {
    case RequestState::WaitingPrefillPreDone:
    case RequestState::WaitingPrefillUpload:
    case RequestState::ReadyPrefillProc:
    case RequestState::WaitingPrefillProcDone:
      return true;
    default:
      return false;
  }
}

} // namespace

MultiprocessorSchedulerConfig buildMultiprocessorSchedulerConfig(
  const SystemConfig& system, const TimingCurves& curves) {
  const AdaptiveSchedulerConfig adaptive = buildAdaptiveSchedulerConfig(system, curves);
  const int cloud_batch_size = std::max(1, adaptive.preferred_cloud_batch_size);
  const double decode_batch_work = system.S + interpolate(curves.decode_proc, cloud_batch_size);

  return {
    .adaptive = adaptive,
    .prefill_proc_curve = curves.prefill_proc,
    .assignment_cost = system.S,
    .decode_work_per_request = decode_batch_work / cloud_batch_size,
    .num_layers = system.num_layers,
  };
}

std::vector<double> estimateCloudWorkloads(
  const WorldState& world, const MultiprocessorSchedulerConfig& config) {
  assert(config.num_layers > 0);
  assert(config.assignment_cost >= 0.0);
  assert(config.decode_work_per_request > 0.0);

  std::vector<double> workloads(world.clouds.size(), 0.0);
  for (const Request& request : world.requests) {
    if (!request.remote.has_value() || request.state == RequestState::Finished) {
      continue;
    }

    const std::size_t remote = static_cast<std::size_t>(*request.remote);
    assert(remote < workloads.size());
    workloads[remote] += config.decode_work_per_request;

    if (hasOutstandingPrefillCompute(request.state)) {
      assert(request.next_prefill_layer >= 0);
      assert(request.next_prefill_layer < config.num_layers);
      const double remaining_fraction = static_cast<double>(config.num_layers - request.next_prefill_layer)
        / config.num_layers;
      workloads[remote] += config.assignment_cost
        + remaining_fraction * interpolate(config.prefill_proc_curve, request.input_length);
    }
  }
  return workloads;
}

std::vector<double> estimatePlacementScores(
  const WorldState& world, int rid, const MultiprocessorSchedulerConfig& config) {
  assert(rid >= 0);
  assert(static_cast<std::size_t>(rid) < world.requests.size());
  const Request& candidate = world.requests[static_cast<std::size_t>(rid)];
  assert(candidate.state == RequestState::ReadyPrefillPre);
  assert(!candidate.remote.has_value());

  std::vector<double> scores = estimateCloudWorkloads(world, config);
  const double candidate_work = config.assignment_cost
    + interpolate(config.prefill_proc_curve, candidate.input_length)
    + config.decode_work_per_request;
  for (double& score : scores) {
    score += candidate_work;
  }
  return scores;
}

int chooseLoadAwareRemote(
  const WorldState& world, int rid, const MultiprocessorSchedulerConfig& config) {
  assert(!world.clouds.empty());
  const std::vector<double> scores = estimatePlacementScores(world, rid, config);
  int best_remote = rid % static_cast<int>(scores.size());
  for (std::size_t remote = 0; remote < scores.size(); ++remote) {
    if (scores[remote] < scores[static_cast<std::size_t>(best_remote)]) {
      best_remote = static_cast<int>(remote);
    }
  }
  return best_remote;
}

std::vector<Assignment> chooseMultiprocessorAssignments(
  const WorldState& world, int num_layers, const MultiprocessorSchedulerConfig& config) {
  assert(num_layers == config.num_layers);
  const PrefillRemoteSelector remote_selector = [&](const WorldState& current, int rid) {
    return chooseLoadAwareRemote(current, rid, config);
  };
  return chooseAdaptiveAssignments(world, num_layers, config.adaptive, remote_selector);
}
