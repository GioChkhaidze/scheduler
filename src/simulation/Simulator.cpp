#include <scheduler/scheduling/SharedLinkScheduler.hpp>
#include <scheduler/simulation/Simulator.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <istream>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>

namespace {

struct ScheduledEvent {
  double timestamp;
  std::uint64_t order;
  Event event;
};

struct EarlierEvent {
  bool operator()(const ScheduledEvent& left, const ScheduledEvent& right) const {
    if (left.timestamp != right.timestamp) {
      return left.timestamp > right.timestamp;
    }
    return left.order > right.order;
  }
};

class SimulationEngine {
public:
  SimulationEngine(
    const SystemConfig& config,
    const TimingCurves& curves,
    const std::vector<SimulationRequest>& workload,
    const SimulationPolicy& policy,
    const SimulationOptions& options)
    : config_(config),
      curves_(curves),
      workload_(workload),
      policy_(policy),
      options_(options),
      world_(config.K),
      prefill_ready_times_(workload.size()),
      token_times_(workload.size()) {
    validateInputs();
    result_.diagnostics.clouds.resize(static_cast<std::size_t>(config.K));
    result_.diagnostics.average_cloud_queue_ms.resize(static_cast<std::size_t>(config.K));
    result_.diagnostics.maximum_cloud_queue_ms.resize(static_cast<std::size_t>(config.K));
    cloud_queue_integral_.resize(static_cast<std::size_t>(config.K));
    for (std::size_t rid = 0; rid < workload_.size(); ++rid) {
      scheduleEvent(workload_[rid].arrival_time, ArrivalEvent{
        static_cast<int>(rid), workload_[rid].input_length});
    }
  }

  SimulationResult run() {
    while (!events_.empty() && result_.frame_count < options_.max_frames) {
      Frame frame = popFrame();
      accumulateObservationInterval(frame.timestamp);
      processCompletedTasks(frame);
      observeLinkFrame(world_, frame, config_);
      applyFrame(world_, frame, config_.num_layers);

      ++result_.frame_count;
      if (options_.record_frames) {
        result_.frames.push_back(frame);
      }

      const std::vector<Assignment> assignments = policy_(world_);
      if (options_.record_observations) {
        recordDecisions(assignments);
      }
      for (const Assignment& assignment : assignments) {
        startAndSchedule(assignment);
      }
      if (options_.record_observations) {
        captureObservation();
      }
    }

    result_.hit_frame_limit = !events_.empty() && result_.frame_count >= options_.max_frames;
    result_.completed = allRequestsFinished();
    result_.link_reconciliation_count = world_.links.reconciliation_count;
    result_.maximum_link_reconciliation_error = world_.links.maximum_reconciliation_error;
    result_.metrics = calculateMetrics();
    finalizeDiagnostics();
    return result_;
  }

private:
  void validateInputs() const {
    assert(config_.K > 0);
    assert(config_.num_layers > 0);
    assert(config_.S >= 0.0);
    assert(!workload_.empty());
    assert(options_.max_frames > 0);

    double previous_arrival = workload_.front().arrival_time;
    for (const SimulationRequest& request : workload_) {
      assert(request.arrival_time >= previous_arrival);
      assert(request.input_length > 0);
      assert(request.output_length > 0);
      previous_arrival = request.arrival_time;
    }
  }

  void scheduleEvent(double timestamp, Event event) {
    assert(timestamp >= world_.current_time);
    events_.push(ScheduledEvent{timestamp, next_event_order_++, std::move(event)});
  }

  Frame popFrame() {
    assert(!events_.empty());
    Frame frame{events_.top().timestamp, {}};

    while (!events_.empty() && events_.top().timestamp == frame.timestamp) {
      ScheduledEvent scheduled = events_.top();
      events_.pop();
      frame.events.push_back(std::move(scheduled.event));
    }

    return frame;
  }

  double taskDuration(const TaskSpec& task) const {
    return estimateTaskDuration(world_, task, curves_, config_.num_layers);
  }

  const Request& request(int rid) const {
    assert(rid >= 0);
    return world_.requests.at(static_cast<std::size_t>(rid));
  }

  void startAndSchedule(const Assignment& assignment) {
    const double duration = taskDuration(assignment.task);
    const double completion_time = world_.current_time + config_.S + duration;

    std::vector<int> finishing_rids;
    if (const DecodePostTask* task = std::get_if<DecodePostTask>(&assignment.task)) {
      for (const int rid : task->rids) {
        const Request& current = request(rid);
        const int output_length = workload_.at(static_cast<std::size_t>(rid)).output_length;
        assert(current.tokens_produced < output_length);
        if (current.tokens_produced + 1 == output_length) {
          finishing_rids.push_back(rid);
        }
      }
    }

    recordLinkAssignment(world_, assignment, config_, curves_);
    startAssignment(world_, assignment, config_.num_layers);
    scheduleEvent(completion_time, TaskDoneEvent{assignment.server, assignment.task, duration});
    for (const int rid : finishing_rids) {
      scheduleEvent(completion_time, FinishEvent{rid});
    }
  }

  void processCompletedTasks(const Frame& frame) {
    for (const Event& event : frame.events) {
      const TaskDoneEvent* task_done = std::get_if<TaskDoneEvent>(&event);
      if (task_done == nullptr) {
        continue;
      }

      recordMilestone(*task_done, frame.timestamp);
      queueTriggeredTransfers(*task_done, frame.timestamp);
    }
  }

  void recordMilestone(const TaskDoneEvent& event, double timestamp) {
    if (const PrefillPostTask* task = std::get_if<PrefillPostTask>(&event.task)) {
      std::optional<double>& ready_time = prefill_ready_times_.at(static_cast<std::size_t>(task->rid));
      assert(!ready_time.has_value());
      ready_time = timestamp;
      return;
    }

    if (const DecodePostTask* task = std::get_if<DecodePostTask>(&event.task)) {
      for (const int rid : task->rids) {
        token_times_.at(static_cast<std::size_t>(rid)).push_back(timestamp);
      }
    }
  }

  void queueTriggeredTransfers(const TaskDoneEvent& event, double timestamp) {
    for (LinkTransferSpec transfer : deriveTriggeredTransfers(world_, event.task, config_.num_layers)) {
      queueTransfer(
        timestamp,
        transfer.direction,
        transfer.remote,
        transfer.stage,
        transfer.length,
        std::move(transfer.rids));
    }
  }

  void queueTransfer(
    double queued_at,
    TransferDirection direction,
    int remote,
    TransferStage stage,
    std::int64_t length,
    std::vector<int> rids) {
    double& available_at = direction == TransferDirection::Up ? up_available_at_ : down_available_at_;
    const double starts_at = std::max(queued_at, available_at);
    const double completes_at = starts_at + transferTime(config_, length);
    available_at = completes_at;

    const std::int64_t size_bytes = length * config_.bytes_per_token;
    scheduleEvent(completes_at, TransferDoneEvent{
      direction, remote, size_bytes, stage, std::move(rids)});
  }

  static int eligibleCount(const std::array<int, scheduled_task_kind_count>& counts) {
    int result = 0;
    for (const int count : counts) {
      result += count;
    }
    return result;
  }

  const Assignment* assignmentForServer(
    const std::vector<Assignment>& assignments, const ServerId& server) const {
    const auto found = std::find_if(assignments.begin(), assignments.end(), [&](const Assignment& assignment) {
      return assignment.server.type == server.type
        && (server.type == ServerType::Edge || assignment.server.cloud_index == server.cloud_index);
    });
    return found == assignments.end() ? nullptr : &*found;
  }

  void recordDecisionForServer(
    const std::vector<Assignment>& assignments, const ServerId& server, bool busy) {
    const std::array<int, scheduled_task_kind_count> eligible = eligibleTaskCounts(world_, server);
    const Assignment* selected = assignmentForServer(assignments, server);
    DecisionObservation decision{
      .timestamp = world_.current_time,
      .server = server,
      .reason = ResourceDecisionReason::NoEligibleWork,
      .selected_task = std::nullopt,
      .eligible_by_task = eligible,
    };
    if (selected != nullptr) {
      decision.reason = ResourceDecisionReason::Selected;
      decision.selected_task = scheduledTaskKind(selected->task);
    } else if (busy) {
      decision.reason = ResourceDecisionReason::Busy;
    } else if (eligibleCount(eligible) > 0) {
      decision.reason = ResourceDecisionReason::PolicyDeferred;
    }
    decision.candidates.reserve(scheduled_task_kind_count);
    for (std::size_t index = 0; index < scheduled_task_kind_count; ++index) {
      const ScheduledTaskKind task = static_cast<ScheduledTaskKind>(index);
      CandidateDecisionReason reason = CandidateDecisionReason::NotEligible;
      if (eligible[index] > 0) {
        if (decision.selected_task == task) {
          reason = CandidateDecisionReason::Selected;
        } else if (busy) {
          reason = CandidateDecisionReason::ResourceBusy;
        } else if (selected != nullptr) {
          reason = CandidateDecisionReason::SupersededBySelectedTask;
        } else {
          reason = CandidateDecisionReason::PolicyRejected;
        }
      }
      decision.candidates.push_back({task, eligible[index], reason});
    }
    result_.diagnostics.decisions.push_back(std::move(decision));
  }

  static void incrementBatch(std::vector<std::size_t>& histogram, int batch_size) {
    assert(batch_size > 0);
    if (histogram.size() <= static_cast<std::size_t>(batch_size)) {
      histogram.resize(static_cast<std::size_t>(batch_size + 1));
    }
    ++histogram[static_cast<std::size_t>(batch_size)];
  }

  void recordDecisions(const std::vector<Assignment>& assignments) {
    recordDecisionForServer(assignments, ServerId{ServerType::Edge, -1}, world_.edge.busy);
    for (std::size_t remote = 0; remote < world_.clouds.size(); ++remote) {
      recordDecisionForServer(
        assignments,
        ServerId{ServerType::Cloud, static_cast<int>(remote)},
        world_.clouds[remote].busy);
    }
    for (const Assignment& assignment : assignments) {
      const int batch_size = scheduledTaskBatchSize(assignment.task);
      switch (scheduledTaskKind(assignment.task)) {
        case ScheduledTaskKind::DecodePre:
          incrementBatch(result_.diagnostics.decode_batches.pre, batch_size);
          break;
        case ScheduledTaskKind::DecodeProc:
          incrementBatch(result_.diagnostics.decode_batches.proc, batch_size);
          break;
        case ScheduledTaskKind::DecodePost:
          incrementBatch(result_.diagnostics.decode_batches.post, batch_size);
          break;
        default:
          break;
      }
    }
  }

  static RequestPopulation requestPopulation(const WorldState& world) {
    RequestPopulation result;
    for (const Request& request : world.requests) {
      switch (classifyRequest(request)) {
        case RequestClass::Prefill:
          ++result.prefill;
          break;
        case RequestClass::Cold:
          ++result.cold;
          break;
        case RequestClass::Hot:
          ++result.hot;
          break;
        case RequestClass::Finished:
          ++result.finished;
          break;
      }
    }
    return result;
  }

  void captureObservation() {
    SimulationObservation observation;
    observation.timestamp = world_.current_time;
    observation.backlog = estimateSchedulerBacklog(world_, config_, curves_);
    observation.pressure = estimateScorePressure(world_, config_);
    observation.population = requestPopulation(world_);
    observation.edge_busy = world_.edge.busy;
    observation.clouds_busy.reserve(world_.clouds.size());
    observation.clouds_eligible_idle.reserve(world_.clouds.size());
    for (std::size_t remote = 0; remote < world_.clouds.size(); ++remote) {
      const bool busy = world_.clouds[remote].busy;
      observation.clouds_busy.push_back(busy);
      const auto eligible = eligibleTaskCounts(
        world_, ServerId{ServerType::Cloud, static_cast<int>(remote)});
      observation.clouds_eligible_idle.push_back(!busy && eligibleCount(eligible) > 0);
    }
    observation.up_busy = up_available_at_ > world_.current_time;
    observation.down_busy = down_available_at_ > world_.current_time;
    const auto edge_eligible = eligibleTaskCounts(world_, ServerId{ServerType::Edge, -1});
    observation.edge_eligible_idle = !world_.edge.busy && eligibleCount(edge_eligible) > 0;

    SimulationDiagnostics& diagnostics = result_.diagnostics;
    diagnostics.maximum_edge_queue_ms = std::max(
      diagnostics.maximum_edge_queue_ms, observation.backlog.edge_ms);
    diagnostics.maximum_up_queue_ms = std::max(
      diagnostics.maximum_up_queue_ms, observation.backlog.up_ms);
    diagnostics.maximum_down_queue_ms = std::max(
      diagnostics.maximum_down_queue_ms, observation.backlog.down_ms);
    for (std::size_t remote = 0; remote < observation.backlog.cloud_ms.size(); ++remote) {
      diagnostics.maximum_cloud_queue_ms[remote] = std::max(
        diagnostics.maximum_cloud_queue_ms[remote], observation.backlog.cloud_ms[remote]);
    }
    diagnostics.maximum_prefill_population = std::max(
      diagnostics.maximum_prefill_population, observation.population.prefill);
    diagnostics.maximum_cold_population = std::max(
      diagnostics.maximum_cold_population, observation.population.cold);
    diagnostics.maximum_hot_population = std::max(
      diagnostics.maximum_hot_population, observation.population.hot);
    diagnostics.maximum_tdr_pressure = std::max(
      diagnostics.maximum_tdr_pressure, observation.pressure.tdr);
    diagnostics.maximum_tpot_pressure = std::max(
      diagnostics.maximum_tpot_pressure, observation.pressure.tpot);
    last_observation_ = observation;
    diagnostics.observations.push_back(std::move(observation));
  }

  void accumulateObservationInterval(double timestamp) {
    if (!options_.record_observations || !last_observation_.has_value()) {
      return;
    }
    const SimulationObservation& observation = *last_observation_;
    const double duration = timestamp - observation.timestamp;
    assert(duration >= 0.0);
    SimulationDiagnostics& diagnostics = result_.diagnostics;
    diagnostics.observation_span += duration;
    diagnostics.edge.busy_time += duration * observation.edge_busy;
    diagnostics.edge.eligible_idle_time += duration * observation.edge_eligible_idle;
    diagnostics.up.busy_time += duration * observation.up_busy;
    diagnostics.down.busy_time += duration * observation.down_busy;
    edge_queue_integral_ += duration * observation.backlog.edge_ms;
    up_queue_integral_ += duration * observation.backlog.up_ms;
    down_queue_integral_ += duration * observation.backlog.down_ms;
    prefill_population_integral_ += duration * observation.population.prefill;
    cold_population_integral_ += duration * observation.population.cold;
    hot_population_integral_ += duration * observation.population.hot;
    diagnostics.accumulated_tdr_pressure_time += duration * observation.pressure.tdr;
    diagnostics.accumulated_tpot_pressure_time += duration * observation.pressure.tpot;
    for (std::size_t remote = 0; remote < observation.clouds_busy.size(); ++remote) {
      diagnostics.clouds[remote].busy_time += duration * observation.clouds_busy[remote];
      diagnostics.clouds[remote].eligible_idle_time +=
        duration * observation.clouds_eligible_idle[remote];
      cloud_queue_integral_[remote] += duration * observation.backlog.cloud_ms[remote];
    }
  }

  void finalizeDiagnostics() {
    if (!options_.record_observations || result_.diagnostics.observation_span <= 0.0) {
      return;
    }
    SimulationDiagnostics& diagnostics = result_.diagnostics;
    const double span = diagnostics.observation_span;
    diagnostics.average_edge_queue_ms = edge_queue_integral_ / span;
    diagnostics.average_up_queue_ms = up_queue_integral_ / span;
    diagnostics.average_down_queue_ms = down_queue_integral_ / span;
    diagnostics.average_prefill_population = prefill_population_integral_ / span;
    diagnostics.average_cold_population = cold_population_integral_ / span;
    diagnostics.average_hot_population = hot_population_integral_ / span;
    for (std::size_t remote = 0; remote < cloud_queue_integral_.size(); ++remote) {
      diagnostics.average_cloud_queue_ms[remote] = cloud_queue_integral_[remote] / span;
    }
  }

  bool allRequestsFinished() const {
    return world_.requests.size() == workload_.size()
      && std::all_of(world_.requests.begin(), world_.requests.end(), [](const Request& current) {
        return current.state == RequestState::Finished;
      });
  }

  SimulationMetrics calculateMetrics() const {
    SimulationMetrics metrics;
    for (const SimulationRequest& item : workload_) {
      metrics.total_tokens += item.output_length;
    }
    if (!allRequestsFinished()) {
      return metrics;
    }

    double total_tdr = 0.0;
    double total_tpot = 0.0;
    std::size_t tpot_count = 0;
    double latest_token_time = 0.0;

    for (std::size_t rid = 0; rid < workload_.size(); ++rid) {
      assert(prefill_ready_times_[rid].has_value());
      assert(token_times_[rid].size() == static_cast<std::size_t>(workload_[rid].output_length));
      total_tdr += *prefill_ready_times_[rid] - workload_[rid].arrival_time;
      latest_token_time = std::max(latest_token_time, token_times_[rid].back());

      for (std::size_t token = 1; token < token_times_[rid].size(); ++token) {
        total_tpot += token_times_[rid][token] - token_times_[rid][token - 1];
        ++tpot_count;
      }
    }

    metrics.total_elapsed_time = latest_token_time - workload_.front().arrival_time;
    assert(metrics.total_elapsed_time > 0.0);
    metrics.tp = metrics.total_tokens / metrics.total_elapsed_time;
    metrics.mean_tdr = total_tdr / workload_.size();
    metrics.mean_tpot = tpot_count == 0 ? 0.0 : total_tpot / tpot_count;

    const double excess_tdr = std::max(0.0, (metrics.mean_tdr - config_.SLO1) / config_.SLO1);
    const double excess_tpot = std::max(0.0, (metrics.mean_tpot - config_.SLO2) / config_.SLO2);
    metrics.dist = std::hypot(excess_tdr, excess_tpot);
    metrics.norm_tp = std::clamp(
      (metrics.tp - config_.tp_base) / (config_.tp_UB - config_.tp_base), 0.0, 1.0);
    metrics.norm_c = config_.dist_base > 0.0
      ? std::max(0.0, 1.0 - metrics.dist / config_.dist_base)
      : static_cast<double>(metrics.dist == 0.0);
    metrics.normalized_score = config_.w_tp * metrics.norm_tp + config_.w_c * metrics.norm_c;
    metrics.score = 1000.0 * metrics.normalized_score;
    return metrics;
  }

  const SystemConfig& config_;
  const TimingCurves& curves_;
  const std::vector<SimulationRequest>& workload_;
  const SimulationPolicy& policy_;
  SimulationOptions options_;
  WorldState world_;
  std::priority_queue<ScheduledEvent, std::vector<ScheduledEvent>, EarlierEvent> events_;
  std::uint64_t next_event_order_ = 0;
  double up_available_at_ = 0.0;
  double down_available_at_ = 0.0;
  std::vector<std::optional<double>> prefill_ready_times_;
  std::vector<std::vector<double>> token_times_;
  std::optional<SimulationObservation> last_observation_;
  std::vector<double> cloud_queue_integral_;
  double edge_queue_integral_ = 0.0;
  double up_queue_integral_ = 0.0;
  double down_queue_integral_ = 0.0;
  double prefill_population_integral_ = 0.0;
  double cold_population_integral_ = 0.0;
  double hot_population_integral_ = 0.0;
  SimulationResult result_;
};

} // namespace

std::vector<SimulationRequest> readSimulationWorkload(std::istream& input) {
  int request_count = 0;
  if (!(input >> request_count) || request_count <= 0) {
    throw std::runtime_error("Simulation workload must start with a positive request count");
  }

  std::vector<SimulationRequest> workload;
  workload.reserve(static_cast<std::size_t>(request_count));
  for (int rid = 0; rid < request_count; ++rid) {
    SimulationRequest request{};
    if (!(input >> request.arrival_time >> request.input_length >> request.output_length)) {
      throw std::runtime_error("Invalid simulation request row");
    }
    workload.push_back(request);
  }
  return workload;
}

SimulationResult simulate(
  const SystemConfig& config,
  const TimingCurves& curves,
  const std::vector<SimulationRequest>& workload,
  const SimulationPolicy& policy,
  const SimulationOptions& options) {
  return SimulationEngine(config, curves, workload, policy, options).run();
}
