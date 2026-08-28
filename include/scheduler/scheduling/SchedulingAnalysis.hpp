#pragma once

#include <scheduler/model/Protocol.hpp>
#include <scheduler/model/SystemConfig.hpp>
#include <scheduler/model/TaskTimeTable.hpp>
#include <scheduler/model/WorldState.hpp>
#include <scheduler/scheduling/ScoreAwareScheduler.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

enum class ScheduledTaskKind {
  PrefillPre,
  PrefillProc,
  PrefillPost,
  DecodePre,
  DecodeProc,
  DecodePost
};

constexpr std::size_t scheduled_task_kind_count = 6;

enum class ResourceBottleneck {
  None,
  Edge,
  Cloud,
  Up,
  Down
};

struct ScorePressure {
  double tdr = 0.0;
  double tpot = 0.0;
  double maximum_tdr = 0.0;
  double maximum_tpot = 0.0;
  int prefill_requests = 0;
  int hot_requests = 0;
};

struct SchedulerBacklog {
  double edge_ms = 0.0;
  std::vector<double> cloud_ms;
  double up_ms = 0.0;
  double down_ms = 0.0;
  double prefill_demand_ms = 0.0;
  double hot_demand_ms = 0.0;
  double observed_arrival_rate = 0.0;
  ResourceBottleneck bottleneck = ResourceBottleneck::None;
};

ScheduledTaskKind scheduledTaskKind(const TaskSpec& task);
int scheduledTaskBatchSize(const TaskSpec& task);
std::array<int, scheduled_task_kind_count> eligibleTaskCounts(
  const WorldState& world, const ServerId& server);
ScorePressure estimateScorePressure(const WorldState& world, const SystemConfig& system);
SchedulerBacklog estimateSchedulerBacklog(
  const WorldState& world, const SystemConfig& system, const TimingCurves& curves);
