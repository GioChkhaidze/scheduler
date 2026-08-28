#include <scheduler/Scheduler.hpp>
#include <scheduler/simulation/Simulator.hpp>

#include <cassert>
#include <cmath>
#include <optional>
#include <variant>
#include <vector>

namespace {

bool isEqual(double left, double right, double tolerance = 1e-8) {
  return std::abs(left - right) <= tolerance;
}

SystemConfig makeSystem(int cloud_count = 2) {
  return {
    .SLO1 = 100.0,
    .SLO2 = 20.0,
    .tp_UB = 1.0,
    .tp_base = 0.0,
    .dist_base = 1.0,
    .w_tp = 1.0,
    .w_c = 0.0,
    .S = 1.0,
    .latency_in_ms = 5.0,
    .bandwidth_gbps = 1.0,
    .K = cloud_count,
    .bytes_per_token = 1,
    .num_layers = 4,
  };
}

TimingCurves makeCurves() {
  return buildTimingCurves({{
    {1, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
    {2, 1.0, 2.0, 1.0, 1.2, 1.2, 1.2},
    {100, 1.0, 100.0, 1.0, 3.0, 3.0, 3.0},
  }});
}

Request makeRequest(
  RequestState state,
  std::optional<int> remote,
  int input_length = 1,
  int tokens_produced = 0,
  std::optional<double> last_token_time = std::nullopt) {
  return {
    .input_length = input_length,
    .arrival_time = 0.0,
    .remote = remote,
    .next_prefill_layer = 0,
    .tokens_produced = tokens_produced,
    .last_token_time = last_token_time,
    .state = state,
  };
}

void testDecodePreReservationsUseIncreasingRemoteOrder() {
  const SystemConfig system = makeSystem();
  const TimingCurves curves = makeCurves();
  WorldState world{system.K};
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 1));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 1));
  const Assignment assignment{
    ServerId{ServerType::Edge, -1}, DecodePreTask{{0, 1, 2}}};

  recordLinkAssignment(world, assignment, system, curves);

  assert(world.links.pending_triggers.size() == 1);
  const std::vector<LinkTransferSpec>& transfers = world.links.pending_triggers.front().transfers;
  assert(transfers.size() == 2);
  assert(transfers[0].remote == 0);
  assert(transfers[0].rids == std::vector<int>({1}));
  assert(transfers[1].remote == 1);
  assert(transfers[1].rids == std::vector<int>({0, 2}));
}

void testUpAndDownQueuesCommitIndependently() {
  const SystemConfig system = makeSystem();
  const TimingCurves curves = makeCurves();
  WorldState world{system.K};
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillPre, std::nullopt));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodeProc, 1));
  const Assignment up{ServerId{ServerType::Edge, -1}, PrefillPreTask{0, 0}};
  const Assignment down{ServerId{ServerType::Cloud, 1}, DecodeProcTask{1, {1}}};
  recordLinkAssignment(world, up, system, curves);
  recordLinkAssignment(world, down, system, curves);

  const double done_at = system.S + 1.0;
  const Frame frame{done_at, {
    TaskDoneEvent{up.server, up.task, 1.0},
    TaskDoneEvent{down.server, down.task, 1.0},
  }};
  observeLinkFrame(world, frame, system);

  assert(world.links.up.committed.size() == 1);
  assert(world.links.down.committed.size() == 1);
  assert(isEqual(world.links.up.committed.front().starts_at, done_at));
  assert(isEqual(world.links.down.committed.front().starts_at, done_at));
}

void testSimultaneousTransfersPreserveInteractorEventOrder() {
  const SystemConfig system = makeSystem();
  const TimingCurves curves = makeCurves();
  WorldState world{system.K};
  world.requests.push_back(makeRequest(RequestState::ReadyDecodeProc, 0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodeProc, 1));
  const Assignment first{ServerId{ServerType::Cloud, 0}, DecodeProcTask{0, {0}}};
  const Assignment second{ServerId{ServerType::Cloud, 1}, DecodeProcTask{1, {1}}};
  recordLinkAssignment(world, first, system, curves);
  recordLinkAssignment(world, second, system, curves);

  assert(world.links.pending_triggers[0].order == 0);
  assert(world.links.pending_triggers[1].order == 1);
  const double done_at = system.S + 1.0;
  observeLinkFrame(world, {done_at, {
    TaskDoneEvent{first.server, first.task, 1.0},
    TaskDoneEvent{second.server, second.task, 1.0},
  }}, system);

  assert(world.links.down.committed.size() == 2);
  assert(world.links.down.committed[0].transfer.rids == std::vector<int>({0}));
  assert(world.links.down.committed[1].transfer.rids == std::vector<int>({1}));
}

void testTrackerReconcilesEverySimulatorTransfer() {
  const SystemConfig system = makeSystem(1);
  const TimingCurves curves = makeCurves();
  const MultiprocessorSchedulerConfig policy = buildMultiprocessorSchedulerConfig(system, curves);
  const SimulationResult result = simulate(system, curves, {{0.0, 2, 2}}, [&](const WorldState& world) {
    return chooseMultiprocessorAssignments(world, system.num_layers, policy);
  });

  assert(result.completed);
  assert(result.link_reconciliation_count == 6);
  assert(result.maximum_link_reconciliation_error <= 1e-8);
}

void testTrackerReconcilesMixedMultiRemoteWorkload() {
  const SystemConfig system = makeSystem(3);
  const TimingCurves curves = makeCurves();
  const SharedLinkSchedulerConfig policy =
    buildSharedLinkSchedulerConfig(system, curves, {false, false, false});
  const std::vector<SimulationRequest> workload{
    {0.0, 100, 4}, {0.0, 1, 3}, {2.0, 2, 2}, {3.0, 100, 3},
  };
  const SimulationResult result = simulate(system, curves, workload, [&](const WorldState& world) {
    return chooseSharedLinkAssignments(world, system.num_layers, policy);
  });

  assert(result.completed);
  assert(result.link_reconciliation_count > 20);
  assert(result.maximum_link_reconciliation_error <= 1e-8);
}

void testTrackingOnlyReproducesV6Exactly() {
  const SystemConfig system = makeSystem();
  const TimingCurves curves = makeCurves();
  const MultiprocessorSchedulerConfig v6 = buildMultiprocessorSchedulerConfig(system, curves);
  const SharedLinkSchedulerConfig tracking =
    buildSharedLinkSchedulerConfig(system, curves, {false, false, false});
  const std::vector<SimulationRequest> workload{
    {0.0, 100, 3}, {0.0, 1, 3}, {0.0, 100, 3}, {2.0, 2, 2},
  };

  const SimulationResult baseline = simulate(system, curves, workload, [&](const WorldState& world) {
    return chooseMultiprocessorAssignments(world, system.num_layers, v6);
  });
  const SimulationResult observed = simulate(system, curves, workload, [&](const WorldState& world) {
    return chooseSharedLinkAssignments(world, system.num_layers, tracking);
  });

  assert(baseline.completed);
  assert(observed.completed);
  assert(baseline.frame_count == observed.frame_count);
  assert(isEqual(baseline.metrics.tp, observed.metrics.tp));
  assert(isEqual(baseline.metrics.mean_tdr, observed.metrics.mean_tdr));
  assert(isEqual(baseline.metrics.mean_tpot, observed.metrics.mean_tpot));
  assert(isEqual(baseline.metrics.score, observed.metrics.score));
}

void testDecodePreCostPricesEveryRepresentedRemote() {
  const SystemConfig system = makeSystem();
  const TimingCurves curves = makeCurves();
  const SharedLinkSchedulerConfig config =
    buildSharedLinkSchedulerConfig(system, curves, {true, false, false});
  WorldState world{system.K};
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 1));

  const double one_remote = estimateDecodePreLinkCost(world, {0, 1}, config);
  const double two_remotes = estimateDecodePreLinkCost(world, {0, 2}, config);
  assert(isEqual(two_remotes - one_remote, system.latency_in_ms));
}

void testDecodePreCostIncludesCommittedUpBacklog() {
  SystemConfig system = makeSystem();
  system.bytes_per_token = 1'000'000;
  system.bandwidth_gbps = 0.001;
  const TimingCurves curves = makeCurves();
  const SharedLinkSchedulerConfig config =
    buildSharedLinkSchedulerConfig(system, curves, {true, false, false});
  WorldState clear{system.K};
  clear.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0));
  WorldState queued = clear;
  queued.requests.push_back(makeRequest(RequestState::ReadyPrefillPre, std::nullopt, 100));
  const Assignment prefill{ServerId{ServerType::Edge, -1}, PrefillPreTask{1, 0}};
  recordLinkAssignment(queued, prefill, system, curves);
  const double task_done = system.S + 1.0;
  observeLinkFrame(queued, {task_done, {TaskDoneEvent{prefill.server, prefill.task, 1.0}}}, system);
  queued.current_time = task_done;
  clear.current_time = task_done;

  assert(estimateDecodePreLinkCost(queued, {0}, config)
    > estimateDecodePreLinkCost(clear, {0}, config));
}

void testLocalityUsesCompatibleFillersButProtectsUrgency() {
  SystemConfig system = makeSystem();
  system.SLO2 = 100.0;
  const TimingCurves curves = makeCurves();
  const SharedLinkSchedulerConfig config =
    buildSharedLinkSchedulerConfig(system, curves, {true, false, false});
  WorldState world{system.K};
  world.current_time = 100.0;
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0, 1, 1, 80.0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 1, 1, 1, 90.0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0, 1, 1, 92.0));
  const Assignment baseline{ServerId{ServerType::Edge, -1}, DecodePreTask{{0, 1}}};

  const Assignment localized = localizeDecodePreAssignment(world, baseline, config);
  assert(std::get<DecodePreTask>(localized.task).rids == std::vector<int>({0, 2}));

  world.requests[2].last_token_time = 99.0;
  const Assignment protected_assignment = localizeDecodePreAssignment(world, baseline, config);
  assert(std::get<DecodePreTask>(protected_assignment.task).rids == std::vector<int>({0, 1}));
}

void testLocalityNeverChangesDecodePost() {
  const SystemConfig system = makeSystem();
  const SharedLinkSchedulerConfig config =
    buildSharedLinkSchedulerConfig(system, makeCurves(), {true, false, false});
  WorldState world{system.K};
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePost, 0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePost, 1));
  const Assignment baseline{ServerId{ServerType::Edge, -1}, DecodePostTask{{0, 1}}};

  const Assignment result = localizeDecodePreAssignment(world, baseline, config);
  assert(std::get<DecodePostTask>(result.task).rids == std::vector<int>({0, 1}));
}

void testUpAdmissionRunsKnownHotDecodeInsteadOfIdling() {
  SystemConfig system = makeSystem();
  system.SLO2 = 1.0;
  system.bytes_per_token = 1'000'000;
  system.bandwidth_gbps = 0.001;
  const TimingCurves curves = makeCurves();
  const SharedLinkSchedulerConfig config =
    buildSharedLinkSchedulerConfig(system, curves, {false, true, false});
  WorldState world{system.K};
  world.current_time = 10.0;
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0, 1, 1, 9.5));
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillPre, std::nullopt, 100));

  world.clouds[1].busy = true;
  const std::vector<Assignment> assignments =
    chooseSharedLinkAssignments(world, system.num_layers, config);
  assert(assignments.size() == 1);
  assert(std::holds_alternative<DecodePreTask>(assignments.front().task));
  assert(std::get<DecodePreTask>(assignments.front().task).rids == std::vector<int>({0}));
}

void testUpAdmissionLeavesPrefillWhenHotUploadHasSloSlack() {
  SystemConfig system = makeSystem();
  system.SLO2 = 1'000'000.0;
  const TimingCurves curves = makeCurves();
  const SharedLinkSchedulerConfig config =
    buildSharedLinkSchedulerConfig(system, curves, {false, true, false});
  WorldState world{system.K};
  world.current_time = 10.0;
  world.requests.push_back(makeRequest(RequestState::ReadyDecodePre, 0, 1, 1, 9.5));
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillPre, std::nullopt, 100));

  const std::vector<Assignment> assignments =
    chooseSharedLinkAssignments(world, system.num_layers, config);
  assert(assignments.size() == 1);
  assert(std::holds_alternative<PrefillPreTask>(assignments.front().task));
}

void testDownAdmissionProtectsKnownHotDecodeWork() {
  SystemConfig system = makeSystem();
  system.SLO2 = 1.0;
  system.bytes_per_token = 1'000'000;
  system.bandwidth_gbps = 0.001;
  const TimingCurves curves = makeCurves();
  const SharedLinkSchedulerConfig config =
    buildSharedLinkSchedulerConfig(system, curves, {false, false, true});
  WorldState world{system.K};
  world.current_time = 10.0;
  world.edge.busy = true;
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillProc, 0));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodeProc, 1, 1, 1, 9.5));

  const std::vector<Assignment> assignments =
    chooseSharedLinkAssignments(world, system.num_layers, config);
  assert(assignments.size() == 1);
  assert(std::holds_alternative<DecodeProcTask>(assignments.front().task));
}

void testDownAdmissionDoesNotDelayPrefillBehindKnownDecodeDownload() {
  SystemConfig system = makeSystem();
  system.SLO2 = 1.0;
  system.bytes_per_token = 1'000'000;
  system.bandwidth_gbps = 0.001;
  const TimingCurves curves = makeCurves();
  const SharedLinkSchedulerConfig config =
    buildSharedLinkSchedulerConfig(system, curves, {false, false, true});
  WorldState world{system.K};
  world.current_time = 10.0;
  world.edge.busy = true;
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillProc, 0, 100));
  world.requests.push_back(makeRequest(RequestState::ReadyDecodeProc, 1, 1, 1, 9.5));

  const std::vector<Assignment> assignments =
    chooseSharedLinkAssignments(world, system.num_layers, config);
  assert(assignments.size() == 2);
  assert(std::holds_alternative<PrefillProcTask>(assignments[0].task));
  assert(std::holds_alternative<DecodeProcTask>(assignments[1].task));
}

void testAdmissionComparesTdrAndTpotPressure() {
  SystemConfig system = makeSystem();
  system.SLO2 = 1.0;
  system.bytes_per_token = 1'000'000;
  system.bandwidth_gbps = 0.001;
  const TimingCurves curves = makeCurves();
  const SharedLinkSchedulerConfig config =
    buildSharedLinkSchedulerConfig(system, curves, {false, false, true});
  WorldState world{system.K};
  world.current_time = 10.0;
  world.edge.busy = true;
  world.requests.push_back(makeRequest(RequestState::ReadyPrefillProc, 0));
  world.requests[0].arrival_time = -10'000'000.0;
  world.requests.push_back(makeRequest(RequestState::ReadyDecodeProc, 1, 1, 1, 9.5));

  const std::vector<Assignment> assignments =
    chooseSharedLinkAssignments(world, system.num_layers, config);
  assert(assignments.size() == 2);
  assert(std::holds_alternative<PrefillProcTask>(assignments[0].task));
}

} // namespace

int main() {
  testDecodePreReservationsUseIncreasingRemoteOrder();
  testUpAndDownQueuesCommitIndependently();
  testSimultaneousTransfersPreserveInteractorEventOrder();
  testTrackerReconcilesEverySimulatorTransfer();
  testTrackerReconcilesMixedMultiRemoteWorkload();
  testTrackingOnlyReproducesV6Exactly();
  testDecodePreCostPricesEveryRepresentedRemote();
  testDecodePreCostIncludesCommittedUpBacklog();
  testLocalityUsesCompatibleFillersButProtectsUrgency();
  testLocalityNeverChangesDecodePost();
  testUpAdmissionRunsKnownHotDecodeInsteadOfIdling();
  testUpAdmissionLeavesPrefillWhenHotUploadHasSloSlack();
  testDownAdmissionProtectsKnownHotDecodeWork();
  testDownAdmissionDoesNotDelayPrefillBehindKnownDecodeDownload();
  testAdmissionComparesTdrAndTpotPressure();
  return 0;
}
