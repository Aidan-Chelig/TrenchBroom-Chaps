/*
 Copyright (C) 2025 Kristian Duske

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

#include "mdl/ColorRange.h"

#include "vm/vec.h"

#include <string>

namespace tb::mdl
{
class EntityNode;
class Map;

enum class SetDefaultPropertyMode;

struct EntityColorPropertyValue;
struct EntityDefinition;

EntityNode* createPointEntity(
  Map& map, const EntityDefinition& definition, const vm::vec3d& delta);
EntityNode* createBrushEntity(Map& map, const EntityDefinition& definition);

bool setEntityProperty(
  Map& map,
  const std::string& key,
  const std::string& value,
  bool defaultToProtected = false);
bool renameEntityProperty(Map& map, const std::string& oldKey, const std::string& newKey);
bool removeEntityProperty(Map& map, const std::string& key);

bool setEntityColorProperty(Map& map, const std::string& key, const Rgb& newColor);
bool convertEntityColorRange(Map& map, const std::string& key, ColorRange::Type range);
bool updateEntitySpawnflag(
  Map& map, const std::string& key, size_t flagIndex, bool setFlag);

bool setProtectedEntityProperty(Map& map, const std::string& key, bool value);
bool clearProtectedEntityProperties(Map& map);
bool canClearProtectedEntityProperties(const Map& map);

/** Returns true if two properties marked unique have the same non-empty value. */
bool hasDuplicateUniqueEntityNames(const Map& map);

/**
 * Assigns collision-free values to empty properties marked unique. If
 * replaceDuplicates is true, all but one occurrence of each duplicate value are also
 * replaced.
 */
bool ensureUniqueEntityNames(Map& map, bool replaceDuplicates = false);

void setDefaultEntityProperties(Map& map, SetDefaultPropertyMode mode);

} // namespace tb::mdl
