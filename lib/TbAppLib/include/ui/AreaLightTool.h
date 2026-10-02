/*
 Copyright (C) 2026 TrenchBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
*/

#pragma once

#include "mdl/AreaLight.h"
#include "mdl/HitType.h"
#include "ui/Tool.h"

#include "vm/ray.h"
#include "vm/vec.h"

#include <optional>

namespace tb
{
namespace gl
{
class Camera;
}

namespace mdl
{
class EntityNode;
class Grid;
class PickResult;
} // namespace mdl

namespace ui
{
class MapDocument;

/**
 * Resizes a selected area_light by dragging handles on the corners and edges of its
 * panel. It is always active, like the move tool, and only applies while a single
 * area light is selected.
 */
class AreaLightTool : public Tool
{
public:
  static const mdl::HitType::Type HandleHitType;

private:
  MapDocument& m_document;
  std::optional<mdl::AreaLightHandle> m_hoveredHandle;
  std::optional<mdl::AreaLightHandle> m_dragHandle;
  std::optional<mdl::AreaLight> m_lightAtDragStart;
  mdl::AreaLightShape m_shape;

public:
  explicit AreaLightTool(MapDocument& document);

  const mdl::Grid& grid() const;

  /** The selected area light's node, if it is the only selected node. */
  mdl::EntityNode* selectedLightNode() const;
  /** The selected area light's panel, if it is the only selected node. */
  std::optional<mdl::AreaLight> selectedLight() const;
  bool applies() const;

  void pick(
    const vm::ray3d& pickRay,
    const gl::Camera& camera,
    mdl::PickResult& pickResult) const;

  const std::optional<mdl::AreaLightHandle>& hoveredHandle() const;
  void updateHoveredHandle(const mdl::PickResult& pickResult);

  const std::optional<mdl::AreaLightHandle>& dragHandle() const;
  bool resizing() const;
  /** The size and origin the current drag gives the light. */
  const mdl::AreaLightShape& shape() const;

  bool startResize(const mdl::AreaLightHandle& handle);
  /**
   * Resizes the light so the dragged handle follows the given point. If symmetric is
   * set, the opposite side moves too and the center stays put.
   */
  void resize(const vm::vec3d& handlePosition, bool symmetric);
  void commitResize();
  void cancelResize();
};

} // namespace ui
} // namespace tb
