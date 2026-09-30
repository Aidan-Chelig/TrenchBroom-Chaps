/*
 Copyright (C) 2010 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "fs/TestEnvironment.h"
#include "mdl/BrushBuilder.h"
#include "mdl/BrushNode.h"
#include "mdl/CatchConfig.h"
#include "mdl/GameInfo.h"
#include "mdl/GroupNode.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Picking.h"
#include "mdl/Map_Selection.h"
#include "mdl/PickResult.h"
#include "mdl/Prefab.h"
#include "mdl/WorldNode.h"
#include "ui/CreateEntityTool.h"
#include "ui/MapDocument.h"
#include "ui/MapDocumentFixture.h"

#include "vm/bbox.h"
#include "vm/ray.h"

#include <catch2/catch_test_macros.hpp>

namespace tb::ui
{

TEST_CASE("CreateEntityTool")
{
  auto env = fs::TestEnvironment{};

  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  auto& map = document.map();
  map.setGamePath(env.dir());

  const auto builder = mdl::BrushBuilder{
    map.worldNode().mapFormat(),
    map.worldBounds(),
    map.gameInfo().gameConfig.faceAttribsConfig.defaultUvAttributes,
    map.gameInfo().gameConfig.faceAttribsConfig.defaultSurfaceAttributes};

  auto* floorNode = new mdl::BrushNode{
    builder.createCuboid(vm::bbox3d{{-512, -512, -16}, {512, 512, 0}}, "floor").value()};
  auto* crateNode = new mdl::BrushNode{
    builder.createCuboid(vm::bbox3d{{0, 0, 0}, {32, 32, 32}}, "crate").value()};
  addNodes(map, {{&mdl::parentForNodes(map), {floorNode, crateNode}}});

  selectNodes(map, {crateNode});
  auto* sourceGroupNode =
    mdl::createPrefabFromSelection(
      map, "Crate", mdl::prefabPath(mdl::prefabDirectory(map), "Crate"))
      .value();
  deselectAll(map);

  auto* layerNode = map.worldNode().defaultLayer();
  REQUIRE(layerNode->childCount() == 2u);

  auto tool = CreateEntityTool{document};

  SECTION("createPrefab")
  {
    REQUIRE(tool.createPrefab("Crate"));

    REQUIRE(map.selection().nodes.size() == 1u);
    auto* groupNode = dynamic_cast<mdl::GroupNode*>(map.selection().nodes.front());
    REQUIRE(groupNode != nullptr);
    CHECK(groupNode != sourceGroupNode);
    CHECK(groupNode->name() == "Crate");
    CHECK(layerNode->childCount() == 3u);

    SECTION("updateEntityPosition3D places it on the face under the cursor")
    {
      // straight down onto the floor, away from the source crate
      const auto pickRay = vm::ray3d{{256, 256, 512}, {0, 0, -1}};

      // After the first move, the ray passes through the placed prefab. The prefab must
      // not climb onto its own brushes when it is moved again.
      for (auto i = 0; i < 2; ++i)
      {
        auto pickResult = mdl::PickResult{};
        mdl::pick(map, pickRay, pickResult);
        tool.updateEntityPosition3D(pickRay, pickResult);
      }

      const auto& bounds = groupNode->logicalBounds();
      CHECK(bounds.min.z() == 0.0);
      CHECK(bounds.contains(vm::vec3d{256, 256, 16}));
      CHECK(sourceGroupNode->logicalBounds() == vm::bbox3d{{0, 0, 0}, {32, 32, 32}});

      tool.commitEntity();
    }

    SECTION("commitEntity keeps it as one undoable step")
    {
      tool.commitEntity();
      CHECK(layerNode->childCount() == 3u);

      map.undoCommand();
      CHECK(layerNode->childCount() == 2u);
    }

    SECTION("removeEntity discards it")
    {
      tool.removeEntity();
      CHECK(layerNode->childCount() == 2u);
    }
  }

  SECTION("createPrefab of a missing prefab")
  {
    const auto undoCommandName = *map.undoCommandName();

    CHECK(!tool.createPrefab("Missing"));
    CHECK(map.selection().nodes.empty());
    CHECK(layerNode->childCount() == 2u);
    CHECK(*map.undoCommandName() == undoCommandName);
  }
}

} // namespace tb::ui
