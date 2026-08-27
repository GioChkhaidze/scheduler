#include <Simulator.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <istream>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>

namespace {

template <typename... Visitors>
struct Overloaded : Visitors... {
  using Visitors::operator()...;
};

template <typename... Visitors>
Overloaded(Visitors...) -> Overloaded<Visitors...>;

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
    for (std::size_t rid = 0; rid < workload_.size(); ++rid) {
      scheduleEvent(workload_[rid].arrival_time, ArrivalEvent{
        static_cast<int>(rid), workload_[rid].input_length});
    }
  }

  SimulationResult run() {
    while (!events_.empty() && result_.frame_count < options_.max_frames) {
      Frame frame = popFrame();
      processCompletedTasks(frame);
      applyFrame(world_, frame, config_.num_layers);

      ++result_.frame_count;
      if (options_.record_frames) {
        result_.frames.push_back(frame);
      }

      const std::vector<Assignment> assignments = policy_(world_);
      for (const Assignment& assignment : assignments) {
        startAndSchedule(assignment);
      }
    }

    result_.hit_frame_limit = !events_.empty() && result_.frame_count >= options_.max_frames;
    result_.completed = allRequestsFinished();
    result_.metrics = calculateMetrics();
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
    return std::visit(Overloaded{
      [&](const PrefillPreTask& value) {
        return interpolate(curves_.prefill_pre, request(value.rid).input_length);
      },
      [&](const PrefillProcTask& value) {
        const double full_duration = interpolate(curves_.prefill_proc, request(value.rid).input_length);
        return full_duration * (value.layer_end - value.layer_begin) / config_.num_layers;
      },
      [&](const PrefillPostTask& value) {
        return interpolate(curves_.prefill_post, request(value.rid).input_length);
      },
      [&](const DecodePreTask& value) {
        return interpolate(curves_.decode_pre, static_cast<int>(value.rids.size()));
      },
      [&](const DecodeProcTask& value) {
        return interpolate(curves_.decode_proc, static_cast<int>(value.rids.size()));
      },
      [&](const DecodePostTask& value) {
        return interpolate(curves_.decode_post, static_cast<int>(value.rids.size()));
      },
    }, task);
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
    std::visit(Overloaded{
      [&](const PrefillPreTask& task) {
        queueTransfer(
          timestamp, TransferDirection::Up, task.remote, TransferStage::Prefill,
          request(task.rid).input_length, {task.rid});
      },
      [&](const PrefillProcTask& task) {
        if (task.layer_end == config_.num_layers) {
          queueTransfer(
            timestamp, TransferDirection::Down, task.remote, TransferStage::Prefill,
            request(task.rid).input_length, {task.rid});
        }
      },
      [&](const PrefillPostTask&) {},
      [&](const DecodePreTask& task) {
        std::vector<std::vector<int>> by_remote(static_cast<std::size_t>(config_.K));
        for (const int rid : task.rids) {
          const int remote = *request(rid).remote;
          by_remote.at(static_cast<std::size_t>(remote)).push_back(rid);
        }
        for (int remote = 0; remote < config_.K; ++remote) {
          std::vector<int>& rids = by_remote.at(static_cast<std::size_t>(remote));
          if (!rids.empty()) {
            const std::int64_t length = static_cast<std::int64_t>(rids.size());
            queueTransfer(
              timestamp, TransferDirection::Up, remote, TransferStage::Decode, length, std::move(rids));
          }
        }
      },
      [&](const DecodeProcTask& task) {
        queueTransfer(
          timestamp, TransferDirection::Down, task.remote, TransferStage::Decode,
          static_cast<std::int64_t>(task.rids.size()), task.rids);
      },
      [&](const DecodePostTask&) {},
    }, event.task);
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
