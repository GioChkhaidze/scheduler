#pragma once

#include <LinkState.hpp>
#include <MultiprocessorScheduler.hpp>
#include <SystemConfig.hpp>
#include <TaskTimeTable.hpp>
#include <WorldState.hpp>

#include <vector>

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
std::vector<Assignment> chooseSharedLinkAssignments(
  const WorldState& world, int num_layers, const SharedLinkSchedulerConfig& config);
