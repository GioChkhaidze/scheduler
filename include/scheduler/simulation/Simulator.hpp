#pragma once

#include <scheduler/model/SystemConfig.hpp>
#include <scheduler/model/TaskTimeTable.hpp>
#include <scheduler/model/WorldState.hpp>
#include <scheduler/simulation/SimulationObservability.hpp>

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <vector>

struct SimulationRequest {
  double arrival_time;
  int input_length;
  int output_length;

  bool operator==(const SimulationRequest&) const = default;
};

struct SimulationMetrics {
  int total_tokens = 0;
  double total_elapsed_time = 0.0;
  double tp = 0.0;
  double mean_tdr = 0.0;
  double mean_tpot = 0.0;
  double dist = 0.0;
  double norm_tp = 0.0;
  double norm_c = 0.0;
  double normalized_score = 0.0;
  double score = 0.0;
};

struct SimulationOptions {
  bool record_frames = false;
  std::size_t max_frames = 2'000'000;
  bool record_observations = false;
};

struct SimulationResult {
  bool completed = false;
  bool hit_frame_limit = false;
  std::size_t frame_count = 0;
  int link_reconciliation_count = 0;
  double maximum_link_reconciliation_error = 0.0;
  SimulationMetrics metrics;
  SimulationDiagnostics diagnostics;
  std::vector<Frame> frames;
};

using SimulationPolicy = std::function<std::vector<Assignment>(const WorldState&)>;

std::vector<SimulationRequest> readSimulationWorkload(std::istream& input);
SimulationResult simulate(
  const SystemConfig& config,
  const TimingCurves& curves,
  const std::vector<SimulationRequest>& workload,
  const SimulationPolicy& policy,
  const SimulationOptions& options = {});
