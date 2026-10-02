/*
 Copyright (C) 2026 TrenchBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
*/

#include "gl/PerspectiveCamera.h"
#include "mdl/CatchConfig.h"
#include "mdl/Entity.h"
#include "mdl/EntityNode.h"
#include "mdl/Hit.h"
#include "mdl/Map.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Selection.h"
#include "mdl/PickResult.h"
#include "ui/AreaLightTool.h"
#include "ui/AreaLightToolController.h"
#include "ui/GestureTracker.h"
#include "ui/InputState.h"
#include "ui/MapDocument.h"
#include "ui/MapDocumentFixture.h"
#include "ui/PickRequest.h"
#include "ui/ToolController.h"

#include "vm/approx.h"
#include "vm/ray.h"
#include "vm/vec.h"

#include <catch2/catch_test_macros.hpp>

namespace tb::ui
{
namespace
{
// A camera at the ray's origin, looking straight up.
gl::PerspectiveCamera cameraLookingUpFrom(const vm::vec3d& position)
{
  return gl::PerspectiveCamera{
    90.0f,
    1.0f,
    8192.0f,
    gl::Camera::Viewport{0, 0, 1024, 768},
    vm::vec3f{position},
    vm::vec3f{0, 0, 1},
    vm::vec3f{1, 0, 0}};
}
} // namespace

TEST_CASE("AreaLightTool")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  auto& map = document.map();

  // A 64 x 32 panel facing down: its width runs along Y.
  auto* lightNode = new mdl::EntityNode{mdl::Entity{{
    {"classname", "area_light"},
    {"origin", "0 0 128"},
    {"angles", "90 0 0"},
    {"width", "64"},
    {"height", "32"},
  }}};
  auto* otherNode = new mdl::EntityNode{mdl::Entity{{{"classname", "light_point"}}}};
  addNodes(map, {{&mdl::parentForNodes(map), {lightNode, otherNode}}});

  auto tool = AreaLightTool{document};
  const auto width = [&]() { return *lightNode->entity().property("width"); };
  const auto origin = [&]() { return lightNode->entity().origin(); };

  SECTION("applies")
  {
    CHECK(!tool.applies());

    selectNodes(map, {lightNode});
    CHECK(tool.applies());

    selectNodes(map, {otherNode});
    CHECK(!tool.applies());
  }

  SECTION("pick")
  {
    selectNodes(map, {lightNode});

    // From below, straight at the handle on the +Y edge.
    const auto pickRay = vm::ray3d{{0, 32, 0}, {0, 0, 1}};
    const auto camera = gl::PerspectiveCamera{
      90.0f,
      1.0f,
      8192.0f,
      gl::Camera::Viewport{0, 0, 1024, 768},
      vm::vec3f{pickRay.origin},
      vm::vec3f{pickRay.direction},
      vm::vec3f{1, 0, 0}};

    auto pickResult = mdl::PickResult{};
    tool.pick(pickRay, camera, pickResult);
    REQUIRE(!pickResult.empty());
    CHECK(
      pickResult.all().front().target<mdl::AreaLightHandle>()
      == mdl::AreaLightHandle{1, 0});
  }

  SECTION("resize")
  {
    selectNodes(map, {lightNode});
    REQUIRE(tool.startResize({1, 0}));

    // Each step replaces the last; the -Y edge stays at -32 and the width snaps to the
    // 16 unit grid.
    tool.resize({0, 50, 128}, false);
    CHECK(width() == "80");
    tool.resize({0, 70, 128}, false);
    CHECK(width() == "96");
    tool.commitResize();

    CHECK(width() == "96");
    CHECK(origin() == vm::approx{vm::vec3d{0, 16, 128}});
    CHECK(!tool.resizing());
    REQUIRE(map.undoCommandName());
    CHECK(*map.undoCommandName() == "Resize Area Light");

    map.undoCommand();
    CHECK(width() == "64");
    CHECK(origin() == vm::approx{vm::vec3d{0, 0, 128}});
  }

  SECTION("symmetric resize")
  {
    selectNodes(map, {lightNode});
    REQUIRE(tool.startResize({-1, 0}));
    tool.resize({0, -48, 128}, true);
    tool.commitResize();

    CHECK(width() == "96");
    CHECK(origin() == vm::approx{vm::vec3d{0, 0, 128}});
  }

  SECTION("cancelResize")
  {
    selectNodes(map, {lightNode});
    REQUIRE(tool.startResize({1, 0}));
    tool.resize({0, 70, 128}, false);
    tool.cancelResize();

    CHECK(width() == "64");
    CHECK(origin() == vm::approx{vm::vec3d{0, 0, 128}});
    // Nothing to undo but the selection.
    CHECK((!map.undoCommandName() || *map.undoCommandName() != "Resize Area Light"));
  }

  SECTION("controller")
  {
    auto areaLightController = AreaLightToolController{tool};
    auto& controller = static_cast<ToolController&>(areaLightController);
    selectNodes(map, {lightNode});

    // From below, the camera sends a ray up through the handle, then drags it to aim.
    const auto drag = [&](
                        const vm::vec3d& cameraPosition,
                        const vm::vec3d& aim,
                        const ModifierKeyState modifiers) {
      const auto camera = cameraLookingUpFrom(cameraPosition);
      auto inputState = InputState{0.0f, 0.0f};
      inputState.setPickRequest(
        PickRequest{vm::ray3d{cameraPosition, {0, 0, 1}}, camera});
      auto pickResult = mdl::PickResult{};
      controller.pick(inputState, pickResult);
      inputState.setPickResult(std::move(pickResult));
      inputState.setModifierKeys(modifiers);
      inputState.mouseDown(MouseButtons::Left);

      auto tracker = controller.acceptMouseDrag(inputState);
      REQUIRE(tracker != nullptr);

      auto dragState = InputState{0.0f, 0.0f};
      dragState.setPickRequest(PickRequest{
        vm::ray3d{cameraPosition, vm::normalize(aim - cameraPosition)}, camera});
      dragState.setModifierKeys(modifiers);
      dragState.mouseDown(MouseButtons::Left);
      CHECK(tracker->update(dragState));
      tracker->end(dragState);
    };

    SECTION("an edge handle slides along the panel")
    {
      drag({0, 32, 0}, {0, 64, 128}, ModifierKeys::None);
      CHECK(width() == "96");
      CHECK(origin() == vm::approx{vm::vec3d{0, 16, 128}});
    }

    SECTION("Alt resizes around the center")
    {
      drag({0, 32, 0}, {0, 48, 128}, ModifierKeys::Alt);
      CHECK(width() == "96");
      CHECK(origin() == vm::approx{vm::vec3d{0, 0, 128}});
    }

    SECTION("a corner handle moves in the panel's plane")
    {
      // The +Y, +X corner (local Z is world X here).
      drag({16, 32, 0}, {32, 48, 128}, ModifierKeys::None);
      CHECK(width() == "80");
      CHECK(*lightNode->entity().property("height") == "48");
      CHECK(origin() == vm::approx{vm::vec3d{8, 8, 128}});
    }

    SECTION("a drag that misses the handles is left to other tools")
    {
      const auto camera = cameraLookingUpFrom({0, 0, 0});
      auto inputState = InputState{0.0f, 0.0f};
      inputState.setPickRequest(PickRequest{vm::ray3d{{0, 0, 0}, {0, 0, 1}}, camera});
      auto pickResult = mdl::PickResult{};
      controller.pick(inputState, pickResult);
      inputState.setPickResult(std::move(pickResult));
      inputState.mouseDown(MouseButtons::Left);
      CHECK(controller.acceptMouseDrag(inputState) == nullptr);
    }
  }
}

} // namespace tb::ui
