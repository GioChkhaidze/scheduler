#include <scheduler/scheduling/HighScoreScheduler.hpp>
#include <scheduler/simulation/Simulator.hpp>

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

void printUsage(const char* program) {
  std::cerr << "Usage: " << program << " [--trace] [--observe] < scenario.txt\n";
}

void printHistogram(const char* name, const std::vector<std::size_t>& histogram) {
  for (std::size_t size = 1; size < histogram.size(); ++size) {
    if (histogram[size] > 0) {
      std::cout << name << " size " << size << " count " << histogram[size] << '\n';
    }
  }
}

} // namespace

int main(int argc, char** argv) {
  bool record_frames = false;
  bool record_observations = false;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--trace") {
      record_frames = true;
    } else if (argument == "--observe") {
      record_observations = true;
    } else {
      printUsage(argv[0]);
      return 2;
    }
  }

  const SystemConfig system = readSystemConfig(std::cin);
  const TimingCurves curves = buildTimingCurves(readTaskTimeTable(std::cin));
  const std::vector<SimulationRequest> workload = readSimulationWorkload(std::cin);
  HighScoreSchedulerConfig scheduler = buildFinalSchedulerConfig(system, curves);
  const SimulationResult result = simulate(system, curves, workload, [&](const WorldState& world) {
    return chooseHighScoreAssignments(world, system.num_layers, scheduler);
  }, {record_frames, 2'000'000, record_observations});

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
    << "link_reconciliations " << result.link_reconciliation_count << '\n'
    << "max_link_error " << result.maximum_link_reconciliation_error << '\n'
    << "tokens " << result.metrics.total_tokens << '\n'
    << "elapsed " << result.metrics.total_elapsed_time << '\n'
    << "tp " << result.metrics.tp << '\n'
    << "mean_tdr " << result.metrics.mean_tdr << '\n'
    << "mean_tpot " << result.metrics.mean_tpot << '\n'
    << "dist " << result.metrics.dist << '\n'
    << "norm_tp " << result.metrics.norm_tp << '\n'
    << "norm_c " << result.metrics.norm_c << '\n'
    << "normalized_score " << result.metrics.normalized_score << '\n'
    << "score " << result.metrics.score << '\n'
    << "control_decisions " << scheduler.telemetry.control_decisions << '\n'
    << "waiting_decisions " << scheduler.telemetry.waiting_decisions << '\n'
    << "throughput_decisions " << scheduler.telemetry.throughput_decisions << '\n'
    << "cold_admissions " << scheduler.telemetry.cold_admissions << '\n'
    << "cold_throttles " << scheduler.telemetry.cold_throttles << '\n'
    << "aged_promotions " << scheduler.telemetry.aged_promotions << '\n'
    << "policy_fallbacks " << scheduler.telemetry.policy_fallbacks << '\n'
    << "bottleneck_switches " << scheduler.telemetry.bottleneck_switches << '\n'
    << "hot_coalescing_waits " << scheduler.telemetry.hot_coalescing_waits << '\n';

  if (record_observations) {
    const SimulationDiagnostics& diagnostics = result.diagnostics;
    std::cout << "observation_span " << diagnostics.observation_span << '\n'
      << "edge_utilization " << resourceUtilization(diagnostics.edge, diagnostics.observation_span) << '\n'
      << "edge_eligible_idle " << diagnostics.edge.eligible_idle_time << '\n'
      << "up_utilization " << resourceUtilization(diagnostics.up, diagnostics.observation_span) << '\n'
      << "down_utilization " << resourceUtilization(diagnostics.down, diagnostics.observation_span) << '\n'
      << "average_edge_queue_ms " << diagnostics.average_edge_queue_ms << '\n'
      << "average_up_queue_ms " << diagnostics.average_up_queue_ms << '\n'
      << "average_down_queue_ms " << diagnostics.average_down_queue_ms << '\n'
      << "average_prefill_population " << diagnostics.average_prefill_population << '\n'
      << "average_cold_population " << diagnostics.average_cold_population << '\n'
      << "average_hot_population " << diagnostics.average_hot_population << '\n'
      << "tdr_pressure_time " << diagnostics.accumulated_tdr_pressure_time << '\n'
      << "tpot_pressure_time " << diagnostics.accumulated_tpot_pressure_time << '\n';
    for (std::size_t remote = 0; remote < diagnostics.clouds.size(); ++remote) {
      std::cout << "cloud " << remote << " utilization "
        << resourceUtilization(diagnostics.clouds[remote], diagnostics.observation_span)
        << " eligible_idle " << diagnostics.clouds[remote].eligible_idle_time
        << " average_queue_ms " << diagnostics.average_cloud_queue_ms[remote] << '\n';
    }
    printHistogram("decode_pre_batch", diagnostics.decode_batches.pre);
    printHistogram("decode_proc_batch", diagnostics.decode_batches.proc);
    printHistogram("decode_post_batch", diagnostics.decode_batches.post);
  }
  return result.completed ? 0 : 1;
}
