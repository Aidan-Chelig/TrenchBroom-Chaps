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

#include "render/PathRenderData.h"

#include "mdl/Entity.h"
#include "mdl/PathEntity.h"

#include "vm/mat_ext.h"

#include <algorithm>
#include <cmath>

namespace tb::render
{
std::optional<PathRenderData> makePathRenderData(const mdl::Entity& entity)
{
  if (!entity.hasProperty("path_version", "1"))
  {
    return std::nullopt;
  }
  const auto parsed = mdl::readPath(entity);
  if (parsed.is_error())
  {
    return std::nullopt;
  }
  // Path control points are authored in the entity's local space. This matches
  // normal entity transforms and the game runtime's map import convention.
  const auto transform = vm::translation_matrix(entity.origin()) * entity.rotation();
  const auto path = parsed.value().transformed(transform);
  auto result = PathRenderData{};
  result.closed = path.closed;
  for (const auto& node : path.nodes)
  {
    result.nodes.emplace_back(node.position);
  }
  for (size_t i = 0; i < path.nodes.size(); ++i)
  {
    if (path.nodes[i].roll == 0.0 || path.segmentCount() == 0)
    {
      continue;
    }
    const auto t = double(i) / double(path.segmentCount());
    const auto tangent = path.tangent(t);
    if (vm::squared_length(tangent) < 0.5)
    {
      continue;
    }
    const auto reference =
      std::abs(tangent.z()) < 0.9 ? vm::vec3d{0, 0, 1} : vm::vec3d{0, 1, 0};
    const auto side = vm::normalize(vm::cross(tangent, reference));
    const auto up = vm::cross(side, tangent);
    const auto radians = path.nodes[i].roll * (std::acos(-1.0) / 180.0);
    const auto spoke = std::cos(radians) * up + std::sin(radians) * side;
    result.rollMarkers.emplace_back(path.nodes[i].position);
    result.rollMarkers.emplace_back(path.nodes[i].position + 16.0 * spoke);
  }
  const auto segments = path.segmentCount();
  // Limit subdivision work on very large imported paths while retaining every node.
  const auto subdivisions =
    path.kind == mdl::PathKind::Linear
      ? size_t{1}
      : std::clamp(size_t{65536} / std::max(size_t{1}, segments), size_t{1}, size_t{32});
  if (segments != 0)
  {
    result.curve.reserve(segments * subdivisions + 1);
    for (size_t i = 0; i <= segments * subdivisions; ++i)
    {
      result.curve.emplace_back(path.sample(double(i) / double(segments * subdivisions)));
    }
  }
  for (size_t i = 0; i < segments; ++i)
  {
    const auto controls = path.segmentControls(i);
    if (path.kind == mdl::PathKind::Bezier)
    {
      for (const auto [node, handle] : {std::pair{0u, 1u}, std::pair{3u, 2u}})
      {
        result.handleLines.emplace_back(controls[node]);
        result.handleLines.emplace_back(controls[handle]);
        result.handles.emplace_back(controls[handle]);
      }
    }
    // A four-sided arrowhead remains visible in each orthographic projection.
    const auto t = (double(i) + 0.5) / double(segments);
    const auto direction = path.tangent(t);
    const auto arrowLength = std::min(8.0, vm::length(controls[3] - controls[0]) * 0.2);
    if (vm::squared_length(direction) < 0.5 || arrowLength <= 0.001)
    {
      continue;
    }
    const auto tip = path.sample(t);
    const auto reference =
      std::abs(direction.z()) < 0.9 ? vm::vec3d{0, 0, 1} : vm::vec3d{0, 1, 0};
    const auto side = vm::normalize(vm::cross(direction, reference));
    const auto up = vm::cross(direction, side);
    const auto base = tip - arrowLength * direction;
    for (const auto& offset : {side, -side, up, -up})
    {
      result.arrows.emplace_back(tip);
      result.arrows.emplace_back(base + arrowLength * 0.4 * offset);
    }
  }
  const auto finite = [](const auto& vertices) {
    return std::ranges::all_of(vertices, [](const auto& vertex) {
      return std::isfinite(vertex.x()) && std::isfinite(vertex.y())
             && std::isfinite(vertex.z());
    });
  };
  if (
    !finite(result.curve) || !finite(result.nodes) || !finite(result.handles)
    || !finite(result.handleLines) || !finite(result.arrows)
    || !finite(result.rollMarkers))
  {
    return std::nullopt;
  }
  return result;
}
} // namespace tb::render
