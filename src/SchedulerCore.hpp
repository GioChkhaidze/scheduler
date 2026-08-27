#pragma once

#include <WorldState.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace scheduler_detail {

inline std::optional<int> findFirstRequest(
  const WorldState& world, RequestState state, std::optional<int> remote = std::nullopt) {
  for (std::size_t i = 0; i < world.requests.size(); ++i) {
    const Request& request = world.requests[i];
    if (request.state == state && (!remote.has_value() || request.remote == remote)) {
      return static_cast<int>(i);
    }
  }
  return std::nullopt;
}

inline std::vector<int> findRequests(
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

template <typename DecodeSelector>
std::vector<int> chooseDecodeRids(
  const WorldState& world,
  RequestState state,
  std::optional<int> remote,
  const DecodeSelector& selector) {
  return selector.select(world, state, remote);
}

inline std::optional<Assignment> choosePrefillPostAssignment(const WorldState& world) {
  const auto rid = findFirstRequest(world, RequestState::ReadyPrefillPost);
  if (!rid.has_value()) {
    return std::nullopt;
  }

  const Request& request = world.requests.at(static_cast<std::size_t>(*rid));
  assert(request.remote.has_value());
  return Assignment{ServerId{ServerType::Edge, -1}, PrefillPostTask{*request.remote, *rid}};
}

template <typename RemoteSelector>
std::optional<Assignment> choosePrefillPreAssignment(
  const WorldState& world, const RemoteSelector& remote_selector) {
  const auto rid = findFirstRequest(world, RequestState::ReadyPrefillPre);
  if (!rid.has_value()) {
    return std::nullopt;
  }
  assert(!world.clouds.empty());
  const int remote = remote_selector(world, *rid);
  assert(remote >= 0);
  assert(static_cast<std::size_t>(remote) < world.clouds.size());
  return Assignment{ServerId{ServerType::Edge, -1}, PrefillPreTask{remote, *rid}};
}

template <typename DecodeSelector>
std::optional<Assignment> chooseDecodePreAssignment(
  const WorldState& world, const DecodeSelector& selector) {
  std::vector<int> rids = chooseDecodeRids(
    world, RequestState::ReadyDecodePre, std::nullopt, selector);
  if (rids.empty()) {
    return std::nullopt;
  }
  return Assignment{ServerId{ServerType::Edge, -1}, DecodePreTask{std::move(rids)}};
}

template <typename DecodeSelector, typename RemoteSelector>
std::optional<Assignment> chooseEdgeAssignment(
  const WorldState& world, const DecodeSelector& selector, const RemoteSelector& remote_selector) {
  if (world.edge.busy) {
    return std::nullopt;
  }

  if (std::vector<int> rids = chooseDecodeRids(
        world, RequestState::ReadyDecodePost, std::nullopt, selector); !rids.empty()) {
    return Assignment{ServerId{ServerType::Edge, -1}, DecodePostTask{std::move(rids)}};
  }

  if (selector.preferPrefillPostBeforeDecodePre(world)) {
    if (const auto assignment = choosePrefillPostAssignment(world)) {
      return assignment;
    }
    if (selector.preferPrefillPreBeforeDecodePre(world)) {
      if (const auto assignment = choosePrefillPreAssignment(world, remote_selector)) {
        return assignment;
      }
    }
    if (const auto assignment = chooseDecodePreAssignment(world, selector)) {
      return assignment;
    }
  } else {
    if (const auto assignment = chooseDecodePreAssignment(world, selector)) {
      return assignment;
    }
    if (const auto assignment = choosePrefillPostAssignment(world)) {
      return assignment;
    }
  }

  if (const auto assignment = choosePrefillPreAssignment(world, remote_selector)) {
    return assignment;
  }

  return std::nullopt;
}

template <typename DecodeSelector>
std::optional<Assignment> chooseCloudAssignment(
  const WorldState& world, int remote, int num_layers, const DecodeSelector& selector) {
  const ServerState& cloud = world.clouds.at(static_cast<std::size_t>(remote));
  if (cloud.busy) {
    return std::nullopt;
  }

  const auto choose_prefill = [&]() -> std::optional<Assignment> {
    const auto rid = findFirstRequest(world, RequestState::ReadyPrefillProc, remote);
    if (!rid.has_value()) {
      return std::nullopt;
    }
    const Request& request = world.requests.at(static_cast<std::size_t>(*rid));
    assert(request.next_prefill_layer < num_layers);
    return Assignment{
      ServerId{ServerType::Cloud, remote},
      PrefillProcTask{request.next_prefill_layer, num_layers, remote, *rid},
    };
  };
  if (selector.preferPrefillProcBeforeDecodeProc(world, remote)) {
    if (const auto assignment = choose_prefill()) {
      return assignment;
    }
  }

  if (std::vector<int> rids = chooseDecodeRids(
        world, RequestState::ReadyDecodeProc, remote, selector); !rids.empty()) {
    return Assignment{ServerId{ServerType::Cloud, remote}, DecodeProcTask{remote, std::move(rids)}};
  }

  if (const auto assignment = choose_prefill()) {
    return assignment;
  }

  return std::nullopt;
}

template <typename DecodeSelector, typename RemoteSelector>
std::vector<Assignment> chooseAssignments(
  const WorldState& world,
  int num_layers,
  const DecodeSelector& selector,
  const RemoteSelector& remote_selector) {
  assert(num_layers > 0);

  std::vector<Assignment> assignments;
  assignments.reserve(world.clouds.size() + 1);
  if (const auto edge_assignment = chooseEdgeAssignment(world, selector, remote_selector)) {
    assignments.push_back(*edge_assignment);
  }
  for (std::size_t remote = 0; remote < world.clouds.size(); ++remote) {
    if (const auto assignment = chooseCloudAssignment(world, static_cast<int>(remote), num_layers, selector)) {
      assignments.push_back(*assignment);
    }
  }
  return assignments;
}

struct RoundRobinRemoteSelector {
  int operator()(const WorldState& world, int rid) const {
    assert(!world.clouds.empty());
    return rid % static_cast<int>(world.clouds.size());
  }
};

template <typename DecodeSelector>
std::vector<Assignment> chooseAssignments(
  const WorldState& world, int num_layers, const DecodeSelector& selector) {
  return chooseAssignments(world, num_layers, selector, RoundRobinRemoteSelector{});
}

} // namespace scheduler_detail
