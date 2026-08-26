#include "WorldState.hpp"

#include <cassert>

// Implement applyFrame() setting world.current_time.
// Implement only the ARR event transition.
// Test single and simultaneous arrivals.
// Add FIN.
// Add resource freeing and TDN transitions.
// Add XDN transitions.
// Later add assignment transitions.

void applyFrame(WorldState& world, const Frame& frame, int num_layers) {
  // came frame, and we have world state
  world.current_time = frame.timestamp;
  for (const Event& event : frame.events) {
    if (const ArrivalEvent* arrival = std::get_if<ArrivalEvent>(&event)) {
      // ARR transition.
      assert(arrival->rid >= 0);
      const auto rid = static_cast<std::size_t>(arrival->rid);
      assert(rid == world.requests.size());

      world.requests.push_back({
        .input_length = arrival->input_length,
        .arrival_time = frame.timestamp,
        .remote = std::nullopt,
        .next_prefill_layer = 0,
        .tokens_produced = 0,
        .state = RequestState::ReadyPrefillPre,
      });
    } 

    if (const TaskDoneEvent* taskDone = std::get_if<TaskDoneEvent>(&event)) {
      // pass
    }

    if (const TransferDoneEvent* transferDone = std::get_if<TransferDoneEvent>(&event)) {
      // pass
    }

    if (const FinishEvent* finish = std::get_if<FinishEvent>(&event)) {
      // FIN transition.
      assert(finish->rid >= 0);
      const auto rid = static_cast<std::size_t>(finish->rid);
      assert(rid < world.requests.size());

      world.requests.at(rid).state = RequestState::Finished;
    } 
  }
}