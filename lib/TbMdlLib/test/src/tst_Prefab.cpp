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
#include "mdl/BrushNode.h"
#include "mdl/CatchConfig.h"
#include "mdl/Entity.h"
#include "mdl/EntityNode.h"
#include "mdl/GameInfo.h"
#include "mdl/GroupNode.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/MapFixture.h"
#include "mdl/Map_Groups.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Selection.h"
#include "mdl/Prefab.h"
#include "mdl/TestFactory.h"
#include "mdl/WorldNode.h"

#include "kd/vector_utils.h"

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
namespace
{

std::vector<std::string> prefabNames(const std::vector<PrefabFile>& prefabs)
{
  auto result = std::vector<std::string>{};
  for (const auto& prefab : prefabs)
  {
    result.push_back(prefab.name);
  }
  return result;
}

std::vector<const EntityNode*> findEntityNodes(const GroupNode& groupNode)
{
  auto result = std::vector<const EntityNode*>{};
  for (const auto* child : groupNode.children())
  {
    if (const auto* entityNode = dynamic_cast<const EntityNode*>(child))
    {
      result.push_back(entityNode);
    }
  }
  return result;
}

} // namespace

TEST_CASE("Prefab")
{
  auto env = fs::TestEnvironment{};
  const auto directory = env.dir() / "prefabs";

  auto fixture = MapFixture{};

  SECTION("prefabDirectory")
  {
    auto& map = fixture.create();

    SECTION("Game path is set")
    {
      map.setGamePath(env.dir());
      CHECK(
        prefabDirectory(map)
        == env.dir() / map.gameInfo().gameConfig.fileSystemConfig.searchPath / "prefabs");
    }

    SECTION("Game path is empty")
    {
      map.setGamePath({});
      CHECK(
        prefabDirectory(map) == map.gameInfo().gameConfig.path.parent_path() / "prefabs");
    }
  }

  SECTION("prefabPath")
  {
    CHECK(prefabPath(directory, "Crate Stack") == directory / "Crate Stack.map");
  }

  SECTION("validatePrefabName")
  {
    CHECK(validatePrefabName("Crate Stack").is_success());
    CHECK(validatePrefabName("door_frame-2").is_success());

    CHECK(validatePrefabName("").is_error());
    CHECK(validatePrefabName("   ").is_error());
    CHECK(validatePrefabName("a/b").is_error());
    CHECK(validatePrefabName("a\\b").is_error());
    CHECK(validatePrefabName("a:b").is_error());
    CHECK(validatePrefabName("a\"b").is_error());
    CHECK(validatePrefabName("what?").is_error());
    CHECK(validatePrefabName("tab\there").is_error());
    CHECK(validatePrefabName("ends with a dot.").is_error());
    CHECK(validatePrefabName("ends with a space ").is_error());
    CHECK(validatePrefabName("con").is_error());
    CHECK(validatePrefabName("LPT1").is_error());
  }

  SECTION("findPrefabs")
  {
    SECTION("Missing directory")
    {
      CHECK(findPrefabs(directory).empty());
    }

    SECTION("Lists map files sorted by name")
    {
      env.createDirectory("prefabs");
      env.createFile("prefabs/crate.map", "");
      env.createFile("prefabs/Barrel.map", "");
      env.createFile("prefabs/door.MAP", "");
      env.createFile("prefabs/notes.txt", "");
      env.createFile("prefabs/crate.map.tmp", "");
      env.createDirectory("prefabs/folder.map");

      const auto prefabs = findPrefabs(directory);
      CHECK(prefabNames(prefabs) == std::vector<std::string>{"Barrel", "crate", "door"});
      CHECK(prefabs[1].path.filename() == "crate.map");
    }
  }

  SECTION("createPrefabFromSelection")
  {
    auto& map = fixture.create();

    auto* brushNode = createBrushNode(map);
    auto* entityNode = new EntityNode{Entity{{
      {"classname", "light"},
      {"origin", "64 0 0"},
      {"light", "300"},
    }}};
    addNodes(map, {{&parentForNodes(map), {brushNode, entityNode}}});

    const auto path = prefabPath(directory, "Lamp");

    SECTION("Groups the selection and writes it")
    {
      selectNodes(map, {brushNode, entityNode});

      const auto result = createPrefabFromSelection(map, "Lamp", path);
      REQUIRE(result.is_success());

      auto* groupNode = result.value();
      REQUIRE(groupNode != nullptr);
      CHECK(groupNode->name() == "Lamp");
      CHECK(groupNode->childCount() == 2u);
      CHECK(brushNode->parent() == groupNode);
      CHECK(entityNode->parent() == groupNode);
      CHECK(map.selection().nodes == std::vector<Node*>{groupNode});

      REQUIRE(env.fileExists("prefabs/Lamp.map"));
      CHECK(!env.fileExists("prefabs/Lamp.map.tmp"));

      const auto contents = env.loadFile("prefabs/Lamp.map");
      CHECK(contents.starts_with("// Game: Test"));
      CHECK(contents.find(R"("_tb_name" "Lamp")") != std::string::npos);
      CHECK(contents.find(R"("light" "300")") != std::string::npos);

      SECTION("Undo removes the group")
      {
        map.undoCommand();
        CHECK(brushNode->parent() == &parentForNodes(map));
        CHECK(entityNode->parent() == &parentForNodes(map));
      }
    }

    SECTION("Renames a single selected group")
    {
      selectNodes(map, {brushNode, entityNode});
      auto* groupNode = groupSelectedNodes(map, "Unnamed");

      const auto result = createPrefabFromSelection(map, "Lamp", path);
      REQUIRE(result.is_success());
      CHECK(result.value() == groupNode);
      CHECK(groupNode->name() == "Lamp");
      CHECK(groupNode->parent() == &parentForNodes(map));
    }

    SECTION("Replaces an existing prefab")
    {
      env.createDirectory("prefabs");
      env.createFile("prefabs/Lamp.map", "old contents");

      selectNodes(map, {entityNode});
      REQUIRE(createPrefabFromSelection(map, "Lamp", path).is_success());
      CHECK(env.loadFile("prefabs/Lamp.map").find("old contents") == std::string::npos);
    }

    SECTION("Nothing is selected")
    {
      CHECK(createPrefabFromSelection(map, "Lamp", path).is_error());
      CHECK(!env.fileExists("prefabs/Lamp.map"));
    }

    SECTION("File cannot be written")
    {
      // a file where the prefab directory should be
      env.createFile("prefabs", "");

      selectNodes(map, {brushNode, entityNode});
      CHECK(createPrefabFromSelection(map, "Lamp", path).is_error());

      // the grouping is undone
      CHECK(brushNode->parent() == &parentForNodes(map));
      CHECK(entityNode->parent() == &parentForNodes(map));
    }
  }

  SECTION("readPrefab and placePrefab")
  {
    auto& sourceMap = fixture.create();

    auto* brushNode = createBrushNode(sourceMap);
    auto* entityNode = new EntityNode{Entity{{
      {"classname", "light"},
      {"origin", "64 0 0"},
      {"light", "300"},
    }}};
    addNodes(sourceMap, {{&parentForNodes(sourceMap), {brushNode, entityNode}}});
    selectNodes(sourceMap, {brushNode, entityNode});

    const auto path = prefabPath(directory, "Lamp");
    auto* sourceGroupNode = createPrefabFromSelection(sourceMap, "Lamp", path).value();

    SECTION("readPrefab")
    {
      auto nodes = readPrefab(sourceMap, path).value();
      REQUIRE(nodes.size() == 1u);

      auto* groupNode = dynamic_cast<GroupNode*>(nodes.front());
      REQUIRE(groupNode != nullptr);
      CHECK(groupNode->name() == "Lamp");
      CHECK(groupNode->childCount() == 2u);

      // Never linked to the group the prefab was made from
      CHECK(groupNode->linkId() != sourceGroupNode->linkId());

      kdl::vec_clear_and_delete(nodes);
    }

    SECTION("readPrefab of a missing file")
    {
      CHECK(readPrefab(sourceMap, prefabPath(directory, "Missing")).is_error());
    }

    SECTION("placePrefab into the source map")
    {
      REQUIRE(placePrefab(sourceMap, path).is_success());

      REQUIRE(sourceMap.selection().nodes.size() == 1u);
      auto* placedGroupNode =
        dynamic_cast<GroupNode*>(sourceMap.selection().nodes.front());
      REQUIRE(placedGroupNode != nullptr);
      CHECK(placedGroupNode != sourceGroupNode);
      CHECK(placedGroupNode->name() == "Lamp");
      CHECK(placedGroupNode->linkId() != sourceGroupNode->linkId());
    }

    SECTION("placePrefab twice into another map")
    {
      auto otherFixture = MapFixture{};
      auto& otherMap = otherFixture.create();

      REQUIRE(placePrefab(otherMap, path).is_success());
      auto* firstGroupNode = dynamic_cast<GroupNode*>(otherMap.selection().nodes.front());

      REQUIRE(placePrefab(otherMap, path).is_success());
      auto* secondGroupNode =
        dynamic_cast<GroupNode*>(otherMap.selection().nodes.front());

      REQUIRE(firstGroupNode != nullptr);
      REQUIRE(secondGroupNode != nullptr);
      CHECK(firstGroupNode != secondGroupNode);

      // Independent copies, not linked duplicates
      CHECK(firstGroupNode->linkId() != secondGroupNode->linkId());

      const auto placedEntityNodes = findEntityNodes(*secondGroupNode);
      REQUIRE(placedEntityNodes.size() == 1u);
      CHECK(*placedEntityNodes.front()->entity().property("light") == "300");

      SECTION("Undo removes the placed prefab")
      {
        otherMap.undoCommand();
        CHECK(firstGroupNode->parent() == &parentForNodes(otherMap));
        CHECK(otherMap.worldNode().defaultLayer()->childCount() == 1u);
      }
    }
  }

  SECTION("renamePrefab")
  {
    env.createDirectory("prefabs");
    env.createFile("prefabs/crate.map", "crate");
    env.createFile("prefabs/barrel.map", "barrel");

    const auto cratePath = prefabPath(directory, "crate");

    SECTION("Renames the file")
    {
      const auto result = renamePrefab(cratePath, "Big Crate");
      REQUIRE(result.is_success());
      CHECK(result.value() == directory / "Big Crate.map");
      CHECK(env.loadFile("prefabs/Big Crate.map") == "crate");
      CHECK(!env.fileExists("prefabs/crate.map"));
    }

    SECTION("Changes only the case")
    {
      REQUIRE(renamePrefab(cratePath, "Crate").is_success());
      CHECK(
        prefabNames(findPrefabs(directory))
        == std::vector<std::string>{"barrel", "Crate"});
    }

    SECTION("Refuses to replace another prefab")
    {
      CHECK(renamePrefab(cratePath, "barrel").is_error());
      CHECK(env.loadFile("prefabs/barrel.map") == "barrel");
      CHECK(env.loadFile("prefabs/crate.map") == "crate");
    }

    SECTION("Refuses an invalid name")
    {
      CHECK(renamePrefab(cratePath, "a/b").is_error());
      CHECK(env.fileExists("prefabs/crate.map"));
    }
  }
}

} // namespace tb::mdl
