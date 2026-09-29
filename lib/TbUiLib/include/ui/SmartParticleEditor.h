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

#include "ui/SmartPropertyEditor.h"

#include <filesystem>
#include <vector>

class QLineEdit;

namespace tb
{
namespace fs
{
class FileSystem;
}

namespace ui
{
class MapDocument;

/**
 * The particle effects a particlepath property can name: the Sprinkles effect files
 * (.ron) under particles/ in the game file system, as sorted asset-relative paths.
 */
std::vector<std::filesystem::path> findParticlePaths(const fs::FileSystem& fs);

/**
 * Edits a particlepath property: the effect's asset path, typed or picked from a
 * browser of the game's particle effects.
 */
class SmartParticleEditor : public SmartPropertyEditor
{
  Q_OBJECT
private:
  QLineEdit* m_pathEditor = nullptr;
  bool m_updating = false;

public:
  explicit SmartParticleEditor(MapDocument& document, QWidget* parent = nullptr);

private:
  void createGui();
  void chooseParticle();
  void commitPath();
  void doUpdateVisual(const std::vector<mdl::EntityNodeBase*>& nodes) override;
};

} // namespace ui
} // namespace tb
