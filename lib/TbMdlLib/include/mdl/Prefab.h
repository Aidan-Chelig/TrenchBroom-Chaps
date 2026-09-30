/*
 Copyright (C) 2010 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "base/Result.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tb::mdl
{
class GroupNode;
class Map;
class Node;

/**
 * A prefab is a group of objects saved to its own .map file so that it can be placed into
 * any map of the same game. Placing a prefab adds an independent copy of its group.
 */
struct PrefabFile
{
  std::string name;
  std::filesystem::path path;
};

/**
 * Returns the directory holding the prefabs of the map's game: `prefabs` in the game's
 * search path (e.g. `<game path>/assets/prefabs`), so prefabs live beside the game's
 * maps. If no game path is set, falls back to `prefabs` beside the game configuration.
 */
std::filesystem::path prefabDirectory(const Map& map);

std::filesystem::path prefabPath(
  const std::filesystem::path& directory, std::string_view name);

/**
 * A prefab name becomes a file name, so it must not be blank, contain characters that
 * are invalid in file names, end with a dot or a space, or be a reserved device name.
 */
Result<void> validatePrefabName(std::string_view name);

/**
 * Returns the prefabs in the given directory sorted by name. A missing directory has no
 * prefabs.
 */
std::vector<PrefabFile> findPrefabs(const std::filesystem::path& directory);

/**
 * Groups the selected nodes under the given name (a single selected group is renamed
 * instead) and writes the group to the given path. The grouping is undone if the file
 * cannot be written.
 */
Result<GroupNode*> createPrefabFromSelection(
  Map& map, const std::string& name, const std::filesystem::path& path);

/**
 * Parses the prefab at the given path. The caller owns the returned nodes. The link IDs
 * of the prefab's groups are replaced with new ones, so the nodes never link to groups
 * already in the map, but linked groups within the prefab stay linked to each other.
 */
Result<std::vector<Node*>> readPrefab(Map& map, const std::filesystem::path& path);

/**
 * Adds a copy of the prefab at the given path to the map and selects it.
 */
Result<void> placePrefab(Map& map, const std::filesystem::path& path);

/**
 * Renames the prefab file at the given path and returns its new path.
 */
Result<std::filesystem::path> renamePrefab(
  const std::filesystem::path& path, std::string_view newName);

} // namespace tb::mdl
