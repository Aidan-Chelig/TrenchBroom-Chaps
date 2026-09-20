#include "mdl/Map_Paths.h"

#include "mdl/ApplyAndSwap.h"
#include "mdl/Map_Groups.h"
#include "mdl/PathEntity.h"

#include "kd/overload.h"

#include <cmath>

namespace tb::mdl
{
namespace
{
template <typename F>
bool updatePath(
  Map& map, EntityNode& entityNode, const std::string& commandName, F update)
{
  const auto nodes = std::vector<EntityNode*>{&entityNode};
  return applyAndSwap(
    map,
    commandName,
    nodes,
    collectContainingGroups(std::vector<Node*>{&entityNode}),
    kdl::overload(
      [&](Entity& entity) {
        const auto parsed = readPath(entity);
        if (!parsed)
        {
          return false;
        }
        auto path = parsed.value();
        if (!update(path))
        {
          return false;
        }
        return writePath(entity, path).is_success();
      },
      [](Layer&) { return true; },
      [](Group&) { return true; },
      [](Brush&) { return true; },
      [](BezierPatch&) { return true; }));
}
} // namespace

bool setPathNodeRoll(
  Map& map, EntityNode& entityNode, const size_t nodeIndex, const double degrees)
{
  if (!std::isfinite(degrees))
  {
    return false;
  }
  return updatePath(map, entityNode, "Set Path Roll", [&](Path& path) {
    if (nodeIndex >= path.nodes.size())
    {
      return false;
    }
    path.nodes[nodeIndex].roll = degrees == 0.0 ? 0.0 : degrees;
    return true;
  });
}

bool setPathNodeCount(Map& map, EntityNode& entityNode, const size_t nodeCount)
{
  if (nodeCount > 65536u)
  {
    return false;
  }
  return updatePath(map, entityNode, "Set Path Point Count", [&](Path& path) {
    while (path.nodes.size() < nodeCount)
    {
      const auto position =
        path.nodes.empty() ? vm::vec3d{}
        : path.nodes.size() == 1u
          ? path.nodes.back().position + vm::vec3d{64, 0, 0}
          : path.nodes.back().position
              + (path.nodes.back().position - path.nodes[path.nodes.size() - 2u].position);
      path.nodes.push_back(PathNode{position});
    }
    path.nodes.resize(nodeCount);
    return true;
  });
}
} // namespace tb::mdl
