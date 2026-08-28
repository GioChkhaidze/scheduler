#pragma once

#include <scheduler/model/LinkState.hpp>
#include <scheduler/model/SystemConfig.hpp>
#include <scheduler/model/TaskTimeTable.hpp>
#include <scheduler/model/WorldState.hpp>
#include <scheduler/scheduling/MultiprocessorScheduler.hpp>

#include <vector>
#include <optional>

struct SharedLinkFeatures {
  bool decode_locality = true;
  bool prefill_up_admission = true;
  bool prefill_down_admission = true;
};

struct SharedLinkSchedulerConfig {
  MultiprocessorSchedulerConfig multiprocessor;
  SystemConfig system;
  TimingCurves curves;
  SharedLinkFeatures features;
};

struct FinalPrefillDownImpact {
  double blocking_saved = 0.0;
  double hot_completion_without_prefill = 0.0;
};

SharedLinkSchedulerConfig buildSharedLinkSchedulerConfig(
  const SystemConfig& system,
  const TimingCurves& curves,
  SharedLinkFeatures features = {});
std::vector<LinkTransferSpec> deriveTriggeredTransfers(
  const WorldState& world, const TaskSpec& task, int num_layers);
double estimateTaskDuration(
  const WorldState& world, const TaskSpec& task, const TimingCurves& curves, int num_layers);
void recordLinkAssignment(
  WorldState& world,
  const Assignment& assignment,
  const SystemConfig& system,
  const TimingCurves& curves);
void observeLinkFrame(WorldState& world, const Frame& frame, const SystemConfig& system);
double estimateDecodePreLinkCost(
  const WorldState& world,
  const std::vector<int>& rids,
  const SharedLinkSchedulerConfig& config);
Assignment localizeDecodePreAssignment(
  const WorldState& world,
  const Assignment& baseline,
  const SharedLinkSchedulerConfig& config);
std::optional<double> predictedHotDecodeReadyTime(
  const WorldState& world, int remote, const SharedLinkSchedulerConfig& config);
std::optional<double> predictedHotDecodeReadyTime(
  const WorldState& world,
  int remote,
  const std::vector<Assignment>& guaranteed_assignments,
  const SharedLinkSchedulerConfig& config);
FinalPrefillDownImpact estimateFinalPrefillDownImpact(
  const WorldState& world,
  std::size_t candidate_index,
  const std::vector<Assignment>& candidates,
  const SharedLinkSchedulerConfig& config);
bool hasGuaranteedSchedulerEvent(const WorldState& world);
std::vector<Assignment> applySharedLinkPolicy(
  const WorldState& world,
  int num_layers,
  const std::vector<Assignment>& baseline,
  const SharedLinkSchedulerConfig& config);
std::vector<Assignment> chooseSharedLinkAssignments(
  const WorldState& world, int num_layers, const SharedLinkSchedulerConfig& config);
