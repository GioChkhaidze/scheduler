#include "Protocol.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {

constexpr double epsilon = 1e-9;

bool isEqual(double actual, double expected) {
  return std::abs(actual - expected) < epsilon;
}

template <typename EventType>
const EventType& getEvent(const Frame& frame, std::size_t index) {
  assert(index < frame.events.size());
  assert(std::holds_alternative<EventType>(frame.events[index]));
  return std::get<EventType>(frame.events[index]);
}

template <typename TaskType>
const TaskType& getTask(const TaskDoneEvent& event) {
  assert(std::holds_alternative<TaskType>(event.task));
  return std::get<TaskType>(event.task);
}

void assertRequestIds(
  const std::vector<int>& actual,
  std::initializer_list<int> expected
) {
  assert(actual == std::vector<int>(expected));
}

void assertText(const std::string& actual, std::initializer_list<char> expected) {
  assert(actual == std::string(expected.begin(), expected.end()));
}

void assertServer(
  const ServerId& server,
  ServerType expected_type,
  int expected_cloud_index
) {
  assert(server.type == expected_type);
  assert(server.cloud_index == expected_cloud_index);
}

void testReadsArrivalsFinishAndTimestamp() {
  std::istringstream input{
    "0.125000000\n"
    "3\n"
    "ARR 0 4096\n"
    "ARR 1 1\n"
    "FIN 7\n"
  };

  const std::optional<Frame> result = readFrame(input);
  assert(result.has_value());

  const Frame& frame = *result;
  assert(isEqual(frame.timestamp, 0.125));
  assert(frame.events.size() == 3);

  const ArrivalEvent& first = getEvent<ArrivalEvent>(frame, 0);
  assert(first.rid == 0);
  assert(first.input_length == 4096);

  const ArrivalEvent& second = getEvent<ArrivalEvent>(frame, 1);
  assert(second.rid == 1);
  assert(second.input_length == 1);

  const FinishEvent& finished = getEvent<FinishEvent>(frame, 2);
  assert(finished.rid == 7);
}

void testReadsEveryTaskSpecification() {
  std::istringstream input{
    "12.500000000\n"
    "6\n"
    "TDN E P PRE 2 10 3.250000000\n"
    "TDN C2 P PROC 0 64 2 10 7.500000000\n"
    "TDN E P POST 2 10 2.000000000\n"
    "TDN E D PRE -1 3 10 11 12 1.500000000\n"
    "TDN C3 D PROC 3 2 11 12 4.250000000\n"
    "TDN E D POST -1 2 10 12 0.750000000\n"
  };

  const std::optional<Frame> result = readFrame(input);
  assert(result.has_value());
  const Frame& frame = *result;
  assert(frame.events.size() == 6);

  const TaskDoneEvent& prefill_pre_event =
    getEvent<TaskDoneEvent>(frame, 0);
  assertServer(prefill_pre_event.server, ServerType::Edge, -1);
  assert(isEqual(prefill_pre_event.duration, 3.25));
  const PrefillPreTask& prefill_pre =
    getTask<PrefillPreTask>(prefill_pre_event);
  assert(prefill_pre.remote == 2);
  assert(prefill_pre.rid == 10);

  const TaskDoneEvent& prefill_proc_event =
    getEvent<TaskDoneEvent>(frame, 1);
  assertServer(prefill_proc_event.server, ServerType::Cloud, 2);
  assert(isEqual(prefill_proc_event.duration, 7.5));
  const PrefillProcTask& prefill_proc =
    getTask<PrefillProcTask>(prefill_proc_event);
  assert(prefill_proc.layer_begin == 0);
  assert(prefill_proc.layer_end == 64);
  assert(prefill_proc.remote == 2);
  assert(prefill_proc.rid == 10);

  const TaskDoneEvent& prefill_post_event =
    getEvent<TaskDoneEvent>(frame, 2);
  assertServer(prefill_post_event.server, ServerType::Edge, -1);
  assert(isEqual(prefill_post_event.duration, 2.0));
  const PrefillPostTask& prefill_post =
    getTask<PrefillPostTask>(prefill_post_event);
  assert(prefill_post.remote == 2);
  assert(prefill_post.rid == 10);

  const TaskDoneEvent& decode_pre_event =
    getEvent<TaskDoneEvent>(frame, 3);
  assertServer(decode_pre_event.server, ServerType::Edge, -1);
  assert(isEqual(decode_pre_event.duration, 1.5));
  const DecodePreTask& decode_pre =
    getTask<DecodePreTask>(decode_pre_event);
  assertRequestIds(decode_pre.rids, {10, 11, 12});

  const TaskDoneEvent& decode_proc_event =
    getEvent<TaskDoneEvent>(frame, 4);
  assertServer(decode_proc_event.server, ServerType::Cloud, 3);
  assert(isEqual(decode_proc_event.duration, 4.25));
  const DecodeProcTask& decode_proc =
    getTask<DecodeProcTask>(decode_proc_event);
  assert(decode_proc.remote == 3);
  assertRequestIds(decode_proc.rids, {11, 12});

  const TaskDoneEvent& decode_post_event =
    getEvent<TaskDoneEvent>(frame, 5);
  assertServer(decode_post_event.server, ServerType::Edge, -1);
  assert(isEqual(decode_post_event.duration, 0.75));
  const DecodePostTask& decode_post =
    getTask<DecodePostTask>(decode_post_event);
  assertRequestIds(decode_post.rids, {10, 12});
}

void testReadsEveryTransferCombinationAndPreservesRequestOrder() {
  std::istringstream input{
    "20.000000000\n"
    "4\n"
    "XDN UP 2 4096000000 PRE 1 10\n"
    "XDN DOWN 2 4096000000 PRE 1 10\n"
    "XDN UP 3 3000000000 DEC 3 9 5 7\n"
    "XDN DOWN 3 3000000000 DEC 3 7 5 9\n"
  };

  const std::optional<Frame> result = readFrame(input);
  assert(result.has_value());
  const Frame& frame = *result;
  assert(frame.events.size() == 4);

  const TransferDoneEvent& prefill_up =
    getEvent<TransferDoneEvent>(frame, 0);
  assert(prefill_up.direction == TransferDirection::Up);
  assert(prefill_up.remote == 2);
  assert(prefill_up.size_bytes == std::int64_t{4'096'000'000});
  assert(prefill_up.stage == TransferStage::Prefill);
  assertRequestIds(prefill_up.rids, {10});

  const TransferDoneEvent& prefill_down =
    getEvent<TransferDoneEvent>(frame, 1);
  assert(prefill_down.direction == TransferDirection::Down);
  assert(prefill_down.remote == 2);
  assert(prefill_down.size_bytes == std::int64_t{4'096'000'000});
  assert(prefill_down.stage == TransferStage::Prefill);
  assertRequestIds(prefill_down.rids, {10});

  const TransferDoneEvent& decode_up =
    getEvent<TransferDoneEvent>(frame, 2);
  assert(decode_up.direction == TransferDirection::Up);
  assert(decode_up.remote == 3);
  assert(decode_up.size_bytes == std::int64_t{3'000'000'000});
  assert(decode_up.stage == TransferStage::Decode);
  assertRequestIds(decode_up.rids, {9, 5, 7});

  const TransferDoneEvent& decode_down =
    getEvent<TransferDoneEvent>(frame, 3);
  assert(decode_down.direction == TransferDirection::Down);
  assert(decode_down.remote == 3);
  assert(decode_down.size_bytes == std::int64_t{3'000'000'000});
  assert(decode_down.stage == TransferStage::Decode);
  assertRequestIds(decode_down.rids, {7, 5, 9});
}

void testReadsConsecutiveFramesThenEnd() {
  std::istringstream input{
    "1.000000000\n"
    "1\n"
    "ARR 3 128\n"
    "2.500000000\n"
    "1\n"
    "FIN 3\n"
    "END\n"
  };

  const std::optional<Frame> first = readFrame(input);
  const std::optional<Frame> second = readFrame(input);
  const std::optional<Frame> end = readFrame(input);

  assert(first.has_value());
  assert(second.has_value());
  assert(!end.has_value());
  assert(isEqual(first->timestamp, 1.0));
  assert(isEqual(second->timestamp, 2.5));
  assert(getEvent<ArrivalEvent>(*first, 0).rid == 3);
  assert(getEvent<FinishEvent>(*second, 0).rid == 3);
}

void testEndAndEofProduceNoFrame() {
  std::istringstream end_input{"END\n"};
  std::istringstream empty_input;

  assert(!readFrame(end_input).has_value());
  assert(!readFrame(empty_input).has_value());
}

void assertReadFrameThrows(const std::string& input_text) {
  std::istringstream input{input_text};
  bool threw = false;

  try {
    (void)readFrame(input);
  } catch (const std::runtime_error&) {
    threw = true;
  }

  assert(threw);
}

void testRejectsUnknownProtocolTokens() {
  assertReadFrameThrows(
    "0\n1\nUNKNOWN\n"
  );
  assertReadFrameThrows(
    "0\n1\nTDN GPU P PRE 0 1 1.0\n"
  );
  assertReadFrameThrows(
    "0\n1\nTDN E P UNKNOWN 1.0\n"
  );
  assertReadFrameThrows(
    "0\n1\nXDN SIDEWAYS 0 100 PRE 1 0\n"
  );
  assertReadFrameThrows(
    "0\n1\nXDN UP 0 100 UNKNOWN 1 0\n"
  );
}

void testWritesEmptyAssignmentResponse() {
  std::ostringstream output;
  writeAssignments(output, {});

  assertText(output.str(), {'0', '\n'});
}

void testWritesEveryAssignmentShapeExactly() {
  const std::vector<Assignment> assignments{
    {
      ServerId{ServerType::Edge, -1},
      PrefillPreTask{2, 0},
    },
    {
      ServerId{ServerType::Cloud, 2},
      PrefillProcTask{0, 4, 2, 0},
    },
    {
      ServerId{ServerType::Edge, -1},
      PrefillPostTask{2, 0},
    },
    {
      ServerId{ServerType::Edge, -1},
      DecodePreTask{{0, 1}},
    },
    {
      ServerId{ServerType::Cloud, 2},
      DecodeProcTask{2, {0, 1}},
    },
    {
      ServerId{ServerType::Edge, -1},
      DecodePostTask{{0, 1}},
    },
  };

  std::ostringstream output;
  writeAssignments(output, assignments);

  assertText(output.str(), {
    '6', '\n',
    'E', ' ', 'P', ' ', 'P', 'R', 'E', ' ', '2', ' ', '0', '\n',
    'C', '2', ' ', 'P', ' ', 'P', 'R', 'O', 'C', ' ',
      '0', ' ', '4', ' ', '2', ' ', '0', '\n',
    'E', ' ', 'P', ' ', 'P', 'O', 'S', 'T', ' ', '2', ' ', '0', '\n',
    'E', ' ', 'D', ' ', 'P', 'R', 'E', ' ', '-', '1', ' ',
      '2', ' ', '0', ' ', '1', '\n',
    'C', '2', ' ', 'D', ' ', 'P', 'R', 'O', 'C', ' ', '2', ' ',
      '2', ' ', '0', ' ', '1', '\n',
    'E', ' ', 'D', ' ', 'P', 'O', 'S', 'T', ' ', '-', '1', ' ',
      '2', ' ', '0', ' ', '1', '\n',
  });
}

} // namespace

int main() {
  testReadsArrivalsFinishAndTimestamp();
  testReadsEveryTaskSpecification();
  testReadsEveryTransferCombinationAndPreservesRequestOrder();
  testReadsConsecutiveFramesThenEnd();
  testEndAndEofProduceNoFrame();
  testRejectsUnknownProtocolTokens();
  testWritesEmptyAssignmentResponse();
  testWritesEveryAssignmentShapeExactly();
  return 0;
}
