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

#pragma once

#include "vm/mat.h"
#include "vm/vec.h"

#include <array>
#include <optional>
#include <vector>

namespace tb::mdl
{

enum class PathKind
{
  Linear,
  CatmullRom,
  Bezier
};
enum class PathHandleMode
{
  Auto,
  Aligned,
  Free
};

struct PathNode
{
  vm::vec3d position;
  std::optional<vm::vec3d> handleIn = std::nullopt;
  std::optional<vm::vec3d> handleOut = std::nullopt;
  PathHandleMode handleMode = PathHandleMode::Auto;
};

// Positions and handles are absolute coordinates in the same space. Auto handles
// use uniform Catmull-Rom tangents; missing manual handles coincide with the node.
struct Path
{
  PathKind kind = PathKind::CatmullRom;
  std::vector<PathNode> nodes;
  bool closed = false;

  size_t segmentCount() const;
  // Cubic Bezier controls for any segment, including generated Auto handles.
  std::array<vm::vec3d, 4> segmentControls(size_t segment) const;
  // Clamp t to [0, 1]. Empty paths return the zero vector; a one-node path is
  // constant. tangent returns a unit vector, or zero for a stationary point.
  vm::vec3d sample(double t) const;
  vm::vec3d tangent(double t) const;

  // Piecewise-linear approximations with samplesPerSegment subdivisions per
  // segment (minimum 1). Distances are clamped, even for closed paths.
  double length(size_t samplesPerSegment = 64) const;
  vm::vec3d sampleDistance(double distance, size_t samplesPerSegment = 64) const;
  vm::vec3d closestPoint(const vm::vec3d& position, size_t samplesPerSegment = 64) const;

  Path transformed(const vm::mat4x4d& transformation) const;
};

} // namespace tb::mdl
