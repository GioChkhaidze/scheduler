#pragma once

#include <Protocol.hpp>

#include <cstdint>
#include <deque>
#include <vector>

struct LinkTransferSpec {
  TransferDirection direction;
  int remote;
  TransferStage stage;
  std::int64_t length;
  std::vector<int> rids;
};

struct PendingLinkTrigger {
  ServerId server;
  TaskSpec task;
  double expected_completion;
  std::uint64_t order;
  std::vector<LinkTransferSpec> transfers;
};

struct CommittedLinkTransfer {
  LinkTransferSpec transfer;
  double queued_at;
  double starts_at;
  double completes_at;
};

struct LinkDirectionState {
  double committed_tail = 0.0;
  std::deque<CommittedLinkTransfer> committed;
};

struct SharedLinkState {
  LinkDirectionState up;
  LinkDirectionState down;
  std::vector<PendingLinkTrigger> pending_triggers;
  std::uint64_t next_trigger_order = 0;
  int reconciliation_count = 0;
  double maximum_reconciliation_error = 0.0;
};
