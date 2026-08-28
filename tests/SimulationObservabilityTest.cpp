#include <scheduler/Scheduler.hpp>
#include <scheduler/simulation/Simulator.hpp>

#include <cassert>
#include <cmath>
#include <vector>

namespace {

SystemConfig makeSystem() {
  return {
    .SLO1 = 2.0,
    .SLO2 = 2.0,
    .tp_UB = 1.0,
    .tp_base = 0.0,
    .dist_base = 10.0,
    .w_tp = 0.5,
    .w_c = 0.5,
    .S = 1.0,
    .latency_in_ms = 1.0,
    .bandwidth_gbps = 100.0,
    .K = 2,
    .bytes_per_token = 1,
    .num_layers = 4,
  };
}

TimingCurves makeCurves() {
  const TimingCurve curve{{{1, 1.0}, {16, 2.0}}};
  return {curve, curve, curve, curve, curve, curve};
}

void testMeasuresIntervalsQueuesPopulationsBatchesAndDecisions() {
  const SystemConfig system = makeSystem();
  const TimingCurves curves = makeCurves();
  bool deferred_once = false;
  const SimulationResult result = simulate(
    system,
    curves,
    {{0.0, 4, 2}, {10.0, 4, 2}},
    [&](const WorldState& world) {
      if (!deferred_once) {
        deferred_once = true;
        return std::vector<Assignment>{};
      }
      return chooseSingletonAssignments(world, system.num_layers);
    },
    {true, 1'000, true});

  assert(result.completed);
  const SimulationDiagnostics& diagnostics = result.diagnostics;
  assert(diagnostics.observation_span == result.metrics.total_elapsed_time);
  assert(diagnostics.edge.busy_time > 0.0);
  assert(diagnostics.edge.eligible_idle_time >= 10.0);
  assert(resourceUtilization(diagnostics.edge, diagnostics.observation_span) > 0.0);
  assert(resourceUtilization(diagnostics.edge, diagnostics.observation_span) <= 1.0);
  assert(diagnostics.clouds.size() == static_cast<std::size_t>(system.K));
  assert(diagnostics.up.busy_time > 0.0);
  assert(diagnostics.down.busy_time > 0.0);
  assert(diagnostics.average_edge_queue_ms > 0.0);
  assert(diagnostics.maximum_edge_queue_ms >= diagnostics.average_edge_queue_ms);
  assert(diagnostics.maximum_prefill_population == 2);
  assert(diagnostics.maximum_hot_population > 0);
  assert(diagnostics.accumulated_tdr_pressure_time > 0.0);
  assert(diagnostics.accumulated_tpot_pressure_time > 0.0);
  assert(diagnostics.decode_batches.pre.size() > 1);
  assert(diagnostics.decode_batches.pre[1] > 0);
  assert(diagnostics.decode_batches.proc[1] > 0);
  assert(diagnostics.decode_batches.post[1] > 0);
  assert(!diagnostics.observations.empty());
  assert(!diagnostics.decisions.empty());
  assert(diagnostics.decisions.front().reason == ResourceDecisionReason::PolicyDeferred);
  assert(diagnostics.decisions.front().eligible_by_task[0] == 1);
  assert(diagnostics.decisions.front().candidates.size() == scheduled_task_kind_count);
  assert(diagnostics.decisions.front().candidates[0].reason == CandidateDecisionReason::PolicyRejected);
}

void testObservabilityCanBeDisabledWithoutChangingMetrics() {
  const SystemConfig system = makeSystem();
  const TimingCurves curves = makeCurves();
  const auto policy = [&](const WorldState& world) {
    return chooseSingletonAssignments(world, system.num_layers);
  };
  const SimulationResult plain = simulate(system, curves, {{0.0, 4, 2}}, policy);
  const SimulationResult observed = simulate(system, curves, {{0.0, 4, 2}}, policy, {false, 1'000, true});
  assert(plain.completed && observed.completed);
  assert(std::abs(plain.metrics.score - observed.metrics.score) < 1e-9);
  assert(plain.diagnostics.observations.empty());
  assert(!observed.diagnostics.observations.empty());
}

} // namespace

int main() {
  testMeasuresIntervalsQueuesPopulationsBatchesAndDecisions();
  testObservabilityCanBeDisabledWithoutChangingMetrics();
  return 0;
}
