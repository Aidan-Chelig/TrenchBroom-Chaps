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

#include "ui/ModelBrowserView.h"

#include "base/PreferenceManager.h"
#include "gl/ActiveShader.h"
#include "gl/FontManager.h"
#include "gl/GlInterface.h"
#include "gl/MaterialIndexRangeRenderer.h"
#include "gl/MaterialRenderFunc.h"
#include "gl/Shaders.h"
#include "gl/TextureFont.h"
#include "mdl/EntityModel.h"
#include "mdl/EntityModelManager.h"
#include "mdl/Map.h"
#include "mdl/ModelSpecification.h"
#include "prefs/Preferences.h"
#include "render/Transformation.h"
#include "ui/MapDocument.h"

#include "kd/contracts.h"
#include "kd/result.h"
#include "kd/string_compare.h"

#include "vm/mat_ext.h"

#include <algorithm>
#include <map>
#include <vector>

namespace tb::ui
{

ModelBrowserView::ModelBrowserView(
  AppController& appController, QScrollBar* scrollBar, MapDocument& document)
  : CellView{appController, scrollBar}
  , m_document{document}
{
  const auto horizontal = vm::quatf{vm::vec3f{0, 0, 1}, vm::to_radians(-30.0f)};
  const auto vertical = vm::quatf{vm::vec3f{0, 1, 0}, vm::to_radians(20.0f)};
  m_rotation = vertical * horizontal;

  m_notifierConnection += m_document.resourcesWereProcessedNotifier.connect(
    this, &ModelBrowserView::resourcesWereProcessed);
}

ModelBrowserView::~ModelBrowserView()
{
  clear();
}

void ModelBrowserView::setFilterText(std::string filterText)
{
  if (filterText != m_filterText)
  {
    m_filterText = std::move(filterText);
    invalidate();
    update();
  }
}

void ModelBrowserView::doInitLayout(Layout& layout)
{
  layout.setOuterMargin(5.0f);
  layout.setGroupMargin(5.0f);
  layout.setRowMargin(5.0f);
  layout.setCellMargin(5.0f);
  layout.setCellWidth(110.0f, 110.0f);
  layout.setCellHeight(72.0f, 136.0f);
  layout.setMaxUpScale(1.5f);
}

void ModelBrowserView::doReloadLayout(Layout& layout)
{
  const auto& fontPath = pref(Preferences::RendererFontPath);
  const auto fontSize = pref(Preferences::BrowserFontSize);
  contract_assert(fontSize > 0);
  const auto font = gl::FontDescriptor{fontPath, static_cast<size_t>(fontSize)};

  auto paths = m_document.map().entityModelManager().findModelPaths()
               | kdl::value_or(std::vector<std::filesystem::path>{});
  std::ranges::sort(paths);

  auto folders = std::map<std::filesystem::path, std::vector<std::filesystem::path>>{};
  for (auto& path : paths)
  {
    if (
      m_filterText.empty() || kdl::ci::str_contains(path.generic_string(), m_filterText))
    {
      folders[path.parent_path()].push_back(std::move(path));
    }
  }

  for (const auto& [folder, modelPaths] : folders)
  {
    layout.addGroup(
      folder.empty() ? tr("Models").toStdString() : folder.generic_string(),
      static_cast<float>(fontSize) + 2.0f);
    for (const auto& path : modelPaths)
    {
      addModelToLayout(layout, path, font);
    }
  }
}

void ModelBrowserView::addModelToLayout(
  Layout& layout, const std::filesystem::path& path, const gl::FontDescriptor& font)
{
  const auto name = path.filename().generic_string();
  const auto maxCellWidth = layout.maxCellWidth();
  const auto actualFont = fontManager().selectFontSize(font, name, maxCellWidth, 5);
  const auto textSize = fontManager().font(actualFont).measure(name);
  const auto spec = mdl::ModelSpecification{path, 0, 0};

  auto& modelManager = m_document.map().entityModelManager();
  const auto* model = modelManager.model(path);
  const auto* modelData = model ? model->data() : nullptr;
  const auto* frame = modelData ? modelData->frame(0) : nullptr;

  auto* renderer = static_cast<gl::MaterialRenderer*>(nullptr);
  auto orientation = mdl::Orientation::Oriented;
  auto bounds = vm::bbox3f{{-16, -16, -16}, {16, 16, 16}};
  if (frame)
  {
    renderer = modelManager.renderer(spec);
    orientation = modelData->orientation();
    bounds = frame->bounds();
  }

  const auto center = bounds.center();
  const auto transform = vm::translation_matrix(center) * vm::rotation_matrix(m_rotation)
                         * vm::translation_matrix(-center);
  const auto rotatedSize = bounds.transform(transform).size();

  layout.addItem(
    ModelCellData{path, renderer, orientation, actualFont, bounds, transform},
    name,
    rotatedSize.y(),
    rotatedSize.z(),
    textSize.x(),
    static_cast<float>(actualFont.size()) + 2.0f);
}

void ModelBrowserView::doClear() {}

void ModelBrowserView::doRender(
  gl::Gl& gl, Layout& layout, const float y, const float height)
{
  const auto projection = vm::ortho_matrix(
    -1024.0f,
    1024.0f,
    0.0f,
    static_cast<float>(size().height()),
    static_cast<float>(size().width()),
    0.0f);
  const auto view =
    vm::view_matrix(CameraDirection, CameraUp) * vm::translation_matrix(CameraPosition);
  auto transformation = render::Transformation{gl, projection, view};
  renderModels(gl, layout, y, height, transformation);
}

void ModelBrowserView::renderModels(
  gl::Gl& gl,
  Layout& layout,
  const float y,
  const float height,
  render::Transformation& transformation)
{
  gl.frontFace(GL_CW);
  auto& modelManager = m_document.map().entityModelManager();
  modelManager.prepare(gl, vboManager());

  auto shader = gl::ActiveShader{gl, shaderManager(), gl::Shaders::EntityModelShader};
  shader.set("ApplyTinting", false);
  shader.set("Brightness", pref(Preferences::Brightness));
  shader.set("GrayScale", false);
  shader.set("CameraPosition", CameraPosition);
  shader.set("CameraDirection", CameraDirection);
  shader.set("CameraRight", vm::cross(CameraDirection, CameraUp));
  shader.set("CameraUp", CameraUp);
  shader.set("ViewMatrix", transformation.viewMatrix());

  for (const auto& group : layout.groups())
  {
    if (!group.intersectsY(y, height))
      continue;
    for (const auto& row : group.rows())
    {
      if (!row.intersectsY(y, height))
        continue;
      for (const auto& cell : row.cells())
      {
        if (auto* renderer = cellData(cell).renderer)
        {
          shader.set("Orientation", static_cast<int>(cellData(cell).orientation));
          const auto transform = itemTransformation(cell, y, height);
          shader.set("ModelMatrix", transform);
          const auto multiply = render::MultiplyModelMatrix{transformation, transform};
          auto renderFunc = gl::AlphaTestedMaterialRenderFunc{
            shader,
            pref(Preferences::TextureMinFilter),
            pref(Preferences::TextureMagFilter)};
          renderer->render(gl, shader.program(), renderFunc);
        }
      }
    }
  }
  gl.frontFace(GL_CCW);
}

vm::mat4x4f ModelBrowserView::itemTransformation(
  const Cell& cell, const float y, const float height) const
{
  const auto& data = cellData(cell);
  const auto offset =
    vm::vec3f{0.0f, cell.itemBounds().left(), height - (cell.itemBounds().bottom() - y)};
  const auto rotatedBounds = data.bounds.transform(data.transform);
  const auto rotationOffset =
    vm::vec3f{0.0f, -rotatedBounds.min.y(), -rotatedBounds.min.z()};
  return vm::translation_matrix(offset)
         * vm::scaling_matrix(vm::vec3f::fill(cell.scale()))
         * vm::translation_matrix(rotationOffset) * data.transform;
}

void ModelBrowserView::doLeftClick(Layout& layout, const float x, const float y)
{
  if (const auto* cell = layout.cellAt(x, y))
  {
    emit modelSelected(QString::fromStdString(cellData(*cell).path.generic_string()));
  }
}

bool ModelBrowserView::shouldRenderFocusIndicator() const
{
  return false;
}

const Color& ModelBrowserView::getBackgroundColor()
{
  return pref(Preferences::BrowserBackgroundColor);
}

QString ModelBrowserView::tooltip(const Cell& cell)
{
  return QString::fromStdString(cellData(cell).path.generic_string());
}

void ModelBrowserView::resourcesWereProcessed(const std::vector<gl::ResourceId>&)
{
  invalidate();
}

const ModelCellData& ModelBrowserView::cellData(const Cell& cell) const
{
  return cell.itemAs<ModelCellData>();
}

} // namespace tb::ui
