#pragma once

#include <scheduler/model/WorldState.hpp>

#include <vector>

std::vector<Assignment> chooseSingletonAssignments(const WorldState& world, int num_layers);
