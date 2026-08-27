#include "WorldState.hpp"

#include <cassert>
#include <variant>

namespace {

template <typename... Visitors>
struct Overloaded : Visitors... {
  using Visitors::operator()...;
};

Request& getRequest(WorldState& world, int rid) {
  assert(rid >= 0);
  const auto index = static_cast<std::size_t>(rid);
  assert(index < world.requests.size());
  return world.requests.at(index);
}

ServerState& getServer(WorldState& world, const ServerId& server) {
  if (server.type == ServerType::Edge) {
    return world.edge;
  }

  assert(server.cloud_index >= 0);
  const auto index = static_cast<std::size_t>(server.cloud_index);
  assert(index < world.clouds.size());
  return world.clouds.at(index);
}

void assertEdgeServer(const ServerId& server) {
  assert(server.type == ServerType::Edge);
}

void assertCloudServer(const ServerId& server, int remote) {
  assert(server.type == ServerType::Cloud);
  assert(server.cloud_index == remote);
}

void assertRequestRemote(const Request& request, int remote) {
  assert(request.remote.has_value());
  assert(*request.remote == remote);
}

void applyArrival(WorldState& world, const ArrivalEvent& arrival) {
  assert(arrival.rid >= 0);
  assert(arrival.input_length > 0);

  const auto rid = static_cast<std::size_t>(arrival.rid);
  assert(rid == world.requests.size());

  world.requests.push_back({
    .input_length = arrival.input_length,
    .arrival_time = world.current_time,
    .remote = std::nullopt,
    .next_prefill_layer = 0,
    .tokens_produced = 0,
    .state = RequestState::ReadyPrefillPre,
  });
}

void applyTaskDone(WorldState& world, const TaskDoneEvent& event, int num_layers) {
  assert(num_layers > 0);

  ServerState& server = getServer(world, event.server);
  assert(server.busy);

  std::visit(Overloaded{
    [&](const PrefillPreTask& task) {
      assertEdgeServer(event.server);

      Request& request = getRequest(world, task.rid);
      assert(request.state == RequestState::WaitingPrefillPreDone);
      assertRequestRemote(request, task.remote);
      request.state = RequestState::WaitingPrefillUpload;
    },
    [&](const PrefillProcTask& task) {
      assertCloudServer(event.server, task.remote);
      assert(task.layer_begin >= 0);
      assert(task.layer_begin < task.layer_end);
      assert(task.layer_end <= num_layers);

      Request& request = getRequest(world, task.rid);
      assert(request.state == RequestState::WaitingPrefillProcDone);
      assertRequestRemote(request, task.remote);
      assert(request.next_prefill_layer == task.layer_begin);

      request.next_prefill_layer = task.layer_end;
      request.state = task.layer_end == num_layers
        ? RequestState::WaitingPrefillDownload
        : RequestState::ReadyPrefillProc;
    },
    [&](const PrefillPostTask& task) {
      assertEdgeServer(event.server);

      Request& request = getRequest(world, task.rid);
      assert(request.state == RequestState::WaitingPrefillPostDone);
      assertRequestRemote(request, task.remote);
      request.state = RequestState::ReadyDecodePre;
    },
    [&](const DecodePreTask& task) {
      assertEdgeServer(event.server);
      assert(!task.rids.empty());

      for (const int rid : task.rids) {
        Request& request = getRequest(world, rid);
        assert(request.state == RequestState::WaitingDecodePreDone);
        assert(request.remote.has_value());
        request.state = RequestState::WaitingDecodeUpload;
      }
    },
    [&](const DecodeProcTask& task) {
      assertCloudServer(event.server, task.remote);
      assert(!task.rids.empty());

      for (const int rid : task.rids) {
        Request& request = getRequest(world, rid);
        assert(request.state == RequestState::WaitingDecodeProcDone);
        assertRequestRemote(request, task.remote);
        request.state = RequestState::WaitingDecodeDownload;
      }
    },
    [&](const DecodePostTask& task) {
      assertEdgeServer(event.server);
      assert(!task.rids.empty());

      for (const int rid : task.rids) {
        Request& request = getRequest(world, rid);
        assert(request.state == RequestState::WaitingDecodePostDone);
        ++request.tokens_produced;
        request.state = RequestState::ReadyDecodePre;
      }
    },
  }, event.task);

  server.busy = false;
}

void applyTransferDone(WorldState& world, const TransferDoneEvent& event) {
  assert(event.remote >= 0);
  assert(static_cast<std::size_t>(event.remote) < world.clouds.size());
  assert(!event.rids.empty());

  if (event.stage == TransferStage::Prefill) {
    assert(event.rids.size() == 1);

    Request& request = getRequest(world, event.rids.front());
    assertRequestRemote(request, event.remote);

    if (event.direction == TransferDirection::Up) {
      assert(request.state == RequestState::WaitingPrefillUpload);
      request.state = RequestState::ReadyPrefillProc;
    } else {
      assert(request.state == RequestState::WaitingPrefillDownload);
      request.state = RequestState::ReadyPrefillPost;
    }

    return;
  }

  for (const int rid : event.rids) {
    Request& request = getRequest(world, rid);
    assertRequestRemote(request, event.remote);

    if (event.direction == TransferDirection::Up) {
      assert(request.state == RequestState::WaitingDecodeUpload);
      request.state = RequestState::ReadyDecodeProc;
    } else {
      assert(request.state == RequestState::WaitingDecodeDownload);
      request.state = RequestState::ReadyDecodePost;
    }
  }
}

void applyFinish(WorldState& world, const FinishEvent& finish) {
  Request& request = getRequest(world, finish.rid);
  assert(request.state == RequestState::ReadyDecodePre);
  assert(request.tokens_produced > 0);
  request.state = RequestState::Finished;
}

} // namespace

void applyFrame(WorldState& world, const Frame& frame, int num_layers) {
  assert(frame.timestamp >= world.current_time);
  world.current_time = frame.timestamp;

  // FIN is deliberately deferred. The statement guarantees that it appears
  // in the same frame as the final D POST TDN, and frame line order carries no
  // priority. Applying all other events first ensures Finished always wins.
  for (const Event& event : frame.events) {
    if (const ArrivalEvent* arrival = std::get_if<ArrivalEvent>(&event)) {
      applyArrival(world, *arrival);
    } else if (const TaskDoneEvent* task_done = std::get_if<TaskDoneEvent>(&event)) {
      applyTaskDone(world, *task_done, num_layers);
    } else if (const TransferDoneEvent* transfer_done = std::get_if<TransferDoneEvent>(&event)) {
      applyTransferDone(world, *transfer_done);
    }
  }

  for (const Event& event : frame.events) {
    if (const FinishEvent* finish = std::get_if<FinishEvent>(&event)) {
      applyFinish(world, *finish);
    }
  }
}
