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

SystemConfig makeSystem(double throughput_weight = 0.8, double distance_base = 1.0) {
  return {
    .SLO1 = 50.0,
    .SLO2 = 20.0,
    .tp_UB = 0.5,
    .tp_base = 0.0,
    .dist_base = distance_base,
    .w_tp = throughput_weight,
    .w_c = 1.0 - throughput_weight,
    .S = 1.0,
    .latency_in_ms = 0.1,
    .bandwidth_gbps = 100.0,
    .K = 2,
    .bytes_per_token = 1,
    .num_layers = 4,
  };
}

TimingCurves makeConstantCurves() {
  const TimingCurve curve{{{1, 1.0}, {2000, 1.0}}};
  return {curve, curve, curve, curve, curve, curve};
}

TimingCurves makeCohortCurves() {
  return buildTimingCurves({{
    {1, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
    {2, -1.0, -1.0, -1.0, 1.4, 1.4, 1.4},
    {4, -1.0, -1.0, -1.0, 2.0, 2.0, 2.0},
    {8, -1.0, -1.0, -1.0, 3.0, 3.0, 3.0},
    {16, -1.0, -1.0, -1.0, 5.0, 5.0, 5.0},
    {32, -1.0, -1.0, -1.0, 8.0, 8.0, 8.0},
    {64, -1.0, -1.0, -1.0, 12.0, 12.0, 12.0},
  }});
}

Request makeRequest(
  RequestState state,
  int tokens_produced = 0,
  std::optional<int> remote = 0,
  std::optional<double> last_token_time = std::nullopt) {
  return {
    .input_length = 1,
    .arrival_time = 0.0,
    .remote = remote,
    .next_prefill_layer = 0,
    .tokens_produced = tokens_produced,
    .last_token_time = last_token_time,
    .state = state,
  };
}

void testClassifiesScoreRegimesFromSuppliedParameters() {
  SystemConfig system = makeSystem(0.5);
  assert(classifyScoreRegime(system) == AdaptiveScoreRegime::Balanced);

  system = makeSystem(0.8);
  assert(classifyScoreRegime(system) == AdaptiveScoreRegime::Throughput);

  system = makeSystem(0.75);
  assert(classifyScoreRegime(system) == AdaptiveScoreRegime::Balanced);

  system = makeSystem(0.2);
  assert(classifyScoreRegime(system) == AdaptiveScoreRegime::Waiting);

  system = makeSystem(0.5, 0.0);
  assert(classifyScoreRegime(system) == AdaptiveScoreRegime::HardSlo);
  assert(buildAdaptiveSchedulerConfig(system, makeConstantCurves()).target_hot_set_size == system.K);

  system = makeSystem(0.99, 0.0);
  assert(classifyScoreRegime(system) == AdaptiveScoreRegime::Throughput);
}

void testEstimatesTheDecodePipelineBottleneck() {
  SystemConfig system = makeSystem();
  system.latency_in_ms = 0.001;
  const double capacity = estimateDecodeCapacity(system, makeConstantCurves(), 4);
  assert(isEqual(capacity, 1.0));
}

void testStopsIncreasingBatchOnceTheThroughputTargetIsReached() {
  const SystemConfig system = makeSystem();
  const AdaptiveSchedulerConfig config = buildAdaptiveSchedulerConfig(system, makeConstantCurves());

  assert(config.regime == AdaptiveScoreRegime::Throughput);
  assert(config.preferred_decode_batch_size == 3);
  assert(config.preferred_cloud_batch_size == 2);
  assert(config.target_hot_set_size == 12);
  assert(config.estimated_decode_capacity >= config.throughput_target);
}

void testBalancedRegimeIsExactlyTheV2Fallback() {
  const SystemConfig system = makeSystem(0.5);
  const TimingCurves curves = makeConstantCurves();
  const AdaptiveSchedulerConfig config = buildAdaptiveSchedulerConfig(system, curves);
  WorldState world{system.K};
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre));

  const std::vector<Assignment> baseline =
    chooseBatchedAssignments(world, system.num_layers, config.baseline_decode_batches);
  const std::vector<Assignment> adaptive =
    chooseAdaptiveAssignments(world, system.num_layers, config);

  assert(baseline.size() == 1);
  assert(adaptive.size() == 1);
  assert(std::get<DecodePreTask>(baseline.front().task).rids
    == std::get<DecodePreTask>(adaptive.front().task).rids);
}

void testWaitingRegimeIsExactlyTheV4Policy() {
  const SystemConfig system = makeSystem(0.2);
  const TimingCurves curves = makeConstantCurves();
  const AdaptiveSchedulerConfig config = buildAdaptiveSchedulerConfig(system, curves);
  WorldState world{system.K};
  world.current_time = 100.0;
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillPost));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 1, 0, 20.0));

  const std::vector<Assignment> score_aware =
    chooseScoreAwareAssignments(world, system.num_layers, config.waiting_policy);
  const std::vector<Assignment> adaptive =
    chooseAdaptiveAssignments(world, system.num_layers, config);

  assert(score_aware.size() == 1);
  assert(adaptive.size() == 1);
  assert(score_aware.front().task.index() == adaptive.front().task.index());
}

void testWaitingRegimeStopsThrottlingAfterBothSlosAreComfortablyMet() {
  const SystemConfig system = makeSystem(0.2);
  AdaptiveSchedulerConfig config = buildAdaptiveSchedulerConfig(system, makeConstantCurves());
  config.waiting_policy.target_hot_set_size = 1;
  WorldState world{system.K};
  world.current_time = 100.0;
  world.clouds[0].busy = true;
  world.requests.push_back(makeRequest(RequestState::ReadyDecodeProc, 1, 0, 90.0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre));

  const std::vector<Assignment> protected_assignments =
    chooseAdaptiveAssignments(world, system.num_layers, config);
  assert(protected_assignments.empty());

  world.observed_tdr_sum = 10.0;
  world.observed_tdr_count = 1;
  world.observed_tpot_sum = 5.0;
  world.observed_tpot_count = 1;
  const std::vector<Assignment> relaxed_assignments =
    chooseAdaptiveAssignments(world, system.num_layers, config);
  assert(relaxed_assignments.size() == 1);
  assert(std::get<DecodePreTask>(relaxed_assignments.front().task).rids == std::vector<int>({1}));
}

void testThroughputRegimeBuildsTheCohortBeforeDecoding() {
  const SystemConfig system = makeSystem();
  const AdaptiveSchedulerConfig config =
    buildAdaptiveSchedulerConfig(system, makeConstantCurves());
  WorldState world{system.K};
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre));
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillPre, 0, std::nullopt));

  const std::vector<Assignment> assignments =
    chooseAdaptiveAssignments(world, system.num_layers, config);

  assert(assignments.size() == 1);
  assert(std::get<PrefillPreTask>(assignments.front().task).rid == 1);
}

void testHardSloWarmsUpKnownPrefillsBeforeFirstDecode() {
  const SystemConfig system = makeSystem(0.0, 0.0);
  const AdaptiveSchedulerConfig config =
    buildAdaptiveSchedulerConfig(system, makeConstantCurves());
  WorldState world{system.K};
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre));
  world.requests.push_back(makeRequest(RequestState::WaitingPrefillUpload));

  const std::vector<Assignment> assignments =
    chooseAdaptiveAssignments(world, system.num_layers, config);

  assert(assignments.empty());
}

void testThroughputRegimePrefillsCloudBeforeDecoding() {
  const SystemConfig system = makeSystem();
  const AdaptiveSchedulerConfig config =
    buildAdaptiveSchedulerConfig(system, makeConstantCurves());
  WorldState world{system.K};
  world.edge.busy = true;
  world.requests.push_back(makeRequest(RequestState::ReadyDecodeProc, 1, 0, 2.0));
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillProc));

  const std::vector<Assignment> assignments =
    chooseAdaptiveAssignments(world, system.num_layers, config);

  assert(assignments.size() == 1);
  assert(std::get<PrefillProcTask>(assignments.front().task).rid == 1);
}

void testThroughputRegimeWaitsForKnownCohortProgress() {
  const SystemConfig system = makeSystem();
  const AdaptiveSchedulerConfig config =
    buildAdaptiveSchedulerConfig(system, makeConstantCurves());
  WorldState world{system.K};
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre));
  world.requests.push_back(makeRequest(RequestState::WaitingPrefillUpload));

  const std::vector<Assignment> assignments =
    chooseAdaptiveAssignments(world, system.num_layers, config);

  assert(assignments.empty());
}

void testThroughputRegimeDoesNotOverAdmitColdRequests() {
  const SystemConfig system = makeSystem();
  AdaptiveSchedulerConfig config = buildAdaptiveSchedulerConfig(system, makeConstantCurves());
  config.target_hot_set_size = 1;
  WorldState world{system.K};
  world.clouds[0].busy = true;
  world.requests.push_back(makeRequest(RequestState::ReadyDecodeProc, 1, 0, 2.0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre));

  const std::vector<Assignment> assignments =
    chooseAdaptiveAssignments(world, system.num_layers, config);

  assert(assignments.empty());
}

void testThroughputCohortImprovesAStaggeredPipeline() {
  SystemConfig system = makeSystem(1.0);
  system.SLO1 = 1000.0;
  system.SLO2 = 1000.0;
  system.tp_UB = 2.0;
  system.S = 5.0;
  const TimingCurves curves = makeCohortCurves();
  const DecodeBatchPolicy baseline_config = buildDecodeBatchPolicy(curves, system.S);
  const AdaptiveSchedulerConfig adaptive_config = buildAdaptiveSchedulerConfig(system, curves);
  std::vector<SimulationRequest> workload(64, {0.0, 1, 32});

  const SimulationResult baseline = simulate(system, curves, workload, [&](const WorldState& world) {
    return chooseBatchedAssignments(world, system.num_layers, baseline_config);
  });
  const SimulationResult adaptive = simulate(system, curves, workload, [&](const WorldState& world) {
    return chooseAdaptiveAssignments(world, system.num_layers, adaptive_config);
  });

  assert(baseline.completed);
  assert(adaptive.completed);
  assert(adaptive.metrics.tp > baseline.metrics.tp * 1.2);
  assert(adaptive.metrics.score > baseline.metrics.score + 90.0);
  assert(adaptive.frame_count < baseline.frame_count);
}

void testHardSloModeCanConvertANearMissIntoFullWaitingCredit() {
  SystemConfig system = makeSystem(0.0, 0.0);
  system.SLO1 = 700.0;
  system.SLO2 = 30.0;
  system.S = 5.0;
  system.K = 4;
  const TimingCurves curves = makeCohortCurves();
  const DecodeBatchPolicy baseline_config = buildDecodeBatchPolicy(curves, system.S);
  const AdaptiveSchedulerConfig adaptive_config = buildAdaptiveSchedulerConfig(system, curves);
  std::vector<SimulationRequest> workload(64, {0.0, 1, 8});

  const SimulationResult baseline = simulate(system, curves, workload, [&](const WorldState& world) {
    return chooseBatchedAssignments(world, system.num_layers, baseline_config);
  });
  const SimulationResult adaptive = simulate(system, curves, workload, [&](const WorldState& world) {
    return chooseAdaptiveAssignments(world, system.num_layers, adaptive_config);
  });

  assert(baseline.completed);
  assert(adaptive.completed);
  assert(baseline.metrics.norm_c == 0.0);
  assert(adaptive.metrics.norm_c == 1.0);
  assert(adaptive.metrics.score == 1000.0);
}

void testCompletesAThroughputConfigurationSweep() {
  const TimingCurves curves = makeConstantCurves();
  for (const int cloud_count : {1, 4, 8}) {
    for (const double throughput_weight : {0.75, 1.0}) {
      SystemConfig system = makeSystem(throughput_weight);
      system.K = cloud_count;
      const AdaptiveSchedulerConfig config = buildAdaptiveSchedulerConfig(system, curves);
      std::vector<SimulationRequest> workload;
      for (int rid = 0; rid < 24; ++rid) {
        workload.push_back({
          throughput_weight == 1.0 ? 0.0 : static_cast<double>(rid / 4),
          1 + rid % 4,
          1 + rid % 8,
        });
      }

      const SimulationResult result = simulate(system, curves, workload, [&](const WorldState& world) {
        return chooseAdaptiveAssignments(world, system.num_layers, config);
      });
      assert(result.completed);
      assert(!result.hit_frame_limit);
      assert(std::isfinite(result.metrics.score));
    }
  }
}

} // namespace

int main() {
  testClassifiesScoreRegimesFromSuppliedParameters();
  testEstimatesTheDecodePipelineBottleneck();
  testStopsIncreasingBatchOnceTheThroughputTargetIsReached();
  testBalancedRegimeIsExactlyTheV2Fallback();
  testWaitingRegimeIsExactlyTheV4Policy();
  testWaitingRegimeStopsThrottlingAfterBothSlosAreComfortablyMet();
  testThroughputRegimeBuildsTheCohortBeforeDecoding();
  testHardSloWarmsUpKnownPrefillsBeforeFirstDecode();
  testThroughputRegimePrefillsCloudBeforeDecoding();
  testThroughputRegimeWaitsForKnownCohortProgress();
  testThroughputRegimeDoesNotOverAdmitColdRequests();
  testThroughputCohortImprovesAStaggeredPipeline();
  testHardSloModeCanConvertANearMissIntoFullWaitingCredit();
  testCompletesAThroughputConfigurationSweep();
  return 0;
}
