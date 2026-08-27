#include <Scheduler.hpp>
#include <Simulator.hpp>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <optional>
#include <sstream>
#include <variant>
#include <vector>

namespace {

bool isEqual(double left, double right, double tolerance = 1e-9) {
  return std::abs(left - right) <= tolerance;
}

SystemConfig makeExampleConfig() {
  return {
    .SLO1 = 30.0,
    .SLO2 = 15.0,
    .tp_UB = 0.0625,
    .tp_base = 0.022222222,
    .dist_base = 0.0,
    .w_tp = 0.5,
    .w_c = 0.5,
    .S = 1.0,
    .latency_in_ms = 2.0,
    .bandwidth_gbps = 1.0,
    .K = 1,
    .bytes_per_token = 125000,
    .num_layers = 4,
  };
}

TimingCurves makeExampleCurves() {
  const TaskTimeTable table{{
    {1, 3.0, 10.0, 2.0, 1.0, 4.0, 1.0},
    {4, 3.0, 10.0, 2.0, 1.0, 4.0, 1.0},
  }};
  return buildTimingCurves(table);
}

SimulationResult simulateExample(int output_length, bool record_frames = true) {
  const SystemConfig config = makeExampleConfig();
  const TimingCurves curves = makeExampleCurves();
  const SimulationPolicy policy = [&](const WorldState& world) {
    return chooseSingletonAssignments(world, config.num_layers);
  };
  return simulate(config, curves, {{0.0, 4, output_length}}, policy, {record_frames, 100});
}

std::optional<int> firstRequest(const WorldState& world, RequestState state, std::optional<int> remote = std::nullopt) {
  for (std::size_t rid = 0; rid < world.requests.size(); ++rid) {
    const Request& request = world.requests[rid];
    if (request.state == state && (!remote.has_value() || request.remote == remote)) {
      return static_cast<int>(rid);
    }
  }
  return std::nullopt;
}

std::vector<int> allRequests(const WorldState& world, RequestState state, std::optional<int> remote = std::nullopt) {
  std::vector<int> rids;
  for (std::size_t rid = 0; rid < world.requests.size(); ++rid) {
    const Request& request = world.requests[rid];
    if (request.state == state && (!remote.has_value() || request.remote == remote)) {
      rids.push_back(static_cast<int>(rid));
    }
  }
  return rids;
}

SimulationPolicy makeSynchronizedTwoRequestPolicy(int num_layers) {
  return [=](const WorldState& world) {
    std::vector<Assignment> assignments;
    if (!world.edge.busy) {
      std::vector<int> rids = allRequests(world, RequestState::ReadyDecodePost);
      if (rids.size() == 2) {
        assignments.push_back({ServerId{ServerType::Edge, -1}, DecodePostTask{std::move(rids)}});
      } else if (const auto prefill_post_rid = firstRequest(world, RequestState::ReadyPrefillPost)) {
        assignments.push_back({
          ServerId{ServerType::Edge, -1},
          PrefillPostTask{*world.requests[*prefill_post_rid].remote, *prefill_post_rid},
        });
      } else {
        rids = allRequests(world, RequestState::ReadyDecodePre);
        if (rids.size() == 2) {
          assignments.push_back({ServerId{ServerType::Edge, -1}, DecodePreTask{std::move(rids)}});
        } else if (const auto prefill_pre_rid = firstRequest(world, RequestState::ReadyPrefillPre)) {
          assignments.push_back({
            ServerId{ServerType::Edge, -1}, PrefillPreTask{*prefill_pre_rid % 2, *prefill_pre_rid}});
        }
      }
    }

    for (int remote = 0; remote < 2; ++remote) {
      if (world.clouds[static_cast<std::size_t>(remote)].busy) {
        continue;
      }
      std::vector<int> rids = allRequests(world, RequestState::ReadyDecodeProc, remote);
      if (!rids.empty()) {
        assignments.push_back({ServerId{ServerType::Cloud, remote}, DecodeProcTask{remote, std::move(rids)}});
      } else if (const auto rid = firstRequest(world, RequestState::ReadyPrefillProc, remote)) {
        assignments.push_back({
          ServerId{ServerType::Cloud, remote}, PrefillProcTask{0, num_layers, remote, *rid}});
      }
    }
    return assignments;
  };
}

void testReproducesStatementExampleAndJudgeScore() {
  const SimulationResult result = simulateExample(1);

  assert(result.completed);
  assert(!result.hit_frame_limit);
  assert(result.frame_count == 11);
  assert(result.metrics.total_tokens == 1);
  assert(isEqual(result.metrics.total_elapsed_time, 45.0));
  assert(isEqual(result.metrics.tp, 1.0 / 45.0));
  assert(isEqual(result.metrics.mean_tdr, 30.0));
  assert(isEqual(result.metrics.mean_tpot, 0.0));
  assert(isEqual(result.metrics.dist, 0.0));
  assert(isEqual(result.metrics.norm_c, 1.0));
  assert(isEqual(result.metrics.score, 500.0000027586, 1e-6));

  const std::vector<double> expected_times{0.0, 4.0, 10.0, 21.0, 27.0, 30.0, 32.0, 35.0, 40.0, 43.0, 45.0};
  assert(result.frames.size() == expected_times.size());
  for (std::size_t i = 0; i < expected_times.size(); ++i) {
    assert(isEqual(result.frames[i].timestamp, expected_times[i]));
  }

  assert(result.frames.back().events.size() == 2);
  assert(std::holds_alternative<TaskDoneEvent>(result.frames.back().events[0]));
  assert(std::holds_alternative<FinishEvent>(result.frames.back().events[1]));
}

void testPoolsConsecutiveTokenGapsExactly() {
  const SimulationResult result = simulateExample(3);

  assert(result.completed);
  assert(result.metrics.total_tokens == 3);
  assert(isEqual(result.metrics.total_elapsed_time, 75.0));
  assert(isEqual(result.metrics.tp, 3.0 / 75.0));
  assert(isEqual(result.metrics.mean_tdr, 30.0));
  assert(isEqual(result.metrics.mean_tpot, 15.0));
  assert(isEqual(result.metrics.dist, 0.0));
}

void testQueuesDecodeUploadsByRemoteAndRunsDirectionsIndependently() {
  SystemConfig config = makeExampleConfig();
  config.K = 2;
  config.num_layers = 1;
  config.latency_in_ms = 5.0;
  config.bandwidth_gbps = 100.0;
  config.bytes_per_token = 1;
  const TimingCurve curve{{{1, 0.001}, {2, 0.001}}};
  const TimingCurves curves{
    .prefill_pre = curve,
    .prefill_proc = curve,
    .prefill_post = curve,
    .decode_pre = curve,
    .decode_proc = curve,
    .decode_post = curve,
  };
  const SimulationResult result = simulate(
    config,
    curves,
    {{0.0, 1, 1}, {0.0, 1, 1}},
    makeSynchronizedTwoRequestPolicy(config.num_layers),
    {true, 100});

  assert(result.completed);
  double decode_pre_done = -1.0;
  double remote_zero_proc_done = -1.0;
  double remote_zero_down_done = -1.0;
  std::vector<std::pair<int, double>> decode_uploads;

  for (const Frame& frame : result.frames) {
    for (const Event& event : frame.events) {
      if (const TaskDoneEvent* done = std::get_if<TaskDoneEvent>(&event)) {
        if (std::holds_alternative<DecodePreTask>(done->task)) {
          decode_pre_done = frame.timestamp;
        }
        if (const DecodeProcTask* task = std::get_if<DecodeProcTask>(&done->task);
            task != nullptr && task->remote == 0) {
          remote_zero_proc_done = frame.timestamp;
        }
      }
      if (const TransferDoneEvent* transfer = std::get_if<TransferDoneEvent>(&event)) {
        if (transfer->stage == TransferStage::Decode && transfer->direction == TransferDirection::Up) {
          decode_uploads.push_back({transfer->remote, frame.timestamp});
        }
        if (transfer->stage == TransferStage::Decode && transfer->direction == TransferDirection::Down
            && transfer->remote == 0) {
          remote_zero_down_done = frame.timestamp;
        }
      }
    }
  }

  const double transfer_duration = transferTime(config, 1);
  assert(decode_uploads.size() == 2);
  assert(decode_uploads[0].first == 0);
  assert(decode_uploads[1].first == 1);
  assert(isEqual(decode_uploads[0].second, decode_pre_done + transfer_duration));
  assert(isEqual(decode_uploads[1].second, decode_pre_done + 2.0 * transfer_duration));
  assert(remote_zero_proc_done < decode_uploads[1].second);
  assert(isEqual(remote_zero_down_done, remote_zero_proc_done + transfer_duration));
}

void testReportsStuckAndFrameLimitWithoutInventingEvents() {
  const SystemConfig config = makeExampleConfig();
  const TimingCurves curves = makeExampleCurves();
  const SimulationPolicy idle = [](const WorldState&) {
    return std::vector<Assignment>{};
  };

  const SimulationResult stuck = simulate(config, curves, {{0.0, 4, 1}}, idle);
  assert(!stuck.completed);
  assert(!stuck.hit_frame_limit);
  assert(stuck.frame_count == 1);
  assert(stuck.metrics.score == 0.0);

  const SimulationPolicy singleton = [&](const WorldState& world) {
    return chooseSingletonAssignments(world, config.num_layers);
  };
  const SimulationResult limited = simulate(config, curves, {{0.0, 4, 1}}, singleton, {false, 1});
  assert(!limited.completed);
  assert(limited.hit_frame_limit);
  assert(limited.frame_count == 1);
}

void testReadsWorkloadRows() {
  std::istringstream input("2\n0.0 4 3\n1.5 8 7\n");
  const std::vector<SimulationRequest> workload = readSimulationWorkload(input);
  assert(workload.size() == 2);
  assert(workload[0].input_length == 4);
  assert(workload[0].output_length == 3);
  assert(isEqual(workload[1].arrival_time, 1.5));
}

} // namespace

int main() {
  testReproducesStatementExampleAndJudgeScore();
  testPoolsConsecutiveTokenGapsExactly();
  testQueuesDecodeUploadsByRemoteAndRunsDirectionsIndependently();
  testReportsStuckAndFrameLimitWithoutInventingEvents();
  testReadsWorkloadRows();
  return 0;
}
