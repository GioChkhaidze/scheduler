#include <Scheduler.hpp>

#include <cassert>
#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

namespace {

Request makeRequest(std::optional<int> remote, RequestState state, int next_prefill_layer = 0) {
  return {
    .input_length = 128,
    .arrival_time = 1.0,
    .remote = remote,
    .next_prefill_layer = next_prefill_layer,
    .tokens_produced = 0,
    .state = state,
  };
}

TimingCurves makeUniformCurves(const TimingCurve& curve) {
  return {
    .prefill_pre = curve,
    .prefill_proc = curve,
    .prefill_post = curve,
    .decode_pre = curve,
    .decode_proc = curve,
    .decode_post = curve,
  };
}

DecodeBatchPolicy makeMaximalBatchPolicy(int max_batch_size = 8) {
  const TimingCurve constant_curve{{{1, 0.0}, {max_batch_size, 0.0}}};
  return buildDecodeBatchPolicy(makeUniformCurves(constant_curve), 10.0, max_batch_size);
}

template <typename Task>
const Task& getTask(const std::vector<Assignment>& assignments, std::size_t index) {
  assert(index < assignments.size());
  assert(std::holds_alternative<Task>(assignments[index].task));
  return std::get<Task>(assignments[index].task);
}

void testChoosesOneTaskPerFreeResourceWithoutMutatingWorld() {
  WorldState world{2};
  world.requests.push_back(makeRequest(std::nullopt, RequestState::ReadyPrefillPre));
  world.requests.push_back(makeRequest(0, RequestState::ReadyDecodePost));
  world.requests.push_back(makeRequest(1, RequestState::ReadyDecodeProc));
  world.requests.push_back(makeRequest(0, RequestState::ReadyPrefillProc));

  const std::vector<Assignment> assignments = chooseSingletonAssignments(world, 4);

  assert(assignments.size() == 3);

  assert(assignments[0].server.type == ServerType::Edge);
  const DecodePostTask& decode_post = getTask<DecodePostTask>(assignments, 0);
  assert(decode_post.rids == std::vector<int>{1});

  assert(assignments[1].server.type == ServerType::Cloud);
  assert(assignments[1].server.cloud_index == 0);
  const PrefillProcTask& prefill_proc = getTask<PrefillProcTask>(assignments, 1);
  assert(prefill_proc.layer_begin == 0);
  assert(prefill_proc.layer_end == 4);
  assert(prefill_proc.remote == 0);
  assert(prefill_proc.rid == 3);

  assert(assignments[2].server.type == ServerType::Cloud);
  assert(assignments[2].server.cloud_index == 1);
  const DecodeProcTask& decode_proc = getTask<DecodeProcTask>(assignments, 2);
  assert(decode_proc.remote == 1);
  assert(decode_proc.rids == std::vector<int>{2});

  assert(!world.edge.busy);
  assert(!world.clouds[0].busy);
  assert(!world.clouds[1].busy);
  assert(world.requests[1].state == RequestState::ReadyDecodePost);

  for (const Assignment& assignment : assignments) {
    startAssignment(world, assignment, 4);
  }

  assert(world.edge.busy);
  assert(world.clouds[0].busy);
  assert(world.clouds[1].busy);
  assert(world.requests[1].state == RequestState::WaitingDecodePostDone);
  assert(world.requests[2].state == RequestState::WaitingDecodeProcDone);
  assert(world.requests[3].state == RequestState::WaitingPrefillProcDone);
}

void testPrefillPlacementIsDeterministic() {
  WorldState world{3};
  world.requests.push_back(makeRequest(0, RequestState::Finished));
  world.requests.push_back(makeRequest(1, RequestState::Finished));
  world.requests.push_back(makeRequest(std::nullopt, RequestState::ReadyPrefillPre));

  const std::vector<Assignment> assignments = chooseSingletonAssignments(world, 8);

  assert(assignments.size() == 1);
  assert(assignments[0].server.type == ServerType::Edge);
  const PrefillPreTask& task = getTask<PrefillPreTask>(assignments, 0);
  assert(task.rid == 2);
  assert(task.remote == 2);
}

void testBusyResourcesAndNoReadyWorkProduceNoAssignments() {
  WorldState blocked{1};
  blocked.edge.busy = true;
  blocked.clouds[0].busy = true;
  blocked.requests.push_back(makeRequest(std::nullopt, RequestState::ReadyPrefillPre));
  blocked.requests.push_back(makeRequest(0, RequestState::ReadyDecodeProc));

  assert(chooseSingletonAssignments(blocked, 4).empty());

  WorldState idle{2};
  assert(chooseSingletonAssignments(idle, 4).empty());
}

void testContinuesPrefillFromNextUnfinishedLayer() {
  WorldState world{1};
  world.requests.push_back(makeRequest(0, RequestState::ReadyPrefillProc, 3));

  const std::vector<Assignment> assignments = chooseSingletonAssignments(world, 8);

  assert(assignments.size() == 1);
  const PrefillProcTask& task = getTask<PrefillProcTask>(assignments, 0);
  assert(task.layer_begin == 3);
  assert(task.layer_end == 8);
}

void testPrecomputesLowestAmortizedCostForEveryReadyCount() {
  const TimingCurve curve{{{1, 10.0}, {2, 12.0}, {3, 60.0}, {4, 14.0}}};
  const DecodeBatchPolicy policy = buildDecodeBatchPolicy(makeUniformCurves(curve), 10.0, 4);

  assert(policy.pre_by_ready_count == std::vector<int>({0, 1, 2, 2, 4}));
  assert(policy.proc_by_ready_count == policy.pre_by_ready_count);
  assert(policy.post_by_ready_count == policy.pre_by_ready_count);
}

void testBatchesDecodePreAcrossRemotesWithoutWaiting() {
  WorldState world{2};
  world.requests.push_back(makeRequest(0, RequestState::ReadyDecodePre));
  world.requests.push_back(makeRequest(1, RequestState::ReadyDecodePre));

  const DecodeBatchPolicy policy = makeMaximalBatchPolicy();
  const std::vector<Assignment> assignments = chooseBatchedAssignments(world, 4, policy);

  assert(assignments.size() == 1);
  const DecodePreTask& task = getTask<DecodePreTask>(assignments, 0);
  assert(task.rids == std::vector<int>({0, 1}));

  startAssignment(world, assignments[0], 4);
  assert(world.requests[0].state == RequestState::WaitingDecodePreDone);
  assert(world.requests[1].state == RequestState::WaitingDecodePreDone);
}

void testBatchesDecodeProcIndependentlyPerRemote() {
  WorldState world{2};
  world.requests.push_back(makeRequest(0, RequestState::ReadyDecodeProc));
  world.requests.push_back(makeRequest(1, RequestState::ReadyDecodeProc));
  world.requests.push_back(makeRequest(0, RequestState::ReadyDecodeProc));
  world.requests.push_back(makeRequest(0, RequestState::ReadyDecodeProc));

  const DecodeBatchPolicy policy = makeMaximalBatchPolicy();
  const std::vector<Assignment> assignments = chooseBatchedAssignments(world, 4, policy);

  assert(assignments.size() == 2);
  assert(assignments[0].server.type == ServerType::Cloud);
  assert(assignments[0].server.cloud_index == 0);
  assert(assignments[1].server.type == ServerType::Cloud);
  assert(assignments[1].server.cloud_index == 1);
  assert(getTask<DecodeProcTask>(assignments, 0).rids == std::vector<int>({0, 2, 3}));
  assert(getTask<DecodeProcTask>(assignments, 1).rids == std::vector<int>({1}));
}

void testBatchesDecodePostAcrossRemotes() {
  WorldState world{2};
  world.requests.push_back(makeRequest(1, RequestState::ReadyDecodePost));
  world.requests.push_back(makeRequest(0, RequestState::ReadyDecodePost));
  world.requests.push_back(makeRequest(1, RequestState::ReadyDecodePost));

  const DecodeBatchPolicy policy = makeMaximalBatchPolicy();
  const std::vector<Assignment> assignments = chooseBatchedAssignments(world, 4, policy);

  assert(assignments.size() == 1);
  assert(getTask<DecodePostTask>(assignments, 0).rids == std::vector<int>({0, 1, 2}));
}

void testUsesSingletonWhenItHasLowestAmortizedCost() {
  const TimingCurve expensive_batches{{{1, 1.0}, {2, 100.0}, {4, 400.0}}};
  const DecodeBatchPolicy policy = buildDecodeBatchPolicy(makeUniformCurves(expensive_batches), 0.0, 4);
  WorldState world{1};
  world.requests.push_back(makeRequest(0, RequestState::ReadyDecodePre));
  world.requests.push_back(makeRequest(0, RequestState::ReadyDecodePre));
  world.requests.push_back(makeRequest(0, RequestState::ReadyDecodePre));

  const std::vector<Assignment> assignments = chooseBatchedAssignments(world, 4, policy);

  assert(assignments.size() == 1);
  assert(getTask<DecodePreTask>(assignments, 0).rids == std::vector<int>({0}));
}

} // namespace

int main() {
  testChoosesOneTaskPerFreeResourceWithoutMutatingWorld();
  testPrefillPlacementIsDeterministic();
  testBusyResourcesAndNoReadyWorkProduceNoAssignments();
  testContinuesPrefillFromNextUnfinishedLayer();
  testPrecomputesLowestAmortizedCostForEveryReadyCount();
  testBatchesDecodePreAcrossRemotesWithoutWaiting();
  testBatchesDecodeProcIndependentlyPerRemote();
  testBatchesDecodePostAcrossRemotes();
  testUsesSingletonWhenItHasLowestAmortizedCost();
  return 0;
}
