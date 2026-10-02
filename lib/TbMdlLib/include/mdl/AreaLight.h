/*
 Copyright (C) 2026 TrenchBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
*/

#pragma once

#include "base/Color.h"

#include "vm/bbox.h"
#include "vm/mat.h"
#include "vm/vec.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace tb::mdl
{
class Entity;

/**
 * A handle on an area light's panel: a corner, or the middle of an edge. Each step is
 * -1, 0 or +1 along the panel's width (local Y) and height (local Z); (0, 0) is not a
 * handle.
 */
struct AreaLightHandle
{
  int widthStep = 0;
  int heightStep = 0;

  bool isCorner() const;

  friend bool operator==(const AreaLightHandle&, const AreaLightHandle&) = default;
};

/** An area light's size and where it sits, as a resize leaves it. */
struct AreaLightShape
{
  vm::vec3d origin;
  double width = 0.0;
  double height = 0.0;
};

/**
 * The glowing panel of an area_light point entity, as the game lights it. The panel
 * spans the entity's local Y (width) and Z (height) around its origin and shines along
 * local +X, all turned by the entity's rotation.
 */
struct AreaLight
{
  double width = 0.0;
  double height = 0.0;
  /** Panel space to world space. */
  vm::mat4x4d transformation;
  RgbF color;

  /** The flat panel in panel space. */
  vm::bbox3d localBounds() const;
  /** The panel thickened a little in panel space, so it can be clicked edge on. */
  vm::bbox3d pickBounds() const;

  vm::vec3d center() const;
  /** The direction the panel shines in, in world space. */
  vm::vec3d normal() const;
  /** The panel's corners in world space, counterclockwise seen from the front. */
  std::array<vm::vec3d, 4> corners() const;
  /** Where the given handle sits in world space. */
  vm::vec3d handlePosition(const AreaLightHandle& handle) const;
  /**
   * Line segments (pairs of world space points) that draw the panel: its edges, its
   * diagonals, and an arrow from its center along the direction it shines in.
   */
  std::vector<vm::vec3d> outlineVertices() const;

  /**
   * Resizes the panel so the given handle follows the given world space point, as far
   * as it moves along the panel. The opposite edge or corner stays put, or the center
   * if symmetric is set. Sizes are rounded to multiples of the given snap size (of half
   * of it from the center if symmetric), unless it is 0, and never shrink below one
   * snap step (or one unit).
   */
  AreaLightShape resized(
    const AreaLightHandle& handle,
    const vm::vec3d& point,
    bool symmetric,
    double snapSize) const;
};

inline constexpr auto AreaLightClassname = "area_light";

/** The four corner and four edge handles of every area light. */
const std::vector<AreaLightHandle>& areaLightHandles();

/** Returns the panel of an area_light point entity. */
std::optional<AreaLight> areaLight(const Entity& entity);

} // namespace tb::mdl
