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

  REQUIRE(setPathNodeCount(map, *node, 4));
  auto resized = readPath(node->entity()).value();
  REQUIRE(resized.nodes.size() == 4u);
  CHECK(resized.nodes[2].position == vm::vec3d{128, 0, 0});
  CHECK(resized.nodes[3].position == vm::vec3d{192, 0, 0});
  map.undoCommand();
  CHECK(readPath(node->entity()).value().nodes.size() == 2u);
  map.redoCommand();
  CHECK(readPath(node->entity()).value().nodes.size() == 4u);
  REQUIRE(setPathNodeCount(map, *node, 3));
  CHECK(readPath(node->entity()).value().nodes.size() == 3u);
  CHECK_FALSE(node->entity().hasProperty("point_3"));
  CHECK_FALSE(setPathNodeCount(map, *node, 65537u));
}
} // namespace tb::mdl
