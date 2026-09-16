/*
 Copyright (C) 2010 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "TestParserStatus.h"
#include "mdl/CatchConfig.h"
#include "mdl/Entity.h"
#include "mdl/EntityNode.h"
#include "mdl/LayerNode.h"
#include "mdl/MapFormat.h"
#include "mdl/NodeReader.h"
#include "mdl/NodeWriter.h"
#include "mdl/PathEntity.h"
#include "mdl/WorldNode.h"

#include "kd/task_manager.h"

#include "vm/approx.h"

#include <limits>
#include <memory>
#include <sstream>

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
TEST_CASE("PathEntity")
{
  auto entity = Entity{
    {{"classname", "camera_path"},
     {"targetname", "intro"},
     {"custom", "preserved"},
     {"point_custom", "also preserved"}}};
  auto path = Path{};
  path.kind = PathKind::Bezier;
  path.closed = true;
  path.nodes = {
    PathNode{
      {0.12345678901234567, 0, 64},
      std::nullopt,
      vm::vec3d{32, 0, 64},
      PathHandleMode::Free},
    PathNode{
      {128, 64, 96}, vm::vec3d{96, 64, 96}, std::nullopt, PathHandleMode::Aligned}};

  SECTION("write and read preserve the path and unrelated properties")
  {
    REQUIRE(writePath(entity, path));
    const auto result = readPath(entity);
    REQUIRE(static_cast<bool>(result));
    const auto& actual = result.value();
    CHECK(actual.kind == path.kind);
    CHECK(actual.closed);
    REQUIRE(actual.nodes.size() == 2u);
    CHECK(actual.nodes[0].position == path.nodes[0].position);
    CHECK(actual.nodes[0].handleOut == path.nodes[0].handleOut);
    CHECK(actual.nodes[1].handleIn == path.nodes[1].handleIn);
    CHECK(actual.nodes[1].handleMode == PathHandleMode::Aligned);
    CHECK(entity.hasProperty("targetname", "intro"));
    CHECK(entity.hasProperty("point_custom", "also preserved"));
    path.nodes.resize(1);
    path.nodes[0].handleOut.reset();
    REQUIRE(writePath(entity, path));
    CHECK_FALSE(entity.hasProperty("point_1"));
    CHECK_FALSE(entity.hasProperty("point_1_in"));
    CHECK_FALSE(entity.hasProperty("point_0_out"));
  }

  SECTION("invalid input is rejected")
  {
    REQUIRE(writePath(entity, path));
    SECTION("future version")
    {
      entity.addOrUpdateProperty("path_version", "2");
    }
    SECTION("missing version")
    {
      entity.removeProperty("path_version");
    }
    SECTION("missing point")
    {
      entity.removeProperty("point_1");
    }
    SECTION("invalid coordinate")
    {
      entity.addOrUpdateProperty("point_0", "0 1 nan");
    }
    SECTION("extra coordinate")
    {
      entity.addOrUpdateProperty("point_0", "0 1 2 3");
    }
    SECTION("invalid handle")
    {
      entity.addOrUpdateProperty("point_0_in", "not a vector");
    }
    SECTION("unknown mode")
    {
      entity.addOrUpdateProperty("point_0_mode", "mystery");
    }
    SECTION("invalid count")
    {
      entity.addOrUpdateProperty("point_count", "-1");
    }
    SECTION("huge count")
    {
      entity.addOrUpdateProperty("point_count", "999999999");
    }
    SECTION("extra point")
    {
      entity.addOrUpdateProperty("point_2", "0 0 0");
    }
    SECTION("ambiguous index")
    {
      entity.addOrUpdateProperty("point_01", "0 0 0");
    }
    SECTION("unknown kind")
    {
      entity.addOrUpdateProperty("path_type", "mystery");
    }
    SECTION("invalid closed")
    {
      entity.addOrUpdateProperty("closed", "yes");
    }
    CHECK(readPath(entity).is_error());
  }

  SECTION("all path kinds support open empty paths and default automatic handles")
  {
    for (const auto kind : {PathKind::Linear, PathKind::CatmullRom, PathKind::Bezier})
    {
      path.kind = kind;
      path.closed = false;
      path.nodes.clear();
      REQUIRE(writePath(entity, path));
      auto result = readPath(entity);
      REQUIRE(static_cast<bool>(result));
      CHECK(result.value().kind == kind);
      CHECK_FALSE(result.value().closed);
      CHECK(result.value().nodes.empty());

      path.nodes.push_back(PathNode{});
      REQUIRE(writePath(entity, path));
      entity.removeProperty("point_0_mode");
      result = readPath(entity);
      REQUIRE(static_cast<bool>(result));
      REQUIRE(result.value().nodes.size() == 1u);
      CHECK(result.value().nodes.front().handleMode == PathHandleMode::Auto);
    }
  }

  SECTION("boolean closed values are accepted")
  {
    REQUIRE(writePath(entity, path));
    entity.addOrUpdateProperty("closed", "true");
    auto result = readPath(entity);
    REQUIRE(static_cast<bool>(result));
    CHECK(result.value().closed);

    entity.addOrUpdateProperty("closed", "false");
    result = readPath(entity);
    REQUIRE(static_cast<bool>(result));
    CHECK_FALSE(result.value().closed);
  }

  SECTION("failed write leaves the entity untouched")
  {
    const auto previous = entity.properties();
    path.nodes[1].position[0] = std::numeric_limits<double>::infinity();
    CHECK(writePath(entity, path).is_error());
    CHECK(entity.properties() == previous);
  }

  SECTION("normal map writer and reader preserve path properties")
  {
    REQUIRE(writePath(entity, path));
    auto world = WorldNode{{}, {}, MapFormat::Standard};
    auto* node = new EntityNode{entity};
    world.defaultLayer()->addChild(node);
    auto stream = std::stringstream{};
    auto tasks = kdl::task_manager{};
    auto writer = NodeWriter{world, stream};
    writer.writeNodes({node}, tasks);
    auto status = TestParserStatus{};
    auto result = NodeReader::read(
      stream.str(), MapFormat::Standard, vm::bbox3d{8192.0}, {}, status, tasks);
    REQUIRE(static_cast<bool>(result));
    auto owned = std::vector<std::unique_ptr<Node>>{};
    for (auto* parsed : result.value())
    {
      owned.emplace_back(parsed);
    }
    REQUIRE(owned.size() == 1u);
    const auto* parsed = dynamic_cast<const EntityNode*>(owned[0].get());
    REQUIRE(parsed);
    const auto decoded = readPath(parsed->entity());
    REQUIRE(static_cast<bool>(decoded));
    CHECK(decoded.value().sample(0.25) == vm::approx{path.sample(0.25)});
    CHECK(parsed->entity().hasProperty("targetname", "intro"));
  }
}
} // namespace tb::mdl
