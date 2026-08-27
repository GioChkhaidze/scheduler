// C++20
#include <SharedLinkScheduler.hpp>
#include "SystemConfig.hpp"
#include "TaskTimeTable.hpp"
#include "WorldState.hpp"
#include "Protocol.hpp"
#include <cassert>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <variant>

#ifndef SCHEDULER_V7_STAGE
#define SCHEDULER_V7_STAGE 4
#endif

namespace {

SharedLinkFeatures submissionFeatures() {
  constexpr int stage = SCHEDULER_V7_STAGE;
  static_assert(stage >= 0 && stage <= 4);
  switch (stage) {
    case 0:
      return {false, false, false};
    case 1:
      return {true, false, false};
    case 2:
      return {false, true, false};
    case 3:
      return {false, false, true};
    default:
      return {true, true, true};
  }
}

} // namespace

int main() {
  std::ios_base::sync_with_stdio(false);
  std::cin.tie(nullptr);

  SystemConfig config = readSystemConfig(std::cin);
  TaskTimeTable table = readTaskTimeTable(std::cin);
  const TimingCurves curves = buildTimingCurves(table);
  const SharedLinkSchedulerConfig scheduler_config =
    buildSharedLinkSchedulerConfig(config, curves, submissionFeatures());
  WorldState world{config.K};
  
  while (const auto frame = readFrame(std::cin)) {
    observeLinkFrame(world, *frame, config);
    applyFrame(world, *frame, config.num_layers);

    const std::vector<Assignment> assignments =
      chooseSharedLinkAssignments(world, config.num_layers, scheduler_config);

    for (const Assignment& assignment : assignments) {
      recordLinkAssignment(world, assignment, config, curves);
      startAssignment(world, assignment, config.num_layers);
    }

    writeAssignments(std::cout, assignments);
    std::cout << std::flush;
  }

  return 0;
}
