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

#include "base/NotifierConnection.h"
#include "gl/FontDescriptor.h"
#include "ui/CellView.h"

#include "vm/bbox.h"
#include "vm/quat.h"

#include <filesystem>
#include <string>

namespace tb::gl
{
class MaterialRenderer;
class ResourceId;
} // namespace tb::gl

namespace tb::mdl
{
enum class Orientation;
}

namespace tb::render
{
class Transformation;
}

namespace tb::ui
{
class AppController;
class MapDocument;

struct ModelCellData
{
  std::filesystem::path path;
  gl::MaterialRenderer* renderer;
  mdl::Orientation orientation;
  gl::FontDescriptor fontDescriptor;
  vm::bbox3f bounds;
  vm::mat4x4f transform;
};

class ModelBrowserView : public CellView
{
  Q_OBJECT
private:
  static constexpr auto CameraPosition = vm::vec3f{256.0f, 0.0f, 0.0f};
  static constexpr auto CameraDirection = vm::vec3f{-1, 0, 0};
  static constexpr auto CameraUp = vm::vec3f{0, 0, 1};

  MapDocument& m_document;
  vm::quatf m_rotation;
  std::string m_filterText;
  NotifierConnection m_notifierConnection;

public:
  ModelBrowserView(
    AppController& appController, QScrollBar* scrollBar, MapDocument& document);
  ~ModelBrowserView() override;

  void setFilterText(std::string filterText);

signals:
  void modelSelected(const QString& path);

private:
  void doInitLayout(Layout& layout) override;
  void doReloadLayout(Layout& layout) override;
  void doClear() override;
  void doRender(gl::Gl& gl, Layout& layout, float y, float height) override;
  void doLeftClick(Layout& layout, float x, float y) override;
  bool shouldRenderFocusIndicator() const override;
  const Color& getBackgroundColor() override;
  QString tooltip(const Cell& cell) override;

  void resourcesWereProcessed(const std::vector<gl::ResourceId>& resources);
  void addModelToLayout(
    Layout& layout, const std::filesystem::path& path, const gl::FontDescriptor& font);
  void renderModels(
    gl::Gl& gl,
    Layout& layout,
    float y,
    float height,
    render::Transformation& transformation);
  vm::mat4x4f itemTransformation(const Cell& cell, float y, float height) const;
  const ModelCellData& cellData(const Cell& cell) const;
};
} // namespace tb::ui
