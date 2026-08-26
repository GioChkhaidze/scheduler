#include "SystemConfig.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <sstream>

namespace {

constexpr double epsilon = 1e-9;

bool isEqual(double actual, double expected) {
  return std::abs(actual - expected) < epsilon;
}

void testExampleConfiguration() {
  std::istringstream input{
    "1 1.000000000 2.000000000 1.000000000 125000 4\n"
    "30.000000000 15.000000000 0.062500000 0.022222222 0.000000000 0.500000000 0.500000000\n"
  };

  const SystemConfig config = readSystemConfig(input);

  assert(config.K == 1);
  assert(config.bytes_per_token == 125000);
  assert(config.num_layers == 4);
  assert(isEqual(config.S, 1.0));
  assert(isEqual(config.latency_in_ms, 2.0));
  assert(isEqual(config.bandwidth_gbps, 1.0));
  assert(isEqual(config.SLO1, 30.0));
  assert(isEqual(config.SLO2, 15.0));
  assert(isEqual(config.tp_UB, 0.0625));
  assert(isEqual(config.tp_base, 0.022222222));
  assert(isEqual(config.dist_base, 0.0));
  assert(isEqual(config.w_tp, 0.5));
  assert(isEqual(config.w_c, 0.5));
}

void testAlternativeConfigurationAndInputConsumption() {
  std::istringstream input{
    "8 10.0 0.5 2.0 4 64\n"
    "1.5 2.5 9.0 0.25 3.0 0.75 0.25\n"
    "777\n"
  };

  const SystemConfig config = readSystemConfig(input);

  assert(config.K == 8);
  assert(config.bytes_per_token == 4);
  assert(config.num_layers == 64);
  assert(isEqual(config.S, 10.0));
  assert(isEqual(config.latency_in_ms, 0.5));
  assert(isEqual(config.bandwidth_gbps, 2.0));
  assert(isEqual(config.SLO1, 1.5));
  assert(isEqual(config.SLO2, 2.5));
  assert(isEqual(config.tp_UB, 9.0));
  assert(isEqual(config.tp_base, 0.25));
  assert(isEqual(config.dist_base, 3.0));
  assert(isEqual(config.w_tp, 0.75));
  assert(isEqual(config.w_c, 0.25));

  int marker = 0;
  input >> marker;
  assert(marker == 777);
}

void testTransferTimesIncludingLargeInput() {
  std::istringstream input{
    "1 1.0 2.0 1.0 125000 4\n"
    "30.0 15.0 0.0625 0.022222222 0.0 0.5 0.5\n"
  };

  const SystemConfig config = readSystemConfig(input);

  assert(isEqual(transferTime(config, std::int64_t{1}), 3.0));
  assert(isEqual(transferTime(config, std::int64_t{4096}), 4098.0));
}

} // namespace

int main() {
  testExampleConfiguration();
  testAlternativeConfigurationAndInputConsumption();
  testTransferTimesIncludingLargeInput();
  return 0;
}
