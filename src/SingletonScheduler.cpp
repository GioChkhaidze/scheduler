#include <SingletonScheduler.hpp>

#include "SchedulerCore.hpp"

#include <cstddef>

namespace {

class SingletonDecodeSelector {
public:
  std::size_t inspectionLimit() const {
    return 1;
  }

  int batchSize(RequestState, std::size_t) const {
    return 1;
  }
};

} // namespace

std::vector<Assignment> chooseSingletonAssignments(const WorldState& world, int num_layers) {
  return scheduler_detail::chooseAssignments(world, num_layers, SingletonDecodeSelector{});
}
