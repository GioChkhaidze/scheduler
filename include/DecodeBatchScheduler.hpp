#pragma once

#include <TaskTimeTable.hpp>
#include <WorldState.hpp>

#include <cstddef>
#include <vector>

struct DecodeBatchPolicy {
  std::vector<int> pre_by_ready_count;
  std::vector<int> proc_by_ready_count;
  std::vector<int> post_by_ready_count;
};

DecodeBatchPolicy buildDecodeBatchPolicy(const TimingCurves& curves, double assignment_cost, int max_batch_size = 4096);
int selectDecodeBatchSize(const DecodeBatchPolicy& policy, RequestState state, std::size_t ready_count);
std::vector<Assignment> chooseBatchedAssignments(
  const WorldState& world, int num_layers, const DecodeBatchPolicy& batch_policy);
