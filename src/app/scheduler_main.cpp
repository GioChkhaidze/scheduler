// C++20
#include <scheduler/model/Protocol.hpp>
#include <scheduler/model/SystemConfig.hpp>
#include <scheduler/model/TaskTimeTable.hpp>
#include <scheduler/model/WorldState.hpp>
#include <scheduler/scheduling/HighScoreScheduler.hpp>

#include <iostream>
#include <vector>

int main() {
  std::ios_base::sync_with_stdio(false);
  std::cin.tie(nullptr);

  const SystemConfig system = readSystemConfig(std::cin);
  const TaskTimeTable table = readTaskTimeTable(std::cin);
  const TimingCurves curves = buildTimingCurves(table);
  HighScoreSchedulerConfig scheduler = buildFinalSchedulerConfig(system, curves);
  WorldState world{system.K};

  while (const std::optional<Frame> frame = readFrame(std::cin)) {
    observeLinkFrame(world, *frame, system);
    applyFrame(world, *frame, system.num_layers);
    const std::vector<Assignment> assignments =
      chooseHighScoreAssignments(world, system.num_layers, scheduler);
    for (const Assignment& assignment : assignments) {
      recordLinkAssignment(world, assignment, system, curves);
      startAssignment(world, assignment, system.num_layers);
    }
    writeAssignments(std::cout, assignments);
    std::cout << std::flush;
  }
}
