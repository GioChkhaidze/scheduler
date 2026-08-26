#pragma once
// times in ms, output rates in tokens/ms
#include <iosfwd>
#include <cstdint>

// K S latency_in_ms bandwidth_gbps bytes_per_token num_layers
// SLO1 SLO2 tp_UB tp_base dist_base w_tp w_c
struct SystemConfig {
  // Scoring parameters
  double SLO1;
  double SLO2;
  double tp_UB;
  double tp_base;
  double dist_base;
  double w_tp;
  double w_c;

  // System parameters
  double S;                   // Schedule Cost
  double latency_in_ms;
  double bandwidth_gbps;
  int K;
  int bytes_per_token;
  int num_layers;
};

double transferTime(const SystemConfig& config, std::int64_t len);
SystemConfig readSystemConfig(std::istream& input);