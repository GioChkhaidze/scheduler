#include <scheduler/Scheduler.hpp>
#include <scheduler/simulation/Simulator.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>
#include <variant>
#include <vector>

namespace {

SystemConfig makeSystem(double throughput_weight = 0.5, double distance_base = 2.0, int clouds = 2) {
  return {
    .SLO1 = 50.0,
    .SLO2 = 20.0,
    .tp_UB = 1.0,
    .tp_base = 0.0,
    .dist_base = distance_base,
    .w_tp = throughput_weight,
    .w_c = 1.0 - throughput_weight,
    .S = 1.0,
    .latency_in_ms = 0.1,
    .bandwidth_gbps = 100.0,
    .K = clouds,
    .bytes_per_token = 1,
    .num_layers = 4,
  };
}

TimingCurves makeCurves() {
  return buildTimingCurves({{
    {1, 1.0, 4.0, 1.0, 1.0, 2.0, 1.0},
    {8, 4.0, 16.0, 4.0, 3.0, 8.0, 3.0},
    {64, 16.0, 64.0, 16.0, 10.0, 32.0, 10.0},
  }});
}

Request makeRequest(
  RequestState state,
  std::optional<int> remote,
  int tokens = 0,
  std::optional<double> last_token = std::nullopt,
  double arrival = 0.0) {
  return {
    .input_length = 8,
    .arrival_time = arrival,
    .remote = remote,
    .next_prefill_layer = 0,
    .tokens_produced = tokens,
    .last_token_time = last_token,
    .state = state,
  };
}

bool sameTaskKind(const Assignment& left, const Assignment& right) {
  return left.server.type == right.server.type
    && left.server.cloud_index == right.server.cloud_index
    && left.task.index() == right.task.index();
}

void testClassifiesScoreRegimes() {
  assert(classifyHighScoreRegime(makeSystem(1.0)) == HighScoreRegime::ThroughputOnly);
  assert(classifyHighScoreRegime(makeSystem(0.0)) == HighScoreRegime::WaitingOnly);
  assert(classifyHighScoreRegime(makeSystem(0.0, 0.0)) == HighScoreRegime::HardSlo);
  assert(classifyHighScoreRegime(makeSystem(0.5)) == HighScoreRegime::Balanced);
}

void testFinalConfigurationOwnsTheWinningFeatureSet() {
  const HighScoreSchedulerConfig config = buildFinalSchedulerConfig(makeSystem(), makeCurves());
  assert(config.mode == HighScoreMode::Adaptive);
  assert(config.features.evidence_gated_waiting);
  assert(config.features.proactive_waiting);
  assert(config.features.waiting_prefill_cloud_expansion);
  assert(config.features.runtime_policy_hysteresis);
  assert(config.features.single_cloud_specialization);
  assert(config.features.throughput_idle_admission);
  assert(config.features.throughput_hot_coalescing);
}

void testControlModeIsExactlyTheSharedLinkScheduler() {
  const SystemConfig system = makeSystem();
  const TimingCurves curves = makeCurves();
  HighScoreSchedulerConfig scheduler =
    buildHighScoreSchedulerConfig(system, curves, HighScoreMode::Control);
  WorldState world{system.K};
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillPre, std::nullopt));

  const std::vector<Assignment> expected =
    chooseSharedLinkAssignments(world, system.num_layers, scheduler.v7);
  const std::vector<Assignment> actual =
    chooseHighScoreAssignments(world, system.num_layers, scheduler);
  assert(actual.size() == expected.size());
  for (std::size_t index = 0; index < actual.size(); ++index) {
    assert(sameTaskKind(actual[index], expected[index]));
  }
}

void testWaitingPolicyCompletesPrefillBeforeDecodeAdmissionUnderTdrPressure() {
  const SystemConfig system = makeSystem(0.0);
  HighScoreSchedulerConfig scheduler =
    buildHighScoreSchedulerConfig(system, makeCurves(), HighScoreMode::Waiting);
  WorldState world{system.K};
  world.current_time = 100.0;
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillPost, 0, 0, std::nullopt, 0.0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0, 1, 90.0, 90.0));

  const std::vector<Assignment> assignments =
    chooseHighScoreAssignments(world, system.num_layers, scheduler);
  assert(assignments.size() == 1);
  assert(std::holds_alternative<PrefillPostTask>(assignments.front().task));
}

void testWaitingPolicyThrottlesColdAdmissionUnderHotPressure() {
  const SystemConfig system = makeSystem(0.0, 2.0, 1);
  HighScoreSchedulerConfig scheduler =
    buildHighScoreSchedulerConfig(system, makeCurves(), HighScoreMode::Waiting);
  WorldState world{system.K};
  world.current_time = 100.0;
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0, 1, 0.0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0, 0, std::nullopt, 90.0));

  const std::vector<Assignment> assignments =
    chooseHighScoreAssignments(world, system.num_layers, scheduler);
  assert(assignments.size() == 1);
  assert(std::get<DecodePreTask>(assignments.front().task).rids == std::vector<int>({0}));
  assert(scheduler.telemetry.cold_throttles > 0);
}

void testWaitingPolicyAdmitsColdWorkIntoAnEmptyPipeline() {
  const SystemConfig system = makeSystem(0.0, 2.0, 1);
  HighScoreSchedulerConfig scheduler =
    buildHighScoreSchedulerConfig(system, makeCurves(), HighScoreMode::Waiting);
  WorldState world{system.K};
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0));

  const std::vector<Assignment> assignments =
    chooseHighScoreAssignments(world, system.num_layers, scheduler);
  assert(assignments.size() == 1);
  assert(std::get<DecodePreTask>(assignments.front().task).rids == std::vector<int>({0}));
  assert(scheduler.telemetry.cold_admissions == 1);
}

void testAgingEventuallyAdmitsADeferredRequest() {
  const SystemConfig system = makeSystem(0.0, 2.0, 1);
  HighScoreSchedulerConfig scheduler =
    buildHighScoreSchedulerConfig(system, makeCurves(), HighScoreMode::Waiting);
  scheduler.waiting_hot_target = 1;
  scheduler.controller.target_hot_set_size = 1;
  WorldState world{system.K};
  world.current_time = 1'000.0;
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0, 1, 990.0, 990.0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0, 0, std::nullopt, 0.0));

  const std::vector<Assignment> assignments =
    chooseHighScoreAssignments(world, system.num_layers, scheduler);
  const std::vector<int>& rids = std::get<DecodePreTask>(assignments.front().task).rids;
  assert(std::find(rids.begin(), rids.end(), 1) != rids.end());
  assert(scheduler.telemetry.aged_promotions > 0);
}

void testTopologySearchAvoidsLatencyDominatedClouds() {
  SystemConfig system = makeSystem(1.0, 2.0, 8);
  system.latency_in_ms = 100.0;
  assert(selectThroughputActiveCloudCount(system, makeCurves(), 8) == 1);
  assert(estimateStaticDecodeBottleneck(system, makeCurves(), 8, 1) == ResourceBottleneck::Up);
}

void testFinalSchedulerCompletesARepresentativeWorkload() {
  const SystemConfig system = makeSystem();
  const TimingCurves curves = makeCurves();
  HighScoreSchedulerConfig scheduler = buildFinalSchedulerConfig(system, curves);
  const std::vector<SimulationRequest> workload{
    {0.0, 8, 4}, {0.0, 8, 4}, {5.0, 16, 3}, {10.0, 4, 2},
  };
  const SimulationResult result = simulate(system, curves, workload, [&](const WorldState& world) {
    return chooseHighScoreAssignments(world, system.num_layers, scheduler);
  });
  assert(result.completed);
  assert(!result.hit_frame_limit);
  assert(std::isfinite(result.metrics.score));
}

} // namespace

int main() {
  testClassifiesScoreRegimes();
  testFinalConfigurationOwnsTheWinningFeatureSet();
  testControlModeIsExactlyTheSharedLinkScheduler();
  testWaitingPolicyCompletesPrefillBeforeDecodeAdmissionUnderTdrPressure();
  testWaitingPolicyThrottlesColdAdmissionUnderHotPressure();
  testWaitingPolicyAdmitsColdWorkIntoAnEmptyPipeline();
  testAgingEventuallyAdmitsADeferredRequest();
  testTopologySearchAvoidsLatencyDominatedClouds();
  testFinalSchedulerCompletesARepresentativeWorkload();
}
