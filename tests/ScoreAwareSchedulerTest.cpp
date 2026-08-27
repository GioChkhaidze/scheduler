#include <Scheduler.hpp>
#include <Simulator.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

namespace {

Request makeRequest(
  RequestState state,
  int tokens_produced = 0,
  double arrival_time = 0.0,
  std::optional<int> remote = 0,
  std::optional<double> last_token_time = std::nullopt) {
  return {
    .input_length = 4,
    .arrival_time = arrival_time,
    .remote = remote,
    .next_prefill_layer = 0,
    .tokens_produced = tokens_produced,
    .last_token_time = last_token_time,
    .state = state,
  };
}

DecodeBatchPolicy makeBatchPolicy(int max_batch_size, bool maximal) {
  std::vector<int> choices(static_cast<std::size_t>(max_batch_size + 1));
  for (int ready = 1; ready <= max_batch_size; ++ready) {
    choices[static_cast<std::size_t>(ready)] = maximal ? ready : 1;
  }
  return {choices, choices, choices};
}

ScoreAwareSchedulerConfig makeConfig(
  int target_hot_set_size,
  bool maximal_batches = false,
  double slo_tdr = 10.0,
  double slo_tpot = 10.0,
  double waiting_weight = 1.0) {
  return {
    .decode_batches = makeBatchPolicy(16, maximal_batches),
    .target_hot_set_size = target_hot_set_size,
    .slo_tdr = slo_tdr,
    .slo_tpot = slo_tpot,
    .waiting_weight = waiting_weight,
  };
}

template <typename Task>
const Task& onlyTask(const std::vector<Assignment>& assignments) {
  assert(assignments.size() == 1);
  assert(std::holds_alternative<Task>(assignments.front().task));
  return std::get<Task>(assignments.front().task);
}

void testClassifiesEveryLifecycleRegionExplicitly() {
  const std::array<RequestState, 8> prefill_states{
    RequestState::ReadyPrefillPre,
    RequestState::WaitingPrefillPreDone,
    RequestState::WaitingPrefillUpload,
    RequestState::ReadyPrefillProc,
    RequestState::WaitingPrefillProcDone,
    RequestState::WaitingPrefillDownload,
    RequestState::ReadyPrefillPost,
    RequestState::WaitingPrefillPostDone,
  };
  for (const RequestState state : prefill_states) {
    assert(classifyRequest(makeRequest(state)) == RequestClass::Prefill);
  }

  assert(classifyRequest(makeRequest(RequestState::ReadyDecodePre)) == RequestClass::Cold);
  assert(classifyRequest(makeRequest(RequestState::WaitingDecodePostDone)) == RequestClass::Cold);
  assert(classifyRequest(
    makeRequest(RequestState::ReadyDecodePre, 1, 0.0, 0, 5.0)) == RequestClass::Hot);
  assert(classifyRequest(makeRequest(RequestState::Finished, 4)) == RequestClass::Finished);
}

void testCountsHotAndFirstTokenWorkAlreadyInFlight() {
  WorldState world{1};
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 2, 0.0, 0, 8.0));
  world.requests.push_back(makeRequest(RequestState::WaitingDecodeUpload));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre));
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillPost));
  world.requests.push_back(makeRequest(RequestState::Finished, 3));

  assert(countActiveHotSet(world) == 2);
}

void testPrioritizesHotDecodeBeforeColdRegardlessOfRid() {
  WorldState world{1};
  world.current_time = 20.0;
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0, 0.0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 2, 0.0, 0, 5.0));

  const std::vector<Assignment> assignments = chooseScoreAwareAssignments(world, 4, makeConfig(2));

  assert(onlyTask<DecodePreTask>(assignments).rids == std::vector<int>({1}));
}

void testFillsBatchWithColdOnlyWithinAvailableHotCapacity() {
  WorldState world{1};
  world.current_time = 20.0;
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0, 0.0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 1, 0.0, 0, 10.0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0, 5.0));

  const std::vector<Assignment> assignments =
    chooseScoreAwareAssignments(world, 4, makeConfig(3, true));

  assert(onlyTask<DecodePreTask>(assignments).rids == std::vector<int>({1, 0, 2}));
}

void testDoesNotOverAdmitColdRequestsWhenTargetIsFull() {
  WorldState world{1};
  world.current_time = 20.0;
  world.requests.push_back(makeRequest(RequestState::WaitingDecodeUpload));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodeProc, 1, 0.0, 0, 10.0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre));
  world.clouds[0].busy = true;

  const std::vector<Assignment> assignments = chooseScoreAwareAssignments(world, 4, makeConfig(2, true));

  assert(assignments.empty());
}

void testPrefersPrefillPostWhenTdrPressureIsGreater() {
  WorldState world{1};
  world.current_time = 30.0;
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillPost, 0, 0.0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 1, 0.0, 0, 25.0));

  const std::vector<Assignment> assignments = chooseScoreAwareAssignments(world, 4, makeConfig(2));

  assert(onlyTask<PrefillPostTask>(assignments).rid == 0);
}

void testFillsConfiguredHotSetBeforeFavoringUnpressuredHotWork() {
  WorldState world{1};
  world.current_time = 5.0;
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillPost, 0, 0.0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 1, 0.0, 0, 4.0));

  const std::vector<Assignment> assignments = chooseScoreAwareAssignments(world, 4, makeConfig(2));

  assert(onlyTask<PrefillPostTask>(assignments).rid == 0);
}

void testPrefersHotDecodeWhenTpotPressureIsGreater() {
  WorldState world{1};
  world.current_time = 30.0;
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillPost, 0, 5.0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 1, 0.0, 0, 0.0));

  const std::vector<Assignment> assignments = chooseScoreAwareAssignments(world, 4, makeConfig(2));

  assert(onlyTask<DecodePreTask>(assignments).rids == std::vector<int>({1}));
}

void testDecodePostAlwaysProducesReadyTokensFirst() {
  WorldState world{1};
  world.current_time = 30.0;
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillPost, 0, 0.0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePost, 1, 0.0, 0, 20.0));

  const std::vector<Assignment> assignments = chooseScoreAwareAssignments(world, 4, makeConfig(2));

  assert(onlyTask<DecodePostTask>(assignments).rids == std::vector<int>({1}));
}

void testCloudSelectsHotDecodeProcBeforeCold() {
  WorldState world{1};
  world.current_time = 30.0;
  world.edge.busy = true;
  world.requests.push_back(makeRequest(RequestState::ReadyDecodeProc));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodeProc, 1, 0.0, 0, 10.0));

  const std::vector<Assignment> assignments = chooseScoreAwareAssignments(world, 4, makeConfig(2));

  assert(onlyTask<DecodeProcTask>(assignments).rids == std::vector<int>({1}));
}

void testBuildsDerivedAndOverriddenHotTargets() {
  const SystemConfig system{
    .SLO1 = 30.0,
    .SLO2 = 15.0,
    .tp_UB = 1.0,
    .tp_base = 0.0,
    .dist_base = 1.0,
    .w_tp = 0.5,
    .w_c = 0.5,
    .S = 1.0,
    .latency_in_ms = 1.0,
    .bandwidth_gbps = 1.0,
    .K = 2,
    .bytes_per_token = 1,
    .num_layers = 4,
  };
  const TimingCurve curve{{{1, 1.0}, {16, 2.0}}};
  const TimingCurves curves{curve, curve, curve, curve, curve, curve};

  assert(buildScoreAwareSchedulerConfig(system, curves).target_hot_set_size == 68);
  assert(buildScoreAwareSchedulerConfig(system, curves, 3).target_hot_set_size == 3);
}

void testCompletesARequestEndToEndInTheSimulator() {
  const SystemConfig system{
    .SLO1 = 30.0,
    .SLO2 = 15.0,
    .tp_UB = 0.0625,
    .tp_base = 0.022222222,
    .dist_base = 0.0,
    .w_tp = 0.5,
    .w_c = 0.5,
    .S = 1.0,
    .latency_in_ms = 2.0,
    .bandwidth_gbps = 1.0,
    .K = 1,
    .bytes_per_token = 125000,
    .num_layers = 4,
  };
  const TaskTimeTable table{{
    {1, 3.0, 10.0, 2.0, 1.0, 4.0, 1.0},
    {4, 3.0, 10.0, 2.0, 1.0, 4.0, 1.0},
  }};
  const TimingCurves curves = buildTimingCurves(table);
  const ScoreAwareSchedulerConfig config = buildScoreAwareSchedulerConfig(system, curves, 1);
  const SimulationPolicy policy = [&](const WorldState& world) {
    return chooseScoreAwareAssignments(world, system.num_layers, config);
  };

  const SimulationResult result = simulate(system, curves, {{0.0, 4, 3}}, policy);

  assert(result.completed);
  assert(result.metrics.total_tokens == 3);
  assert(result.metrics.total_elapsed_time == 75.0);
  assert(result.metrics.mean_tdr == 30.0);
  assert(result.metrics.mean_tpot == 15.0);
}

void testCompletesSyntheticConfigurationSweepWithoutStarvation() {
  const TaskTimeTable table{{
    {1, 0.5, 1.0, 0.5, 0.2, 0.4, 0.2},
    {64, 1.0, 2.0, 1.0, 0.3, 0.6, 0.3},
    {4096, 4.0, 8.0, 4.0, 1.0, 2.0, 1.0},
  }};
  const TimingCurves curves = buildTimingCurves(table);
  const std::array<int, 3> cloud_counts{1, 2, 8};
  const std::array<int, 2> layer_counts{1, 64};
  const std::array<double, 3> throughput_weights{0.0, 0.5, 1.0};

  for (const int cloud_count : cloud_counts) {
    for (const int layer_count : layer_counts) {
      for (const double throughput_weight : throughput_weights) {
        const SystemConfig system{
          .SLO1 = 20.0,
          .SLO2 = 5.0,
          .tp_UB = 2.0,
          .tp_base = 0.0,
          .dist_base = 2.0,
          .w_tp = throughput_weight,
          .w_c = 1.0 - throughput_weight,
          .S = 1.0,
          .latency_in_ms = 0.2,
          .bandwidth_gbps = 10.0,
          .K = cloud_count,
          .bytes_per_token = 128,
          .num_layers = layer_count,
        };
        std::vector<SimulationRequest> workload;
        for (int rid = 0; rid < 12; ++rid) {
          const double arrival_time = static_cast<double>(rid / 4);
          const int input_length = std::array<int, 4>{1, 64, 512, 4096}[static_cast<std::size_t>(rid % 4)];
          workload.push_back({arrival_time, input_length, 1 + rid % 4});
        }

        const ScoreAwareSchedulerConfig config = buildScoreAwareSchedulerConfig(system, curves, 1);
        const SimulationPolicy policy = [&](const WorldState& world) {
          return chooseScoreAwareAssignments(world, system.num_layers, config);
        };
        const SimulationResult result = simulate(system, curves, workload, policy);

        assert(result.completed);
        assert(!result.hit_frame_limit);
        assert(result.frame_count < 10'000);
        assert(std::isfinite(result.metrics.score));
        assert(result.metrics.score >= 0.0);
        assert(result.metrics.score <= 1000.0);
      }
    }
  }
}

} // namespace

int main() {
  testClassifiesEveryLifecycleRegionExplicitly();
  testCountsHotAndFirstTokenWorkAlreadyInFlight();
  testPrioritizesHotDecodeBeforeColdRegardlessOfRid();
  testFillsBatchWithColdOnlyWithinAvailableHotCapacity();
  testDoesNotOverAdmitColdRequestsWhenTargetIsFull();
  testPrefersPrefillPostWhenTdrPressureIsGreater();
  testFillsConfiguredHotSetBeforeFavoringUnpressuredHotWork();
  testPrefersHotDecodeWhenTpotPressureIsGreater();
  testDecodePostAlwaysProducesReadyTokensFirst();
  testCloudSelectsHotDecodeProcBeforeCold();
  testBuildsDerivedAndOverriddenHotTargets();
  testCompletesARequestEndToEndInTheSimulator();
  testCompletesSyntheticConfigurationSweepWithoutStarvation();
  return 0;
}
