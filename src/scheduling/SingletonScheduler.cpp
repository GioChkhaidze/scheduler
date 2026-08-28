#include <scheduler/scheduling/SingletonScheduler.hpp>

#include "SchedulerCore.hpp"

#include <cstddef>

namespace {

class SingletonDecodeSelector {
public:
  std::vector<int> select(
    const WorldState& world, RequestState state, std::optional<int> remote) const {
    return scheduler_detail::findRequests(world, state, remote, 1);
  }

  bool preferPrefillPostBeforeDecodePre(const WorldState&) const {
    return true;
  }

  bool preferPrefillPreBeforeDecodePre(const WorldState&) const {
    return false;
  }

  bool preferPrefillProcBeforeDecodeProc(const WorldState&, int) const {
    return false;
  }
};

} // namespace

std::vector<Assignment> chooseSingletonAssignments(const WorldState& world, int num_layers) {
  return scheduler_detail::chooseAssignments(world, num_layers, SingletonDecodeSelector{});
}
