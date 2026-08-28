#pragma once

#include <scheduler/scheduling/SchedulingAnalysis.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

enum class ResourceDecisionReason {
  Selected,
  Busy,
  NoEligibleWork,
  PolicyDeferred
};

enum class CandidateDecisionReason {
  Selected,
  ResourceBusy,
  NotEligible,
  PolicyRejected,
  SupersededBySelectedTask
};

struct CandidateDecisionObservation {
  ScheduledTaskKind task;
  int eligible_count = 0;
  CandidateDecisionReason reason = CandidateDecisionReason::NotEligible;
};

struct ResourceUsage {
  double busy_time = 0.0;
  double eligible_idle_time = 0.0;
};

struct RequestPopulation {
  int prefill = 0;
  int cold = 0;
  int hot = 0;
  int finished = 0;
};

struct SimulationObservation {
  double timestamp = 0.0;
  SchedulerBacklog backlog;
  ScorePressure pressure;
  RequestPopulation population;
  bool edge_busy = false;
  std::vector<bool> clouds_busy;
  bool up_busy = false;
  bool down_busy = false;
  bool edge_eligible_idle = false;
  std::vector<bool> clouds_eligible_idle;
};

struct DecisionObservation {
  double timestamp = 0.0;
  ServerId server;
  ResourceDecisionReason reason = ResourceDecisionReason::NoEligibleWork;
  std::optional<ScheduledTaskKind> selected_task;
  std::array<int, scheduled_task_kind_count> eligible_by_task{};
  std::vector<CandidateDecisionObservation> candidates;
};

struct DecodeBatchHistogram {
  std::vector<std::size_t> pre;
  std::vector<std::size_t> proc;
  std::vector<std::size_t> post;
};

struct SimulationDiagnostics {
  double observation_span = 0.0;
  ResourceUsage edge;
  std::vector<ResourceUsage> clouds;
  ResourceUsage up;
  ResourceUsage down;
  double average_edge_queue_ms = 0.0;
  std::vector<double> average_cloud_queue_ms;
  double average_up_queue_ms = 0.0;
  double average_down_queue_ms = 0.0;
  double maximum_edge_queue_ms = 0.0;
  std::vector<double> maximum_cloud_queue_ms;
  double maximum_up_queue_ms = 0.0;
  double maximum_down_queue_ms = 0.0;
  double average_prefill_population = 0.0;
  double average_cold_population = 0.0;
  double average_hot_population = 0.0;
  int maximum_prefill_population = 0;
  int maximum_cold_population = 0;
  int maximum_hot_population = 0;
  double accumulated_tdr_pressure_time = 0.0;
  double accumulated_tpot_pressure_time = 0.0;
  double maximum_tdr_pressure = 0.0;
  double maximum_tpot_pressure = 0.0;
  DecodeBatchHistogram decode_batches;
  std::vector<SimulationObservation> observations;
  std::vector<DecisionObservation> decisions;
};

double resourceUtilization(const ResourceUsage& usage, double observation_span);
