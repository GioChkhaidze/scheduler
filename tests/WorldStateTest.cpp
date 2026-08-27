#include <WorldState.hpp>

#include <cassert>
#include <optional>
#include <utility>
#include <vector>

namespace {

Request makeRequest(
  int remote,
  RequestState state,
  int next_prefill_layer = 0,
  int tokens_produced = 0
) {
  return {
    .input_length = 128,
    .arrival_time = 1.0,
    .remote = std::optional<int>{remote},
    .next_prefill_layer = next_prefill_layer,
    .tokens_produced = tokens_produced,
    .state = state,
  };
}

void applyEvents(
  WorldState& world,
  double timestamp,
  std::vector<Event> events,
  int num_layers = 4
) {
  applyFrame(world, Frame{timestamp, std::move(events)}, num_layers);
}

void testArrivalCreatesPersistentRequestState() {
  WorldState world{2};

  applyEvents(world, 3.5, {
    ArrivalEvent{0, 64},
    ArrivalEvent{1, 4096},
  });

  assert(world.current_time == 3.5);
  assert(world.requests.size() == 2);

  const Request& first = world.requests.at(0);
  assert(first.input_length == 64);
  assert(first.arrival_time == 3.5);
  assert(!first.remote.has_value());
  assert(first.next_prefill_layer == 0);
  assert(first.tokens_produced == 0);
  assert(first.state == RequestState::ReadyPrefillPre);

  const Request& second = world.requests.at(1);
  assert(second.input_length == 4096);
  assert(second.arrival_time == 3.5);
  assert(second.state == RequestState::ReadyPrefillPre);
}

void testCompletePrefillTransitionChainIncludingChunking() {
  WorldState world{1};
  world.requests.push_back(makeRequest(
    0,
    RequestState::WaitingPrefillPreDone
  ));

  world.edge.busy = true;
  applyEvents(world, 2.0, {
    TaskDoneEvent{
      ServerId{ServerType::Edge, -1},
      PrefillPreTask{0, 0},
      3.0,
    },
  });
  assert(!world.edge.busy);
  assert(world.requests.at(0).state == RequestState::WaitingPrefillUpload);

  applyEvents(world, 3.0, {
    TransferDoneEvent{
      TransferDirection::Up,
      0,
      1024,
      TransferStage::Prefill,
      {0},
    },
  });
  assert(world.requests.at(0).state == RequestState::ReadyPrefillProc);

  world.requests.at(0).state = RequestState::WaitingPrefillProcDone;
  world.clouds.at(0).busy = true;
  applyEvents(world, 4.0, {
    TaskDoneEvent{
      ServerId{ServerType::Cloud, 0},
      PrefillProcTask{0, 2, 0, 0},
      5.0,
    },
  });
  assert(!world.clouds.at(0).busy);
  assert(world.requests.at(0).next_prefill_layer == 2);
  assert(world.requests.at(0).state == RequestState::ReadyPrefillProc);

  world.requests.at(0).state = RequestState::WaitingPrefillProcDone;
  world.clouds.at(0).busy = true;
  applyEvents(world, 5.0, {
    TaskDoneEvent{
      ServerId{ServerType::Cloud, 0},
      PrefillProcTask{2, 4, 0, 0},
      5.0,
    },
  });
  assert(!world.clouds.at(0).busy);
  assert(world.requests.at(0).next_prefill_layer == 4);
  assert(world.requests.at(0).state == RequestState::WaitingPrefillDownload);

  applyEvents(world, 6.0, {
    TransferDoneEvent{
      TransferDirection::Down,
      0,
      1024,
      TransferStage::Prefill,
      {0},
    },
  });
  assert(world.requests.at(0).state == RequestState::ReadyPrefillPost);

  world.requests.at(0).state = RequestState::WaitingPrefillPostDone;
  world.edge.busy = true;
  applyEvents(world, 7.0, {
    TaskDoneEvent{
      ServerId{ServerType::Edge, -1},
      PrefillPostTask{0, 0},
      2.0,
    },
  });
  assert(!world.edge.busy);
  assert(world.requests.at(0).state == RequestState::ReadyDecodePre);
}

void testDecodePreAndUploadsAcrossTwoClouds() {
  WorldState world{2};
  world.requests.push_back(makeRequest(
    0,
    RequestState::WaitingDecodePreDone
  ));
  world.requests.push_back(makeRequest(
    1,
    RequestState::WaitingDecodePreDone,
    0,
    3
  ));

  world.edge.busy = true;
  applyEvents(world, 2.0, {
    TaskDoneEvent{
      ServerId{ServerType::Edge, -1},
      DecodePreTask{{0, 1}},
      1.5,
    },
  });
  assert(!world.edge.busy);
  assert(world.requests.at(0).state == RequestState::WaitingDecodeUpload);
  assert(world.requests.at(1).state == RequestState::WaitingDecodeUpload);

  applyEvents(world, 3.0, {
    TransferDoneEvent{
      TransferDirection::Up,
      0,
      128,
      TransferStage::Decode,
      {0},
    },
  });
  assert(world.requests.at(0).state == RequestState::ReadyDecodeProc);
  assert(world.requests.at(1).state == RequestState::WaitingDecodeUpload);

  applyEvents(world, 4.0, {
    TransferDoneEvent{
      TransferDirection::Up,
      1,
      128,
      TransferStage::Decode,
      {1},
    },
  });
  assert(world.requests.at(0).state == RequestState::ReadyDecodeProc);
  assert(world.requests.at(1).state == RequestState::ReadyDecodeProc);
}

void testDecodeTransferUpdatesEveryCarriedRequestOnly() {
  WorldState world{2};
  world.requests.push_back(makeRequest(
    0,
    RequestState::WaitingDecodeUpload
  ));
  world.requests.push_back(makeRequest(
    1,
    RequestState::WaitingDecodeUpload
  ));
  world.requests.push_back(makeRequest(
    0,
    RequestState::WaitingDecodeUpload
  ));

  applyEvents(world, 2.0, {
    TransferDoneEvent{
      TransferDirection::Up,
      0,
      256,
      TransferStage::Decode,
      {2, 0},
    },
  });
  assert(world.requests.at(0).state == RequestState::ReadyDecodeProc);
  assert(world.requests.at(1).state == RequestState::WaitingDecodeUpload);
  assert(world.requests.at(2).state == RequestState::ReadyDecodeProc);

  world.requests.at(0).state = RequestState::WaitingDecodeDownload;
  world.requests.at(2).state = RequestState::WaitingDecodeDownload;
  applyEvents(world, 3.0, {
    TransferDoneEvent{
      TransferDirection::Down,
      0,
      256,
      TransferStage::Decode,
      {0, 2},
    },
  });
  assert(world.requests.at(0).state == RequestState::ReadyDecodePost);
  assert(world.requests.at(1).state == RequestState::WaitingDecodeUpload);
  assert(world.requests.at(2).state == RequestState::ReadyDecodePost);
}

void testDecodeProcDownloadsAndGroupedPost() {
  WorldState world{2};
  world.requests.push_back(makeRequest(
    0,
    RequestState::WaitingDecodeProcDone
  ));
  world.requests.push_back(makeRequest(
    1,
    RequestState::WaitingDecodeProcDone,
    0,
    3
  ));
  world.clouds.at(0).busy = true;
  world.clouds.at(1).busy = true;

  applyEvents(world, 5.0, {
    TaskDoneEvent{
      ServerId{ServerType::Cloud, 1},
      DecodeProcTask{1, {1}},
      4.0,
    },
    TaskDoneEvent{
      ServerId{ServerType::Cloud, 0},
      DecodeProcTask{0, {0}},
      4.0,
    },
  });
  assert(!world.clouds.at(0).busy);
  assert(!world.clouds.at(1).busy);
  assert(world.requests.at(0).state == RequestState::WaitingDecodeDownload);
  assert(world.requests.at(1).state == RequestState::WaitingDecodeDownload);

  applyEvents(world, 6.0, {
    TransferDoneEvent{
      TransferDirection::Down,
      1,
      128,
      TransferStage::Decode,
      {1},
    },
  });
  assert(world.requests.at(0).state == RequestState::WaitingDecodeDownload);
  assert(world.requests.at(1).state == RequestState::ReadyDecodePost);

  applyEvents(world, 7.0, {
    TransferDoneEvent{
      TransferDirection::Down,
      0,
      128,
      TransferStage::Decode,
      {0},
    },
  });
  assert(world.requests.at(0).state == RequestState::ReadyDecodePost);
  assert(world.requests.at(1).state == RequestState::ReadyDecodePost);

  world.requests.at(0).state = RequestState::WaitingDecodePostDone;
  world.requests.at(1).state = RequestState::WaitingDecodePostDone;
  world.edge.busy = true;
  applyEvents(world, 8.0, {
    TaskDoneEvent{
      ServerId{ServerType::Edge, -1},
      DecodePostTask{{0, 1}},
      1.0,
    },
  });
  assert(!world.edge.busy);
  assert(world.requests.at(0).state == RequestState::ReadyDecodePre);
  assert(world.requests.at(1).state == RequestState::ReadyDecodePre);
  assert(world.requests.at(0).tokens_produced == 1);
  assert(world.requests.at(1).tokens_produced == 4);
}

void testFinishWinsRegardlessOfFrameLineOrder() {
  WorldState world{1};
  world.requests.push_back(makeRequest(
    0,
    RequestState::WaitingDecodePostDone,
    0,
    2
  ));
  world.requests.push_back(makeRequest(
    0,
    RequestState::WaitingDecodePostDone,
    0,
    7
  ));
  world.edge.busy = true;

  applyEvents(world, 9.0, {
    FinishEvent{0},
    TaskDoneEvent{
      ServerId{ServerType::Edge, -1},
      DecodePostTask{{0, 1}},
      1.0,
    },
  });

  assert(!world.edge.busy);
  assert(world.requests.at(0).tokens_produced == 3);
  assert(world.requests.at(0).state == RequestState::Finished);
  assert(world.requests.at(1).tokens_produced == 8);
  assert(world.requests.at(1).state == RequestState::ReadyDecodePre);
}

} // namespace

int main() {
  testArrivalCreatesPersistentRequestState();
  testCompletePrefillTransitionChainIncludingChunking();
  testDecodePreAndUploadsAcrossTwoClouds();
  testDecodeTransferUpdatesEveryCarriedRequestOnly();
  testDecodeProcDownloadsAndGroupedPost();
  testFinishWinsRegardlessOfFrameLineOrder();
  return 0;
}
