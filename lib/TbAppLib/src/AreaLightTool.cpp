/*
 Copyright (C) 2026 TrenchBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
*/

#include "ui/AreaLightTool.h"

#include "base/PreferenceManager.h"
#include "gl/Camera.h"
#include "mdl/ApplyAndSwap.h"
#include "mdl/Entity.h"
#include "mdl/EntityNode.h"
#include "mdl/Grid.h"
#include "mdl/Hit.h"
#include "mdl/HitFilter.h"
#include "mdl/Map.h"
#include "mdl/ModelUtils.h"
#include "mdl/PickResult.h"
#include "mdl/Selection.h"
#include "mdl/TransactionScope.h"
#include "prefs/Preferences.h"
#include "ui/MapDocument.h"

#include "kd/overload.h"
#include "kd/string_utils.h"

#include "vm/scalar.h"
#include "vm/vec.h"

#include <vector>

namespace tb::ui
{
namespace
{
std::string formatSize(const double size)
{
  return kdl::str_to_string(vm::correct(size, 3));
}

bool sameShape(const mdl::AreaLightShape& lhs, const mdl::AreaLightShape& rhs)
{
  return lhs.width == rhs.width && lhs.height == rhs.height
         && vm::is_equal(lhs.origin, rhs.origin, vm::Cd::almost_zero());
}
} // namespace

const mdl::HitType::Type AreaLightTool::HandleHitType = mdl::HitType::freeType();

AreaLightTool::AreaLightTool(MapDocument& document)
  : Tool{true}
  , m_document{document}
{
}

const mdl::Grid& AreaLightTool::grid() const
{
  return m_document.map().grid();
}

mdl::EntityNode* AreaLightTool::selectedLightNode() const
{
  const auto& selection = m_document.map().selection();
  if (selection.nodes.size() != 1 || selection.entities.size() != 1)
  {
    return nullptr;
  }

  auto* entityNode = selection.entities.front();
  return mdl::areaLight(entityNode->entity()) ? entityNode : nullptr;
}

std::optional<mdl::AreaLight> AreaLightTool::selectedLight() const
{
  if (const auto* entityNode = selectedLightNode())
  {
    return mdl::areaLight(entityNode->entity());
  }
  return std::nullopt;
}

bool AreaLightTool::applies() const
{
  return selectedLightNode() != nullptr;
}

void AreaLightTool::pick(
  const vm::ray3d& pickRay, const gl::Camera& camera, mdl::PickResult& pickResult) const
{
  const auto light = selectedLight();
  if (!light)
  {
    return;
  }

  const auto radius = double(pref(Preferences::HandleRadius));
  for (const auto& handle : mdl::areaLightHandles())
  {
    if (
      const auto distance =
        camera.pickPointHandle(pickRay, light->handlePosition(handle), radius))
    {
      pickResult.addHit(mdl::Hit{
        HandleHitType, *distance, vm::point_at_distance(pickRay, *distance), handle});
    }
  }
}

const std::optional<mdl::AreaLightHandle>& AreaLightTool::hoveredHandle() const
{
  return m_hoveredHandle;
}

void AreaLightTool::updateHoveredHandle(const mdl::PickResult& pickResult)
{
  const auto& hit = pickResult.first(mdl::HitFilters::type(HandleHitType));
  const auto handle =
    hit.isMatch() ? std::optional{hit.target<mdl::AreaLightHandle>()} : std::nullopt;
  if (handle != m_hoveredHandle)
  {
    m_hoveredHandle = handle;
    refreshViews();
  }
}

const std::optional<mdl::AreaLightHandle>& AreaLightTool::dragHandle() const
{
  return m_dragHandle;
}

bool AreaLightTool::resizing() const
{
  return m_dragHandle.has_value();
}

const mdl::AreaLightShape& AreaLightTool::shape() const
{
  return m_shape;
}

bool AreaLightTool::startResize(const mdl::AreaLightHandle& handle)
{
  const auto light = selectedLight();
  if (!light || resizing())
  {
    return false;
  }

  m_lightAtDragStart = light;
  m_dragHandle = handle;
  m_shape = mdl::AreaLightShape{light->center(), light->width, light->height};
  m_document.map().startTransaction(
    "Resize Area Light", mdl::TransactionScope::LongRunning);
  return true;
}

void AreaLightTool::resize(const vm::vec3d& handlePosition, const bool symmetric)
{
  auto* entityNode = selectedLightNode();
  if (!entityNode || !m_dragHandle || !m_lightAtDragStart)
  {
    return;
  }

  const auto& grid = this->grid();
  const auto snapSize = grid.snap() ? grid.actualSize() : 0.0;
  const auto shape =
    m_lightAtDragStart->resized(*m_dragHandle, handlePosition, symmetric, snapSize);
  if (sameShape(shape, m_shape))
  {
    return;
  }
  m_shape = shape;

  auto& map = m_document.map();
  map.rollbackTransaction();

  const auto nodes = std::vector<mdl::Node*>{entityNode};
  mdl::applyAndSwap(
    map,
    "Resize Area Light",
    nodes,
    mdl::collectContainingGroups(nodes),
    kdl::overload(
      [](mdl::Layer&) { return true; },
      [](mdl::Group&) { return true; },
      [&](mdl::Entity& entity) {
        entity.addOrUpdateProperty("width", formatSize(shape.width));
        entity.addOrUpdateProperty("height", formatSize(shape.height));
        entity.setOrigin(shape.origin);
        return true;
      },
      [](mdl::Brush&) { return true; },
      [](mdl::BezierPatch&) { return true; }));
}

void AreaLightTool::commitResize()
{
  auto& map = m_document.map();
  const auto atStart = m_lightAtDragStart;
  if (
    atStart
    && sameShape(
      m_shape, mdl::AreaLightShape{atStart->center(), atStart->width, atStart->height}))
  {
    map.cancelTransaction();
  }
  else
  {
    map.commitTransaction();
  }

  m_dragHandle = std::nullopt;
  m_lightAtDragStart = std::nullopt;
}

void AreaLightTool::cancelResize()
{
  m_document.map().cancelTransaction();
  m_dragHandle = std::nullopt;
  m_lightAtDragStart = std::nullopt;
}

} // namespace tb::ui
