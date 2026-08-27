#include <Scheduler.hpp>
#include <Simulator.hpp>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

void printResult(
  const std::string& policy_name,
  const SimulationResult& result,
  std::chrono::microseconds wall_time) {
  const SimulationMetrics& metrics = result.metrics;
  std::cout << policy_name << ','
    << result.completed << ','
    << result.frame_count << ','
    << metrics.tp << ','
    << metrics.mean_tdr << ','
    << metrics.mean_tpot << ','
    << metrics.dist << ','
    << metrics.norm_tp << ','
    << metrics.norm_c << ','
    << metrics.score << ','
    << result.link_reconciliation_count << ','
    << result.maximum_link_reconciliation_error << ','
    << wall_time.count() << '\n';
}

template <typename Policy>
void runPolicy(
  const std::string& name,
  const SystemConfig& config,
  const TimingCurves& curves,
  const std::vector<SimulationRequest>& workload,
  Policy policy) {
  const auto started_at = std::chrono::steady_clock::now();
  const SimulationResult result = simulate(config, curves, workload, policy);
  const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::steady_clock::now() - started_at);
  printResult(name, result, elapsed);
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2 && argc != 4) {
    std::cerr << "Usage: " << argv[0] << " scenario.txt [--target-hot count]\n";
    return 2;
  }
  std::optional<int> target_hot_set_size;
  if (argc == 4) {
    if (std::string(argv[2]) != "--target-hot") {
      return 2;
    }
    target_hot_set_size = std::stoi(argv[3]);
    if (*target_hot_set_size < 1) {
      return 2;
    }
  }

  std::ifstream input(argv[1]);
  if (!input) {
    std::cerr << "Cannot open scenario: " << argv[1] << '\n';
    return 2;
  }

  const SystemConfig config = readSystemConfig(input);
  const TaskTimeTable table = readTaskTimeTable(input);
  const TimingCurves curves = buildTimingCurves(table);
  const std::vector<SimulationRequest> workload = readSimulationWorkload(input);
  const DecodeBatchPolicy batch_policy = buildDecodeBatchPolicy(curves, config.S);
  const ScoreAwareSchedulerConfig score_config =
    buildScoreAwareSchedulerConfig(config, curves, target_hot_set_size);
  const AdaptiveSchedulerConfig adaptive_config = buildAdaptiveSchedulerConfig(config, curves);
  const MultiprocessorSchedulerConfig multiprocessor_config =
    buildMultiprocessorSchedulerConfig(config, curves);
  const SharedLinkSchedulerConfig tracking_config =
    buildSharedLinkSchedulerConfig(config, curves, {false, false, false});
  const SharedLinkSchedulerConfig locality_config =
    buildSharedLinkSchedulerConfig(config, curves, {true, false, false});
  const SharedLinkSchedulerConfig up_config =
    buildSharedLinkSchedulerConfig(config, curves, {false, true, false});
  const SharedLinkSchedulerConfig down_config =
    buildSharedLinkSchedulerConfig(config, curves, {false, false, true});
  const SharedLinkSchedulerConfig combined_config = buildSharedLinkSchedulerConfig(config, curves);

  std::cout << std::fixed << std::setprecision(9);
  std::cout
    << "policy,completed,frames,tp,mean_tdr,mean_tpot,dist,norm_tp,norm_c,score,"
    << "link_reconciliations,max_link_error,wall_us\n";
  runPolicy("singleton", config, curves, workload, [&](const WorldState& world) {
    return chooseSingletonAssignments(world, config.num_layers);
  });
  runPolicy("decode_batch", config, curves, workload, [&](const WorldState& world) {
    return chooseBatchedAssignments(world, config.num_layers, batch_policy);
  });
  runPolicy(
    "score_aware_target_" + std::to_string(score_config.target_hot_set_size),
    config,
    curves,
    workload,
    [&](const WorldState& world) {
      return chooseScoreAwareAssignments(world, config.num_layers, score_config);
    });
  runPolicy(
    "adaptive_target_" + std::to_string(adaptive_config.target_hot_set_size),
    config,
    curves,
    workload,
    [&](const WorldState& world) {
      return chooseAdaptiveAssignments(world, config.num_layers, adaptive_config);
    });
  runPolicy(
    "multiprocessor_target_" + std::to_string(multiprocessor_config.adaptive.target_hot_set_size),
    config,
    curves,
    workload,
    [&](const WorldState& world) {
      return chooseMultiprocessorAssignments(world, config.num_layers, multiprocessor_config);
    });
  runPolicy("v7_tracking", config, curves, workload, [&](const WorldState& world) {
    return chooseSharedLinkAssignments(world, config.num_layers, tracking_config);
  });
  runPolicy("v7_decode_locality", config, curves, workload, [&](const WorldState& world) {
    return chooseSharedLinkAssignments(world, config.num_layers, locality_config);
  });
  runPolicy("v7_up_admission", config, curves, workload, [&](const WorldState& world) {
    return chooseSharedLinkAssignments(world, config.num_layers, up_config);
  });
  runPolicy("v7_down_admission", config, curves, workload, [&](const WorldState& world) {
    return chooseSharedLinkAssignments(world, config.num_layers, down_config);
  });
  runPolicy("v7_combined", config, curves, workload, [&](const WorldState& world) {
    return chooseSharedLinkAssignments(world, config.num_layers, combined_config);
  });
  return 0;
}
