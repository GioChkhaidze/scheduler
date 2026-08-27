#include <Scheduler.hpp>

#include <cassert>
#include <cstddef>
#include <optional>

namespace {

std::optional<int> findFirstRequest(const WorldState& world, RequestState state, std::optional<int> remote = std::nullopt) {
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

std::optional<Assignment> chooseEdgeAssignment(const WorldState& world) {
  if (world.edge.busy) {
    return std::nullopt;
  }

  if (const auto rid = findFirstRequest(world, RequestState::ReadyDecodePost)) {
    return Assignment{ServerId{ServerType::Edge, -1}, DecodePostTask{{*rid}}};
  }

  if (const auto rid = findFirstRequest(world, RequestState::ReadyPrefillPost)) {
    const Request& request = world.requests.at(static_cast<std::size_t>(*rid));
    assert(request.remote.has_value());
    return Assignment{ServerId{ServerType::Edge, -1}, PrefillPostTask{*request.remote, *rid}};
  }

  if (const auto rid = findFirstRequest(world, RequestState::ReadyDecodePre)) {
    return Assignment{ServerId{ServerType::Edge, -1}, DecodePreTask{{*rid}}};
  }

  if (const auto rid = findFirstRequest(world, RequestState::ReadyPrefillPre)) {
    assert(!world.clouds.empty());
    const int remote = *rid % static_cast<int>(world.clouds.size());
    return Assignment{ServerId{ServerType::Edge, -1}, PrefillPreTask{remote, *rid}};
  }

  return std::nullopt;
}

std::optional<Assignment> chooseCloudAssignment(const WorldState& world, int remote, int num_layers) {
  const ServerState& cloud = world.clouds.at(static_cast<std::size_t>(remote));
  if (cloud.busy) {
    return std::nullopt;
  }

  if (const auto rid = findFirstRequest(world, RequestState::ReadyDecodeProc, remote)) {
    return Assignment{ServerId{ServerType::Cloud, remote}, DecodeProcTask{remote, {*rid}}};
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

} // namespace

std::vector<Assignment> chooseSingletonAssignments(const WorldState& world, int num_layers) {
  assert(num_layers > 0);

  std::vector<Assignment> assignments;
  assignments.reserve(world.clouds.size() + 1);

  if (const auto edge_assignment = chooseEdgeAssignment(world)) {
    assignments.push_back(*edge_assignment);
  }

  for (std::size_t i = 0; i < world.clouds.size(); ++i) {
    if (const auto cloud_assignment = chooseCloudAssignment(world, static_cast<int>(i), num_layers)) {
      assignments.push_back(*cloud_assignment);
    }
  }

  return assignments;
}
