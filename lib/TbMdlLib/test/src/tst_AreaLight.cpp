/*
 Copyright (C) 2026 TrenchBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
*/

#include "mdl/AreaLight.h"
#include "mdl/CatchConfig.h"
#include "mdl/EditorContext.h"
#include "mdl/Entity.h"
#include "mdl/EntityNode.h"
#include "mdl/PickResult.h"

#include "vm/approx.h"
#include "vm/bbox.h"
#include "vm/ray.h"
#include "vm/vec.h"

#include <algorithm>

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
namespace
{
// A 64 x 32 panel high up, facing straight down: its width runs along Y and its
// height along X.
Entity ceilingPanel()
{
  return Entity{{
    {"classname", "area_light"},
    {"origin", "0 0 128"},
    {"angles", "90 0 0"},
    {"width", "64"},
    {"height", "32"},
  }};
}

bool containsPoint(const auto& points, const vm::vec3d& point)
{
  return std::ranges::any_of(
    points, [&](const auto& p) { return p == vm::approx{point}; });
}
} // namespace

TEST_CASE("AreaLight")
{
  SECTION("areaLight")
  {
    CHECK(areaLight(Entity{{{"classname", "image_projector"}}}) == std::nullopt);

    const auto light = areaLight(ceilingPanel());
    REQUIRE(light);
    CHECK(light->width == 64.0);
    CHECK(light->height == 32.0);

    // An unset color is white; 0-255 colors are scaled down.
    CHECK(light->color == RgbF{1.0f, 1.0f, 1.0f});
    const auto orange = areaLight(Entity{{
      {"classname", "area_light"},
      {"color", "255 127.5 0"},
    }});
    REQUIRE(orange);
    CHECK(orange->color == RgbF{1.0f, 0.5f, 0.0f});

    // Missing or broken sizes fall back to the game's defaults.
    const auto broken = areaLight(Entity{{
      {"classname", "area_light"},
      {"width", "-3"},
      {"height", "nonsense"},
    }});
    REQUIRE(broken);
    CHECK(broken->width == 64.0);
    CHECK(broken->height == 32.0);
  }

  SECTION("normal and corners")
  {
    const auto light = *areaLight(ceilingPanel());
    CHECK(light.center() == vm::approx{vm::vec3d{0, 0, 128}});
    CHECK(light.normal() == vm::approx{vm::vec3d{0, 0, -1}});

    const auto corners = light.corners();
    for (const auto& corner : {
           vm::vec3d{-16, -32, 128},
           vm::vec3d{16, -32, 128},
           vm::vec3d{16, 32, 128},
           vm::vec3d{-16, 32, 128},
         })
    {
      CHECK(containsPoint(corners, corner));
    }

    // Counterclockwise seen from below, the side it shines toward.
    const auto turn = vm::cross(corners[1] - corners[0], corners[2] - corners[1]);
    CHECK(vm::dot(turn, light.normal()) > 0.0);
  }

  SECTION("handlePosition")
  {
    const auto light = *areaLight(ceilingPanel());
    CHECK(light.handlePosition({1, 0}) == vm::approx{vm::vec3d{0, 32, 128}});
    CHECK(light.handlePosition({0, -1}) == vm::approx{vm::vec3d{-16, 0, 128}});
    CHECK(light.handlePosition({-1, 1}) == vm::approx{vm::vec3d{16, -32, 128}});

    CHECK(areaLightHandles().size() == 8u);
    CHECK(std::ranges::count_if(areaLightHandles(), &AreaLightHandle::isCorner) == 4);
  }

  SECTION("outlineVertices")
  {
    const auto light = *areaLight(ceilingPanel());
    const auto outline = light.outlineVertices();
    REQUIRE(outline.size() % 2 == 0u);
    REQUIRE(outline.size() <= 24u);

    // The arrow points down from the middle of the panel.
    CHECK(containsPoint(outline, vm::vec3d{0, 0, 128 - 32}));
  }

  SECTION("resized")
  {
    const auto light = *areaLight(ceilingPanel());

    SECTION("an edge handle keeps the opposite edge in place")
    {
      // Drag the +Y edge from 32 to 50: the panel would be 82 wide, snapped to 80.
      const auto shape = light.resized({1, 0}, {3, 50, 140}, false, 16.0);
      CHECK(shape.width == vm::approx{80.0});
      CHECK(shape.height == vm::approx{32.0});
      CHECK(shape.origin == vm::approx{vm::vec3d{0, 8, 128}});
    }

    SECTION("a corner handle resizes both ways")
    {
      // The corner at x -16, y -32 (local Z is world X here) dragged out to -24, -48.
      const auto shape = light.resized({-1, -1}, {-24, -48, 128}, false, 8.0);
      CHECK(shape.width == vm::approx{80.0});
      CHECK(shape.height == vm::approx{40.0});
      CHECK(shape.origin == vm::approx{vm::vec3d{-4, -8, 128}});
    }

    SECTION("a symmetric resize keeps the center in place")
    {
      const auto shape = light.resized({1, 0}, {0, 50, 128}, true, 16.0);
      CHECK(shape.width == vm::approx{96.0});
      CHECK(shape.origin == vm::approx{vm::vec3d{0, 0, 128}});
    }

    SECTION("without a snap size the handle lands where it is dragged")
    {
      const auto shape = light.resized({0, 1}, {21.5, 0, 128}, false, 0.0);
      CHECK(shape.height == vm::approx{37.5});
      CHECK(shape.origin == vm::approx{vm::vec3d{2.75, 0, 128}});
    }

    SECTION("dragging past the opposite edge leaves one snap step")
    {
      const auto shape = light.resized({1, 0}, {0, -100, 128}, false, 16.0);
      CHECK(shape.width == vm::approx{16.0});
      CHECK(shape.origin == vm::approx{vm::vec3d{0, -24, 128}});
    }
  }

  SECTION("entity node")
  {
    auto node = EntityNode{ceilingPanel()};

    // A thin slab around the panel, so it can be clicked from the side.
    CHECK(node.logicalBounds().min == vm::approx{vm::vec3d{-16, -32, 126}});
    CHECK(node.logicalBounds().max == vm::approx{vm::vec3d{16, 32, 130}});

    const auto edges = node.boundsEdgeVertices();
    CHECK(containsPoint(edges, vm::vec3d{16, 32, 128}));
    CHECK(containsPoint(edges, vm::vec3d{0, 0, 128 - 32}));

    auto fromBelow = PickResult{};
    node.pick(EditorContext{}, vm::ray3d{{10, 20, 0}, {0, 0, 1}}, fromBelow);
    REQUIRE(fromBelow.size() == 1u);
    CHECK(fromBelow.all().front().hitPoint() == vm::approx{vm::vec3d{10, 20, 126}});

    auto besidePanel = PickResult{};
    node.pick(EditorContext{}, vm::ray3d{{10, 40, 0}, {0, 0, 1}}, besidePanel);
    CHECK(besidePanel.empty());
  }
}

} // namespace tb::mdl
