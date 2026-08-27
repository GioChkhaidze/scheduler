#pragma once

#include <AdaptiveScheduler.hpp>
#include <SystemConfig.hpp>
#include <TaskTimeTable.hpp>
#include <WorldState.hpp>

#include <vector>

struct MultiprocessorSchedulerConfig {
  AdaptiveSchedulerConfig adaptive;
  TimingCurve prefill_proc_curve;
  double assignment_cost;
  double decode_work_per_request;
  int num_layers;
};

MultiprocessorSchedulerConfig buildMultiprocessorSchedulerConfig(
  const SystemConfig& system, const TimingCurves& curves);
std::vector<double> estimateCloudWorkloads(
  const WorldState& world, const MultiprocessorSchedulerConfig& config);
std::vector<double> estimatePlacementScores(
  const WorldState& world, int rid, const MultiprocessorSchedulerConfig& config);
int chooseLoadAwareRemote(
  const WorldState& world, int rid, const MultiprocessorSchedulerConfig& config);
std::vector<Assignment> chooseMultiprocessorAssignments(
  const WorldState& world, int num_layers, const MultiprocessorSchedulerConfig& config);
