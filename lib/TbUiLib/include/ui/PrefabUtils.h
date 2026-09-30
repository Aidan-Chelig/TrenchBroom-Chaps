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

class QWidget;

namespace tb
{
namespace mdl
{
class Map;
struct PrefabFile;
} // namespace mdl

namespace ui
{

bool canCreatePrefabFromSelection(const mdl::Map& map);

/**
 * Asks for a name, then groups the selection and saves it as a prefab that the entity
 * browser offers for every map of the game. Returns true if the prefab was saved.
 */
bool createPrefabFromSelection(QWidget* parent, mdl::Map& map);

/**
 * Asks for a new name and renames the prefab. Returns true if it was renamed.
 */
bool renamePrefab(QWidget* parent, const mdl::PrefabFile& prefab);

/**
 * Asks for confirmation and moves the prefab to the trash. Returns true if it was moved.
 */
bool deletePrefab(QWidget* parent, const mdl::PrefabFile& prefab);

} // namespace ui
} // namespace tb
