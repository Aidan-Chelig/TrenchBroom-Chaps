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

class QLineEdit;

namespace tb::ui
{
class AppController;
class MapDocument;

class SmartModelEditor : public SmartPropertyEditor
{
  Q_OBJECT
private:
  AppController& m_appController;
  QLineEdit* m_pathEditor = nullptr;
  bool m_updating = false;

public:
  SmartModelEditor(
    AppController& appController, MapDocument& document, QWidget* parent = nullptr);

private:
  void createGui();
  void chooseModel();
  void commitPath();
  void doUpdateVisual(const std::vector<mdl::EntityNodeBase*>& nodes) override;
};
} // namespace tb::ui
