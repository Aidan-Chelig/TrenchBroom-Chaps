/*
 Copyright (C) 2026 TrenchBroom contributors

 This file is part of TrenchBroom.
 TrenchBroom is free software: you can redistribute it and/or modify it under the
 terms of the GNU General Public License as published by the Free Software
 Foundation, either version 3 of the License, or (at your option) any later version.
 TrenchBroom is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 PARTICULAR PURPOSE. See the GNU General Public License for more details.
 You should have received a copy of the GNU General Public License along with
 TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mdl/Entity.h"
#include "mdl/PathEntity.h"
#include "render/PathRenderData.h"

#include "vm/approx.h"
#include "vm/vec_io.h"

#include <catch2/catch_test_macros.hpp>

namespace tb::render
{
TEST_CASE("PathRenderData")
{
  auto entity = mdl::Entity{{
    {"classname", "camera_route"},
    {"origin", "999 999 999"},
    {"angles", "0 90 0"},
  }};
  auto path = mdl::Path{};
  path.nodes = {
    mdl::PathNode{{0, 0, 0}},
    mdl::PathNode{{64, 64, 0}},
    mdl::PathNode{{128, 0, 0}},
  };

  SECTION("ordinary and invalid entities do not generate geometry")
  {
    CHECK_FALSE(makePathRenderData(entity).has_value());
    REQUIRE(mdl::writePath(entity, path));
    entity.addOrUpdateProperty("point_1", "invalid");
    CHECK_FALSE(makePathRenderData(entity).has_value());
    REQUIRE(mdl::writePath(entity, path));
    entity.addOrUpdateProperty("path_version", "2");
    CHECK_FALSE(makePathRenderData(entity).has_value());
  }

  SECTION("curves transform local coordinates and update when properties change")
  {
    REQUIRE(mdl::writePath(entity, path));
    const auto geometry = makePathRenderData(entity);
    REQUIRE(geometry.has_value());
    CHECK(geometry->nodes.front() == vm::vec3f{999, 999, 999});
    CHECK(geometry->nodes[1] == vm::vec3f{935, 1063, 999});
    CHECK(geometry->curve.front() == geometry->nodes.front());
    CHECK(geometry->curve.back() == geometry->nodes.back());
    CHECK(geometry->curve[16] == vm::approx{vm::vec3f{963, 1027, 999}});
    CHECK(geometry->handles.empty());
    CHECK_FALSE(geometry->arrows.empty());

    entity.addOrUpdateProperty("point_2", "256 0 0");
    const auto changed = makePathRenderData(entity);
    REQUIRE(changed.has_value());
    CHECK(changed->curve.back() == vm::vec3f{999, 1255, 999});
  }

  SECTION("closed linear paths include the closing edge")
  {
    path.kind = mdl::PathKind::Linear;
    path.closed = true;
    REQUIRE(mdl::writePath(entity, path));
    const auto geometry = makePathRenderData(entity);
    REQUIRE(geometry.has_value());
    CHECK(geometry->closed);
    REQUIRE(geometry->curve.size() == 4u);
    CHECK(geometry->curve.front() == geometry->curve.back());
    CHECK(geometry->handles.empty());
  }

  SECTION("Bezier handles include generated Auto and explicit Free controls")
  {
    path.kind = mdl::PathKind::Bezier;
    path.nodes[0].handleMode = mdl::PathHandleMode::Free;
    path.nodes[0].handleOut = vm::vec3d{0, 64, 32};
    REQUIRE(mdl::writePath(entity, path));
    const auto geometry = makePathRenderData(entity);
    REQUIRE(geometry.has_value());
    REQUIRE(geometry->handles.size() == 4u);
    CHECK(geometry->handles[0] == vm::vec3f{935, 999, 1031});
    CHECK(
      geometry->handles[1]
      == vm::approx{vm::vec3f{935, 999 + 64.0f - 128.0f / 6.0f, 999}});
    CHECK(geometry->handleLines[0] == geometry->nodes[0]);
    CHECK(geometry->handleLines[1] == geometry->handles[0]);
    CHECK(geometry->curve[16] == vm::approx{vm::vec3f{943, 1023, 1011}});
  }

  SECTION("empty and coincident paths produce no invalid arrows")
  {
    path.nodes.clear();
    REQUIRE(mdl::writePath(entity, path));
    auto geometry = makePathRenderData(entity);
    REQUIRE(geometry.has_value());
    CHECK(geometry->curve.empty());
    CHECK(geometry->nodes.empty());
    path.nodes = {mdl::PathNode{}, mdl::PathNode{}};
    REQUIRE(mdl::writePath(entity, path));
    geometry = makePathRenderData(entity);
    REQUIRE(geometry.has_value());
    CHECK(geometry->arrows.empty());
  }
}
} // namespace tb::render
