#include <scheduler/model/Protocol.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <variant>
#include <cassert>
#include <stdexcept>

std::vector<int> readRequestIds(std::istream& input) {
  int count;
  input >> count;
  assert(count >= 1);

  std::vector<int> rids(static_cast<std::size_t>(count));
  for (int& rid : rids) {
    input >> rid;
  }

  return rids;
}

ServerId readServer(std::istream& input) {
  std::string server_token;
  input >> server_token;
  if (server_token == "E") {
    return {ServerType::Edge, -1};
  }

  if (server_token.size() >= 2 && server_token.front() == 'C') {
    const int cloud_index = std::stoi(server_token.substr(1));
    return {ServerType::Cloud, cloud_index};
  }

  throw std::runtime_error("Unknown server");
}

TaskSpec readTaskSpec(std::istream& input) {
  int marker;
  std::string family, stage;
  input >> family >> stage;

  if (family == "P") {
    if (stage == "PRE") {
      PrefillPreTask task{};
      input >> task.remote >> task.rid;
      return task;
    }

    if (stage == "PROC") {
      PrefillProcTask task{};
      input 
        >> task.layer_begin
        >> task.layer_end
        >> task.remote
        >> task.rid;
      return task;
    }

    if (stage == "POST") {
      PrefillPostTask task{};
      input >> task.remote >> task.rid;
      return task;
    }
  }

  if (family == "D") {
    if (stage == "PRE") {
      DecodePreTask task{};
      input >> marker;
      assert(marker == -1);
      task.rids = readRequestIds(input);
      return task;
    }

    if (stage == "PROC") {
      DecodeProcTask task{};
      input >> task.remote;
      task.rids = readRequestIds(input);
      return task;
    }

    if (stage == "POST") {
      DecodePostTask task{};
      input >> marker;
      assert(marker == -1);
      task.rids = readRequestIds(input);
      return task;
    }
  }

  throw std::runtime_error("Unknown task spec");
}

TransferDirection parseTransferDirection(const std::string& token) {
  if (token == "UP") return TransferDirection::Up;
  if (token == "DOWN") return TransferDirection::Down;
  throw std::runtime_error("Unknown transfer direction");
}

TransferStage parseTransferStage(const std::string& token) {
  if (token == "PRE") return TransferStage::Prefill;
  if (token == "DEC") return TransferStage::Decode;
  throw std::runtime_error("Unknown transfer stage");
}

Event readEvent(std::istream& input) {
  std::string type;
  input >> type;

  if (type == "ARR") {
    ArrivalEvent event{};
    input >> event.rid >> event.input_length;
    return event;
  } 
  
  if (type == "TDN") {
    TaskDoneEvent event{};
    event.server = readServer(input); 
    event.task = readTaskSpec(input);
    input >> event.duration;
    return event;
  } 
  
  if (type == "XDN") {
    TransferDoneEvent event{};
    std::string direction_token;
    std::string stage_token;
    
    input 
      >> direction_token
      >> event.remote
      >> event.size_bytes
      >> stage_token;

    event.direction = parseTransferDirection(direction_token);
    event.stage = parseTransferStage(stage_token);
    event.rids = readRequestIds(input);

    return event;
  }
  
  if (type == "FIN") {
    FinishEvent event{};
    input >> event.rid;
    return event;
  } 

  throw std::runtime_error("Unknown event type");
}

std::optional<Frame> readFrame(std::istream& input) {
  std::string timestamp_token;

  if (!(input >> timestamp_token) || timestamp_token == "END") {
    return std::nullopt;
  }

  Frame frame{};
  frame.timestamp = std::stod(timestamp_token);

  int event_count;
  input >> event_count;
  assert(event_count >= 0);

  frame.events.reserve(static_cast<std::size_t>(event_count));

  for (int i = 0; i < event_count; ++i) {
    frame.events.push_back(readEvent(input));
  }

  return frame;
}

namespace {

template <typename... Visitors>
struct OutputVisitor : Visitors... {
  using Visitors::operator()...;
};

template <typename... Visitors>
OutputVisitor(Visitors...) -> OutputVisitor<Visitors...>;

void writeRequestIds(std::ostream& output, const std::vector<int>& rids) {
  output << rids.size();
  for (const int rid : rids) {
    output << ' ' << rid;
  }
}

void writeServer(std::ostream& output, const ServerId& server) {
  if (server.type == ServerType::Edge) {
    output << "E";
  } else {
    output << "C" << server.cloud_index;
  }
}

void writeTaskSpec(std::ostream& output, const TaskSpec& task) {
  std::visit(OutputVisitor{
    [&](const PrefillPreTask& value) {
      output << "P PRE " << value.remote << ' ' << value.rid;
    },
    [&](const PrefillProcTask& value) {
      output << "P PROC " << value.layer_begin << ' ' << value.layer_end << ' ' << value.remote << ' ' << value.rid;
    },
    [&](const PrefillPostTask& value) {
      output << "P POST " << value.remote << ' ' << value.rid;
    },
    [&](const DecodePreTask& value) {
      output << "D PRE -1 ";
      writeRequestIds(output, value.rids);
    },
    [&](const DecodeProcTask& value) {
      output << "D PROC " << value.remote << ' ';
      writeRequestIds(output, value.rids);
    },
    [&](const DecodePostTask& value) {
      output << "D POST -1 ";
      writeRequestIds(output, value.rids);
    },
  }, task);
}

} // namespace

void writeAssignments(std::ostream& output, const std::vector<Assignment>& assignments) {
  output << assignments.size() << '\n';

  for (const Assignment& assignment : assignments) {
    writeServer(output, assignment.server);
    output << ' ';
    writeTaskSpec(output, assignment.task);
    output << '\n';
  }
}
