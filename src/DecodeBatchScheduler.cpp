#include <DecodeBatchScheduler.hpp>

#include "SchedulerCore.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>
#include <vector>

namespace {

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

class BatchedDecodeSelector {
public:
  explicit BatchedDecodeSelector(const DecodeBatchPolicy& policy)
    : policy_(policy) {}

  std::vector<int> select(
    const WorldState& world, RequestState state, std::optional<int> remote) const {
    std::vector<int> rids = scheduler_detail::findRequests(
      world, state, remote, std::numeric_limits<std::size_t>::max());
    if (!rids.empty()) {
      rids.resize(static_cast<std::size_t>(selectDecodeBatchSize(policy_, state, rids.size())));
    }
    return rids;
  }

  bool preferPrefillPostBeforeDecodePre(const WorldState&) const {
    return true;
  }

private:
  const DecodeBatchPolicy& policy_;
};

} // namespace

int selectDecodeBatchSize(const DecodeBatchPolicy& policy, RequestState state, std::size_t ready_count) {
  assert(ready_count > 0);
  const std::vector<int>* choices = nullptr;
  if (state == RequestState::ReadyDecodePre) {
    choices = &policy.pre_by_ready_count;
  } else if (state == RequestState::ReadyDecodeProc) {
    choices = &policy.proc_by_ready_count;
  } else {
    assert(state == RequestState::ReadyDecodePost);
    choices = &policy.post_by_ready_count;
  }

  assert(choices->size() > 1);
  const std::size_t index = std::min(ready_count, choices->size() - 1);
  return choices->at(index);
}

DecodeBatchPolicy buildDecodeBatchPolicy(const TimingCurves& curves, double assignment_cost, int max_batch_size) {
  return {
    .pre_by_ready_count = buildBatchChoices(curves.decode_pre, assignment_cost, max_batch_size),
    .proc_by_ready_count = buildBatchChoices(curves.decode_proc, assignment_cost, max_batch_size),
    .post_by_ready_count = buildBatchChoices(curves.decode_post, assignment_cost, max_batch_size),
  };
}

std::vector<Assignment> chooseBatchedAssignments(
  const WorldState& world, int num_layers, const DecodeBatchPolicy& batch_policy) {
  return scheduler_detail::chooseAssignments(world, num_layers, BatchedDecodeSelector{batch_policy});
}
