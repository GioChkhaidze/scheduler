#include <Scheduler.hpp>
#include <Simulator.hpp>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

namespace {

bool isEqual(double left, double right, double tolerance = 1e-9) {
  return std::abs(left - right) <= tolerance;
}

SystemConfig makeSystem(int cloud_count = 3) {
  return {
    .SLO1 = 1000.0,
    .SLO2 = 1000.0,
    .tp_UB = 1.0,
    .tp_base = 0.0,
    .dist_base = 1.0,
    .w_tp = 0.5,
    .w_c = 0.5,
    .S = 2.0,
    .latency_in_ms = 0.001,
    .bandwidth_gbps = 100.0,
    .K = cloud_count,
    .bytes_per_token = 1,
    .num_layers = 4,
  };
}

TimingCurves makeLengthSensitiveCurves() {
  return buildTimingCurves({{
    {1, 0.001, 1.0, 0.001, 0.001, 1.0, 0.001},
    {100, 0.001, 100.0, 0.001, 0.001, 1.0, 0.001},
  }});
}

Request makeRequest(
  int input_length,
  RequestState state,
  std::optional<int> remote,
  int next_prefill_layer = 0,
  int tokens_produced = 0) {
  return {
    .input_length = input_length,
    .arrival_time = 0.0,
    .remote = remote,
    .next_prefill_layer = next_prefill_layer,
    .tokens_produced = tokens_produced,
    .last_token_time = tokens_produced == 0 ? std::nullopt : std::optional<double>{0.0},
    .state = state,
  };
}

MultiprocessorSchedulerConfig makePlacementConfig(int cloud_count = 3) {
  MultiprocessorSchedulerConfig config =
    buildMultiprocessorSchedulerConfig(makeSystem(cloud_count), makeLengthSensitiveCurves());
  config.assignment_cost = 2.0;
  config.decode_work_per_request = 5.0;
  return config;
}

void testScoreContainsBusyPrefillDecodeAndCandidateWork() {
  WorldState world{3};
  world.clouds[0].busy = true;
  world.requests.push_back(makeRequest(1, RequestState::ReadyDecodePre, 0));
  world.requests.push_back(makeRequest(100, RequestState::WaitingPrefillUpload, 1));
  world.requests.push_back(makeRequest(100, RequestState::ReadyPrefillProc, 2, 2));
  world.requests.push_back(makeRequest(1, RequestState::ReadyDecodePre, 2, 0, 1));
  world.requests.push_back(makeRequest(10, RequestState::ReadyPrefillPre, std::nullopt));

  const MultiprocessorSchedulerConfig config = makePlacementConfig();
  const std::vector<double> workloads = estimateCloudWorkloads(world, config);
  const std::vector<double> scores = estimatePlacementScores(world, 4, config);

  assert(workloads.size() == 3);
  assert(isEqual(workloads[0], 5.0));
  assert(isEqual(workloads[1], 107.0));
  assert(isEqual(workloads[2], 62.0));
  for (std::size_t remote = 0; remote < scores.size(); ++remote) {
    assert(isEqual(scores[remote] - workloads[remote], 17.0));
  }
}

void testChoosesLowestWorkAndUsesRoundRobinForEqualScores() {
  WorldState world{3};
  world.requests.push_back(makeRequest(100, RequestState::WaitingPrefillUpload, 0));
  world.requests.push_back(makeRequest(10, RequestState::ReadyPrefillPre, std::nullopt));

  const MultiprocessorSchedulerConfig config = makePlacementConfig();
  assert(chooseLoadAwareRemote(world, 1, config) == 1);

  world.requests[0].state = RequestState::Finished;
  assert(chooseLoadAwareRemote(world, 1, config) == 1);
}

void testInFlightWorkMakesABusyCloudLoseWithoutDoubleCountingBusy() {
  WorldState world{2};
  world.clouds[0].busy = true;
  world.requests.push_back(makeRequest(100, RequestState::WaitingPrefillProcDone, 0));
  world.requests.push_back(makeRequest(10, RequestState::ReadyPrefillPre, std::nullopt));

  assert(chooseLoadAwareRemote(world, 1, makePlacementConfig(2)) == 1);
}

void testFinishedRequestsDoNotCreatePhantomLoad() {
  WorldState world{2};
  world.requests.push_back(makeRequest(100, RequestState::Finished, 0));
  world.requests.push_back(makeRequest(10, RequestState::ReadyPrefillPre, std::nullopt));

  assert(chooseLoadAwareRemote(world, 1, makePlacementConfig(2)) == 1);
}

void testV5RemainsRoundRobinWhileV6ChangesOnlyPlacement() {
  const SystemConfig system = makeSystem();
  const TimingCurves curves = makeLengthSensitiveCurves();
  const AdaptiveSchedulerConfig adaptive = buildAdaptiveSchedulerConfig(system, curves);
  const MultiprocessorSchedulerConfig multiprocessor = buildMultiprocessorSchedulerConfig(system, curves);
  WorldState world{system.K};
  world.requests.push_back(makeRequest(100, RequestState::WaitingPrefillUpload, 2));
  world.requests.push_back(makeRequest(1, RequestState::Finished, 0));
  world.requests.push_back(makeRequest(10, RequestState::ReadyPrefillPre, std::nullopt));

  const std::vector<Assignment> v5 = chooseAdaptiveAssignments(world, system.num_layers, adaptive);
  const std::vector<Assignment> v6 =
    chooseMultiprocessorAssignments(world, system.num_layers, multiprocessor);

  assert(v5.size() == 1);
  assert(v6.size() == 1);
  const PrefillPreTask& v5_task = std::get<PrefillPreTask>(v5.front().task);
  const PrefillPreTask& v6_task = std::get<PrefillPreTask>(v6.front().task);
  assert(v5_task.rid == v6_task.rid);
  assert(v5_task.remote == 2);
  assert(v6_task.remote == 0);
  assert(multiprocessor.adaptive.preferred_decode_batch_size == adaptive.preferred_decode_batch_size);
  assert(multiprocessor.adaptive.target_hot_set_size == adaptive.target_hot_set_size);
}

void testBalancesLongPrefillsBetterThanRoundRobin() {
  SystemConfig system = makeSystem(2);
  system.S = 1.0;
  const TimingCurves curves = makeLengthSensitiveCurves();
  const AdaptiveSchedulerConfig adaptive = buildAdaptiveSchedulerConfig(system, curves);
  const MultiprocessorSchedulerConfig multiprocessor = buildMultiprocessorSchedulerConfig(system, curves);
  const std::vector<SimulationRequest> workload{
    {0.0, 100, 1},
    {0.0, 1, 1},
    {0.0, 100, 1},
  };

  const SimulationResult v5 = simulate(system, curves, workload, [&](const WorldState& world) {
    return chooseAdaptiveAssignments(world, system.num_layers, adaptive);
  });
  const SimulationResult v6 = simulate(system, curves, workload, [&](const WorldState& world) {
    return chooseMultiprocessorAssignments(world, system.num_layers, multiprocessor);
  });

  assert(v5.completed);
  assert(v6.completed);
  assert(v6.metrics.mean_tdr < v5.metrics.mean_tdr * 0.8);
  assert(v6.metrics.total_elapsed_time < v5.metrics.total_elapsed_time * 0.8);
  assert(v6.metrics.score > v5.metrics.score);
}

} // namespace

int main() {
  testScoreContainsBusyPrefillDecodeAndCandidateWork();
  testChoosesLowestWorkAndUsesRoundRobinForEqualScores();
  testInFlightWorkMakesABusyCloudLoseWithoutDoubleCountingBusy();
  testFinishedRequestsDoNotCreatePhantomLoad();
  testV5RemainsRoundRobinWhileV6ChangesOnlyPlacement();
  testBalancesLongPrefillsBetterThanRoundRobin();
  return 0;
}
