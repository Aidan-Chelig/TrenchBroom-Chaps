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

#include "mdl/Path.h"

#include "vm/approx.h"
#include "vm/mat_ext.h"
#include "vm/vec_io.h"

#include <cmath>
#include <limits>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
namespace
{
PathNode node(const vm::vec3d& position)
{
  auto result = PathNode{};
  result.position = position;
  return result;
}
} // namespace

TEST_CASE("Path")
{
  SECTION("roll interpolates across the shortest angle")
  {
    auto path = Path{};
    path.nodes = {PathNode{{0, 0, 0}}, PathNode{{64, 0, 0}}};
    CHECK(path.sampleRoll(0.5) == 0.0);
    path.nodes[0].roll = 170.0;
    path.nodes[1].roll = -170.0;
    CHECK(path.sampleRoll(0.5) == 180.0);
    CHECK(path.sampleRoll(1.0) == 190.0);
    path.closed = true;
    CHECK(path.sampleRoll(0.75) == -180.0);
    CHECK(path.sampleRoll(1.0) == -190.0);
  }
  auto path =
    Path{PathKind::Linear, {node({0, 0, 0}), node({10, 0, 0}), node({10, 30, 0})}, false};

  SECTION("sample and tangent")
  {
    CHECK(path.sample(-1) == vm::vec3d{0, 0, 0});
    CHECK(path.sample(2) == vm::vec3d{10, 30, 0});
    CHECK(path.sample(0.25) == vm::approx{vm::vec3d{5, 0, 0}});
    CHECK(path.sample(0.5) == vm::vec3d{10, 0, 0});
    CHECK(path.tangent(0.75) == vm::approx{vm::vec3d{0, 1, 0}});
    CHECK_THROWS_AS(
      path.sample(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
  }

  SECTION("distance and closest point")
  {
    CHECK(path.length() == vm::approx{40.0});
    CHECK(path.sampleDistance(20) == vm::approx{vm::vec3d{10, 10, 0}});
    CHECK(path.sampleDistance(-1) == path.sample(0));
    CHECK(path.sampleDistance(100) == path.sample(1));
    CHECK(path.closestPoint({13, 12, 5}) == vm::approx{vm::vec3d{10, 12, 0}});
    CHECK(path.length(0) == vm::approx{40.0});
  }

  SECTION("Bezier uses explicit handles")
  {
    path.kind = PathKind::Bezier;
    path.nodes = {node({0, 0, 0}), node({8, 0, 0})};
    path.nodes[0].handleMode = PathHandleMode::Free;
    path.nodes[1].handleMode = PathHandleMode::Aligned;
    path.nodes[0].handleOut = vm::vec3d{0, 8, 0};
    path.nodes[1].handleIn = vm::vec3d{8, 8, 0};
    CHECK(path.sample(0.5) == vm::approx{vm::vec3d{4, 6, 0}});
    CHECK(path.tangent(0) == vm::approx{vm::vec3d{0, 1, 0}});
    CHECK(path.tangent(1) == vm::approx{vm::vec3d{0, -1, 0}});
    CHECK(path.length() > 8.0);
    CHECK(
      path.sampleDistance(path.length() / 2) == vm::approx{vm::vec3d{4, 6, 0}, 0.001});
  }

  SECTION("uniform Catmull-Rom interpolation and automatic Bezier handles")
  {
    path.kind = PathKind::CatmullRom;
    path.nodes = {node({0, 0, 0}), node({0, 8, 0}), node({8, 8, 0}), node({8, 0, 0})};
    CHECK(path.sample(1.0 / 3.0) == vm::approx{vm::vec3d{0, 8, 0}});
    CHECK(path.sample(0.5) == vm::approx{vm::vec3d{4, 9, 0}});
    const auto curve = path;
    path.kind = PathKind::Bezier;
    path.nodes[1].handleOut = vm::vec3d{100, 100, 100}; // ignored in Auto mode
    for (int i = 0; i <= 20; ++i)
    {
      CHECK(path.sample(double(i) / 20) == vm::approx{curve.sample(double(i) / 20)});
    }
  }

  SECTION("closed paths include the closing segment")
  {
    path.closed = true;
    CHECK(path.segmentCount() == 3u);
    CHECK(path.sample(1) == path.sample(0));
    CHECK(path.length() == vm::approx{40.0 + std::sqrt(1000.0)});
    path.kind = PathKind::CatmullRom;
    CHECK(path.tangent(0) == vm::approx{path.tangent(1)});
  }

  SECTION("empty single-node and coincident paths")
  {
    path.nodes.clear();
    CHECK(path.sample(0.5) == vm::vec3d{});
    CHECK(path.length() == 0.0);
    CHECK(path.sampleDistance(1) == vm::vec3d{});
    path.nodes = {node({1, 2, 3})};
    CHECK(path.sample(0.5) == vm::vec3d{1, 2, 3});
    CHECK(path.tangent(0.5) == vm::vec3d{});
    path.nodes.push_back(path.nodes.front());
    CHECK(path.length() == 0.0);
    CHECK(path.tangent(0.5) == vm::vec3d{});
    CHECK(path.sampleDistance(1) == vm::vec3d{1, 2, 3});
    CHECK(path.closestPoint({100, 100, 100}) == vm::vec3d{1, 2, 3});
  }

  SECTION("affine transforms include handles and automatically generated tangents")
  {
    path.kind = PathKind::Bezier;
    path.nodes[0].handleMode = PathHandleMode::Free;
    path.nodes[0].handleOut = vm::vec3d{3, 4, 5};
    const auto transform = vm::translation_matrix(vm::vec3d{10, 20, 30})
                           * vm::rotation_matrix(0.0, 0.0, 1.0)
                           * vm::scaling_matrix(vm::vec3d{2, 3, -4});
    const auto transformed = path.transformed(transform);
    for (int i = 0; i <= 20; ++i)
    {
      CHECK(
        transformed.sample(double(i) / 20)
        == vm::approx{transform * path.sample(double(i) / 20)});
    }
    CHECK(
      *transformed.nodes[0].handleOut
      == vm::approx{transform * *path.nodes[0].handleOut});
    CHECK(path.nodes[0].position == vm::vec3d{});
  }
}
} // namespace tb::mdl
