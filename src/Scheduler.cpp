#include <Scheduler.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace {

std::optional<int> findFirstRequest(
  const WorldState& world, RequestState state, std::optional<int> remote = std::nullopt) {
  for (std::size_t i = 0; i < world.requests.size(); ++i) {
    const Request& request = world.requests[i];
    if (request.state != state) {
      continue;
    }
    if (remote.has_value() && request.remote != remote) {
      continue;
    }

    return static_cast<int>(i);
  }

  return std::nullopt;
}

std::vector<int> findRequests(
  const WorldState& world, RequestState state, std::optional<int> remote, std::size_t limit) {
  std::vector<int> rids;
  rids.reserve(std::min(limit, world.requests.size()));

  for (std::size_t i = 0; i < world.requests.size() && rids.size() < limit; ++i) {
    const Request& request = world.requests[i];
    if (request.state == state && (!remote.has_value() || request.remote == remote)) {
      rids.push_back(static_cast<int>(i));
    }
  }

  return rids;
}

int batchSizeForReadyCount(const std::vector<int>& choices, std::size_t ready_count) {
  assert(ready_count > 0);
  assert(choices.size() > 1);
  const std::size_t index = std::min(ready_count, choices.size() - 1);
  return choices[index];
}

std::vector<int> chooseDecodeRids(
  const WorldState& world,
  RequestState state,
  std::optional<int> remote,
  const std::vector<int>* batch_choices) {
  const std::size_t limit = batch_choices == nullptr ? 1 : std::numeric_limits<std::size_t>::max();
  std::vector<int> rids = findRequests(world, state, remote, limit);
  if (rids.empty() || batch_choices == nullptr) {
    return rids;
  }

  rids.resize(static_cast<std::size_t>(batchSizeForReadyCount(*batch_choices, rids.size())));
  return rids;
}

std::optional<Assignment> chooseEdgeAssignment(const WorldState& world, const DecodeBatchPolicy* batch_policy) {
  if (world.edge.busy) {
    return std::nullopt;
  }

  const std::vector<int>* post_choices = batch_policy == nullptr ? nullptr : &batch_policy->post_by_ready_count;
  if (std::vector<int> rids = chooseDecodeRids(
        world, RequestState::ReadyDecodePost, std::nullopt, post_choices); !rids.empty()) {
    return Assignment{ServerId{ServerType::Edge, -1}, DecodePostTask{std::move(rids)}};
  }

  if (const auto rid = findFirstRequest(world, RequestState::ReadyPrefillPost)) {
    const Request& request = world.requests.at(static_cast<std::size_t>(*rid));
    assert(request.remote.has_value());
    return Assignment{ServerId{ServerType::Edge, -1}, PrefillPostTask{*request.remote, *rid}};
  }

  const std::vector<int>* pre_choices = batch_policy == nullptr ? nullptr : &batch_policy->pre_by_ready_count;
  if (std::vector<int> rids = chooseDecodeRids(
        world, RequestState::ReadyDecodePre, std::nullopt, pre_choices); !rids.empty()) {
    return Assignment{ServerId{ServerType::Edge, -1}, DecodePreTask{std::move(rids)}};
  }

  if (const auto rid = findFirstRequest(world, RequestState::ReadyPrefillPre)) {
    assert(!world.clouds.empty());
    const int remote = *rid % static_cast<int>(world.clouds.size());
    return Assignment{ServerId{ServerType::Edge, -1}, PrefillPreTask{remote, *rid}};
  }

  return std::nullopt;
}

std::optional<Assignment> chooseCloudAssignment(
  const WorldState& world, int remote, int num_layers, const DecodeBatchPolicy* batch_policy) {
  const ServerState& cloud = world.clouds.at(static_cast<std::size_t>(remote));
  if (cloud.busy) {
    return std::nullopt;
  }

  const std::vector<int>* proc_choices = batch_policy == nullptr ? nullptr : &batch_policy->proc_by_ready_count;
  if (std::vector<int> rids = chooseDecodeRids(
        world, RequestState::ReadyDecodeProc, remote, proc_choices); !rids.empty()) {
    return Assignment{ServerId{ServerType::Cloud, remote}, DecodeProcTask{remote, std::move(rids)}};
  }

  if (const auto rid = findFirstRequest(world, RequestState::ReadyPrefillProc, remote)) {
    const Request& request = world.requests.at(static_cast<std::size_t>(*rid));
    assert(request.next_prefill_layer < num_layers);
    return Assignment{
      ServerId{ServerType::Cloud, remote},
      PrefillProcTask{request.next_prefill_layer, num_layers, remote, *rid},
    };
  }

  return std::nullopt;
}

std::vector<Assignment> chooseAssignments(
  const WorldState& world, int num_layers, const DecodeBatchPolicy* batch_policy) {
  assert(num_layers > 0);

  std::vector<Assignment> assignments;
  assignments.reserve(world.clouds.size() + 1);

  if (const auto edge_assignment = chooseEdgeAssignment(world, batch_policy)) {
    assignments.push_back(*edge_assignment);
  }

  for (std::size_t i = 0; i < world.clouds.size(); ++i) {
    if (const auto cloud_assignment = chooseCloudAssignment(world, static_cast<int>(i), num_layers, batch_policy)) {
      assignments.push_back(*cloud_assignment);
    }
  }

  return assignments;
}

std::vector<int> buildBatchChoices(const TimingCurve& curve, double assignment_cost, int max_batch_size) {
  assert(assignment_cost >= 0.0);
  assert(max_batch_size >= 1);

  std::vector<int> choices(static_cast<std::size_t>(max_batch_size + 1));
  int best_batch_size = 1;
  double best_cost = assignment_cost + interpolate(curve, 1);
  choices[1] = 1;

  for (int batch_size = 2; batch_size <= max_batch_size; ++batch_size) {
    const double cost = (assignment_cost + interpolate(curve, batch_size)) / batch_size;
    if (cost < best_cost) {
      best_batch_size = batch_size;
      best_cost = cost;
    }
    choices[static_cast<std::size_t>(batch_size)] = best_batch_size;
  }

  return choices;
}

} // namespace

DecodeBatchPolicy buildDecodeBatchPolicy(const TimingCurves& curves, double assignment_cost, int max_batch_size) {
  return {
    .pre_by_ready_count = buildBatchChoices(curves.decode_pre, assignment_cost, max_batch_size),
    .proc_by_ready_count = buildBatchChoices(curves.decode_proc, assignment_cost, max_batch_size),
    .post_by_ready_count = buildBatchChoices(curves.decode_post, assignment_cost, max_batch_size),
  };
}

std::vector<Assignment> chooseSingletonAssignments(const WorldState& world, int num_layers) {
  return chooseAssignments(world, num_layers, nullptr);
}

std::vector<Assignment> chooseBatchedAssignments(
  const WorldState& world, int num_layers, const DecodeBatchPolicy& batch_policy) {
  return chooseAssignments(world, num_layers, &batch_policy);
}
