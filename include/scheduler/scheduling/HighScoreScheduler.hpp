#pragma once

#include <scheduler/scheduling/SchedulingAnalysis.hpp>
#include <scheduler/scheduling/SharedLinkScheduler.hpp>

#include <array>
#include <cstddef>
#include <vector>

enum class HighScoreMode {
  Control,
  Waiting,
  Throughput,
  Adaptive
};

enum class HighScoreRegime {
  ThroughputOnly,
  WaitingOnly,
  HardSlo,
  Balanced
};

struct HighScoreFeatures {
  bool waiting_policy = true;
  bool throughput_policy = true;
  bool bottleneck_controller = true;
  bool deadline_batching = true;
  bool cold_admission = true;
  bool link_cloud_limit = true;
  bool prefill_aware_cloud_limit = false;
  bool evidence_gated_waiting = false;
  bool proactive_waiting = false;
  bool waiting_prefill_cloud_expansion = false;
  bool runtime_policy_hysteresis = false;
  bool single_cloud_specialization = false;
  bool throughput_idle_admission = false;
  bool throughput_hot_coalescing = false;
};

struct HighScoreTelemetry {
  std::size_t control_decisions = 0;
  std::size_t waiting_decisions = 0;
  std::size_t throughput_decisions = 0;
  std::size_t cold_admissions = 0;
  std::size_t cold_throttles = 0;
  std::size_t aged_promotions = 0;
  std::size_t policy_fallbacks = 0;
  std::size_t bottleneck_switches = 0;
  std::size_t runtime_policy_switches = 0;
  std::size_t hot_coalescing_waits = 0;
  std::array<std::size_t, 5> bottleneck_observations{};
};

struct BottleneckControllerState {
  ResourceBottleneck active = ResourceBottleneck::None;
  ResourceBottleneck candidate = ResourceBottleneck::None;
  int candidate_streak = 0;
  int target_hot_set_size = 1;
  int active_cloud_count = 1;
  bool waiting_specialization_latched = false;
  bool runtime_policy_initialized = false;
  bool active_runtime_policy_waiting = false;
  bool candidate_runtime_policy_waiting = false;
  int runtime_policy_candidate_streak = 0;
};

struct HighScoreSchedulerConfig {
  SharedLinkSchedulerConfig v7;
  SystemConfig system;
  TimingCurves curves;
  DecodeBatchPolicy decode_batches;
  HighScoreMode mode = HighScoreMode::Adaptive;
  HighScoreRegime score_regime = HighScoreRegime::Balanced;
  HighScoreFeatures features;
  int waiting_hot_target = 1;
  int throughput_hot_target = 1;
  int throughput_active_clouds = 1;
  double estimated_decode_capacity = 0.0;
  ResourceBottleneck static_decode_bottleneck = ResourceBottleneck::None;
  BottleneckControllerState controller;
  HighScoreTelemetry telemetry;
};

HighScoreRegime classifyHighScoreRegime(const SystemConfig& system);
int selectThroughputActiveCloudCount(const SystemConfig& system, const TimingCurves& curves, int batch_size);
ResourceBottleneck estimateStaticDecodeBottleneck(
  const SystemConfig& system, const TimingCurves& curves, int batch_size, int active_clouds);
HighScoreSchedulerConfig buildHighScoreSchedulerConfig(
  const SystemConfig& system,
  const TimingCurves& curves,
  HighScoreMode mode = HighScoreMode::Adaptive,
  HighScoreFeatures features = {});
HighScoreSchedulerConfig buildFinalSchedulerConfig(const SystemConfig& system, const TimingCurves& curves);
std::vector<Assignment> chooseHighScoreAssignments(
  const WorldState& world, int num_layers, HighScoreSchedulerConfig& config);
