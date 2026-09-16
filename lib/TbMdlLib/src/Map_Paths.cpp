#include "mdl/Map_Paths.h"

#include "mdl/ApplyAndSwap.h"
#include "mdl/Map_Groups.h"
#include "mdl/PathEntity.h"

#include "kd/overload.h"

#include <cmath>

namespace tb::mdl
{
bool setPathNodeRoll(
  Map& map, EntityNode& entityNode, const size_t nodeIndex, const double degrees)
{
  if (!std::isfinite(degrees))
  {
    return false;
  }
  const auto nodes = std::vector<EntityNode*>{&entityNode};
  return applyAndSwap(
    map,
    "Set Path Roll",
    nodes,
    collectContainingGroups(std::vector<Node*>{&entityNode}),
    kdl::overload(
      [&](Entity& entity) {
        const auto parsed = readPath(entity);
        if (!parsed || nodeIndex >= parsed.value().nodes.size())
        {
          return false;
        }
        auto path = parsed.value();
        path.nodes[nodeIndex].roll = degrees == 0.0 ? 0.0 : degrees;
        return writePath(entity, path).is_success();
      },
      [](Layer&) { return true; },
      [](Group&) { return true; },
      [](Brush&) { return true; },
      [](BezierPatch&) { return true; }));
}
} // namespace tb::mdl
