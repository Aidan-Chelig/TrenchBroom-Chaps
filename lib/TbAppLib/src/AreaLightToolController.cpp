/*
 Copyright (C) 2026 TrenchBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
*/

#include "ui/AreaLightToolController.h"

#include "base/PreferenceManager.h"
#include "gl/Camera.h"
#include "mdl/AreaLight.h"
#include "mdl/Hit.h"
#include "mdl/HitFilter.h"
#include "mdl/PickResult.h"
#include "prefs/Preferences.h"
#include "render/RenderContext.h"
#include "render/RenderService.h"
#include "ui/AreaLightTool.h"
#include "ui/HandleDragTracker.h"
#include "ui/InputState.h"

#include "kd/ranges/to.h"

#include "vm/line.h"
#include "vm/mat_ext.h"
#include "vm/plane.h"
#include "vm/vec.h"

#include <fmt/format.h>

#include <cmath>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace tb::ui
{
namespace
{
bool symmetric(const InputState& inputState)
{
  return inputState.modifierKeysDown(ModifierKeys::Alt);
}

class AreaLightDragDelegate : public HandleDragTrackerDelegate
{
private:
  AreaLightTool& m_tool;
  mdl::AreaLight m_light;
  mdl::AreaLightHandle m_handle;

public:
  AreaLightDragDelegate(
    AreaLightTool& tool, mdl::AreaLight light, const mdl::AreaLightHandle& handle)
    : m_tool{tool}
    , m_light{std::move(light)}
    , m_handle{handle}
  {
  }

  HandlePositionProposer start(
    const InputState& inputState,
    const vm::vec3d& initialHandlePosition,
    const vm::vec3d& handleOffset) override
  {
    return makeProposer(inputState, initialHandlePosition, handleOffset);
  }

  std::optional<UpdateDragConfig> modifierKeyChange(
    const InputState& inputState, const DragState& dragState) override
  {
    // Alt switches between resizing from the opposite side and from the center.
    return UpdateDragConfig{
      makeProposer(inputState, dragState.initialHandlePosition, dragState.handleOffset),
      ResetInitialHandlePosition::Keep};
  }

  DragStatus update(
    const InputState& inputState,
    const DragState&,
    const vm::vec3d& proposedHandlePosition) override
  {
    if (!m_tool.applies())
    {
      return DragStatus::End;
    }
    m_tool.resize(proposedHandlePosition, symmetric(inputState));
    return DragStatus::Continue;
  }

  void end(const InputState& inputState, const DragState&) override
  {
    m_tool.commitResize();
    m_tool.updateHoveredHandle(inputState.pickResult());
  }

  void cancel(const DragState&) override { m_tool.cancelResize(); }

private:
  /**
   * An edge handle slides along the panel's width or height. A corner handle moves in
   * the panel's plane, or in the view's plane when the panel is seen nearly edge on.
   */
  HandlePositionProposer makeProposer(
    const InputState& inputState,
    const vm::vec3d& handlePosition,
    const vm::vec3d& handleOffset) const
  {
    const auto viewDirection = vm::vec3d{inputState.camera().direction()};
    const auto viewPlane = vm::plane3d{handlePosition, -viewDirection};

    if (!m_handle.isCorner())
    {
      const auto localAxis =
        m_handle.widthStep != 0 ? vm::vec3d{0, 1, 0} : vm::vec3d{0, 0, 1};
      const auto axis =
        vm::normalize(vm::strip_translation(m_light.transformation) * localAxis);
      if (std::abs(vm::dot(axis, viewDirection)) < 0.98)
      {
        return makeHandlePositionProposer(
          makeLineHandlePicker(vm::line3d{handlePosition, axis}, handleOffset),
          makeIdentityHandleSnapper());
      }
      return makeHandlePositionProposer(
        makePlaneHandlePicker(viewPlane, handleOffset), makeIdentityHandleSnapper());
    }

    const auto normal = m_light.normal();
    const auto plane = std::abs(vm::dot(normal, viewDirection)) > 0.2
                         ? vm::plane3d{handlePosition, normal}
                         : viewPlane;
    return makeHandlePositionProposer(
      makePlaneHandlePicker(plane, handleOffset), makeIdentityHandleSnapper());
  }
};

std::string sizeLabel(const double width, const double height)
{
  return fmt::format("{:g} x {:g}", width, height);
}
} // namespace

AreaLightToolController::AreaLightToolController(AreaLightTool& tool)
  : m_tool{tool}
{
}

AreaLightToolController::~AreaLightToolController() = default;

Tool& AreaLightToolController::tool()
{
  return m_tool;
}

const Tool& AreaLightToolController::tool() const
{
  return m_tool;
}

void AreaLightToolController::pick(
  const InputState& inputState, mdl::PickResult& pickResult)
{
  m_tool.pick(inputState.pickRay(), inputState.camera(), pickResult);
}

void AreaLightToolController::mouseMove(const InputState& inputState)
{
  if (!inputState.anyToolDragging())
  {
    m_tool.updateHoveredHandle(inputState.pickResult());
  }
}

std::unique_ptr<GestureTracker> AreaLightToolController::acceptMouseDrag(
  const InputState& inputState)
{
  using namespace mdl::HitFilters;

  if (
    !inputState.mouseButtonsPressed(MouseButtons::Left)
    || !(
      inputState.modifierKeysPressed(ModifierKeys::None)
      || inputState.modifierKeysPressed(ModifierKeys::Alt)))
  {
    return nullptr;
  }

  const auto& hit = inputState.pickResult().first(type(AreaLightTool::HandleHitType));
  if (!hit.isMatch())
  {
    return nullptr;
  }

  const auto light = m_tool.selectedLight();
  const auto handle = hit.target<mdl::AreaLightHandle>();
  if (!light || !m_tool.startResize(handle))
  {
    return nullptr;
  }

  return createHandleDragTracker(
    AreaLightDragDelegate{m_tool, *light, handle},
    inputState,
    light->handlePosition(handle),
    hit.hitPoint());
}

void AreaLightToolController::render(
  const InputState&,
  render::RenderContext& renderContext,
  render::RenderBatch& renderBatch)
{
  const auto light = m_tool.selectedLight();
  if (!light)
  {
    return;
  }

  const auto active = m_tool.dragHandle() ? m_tool.dragHandle() : m_tool.hoveredHandle();

  // The panel's outline, bold while a handle is under the mouse or being dragged.
  if (active)
  {
    auto renderService = render::RenderService{renderContext, renderBatch};
    renderService.setShowOccludedObjects();
    renderService.setForegroundColor(pref(Preferences::ScaleOutlineColor));
    renderService.setLineWidth(2.0f);
    const auto corners = light->corners();
    renderService.renderPolygonOutline(
      corners | std::views::transform([](const auto& c) { return vm::vec3f{c}; })
      | kdl::ranges::to<std::vector>());
  }

  {
    auto renderService = render::RenderService{renderContext, renderBatch};
    renderService.setShowOccludedObjects();
    renderService.setForegroundColor(pref(Preferences::ScaleHandleColor));
    for (const auto& handle : mdl::areaLightHandles())
    {
      renderService.renderHandle(vm::vec3f{light->handlePosition(handle)});
    }

    if (active)
    {
      renderService.setForegroundColor(pref(Preferences::SelectionBoundsColor));
      renderService.renderHandleHighlight(vm::vec3f{light->handlePosition(*active)});
    }
  }

  // While resizing, show the panel's size next to it.
  if (m_tool.resizing())
  {
    auto renderService = render::RenderService{renderContext, renderBatch};
    renderService.setForegroundColor(pref(Preferences::InfoOverlayTextColor));
    renderService.setBackgroundColor(pref(Preferences::InfoOverlayBackgroundColor));
    renderService.setShowOccludedObjects();
    renderService.renderString(
      sizeLabel(light->width, light->height), vm::vec3f{light->center()});
  }
}

bool AreaLightToolController::cancel()
{
  return false;
}

} // namespace tb::ui
