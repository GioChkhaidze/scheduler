// C++20
#include <Scheduler.hpp>
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

// class Scheduler {
//   SystemConfig config_;
//   TimingCurves timing_curves_;
//   WorldState world_;
// };

// class Scheduler {
// public:
//   void onFrame(const Frame& frame);

//   std::vector<Assignment> decide(double currentTime);

// private:
//   WorldState world_;
// }

/*
read the 2 parameter lines, then N and the N warmup rows
loop:
    read one line; if it is END: exit
    parse it as timestamp t; read event count e from the next line
    read the e event lines
    update your state (completions, arrivals, transfers, FINs)
    choose assignments for currently free resources (possibly none)
    print n and the n assignment lines; flush
*/

int main() {
  std::ios_base::sync_with_stdio(false);
  std::cin.tie(nullptr);

  SystemConfig config = readSystemConfig(std::cin);
  TaskTimeTable table = readTaskTimeTable(std::cin);
  WorldState world{config.K};
  
  while (const auto frame = readFrame(std::cin)) {
    applyFrame(world, *frame, config.num_layers);

    const std::vector<Assignment> assignments = chooseSingletonAssignments(world, config.num_layers);

    for (const Assignment& assignment : assignments) {
      startAssignment(world, assignment, config.num_layers);
    }

    writeAssignments(std::cout, assignments);
    std::cout << std::flush;
  }

  return 0;
}
