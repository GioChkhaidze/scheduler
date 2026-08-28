#pragma once

#include <scheduler/model/SystemConfig.hpp>
#include <scheduler/model/TaskTimeTable.hpp>
#include <scheduler/model/WorldState.hpp>
#include <scheduler/scheduling/DecodeBatchScheduler.hpp>
#include <scheduler/scheduling/RemotePlacement.hpp>
#include <scheduler/scheduling/ScoreAwareScheduler.hpp>

#include <vector>

enum class AdaptiveScoreRegime {
  HardSlo,
  Waiting,
  Balanced,
  Throughput
};

struct AdaptiveSchedulerConfig {
  DecodeBatchPolicy baseline_decode_batches;
  DecodeBatchPolicy throughput_decode_batches;
  ScoreAwareSchedulerConfig waiting_policy;
  AdaptiveScoreRegime regime;
  int preferred_decode_batch_size;
  int preferred_cloud_batch_size;
  int target_hot_set_size;
  double estimated_decode_capacity;
  double throughput_target;
};

AdaptiveScoreRegime classifyScoreRegime(const SystemConfig& system);
double estimateDecodeCapacity(
  const SystemConfig& system, const TimingCurves& curves, int total_batch_size);
AdaptiveSchedulerConfig buildAdaptiveSchedulerConfig(
  const SystemConfig& system, const TimingCurves& curves);
std::vector<Assignment> chooseAdaptiveAssignments(
  const WorldState& world, int num_layers, const AdaptiveSchedulerConfig& config);
std::vector<Assignment> chooseAdaptiveAssignments(
  const WorldState& world,
  int num_layers,
  const AdaptiveSchedulerConfig& config,
  const PrefillRemoteSelector& remote_selector);
