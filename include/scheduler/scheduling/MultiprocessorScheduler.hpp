#pragma once

#include <scheduler/model/SystemConfig.hpp>
#include <scheduler/model/TaskTimeTable.hpp>
#include <scheduler/model/WorldState.hpp>
#include <scheduler/scheduling/AdaptiveScheduler.hpp>

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
