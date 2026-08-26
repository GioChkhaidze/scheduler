#pragma once
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <variant>
#include <vector>

/************************************/
/************** SERVERS *************/
/************************************/

enum class ServerType {
  Edge,
  Cloud
};

struct ServerId {
  ServerType type;
  int cloud_index; // Ignored when type == Edge
};

/*********************************/
/************* TASKS *************/
/*********************************/

struct PrefillPreTask {
  int remote;
  int rid;
};

struct PrefillProcTask {
  int layer_begin;
  int layer_end;
  int remote;
  int rid;
};

struct PrefillPostTask {
  int remote;
  int rid;
};

struct DecodePreTask {
  std::vector<int> rids;
};

struct DecodeProcTask {
  int remote;
  std::vector<int> rids;
};

struct DecodePostTask {
  std::vector<int> rids;
};

// TaskSpec: temporary work assigned to a computer
using TaskSpec = std::variant<
  PrefillPreTask,
  PrefillProcTask,
  PrefillPostTask,
  DecodePreTask,
  DecodeProcTask,
  DecodePostTask
>;

/**********************************/
/************* EVENTS *************/
/**********************************/

struct ArrivalEvent { 
  int rid;
  int input_length;
  /* rid, input length */ 
};

struct TaskDoneEvent {
  ServerId server;
  TaskSpec task;
  double duration;
  /* server, task specification, duration */ 
};

enum class TransferDirection {
  Up,
  Down
};

enum class TransferStage {
  Prefill,
  Decode
};

// XDN <UP|DOWN> <remote> <size> <PRE|DEC> <m> <rid...>
struct TransferDoneEvent { 
  TransferDirection direction;
  int remote;
  std::int64_t size_bytes;
  TransferStage stage;
  std::vector < int > rids;
  /* direction, remote, size, stage, rids */ 
};

struct FinishEvent { 
  int rid;
  /* rid */ 
};

// Event: notification that something arrived or completed
using Event = std::variant<
  ArrivalEvent,
  TaskDoneEvent,
  TransferDoneEvent,
  FinishEvent
>;

/********************************/
/************* FRAME ************/
/********************************/

struct Frame {
  double timestamp;
  std::vector < Event > events;
};

std::optional < Frame > readFrame(std::istream& input);