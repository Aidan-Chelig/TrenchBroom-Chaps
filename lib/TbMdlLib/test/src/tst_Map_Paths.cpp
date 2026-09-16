#include "mdl/EntityNode.h"
#include "mdl/Map.h"
#include "mdl/MapFixture.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Paths.h"
#include "mdl/PathEntity.h"

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
TEST_CASE("Map_Paths")
{
  auto fixture = MapFixture{};
  auto& map = fixture.create();
  auto path = Path{};
  path.nodes = {PathNode{{0, 0, 0}}, PathNode{{64, 0, 0}}};
  auto entity = Entity{{{"classname", "path"}}};
  REQUIRE(writePath(entity, path));
  auto* node = new EntityNode{std::move(entity)};
  addNodes(map, {{&parentForNodes(map), {node}}});

  REQUIRE(setPathNodeRoll(map, *node, 0, 45.0));
  CHECK(node->entity().hasProperty("point_0_roll", "45"));
  map.undoCommand();
  CHECK_FALSE(node->entity().hasProperty("point_0_roll"));
  map.redoCommand();
  CHECK(node->entity().hasProperty("point_0_roll", "45"));
  REQUIRE(setPathNodeRoll(map, *node, 0, 0.0));
  CHECK_FALSE(node->entity().hasProperty("point_0_roll"));
  CHECK_FALSE(setPathNodeRoll(map, *node, 2, 20.0));
}
} // namespace tb::mdl
