#pragma once

#include <cstddef>

namespace tb::mdl
{
class EntityNode;
class Map;

bool setPathNodeRoll(Map& map, EntityNode& entityNode, size_t nodeIndex, double degrees);
bool setPathNodeCount(Map& map, EntityNode& entityNode, size_t nodeCount);
} // namespace tb::mdl
