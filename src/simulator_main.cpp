#include <Scheduler.hpp>
#include <Simulator.hpp>

#include <iomanip>
#include <iostream>
#include <string>

namespace {

void printUsage(const char* program) {
  std::cerr << "Usage: " << program << " [--policy singleton|batch] [--trace] < scenario.txt\n";
}

} // namespace

int main(int argc, char** argv) {
  std::string policy_name = "batch";
  bool record_frames = false;
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--policy" && i + 1 < argc) {
      policy_name = argv[++i];
    } else if (argument == "--trace") {
      record_frames = true;
    } else {
      printUsage(argv[0]);
      return 2;
    }
  }

  if (policy_name != "singleton" && policy_name != "batch") {
    printUsage(argv[0]);
    return 2;
  }

  const SystemConfig config = readSystemConfig(std::cin);
  const TaskTimeTable table = readTaskTimeTable(std::cin);
  const TimingCurves curves = buildTimingCurves(table);
  const std::vector<SimulationRequest> workload = readSimulationWorkload(std::cin);
  const DecodeBatchPolicy batch_policy = buildDecodeBatchPolicy(curves, config.S);

  const SimulationPolicy policy = [&](const WorldState& world) {
    return policy_name == "singleton"
      ? chooseSingletonAssignments(world, config.num_layers)
      : chooseBatchedAssignments(world, config.num_layers, batch_policy);
  };
  const SimulationResult result = simulate(config, curves, workload, policy, {record_frames, 2'000'000});

  if (record_frames) {
    for (const Frame& frame : result.frames) {
      std::cout << "frame " << std::fixed << std::setprecision(9) << frame.timestamp
        << " events " << frame.events.size() << '\n';
    }
  }

  std::cout << std::fixed << std::setprecision(9)
    << "completed " << result.completed << '\n'
    << "hit_frame_limit " << result.hit_frame_limit << '\n'
    << "frames " << result.frame_count << '\n'
    << "tokens " << result.metrics.total_tokens << '\n'
    << "elapsed " << result.metrics.total_elapsed_time << '\n'
    << "tp " << result.metrics.tp << '\n'
    << "mean_tdr " << result.metrics.mean_tdr << '\n'
    << "mean_tpot " << result.metrics.mean_tpot << '\n'
    << "dist " << result.metrics.dist << '\n'
    << "norm_tp " << result.metrics.norm_tp << '\n'
    << "norm_c " << result.metrics.norm_c << '\n'
    << "normalized_score " << result.metrics.normalized_score << '\n'
    << "score " << result.metrics.score << '\n';
  return result.completed ? 0 : 1;
}
