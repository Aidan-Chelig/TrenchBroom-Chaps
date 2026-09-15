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

#include "base/Result.h"
#include "mdl/Path.h"

namespace tb::mdl
{
class Entity;

// Version 1 uses world-space positions and handles. No classname is assumed.
// Reading requires path_version, path_type, closed, point_count, and contiguous
// point_N positions. Writing preserves unrelated properties and is atomic on error.
Result<Path> readPath(const Entity& entity);
Result<void> writePath(Entity& entity, const Path& path);

} // namespace tb::mdl
