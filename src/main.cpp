// C++20
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
  
/*
Build the six usable timing curves from TaskTimeTable.
Define Frame, the four event types, servers, and all six task specifications.
Implement and test readFrame() for ARR, TDN, XDN, FIN, END, and EOF.
Define request lifecycle and edge/cloud busy state.
Implement legal task-generation functions.
Connect everything into the interactive read-update-decide-print-flush loop.
Replay the worked examples and test simultaneous events and degenerate cases.
*/

  // while (readFrame(std::cin)) {
  //   if (frame.end) break;

  //   applyEvents(frame);

  //   std::vector<Assignment> actions = scheduler.decide();
  //   print(actions);
  //   flush();
  // }

  // while (true) {
  //   Frame frame = readFrame();
  //   if (frame.end) break;

  //   applyEvents(frame);

  //   std::vector<Assignment> actions = scheduler.decide();
  //   print(actions);
  //   flush();
  // }

  return 0;
}
