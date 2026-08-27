#pragma once

#include <DecodeBatchScheduler.hpp>
#include <SystemConfig.hpp>
#include <TaskTimeTable.hpp>
#include <WorldState.hpp>

#include <optional>
#include <vector>

enum class RequestClass {
  Prefill,
  Cold,
  Hot,
  Finished
};

struct ScoreAwareSchedulerConfig {
  DecodeBatchPolicy decode_batches;
  int target_hot_set_size;
  double slo_tdr;
  double slo_tpot;
  double waiting_weight;
  bool prefill_warmup;
};

RequestClass classifyRequest(const Request& request);
int countActiveHotSet(const WorldState& world);
ScoreAwareSchedulerConfig buildScoreAwareSchedulerConfig(
  const SystemConfig& system,
  const TimingCurves& curves,
  std::optional<int> target_hot_set_size = std::nullopt);
std::vector<Assignment> chooseScoreAwareAssignments(
  const WorldState& world, int num_layers, const ScoreAwareSchedulerConfig& config);
