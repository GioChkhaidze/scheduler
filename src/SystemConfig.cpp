#include "SystemConfig.hpp"
#include <istream>
#include <cassert>

// move this to correct file
double transferTime(const SystemConfig& config, std::int64_t len) {
  // for input transfer: len = L_in[i]
  // for output transfer: 
  //   for output, use the per-remote computer local-to-remote transfer and 
  //   per-group remote-to-local transfer sizes defined in Output Steps
  assert(config.latency_in_ms > 0);
  const std::int64_t data_bytes = len * config.bytes_per_token;
  return config.latency_in_ms + 8.0 * static_cast<double>(data_bytes) 
    / (config.bandwidth_gbps * 1'000'000.0); // ms
}

SystemConfig readSystemConfig(std::istream& input) {
  SystemConfig config{};
  
  input >> config.K
    >> config.S
    >> config.latency_in_ms
    >> config.bandwidth_gbps
    >> config.bytes_per_token
    >> config.num_layers
    >> config.SLO1
    >> config.SLO2
    >> config.tp_UB
    >> config.tp_base
    >> config.dist_base
    >> config.w_tp
    >> config.w_c;

  return config;
}