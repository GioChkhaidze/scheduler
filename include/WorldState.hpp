#pragma once
#include "Protocol.hpp"
#include <cstddef>
#include <optional>
#include <vector>

// RequestState: current position of that request
enum class RequestState {
  ReadyPrefillPre,
  WaitingPrefillPreDone,
  WaitingPrefillUpload,
  
  ReadyPrefillProc,
  WaitingPrefillProcDone,
  WaitingPrefillDownload,

  ReadyPrefillPost,
  WaitingPrefillPostDone,
  
  ReadyDecodePre,
  WaitingDecodePreDone,
  WaitingDecodeUpload,

  ReadyDecodeProc,
  WaitingDecodeProcDone,
  WaitingDecodeDownload,

  ReadyDecodePost,
  WaitingDecodePostDone,

  Finished
};

// Request: persistent end-to-end workload
struct Request {
  int input_length;
  double arrival_time;
  
  std::optional<int> remote;
  int next_prefill_layer = 0;
  int tokens_produced = 0;
  
  RequestState state;
};

struct ServerState  {
  bool busy = false;
};

struct WorldState {
  double current_time = 0.0;

  ServerState edge;
  std::vector<ServerState> clouds;
  std::vector<Request> requests;

  explicit WorldState(int cloud_count)
    : clouds(static_cast<std::size_t>(cloud_count)) {}
};

struct Assignment {
  ServerId server;
  TaskSpec task;
};

void applyFrame(WorldState& world, const Frame& frame, int num_layers);
void startAssignment(WorldState& world, const Assignment& assignment, int num_layers);
