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

#pragma once

#include "vm/vec.h"

#include <optional>
#include <vector>

namespace tb::mdl
{
class Entity;
}

namespace tb::render
{
struct PathRenderData
{
  std::vector<vm::vec3f> curve;
  std::vector<vm::vec3f> nodes;
  std::vector<vm::vec3f> handles;
  std::vector<vm::vec3f> handleLines;
  std::vector<vm::vec3f> arrows;
  std::vector<vm::vec3f> rollMarkers;
  bool closed = false;
};

// Returns no geometry for ordinary entities or malformed/unsupported paths.
std::optional<PathRenderData> makePathRenderData(const mdl::Entity& entity);
} // namespace tb::render
