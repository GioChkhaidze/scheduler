#include <scheduler/simulation/SimulationObservability.hpp>

#include <algorithm>

double resourceUtilization(const ResourceUsage& usage, double observation_span) {
  if (observation_span <= 0.0) {
    return 0.0;
  }
  return std::clamp(usage.busy_time / observation_span, 0.0, 1.0);
}
