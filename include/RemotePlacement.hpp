#pragma once

#include <functional>

struct WorldState;

using PrefillRemoteSelector = std::function<int(const WorldState&, int)>;
