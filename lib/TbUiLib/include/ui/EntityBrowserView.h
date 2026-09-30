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

#include <QFileSystemWatcher>

#include "base/NotifierConnection.h"
#include "el/Expression.h"
#include "gl/FontDescriptor.h"
#include "gl/VertexType.h"
#include "mdl/Entity.h"
#include "mdl/Prefab.h"
#include "ui/CellView.h"

#include "vm/bbox.h"
#include "vm/quat.h" // IWYU pragma: keep

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tb
{
class Logger;

namespace gl
{
class MaterialRenderer;
class ResourceId;
} // namespace gl

namespace mdl
{
enum class EntityDefinitionSortOrder;
enum class Orientation;

struct EntityDefinition;
} // namespace mdl

namespace render
{
class FontDescriptor;
class Transformation;
} // namespace render

namespace ui
{
class AppController;
class MapDocument;

using EntityGroupData = std::string;

struct EntityCellData
{
  using EntityRenderer = gl::MaterialRenderer;
  const mdl::EntityDefinition& entityDefinition;
  EntityRenderer* modelRenderer;
  mdl::Orientation modelOrientation;
  gl::FontDescriptor fontDescriptor;
  vm::bbox3f bounds;
  vm::mat4x4f transform;
  vm::vec3f modelScale;
};

struct PrefabEdgeVertex
{
  vm::vec3f position;
  vm::vec4f color;
};

struct PrefabModelData
{
  gl::MaterialRenderer* modelRenderer;
  mdl::Orientation modelOrientation;
  vm::mat4x4f transform;
};

/**
 * A prefab is drawn as the edges of its brushes, plus the models of its point entities,
 * or their bounds if they have no model.
 */
struct PrefabCellData
{
  mdl::PrefabFile prefab;
  std::vector<PrefabEdgeVertex> edgeVertices;
  std::vector<PrefabModelData> models;
  vm::bbox3f bounds;
  vm::mat4x4f transform;
};

class EntityBrowserView : public CellView
{
  Q_OBJECT
private:
  using EntityRenderer = gl::MaterialRenderer;

  using TextVertex = gl::VertexTypes::P2Uv2C4::Vertex;
  using StringMap = std::map<gl::FontDescriptor, std::vector<TextVertex>>;

  /**
   * What a prefab file contains, parsed once per modification of the file.
   */
  struct PrefabContents
  {
    std::filesystem::file_time_type modificationTime;
    std::vector<vm::vec3d> brushEdges;
    std::vector<mdl::Entity> pointEntities;
  };

  static constexpr auto CameraPosition = vm::vec3f{256.0f, 0.0f, 0.0f};
  static constexpr auto CameraDirection = vm::vec3f{-1, 0, 0};
  static constexpr auto CameraUp = vm::vec3f{0, 0, 1};

  MapDocument& m_document;
  std::optional<el::ExpressionNode> m_defaultScaleModelExpression;
  vm::quatf m_rotation;

  bool m_group = false;
  bool m_hideUnused = false;
  mdl::EntityDefinitionSortOrder m_sortOrder;
  std::string m_filterText;

  std::map<std::filesystem::path, PrefabContents> m_prefabContents;
  QFileSystemWatcher m_prefabWatcher;

  NotifierConnection m_notifierConnection;

public:
  EntityBrowserView(
    AppController& appController, QScrollBar* scrollBar, MapDocument& document);
  ~EntityBrowserView() override;

public:
  void setDefaultModelScaleExpression(
    std::optional<el::ExpressionNode> defaultModelScaleExpression);

  void setSortOrder(mdl::EntityDefinitionSortOrder sortOrder);
  void setGroup(bool group);
  void setHideUnused(bool hideUnused);
  void setFilterText(const std::string& filterText);

private:
  void doInitLayout(Layout& layout) override;
  void doReloadLayout(Layout& layout) override;

  bool dndEnabled() override;
  QString dndData(const Cell& cell) override;

  void resourcesWereProcessed(const std::vector<gl::ResourceId>& resources);

  void addEntitiesToLayout(
    Layout& layout,
    const std::vector<const mdl::EntityDefinition*>& definitions,
    const gl::FontDescriptor& font);
  void addEntityToLayout(
    Layout& layout,
    const mdl::EntityDefinition& definition,
    const gl::FontDescriptor& font);
  bool isShown(const mdl::EntityDefinition& definition) const;

  std::vector<mdl::PrefabFile> updatePrefabs();
  void watchPrefabDirectory(const std::filesystem::path& directory);
  const PrefabContents& prefabContents(const mdl::PrefabFile& prefab);
  void addPrefabToLayout(
    Layout& layout, const mdl::PrefabFile& prefab, const gl::FontDescriptor& font);

  void doClear() override;
  void doRender(gl::Gl& gl, Layout& layout, float y, float height) override;
  bool shouldRenderFocusIndicator() const override;
  const Color& getBackgroundColor() override;

  void renderBounds(gl::Gl& gl, Layout& layout, float y, float height);
  void renderPrefabEdges(gl::Gl& gl, Layout& layout, float y, float height);

  class MeshFunc;
  void renderModels(
    gl::Gl& gl,
    Layout& layout,
    float y,
    float height,
    render::Transformation& transformation);

  vm::mat4x4f itemTransformation(
    const Cell& cell,
    const vm::bbox3f& bounds,
    const vm::mat4x4f& transform,
    float y,
    float height) const;

  QString tooltip(const Cell& cell) override;
  void doContextMenu(Layout& layout, float x, float y, QContextMenuEvent* event) override;

  const EntityCellData* entityCellData(const Cell& cell) const;
  const PrefabCellData* prefabCellData(const Cell& cell) const;
};

} // namespace ui
} // namespace tb
