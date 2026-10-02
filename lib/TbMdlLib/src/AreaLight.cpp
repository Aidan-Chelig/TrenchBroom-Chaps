/*
 Copyright (C) 2026 TrenchBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
*/

#include "mdl/AreaLight.h"

#include "mdl/Entity.h"

#include "kd/string_utils.h"

#include "vm/mat.h"
#include "vm/mat_ext.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <utility>

namespace tb::mdl
{
namespace
{
// The game's defaults, for an entity that doesn't set its size.
constexpr auto DefaultWidth = 64.0;
constexpr auto DefaultHeight = 32.0;
// How far the clickable panel reaches out of its plane on either side.
constexpr auto PickHalfThickness = 2.0;

double dimension(const Entity& entity, const std::string& key, const double fallback)
{
  const auto* value = entity.property(key);
  const auto parsed = value ? kdl::str_to_double(*value) : std::nullopt;
  return parsed && std::isfinite(*parsed) && *parsed > 0.0 ? *parsed : fallback;
}

/** Reads a color written either as 0-1 floats or as 0-255 bytes. White if unreadable. */
RgbF color(const Entity& entity, const std::string& key)
{
  const auto* value = entity.property(key);
  if (!value)
  {
    return RgbF{1.0f, 1.0f, 1.0f};
  }

  auto components = std::vector<double>{};
  for (const auto& part : kdl::str_split(*value, " "))
  {
    if (const auto number = kdl::str_to_double(part); number && std::isfinite(*number))
    {
      components.push_back(*number);
    }
  }
  if (components.size() < 3)
  {
    return RgbF{1.0f, 1.0f, 1.0f};
  }

  const auto scale =
    std::ranges::any_of(components | std::views::take(3), [](auto c) { return c > 1.0; })
      ? 1.0 / 255.0
      : 1.0;
  const auto channel = [&](const size_t i) {
    return float(std::clamp(components[i] * scale, 0.0, 1.0));
  };
  return RgbF{channel(0), channel(1), channel(2)};
}

/**
 * The panel's new extent along one axis, and where its middle moves to along it, when
 * the handle on side `step` (-1 or +1) of an extent `size` is dragged to `local`.
 */
std::pair<double, double> resizedAxis(
  const int step,
  const double size,
  const double local,
  const bool symmetric,
  const double snapSize)
{
  if (step == 0)
  {
    return {size, 0.0};
  }

  const auto minSize = snapSize > 0.0 ? snapSize : 1.0;
  if (symmetric)
  {
    auto half = step * local;
    if (snapSize > 0.0)
    {
      half = std::round(half / snapSize) * snapSize;
    }
    return {std::max(2.0 * half, minSize), 0.0};
  }

  // The opposite edge stays where it is.
  const auto fixed = -step * size / 2.0;
  auto newSize = step * (local - fixed);
  if (snapSize > 0.0)
  {
    newSize = std::round(newSize / snapSize) * snapSize;
  }
  newSize = std::max(newSize, minSize);
  return {newSize, fixed + step * newSize / 2.0};
}

} // namespace

bool AreaLightHandle::isCorner() const
{
  return widthStep != 0 && heightStep != 0;
}

vm::bbox3d AreaLight::localBounds() const
{
  return vm::bbox3d{{0, -width / 2.0, -height / 2.0}, {0, width / 2.0, height / 2.0}};
}

vm::bbox3d AreaLight::pickBounds() const
{
  return vm::bbox3d{
    {-PickHalfThickness, -width / 2.0, -height / 2.0},
    {PickHalfThickness, width / 2.0, height / 2.0}};
}

vm::vec3d AreaLight::center() const
{
  return transformation * vm::vec3d{0, 0, 0};
}

vm::vec3d AreaLight::normal() const
{
  return vm::normalize(vm::strip_translation(transformation) * vm::vec3d{1, 0, 0});
}

std::array<vm::vec3d, 4> AreaLight::corners() const
{
  const auto w = width / 2.0;
  const auto h = height / 2.0;
  // Counterclockwise seen from +X, the side the panel shines toward.
  return {
    transformation * vm::vec3d{0, -w, -h},
    transformation * vm::vec3d{0, w, -h},
    transformation * vm::vec3d{0, w, h},
    transformation * vm::vec3d{0, -w, h},
  };
}

vm::vec3d AreaLight::handlePosition(const AreaLightHandle& handle) const
{
  return transformation
         * vm::vec3d{0, handle.widthStep * width / 2.0, handle.heightStep * height / 2.0};
}

std::vector<vm::vec3d> AreaLight::outlineVertices() const
{
  const auto [c0, c1, c2, c3] = corners();
  const auto arrowLength = std::clamp(std::max(width, height) / 2.0, 16.0, 64.0);
  const auto headLength = arrowLength * 0.3;
  const auto headWidth = arrowLength * 0.12;
  const auto local = [&](const double x, const double y, const double z) {
    return transformation * vm::vec3d{x, y, z};
  };

  const auto origin = center();
  const auto tip = local(arrowLength, 0, 0);
  const auto headBase = arrowLength - headLength;
  return {
    c0,     c1,
    c1,     c2,
    c2,     c3,
    c3,     c0,
    c0,     c2,
    c1,     c3,
    origin, tip,
    tip,    local(headBase, headWidth, 0),
    tip,    local(headBase, -headWidth, 0),
    tip,    local(headBase, 0, headWidth),
    tip,    local(headBase, 0, -headWidth),
  };
}

AreaLightShape AreaLight::resized(
  const AreaLightHandle& handle,
  const vm::vec3d& point,
  const bool symmetric,
  const double snapSize) const
{
  const auto inverse = vm::invert(transformation);
  if (!inverse)
  {
    return AreaLightShape{center(), width, height};
  }

  const auto local = *inverse * point;
  const auto [newWidth, middleY] =
    resizedAxis(handle.widthStep, width, local.y(), symmetric, snapSize);
  const auto [newHeight, middleZ] =
    resizedAxis(handle.heightStep, height, local.z(), symmetric, snapSize);
  return AreaLightShape{
    transformation * vm::vec3d{0, middleY, middleZ}, newWidth, newHeight};
}

const std::vector<AreaLightHandle>& areaLightHandles()
{
  static const auto handles = std::vector<AreaLightHandle>{
    {-1, -1},
    {1, -1},
    {1, 1},
    {-1, 1},
    {0, -1},
    {1, 0},
    {0, 1},
    {-1, 0},
  };
  return handles;
}

std::optional<AreaLight> areaLight(const Entity& entity)
{
  if (!entity.pointEntity() || entity.classname() != AreaLightClassname)
  {
    return std::nullopt;
  }

  return AreaLight{
    dimension(entity, "width", DefaultWidth),
    dimension(entity, "height", DefaultHeight),
    vm::translation_matrix(entity.origin()) * entity.rotation(),
    color(entity, "color"),
  };
}

} // namespace tb::mdl
