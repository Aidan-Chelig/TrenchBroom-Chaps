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

#include "ui/EntityBrowserView.h"

#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QFileInfo>
#include <QMenu>
#include <QUrl>

#include "base/Logger.h"
#include "base/PreferenceManager.h"
#include "el/VariableStore.h"
#include "gl/ActiveShader.h"
#include "gl/FontDescriptor.h"
#include "gl/FontManager.h"
#include "gl/GlInterface.h"
#include "gl/GlUtils.h"
#include "gl/MaterialIndexRangeRenderer.h"
#include "gl/MaterialRenderFunc.h"
#include "gl/PrimType.h"
#include "gl/ResourceId.h"
#include "gl/Shaders.h"
#include "gl/TextureFont.h"
#include "gl/VertexArray.h"
#include "mdl/AssetUtils.h"
#include "mdl/Brush.h"
#include "mdl/BrushNode.h"
#include "mdl/EntityDefinition.h"
#include "mdl/EntityDefinitionGroup.h"
#include "mdl/EntityDefinitionManager.h"
#include "mdl/EntityDefinitionUtils.h"
#include "mdl/EntityModel.h"
#include "mdl/EntityModelManager.h"
#include "mdl/EntityNode.h"
#include "mdl/GroupNode.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/PatchNode.h"
#include "mdl/WorldNode.h"
#include "prefs/Preferences.h"
#include "render/Transformation.h"
#include "ui/MapDocument.h"
#include "ui/PrefabUtils.h"
#include "ui/QPathUtils.h"

#include "kd/contracts.h"
#include "kd/overload.h"
#include "kd/ranges/to.h"
#include "kd/string_compare.h"
#include "kd/string_utils.h"
#include "kd/vector_utils.h"

#include "vm/mat.h"
#include "vm/mat_ext.h"
#include "vm/quat.h"
#include "vm/vec.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <vector>

namespace tb::ui
{
namespace
{

bool matchesFilterText(const std::string_view name, const std::string& filterText)
{
  return filterText.empty()
         || std::ranges::all_of(
           kdl::str_split(filterText, " "),
           [&](const auto& pattern) { return kdl::ci::str_contains(name, pattern); });
}

} // namespace

EntityBrowserView::EntityBrowserView(
  AppController& appController, QScrollBar* scrollBar, MapDocument& document)
  : CellView{appController, scrollBar}
  , m_document{document}
  , m_sortOrder{mdl::EntityDefinitionSortOrder::Name}
{
  const auto hRotation = vm::quatf{vm::vec3f{0, 0, 1}, vm::to_radians(-30.0f)};
  const auto vRotation = vm::quatf{vm::vec3f{0, 1, 0}, vm::to_radians(20.0f)};
  m_rotation = vRotation * hRotation;

  m_notifierConnection += m_document.resourcesWereProcessedNotifier.connect(
    this, &EntityBrowserView::resourcesWereProcessed);

  // Prefabs can be created, renamed or deleted in other windows or outside the editor.
  connect(&m_prefabWatcher, &QFileSystemWatcher::directoryChanged, this, [&]() {
    invalidate();
    update();
  });
}

EntityBrowserView::~EntityBrowserView()
{
  clear();
}

void EntityBrowserView::setDefaultModelScaleExpression(
  std::optional<el::ExpressionNode> defaultScaleExpression)
{
  m_defaultScaleModelExpression = std::move(defaultScaleExpression);
}

void EntityBrowserView::setSortOrder(const mdl::EntityDefinitionSortOrder sortOrder)
{
  if (sortOrder != m_sortOrder)
  {
    m_sortOrder = sortOrder;
    invalidate();
    update();
  }
}

void EntityBrowserView::setGroup(const bool group)
{
  if (group != m_group)
  {
    m_group = group;
    invalidate();
    update();
  }
}

void EntityBrowserView::setHideUnused(const bool hideUnused)
{
  if (hideUnused != m_hideUnused)
  {
    m_hideUnused = hideUnused;
    invalidate();
    update();
  }
}

void EntityBrowserView::setFilterText(const std::string& filterText)
{
  if (filterText != m_filterText)
  {
    m_filterText = filterText;
    invalidate();
    update();
  }
}

void EntityBrowserView::doInitLayout(Layout& layout)
{
  layout.setOuterMargin(5.0f);
  layout.setGroupMargin(5.0f);
  layout.setRowMargin(5.0f);
  layout.setCellMargin(5.0f);
  layout.setCellWidth(93.0f, 93.0f);
  layout.setCellHeight(64.0f, 128.0f);
  layout.setMaxUpScale(1.5f);
}

void EntityBrowserView::doReloadLayout(Layout& layout)
{
  const auto& fontPath = pref(Preferences::RendererFontPath);
  const auto fontSize = pref(Preferences::BrowserFontSize);
  contract_assert(fontSize > 0);

  const auto& entityDefinitionManager = m_document.map().entityDefinitionManager();
  const auto font = gl::FontDescriptor{fontPath, static_cast<size_t>(fontSize)};
  const auto titleHeight = static_cast<float>(fontSize) + 2.0f;

  // Prefabs are not entity definitions, so they are never in use.
  const auto prefabs =
    updatePrefabs() | std::views::filter([&](const auto& prefab) {
      return !m_hideUnused && matchesFilterText(prefab.name, m_filterText);
    })
    | kdl::ranges::to<std::vector>();

  if (!prefabs.empty())
  {
    layout.addGroup("Prefabs", titleHeight);
    for (const auto& prefab : prefabs)
    {
      addPrefabToLayout(layout, prefab, font);
    }
  }

  if (m_group)
  {
    for (const auto& group : entityDefinitionManager.groups())
    {
      if (const auto definitions = filterAndSort(
            group.definitions, mdl::EntityDefinitionType::Point, m_sortOrder);
          !definitions.empty())
      {
        layout.addGroup(displayName(group), titleHeight);

        addEntitiesToLayout(layout, definitions, font);
      }
    }
  }
  else
  {
    const auto& definitions =
      entityDefinitionManager.definitions(mdl::EntityDefinitionType::Point, m_sortOrder);

    // keep the entities out of the prefab group
    if (!prefabs.empty() && std::ranges::any_of(definitions, [&](const auto* definition) {
          return isShown(*definition);
        }))
    {
      layout.addGroup("Entities", titleHeight);
    }

    addEntitiesToLayout(layout, definitions, font);
  }
}

bool EntityBrowserView::dndEnabled()
{
  return true;
}

QString EntityBrowserView::dndData(const Cell& cell)
{
  if (const auto* prefabData = prefabCellData(cell))
  {
    return QString{"prefab:"} + QString::fromStdString(prefabData->prefab.name);
  }

  static const auto prefix = QString{"entity:"};
  const auto name = QString::fromStdString(entityCellData(cell)->entityDefinition.name);
  return prefix + name;
}

void EntityBrowserView::resourcesWereProcessed(const std::vector<gl::ResourceId>&)
{
  invalidate();
}

void EntityBrowserView::addEntitiesToLayout(
  Layout& layout,
  const std::vector<const mdl::EntityDefinition*>& definitions,
  const gl::FontDescriptor& font)
{
  for (const auto* definition : definitions)
  {
    addEntityToLayout(layout, *definition, font);
  }
}

void EntityBrowserView::addEntityToLayout(
  Layout& layout, const mdl::EntityDefinition& definition, const gl::FontDescriptor& font)
{
  auto& map = m_document.map();
  const auto name =
    m_group ? std::string{mdl::getShortName(definition)} : definition.name;

  if (isShown(definition))
  {
    contract_assert(definition.pointEntityDefinition != std::nullopt);
    const auto& pointEntityDefinition = *definition.pointEntityDefinition;

    const auto maxCellWidth = layout.maxCellWidth();
    const auto actualFont = fontManager().selectFontSize(font, name, maxCellWidth, 5);
    const auto actualSize = fontManager().font(actualFont).measure(name);
    const auto spec =
      mdl::safeGetModelSpecification(map.logger(), definition.name, [&]() {
        return pointEntityDefinition.modelDefinition.defaultModelSpecification();
      });

    const auto modelScale = vm::vec3f{mdl::safeGetModelScale(
      pointEntityDefinition.modelDefinition,
      el::NullVariableStore{},
      m_defaultScaleModelExpression)};

    auto* modelRenderer = static_cast<gl::MaterialRenderer*>(nullptr);
    auto bounds = vm::bbox3f{};
    auto transform = vm::mat4x4f{};
    auto modelOrientation = mdl::Orientation::Oriented;

    const auto& entityModelManager = map.entityModelManager();
    const auto* model = entityModelManager.model(spec.path);
    const auto* modelData = model ? model->data() : nullptr;
    const auto* modelFrame = modelData ? modelData->frame(spec.frameIndex) : nullptr;
    if (modelFrame)
    {
      modelRenderer = entityModelManager.renderer(spec);
      modelOrientation = modelData->orientation();

      bounds = modelFrame->bounds();

      const auto scalingMatrix = vm::scaling_matrix(modelScale);
      const auto center = bounds.center();
      const auto scaledCenter = scalingMatrix * center;
      transform = vm::translation_matrix(scaledCenter) * vm::rotation_matrix(m_rotation)
                  * scalingMatrix * vm::translation_matrix(-center);
    }
    else
    {
      bounds = vm::bbox3f{pointEntityDefinition.bounds};

      const auto center = bounds.center();
      transform = vm::translation_matrix(-center) * vm::rotation_matrix(m_rotation)
                  * vm::translation_matrix(center);
    }

    const auto rotatedBounds = bounds.transform(transform);
    const auto rotatedBoundsSize = rotatedBounds.size();

    layout.addItem(
      EntityCellData{
        definition,
        modelRenderer,
        modelOrientation,
        actualFont,
        bounds,
        transform,
        modelScale},
      name,
      rotatedBoundsSize.y(),
      rotatedBoundsSize.z(),
      actualSize.x(),
      static_cast<float>(font.size()) + 2.0f);
  }
}

bool EntityBrowserView::isShown(const mdl::EntityDefinition& definition) const
{
  return (!m_hideUnused || definition.usageCount() > 0)
         && matchesFilterText(definition.name, m_filterText);
}

std::vector<mdl::PrefabFile> EntityBrowserView::updatePrefabs()
{
  const auto directory = mdl::prefabDirectory(m_document.map());
  watchPrefabDirectory(directory);

  auto prefabs = mdl::findPrefabs(directory);
  std::erase_if(m_prefabContents, [&](const auto& entry) {
    return std::ranges::none_of(
      prefabs, [&](const auto& prefab) { return prefab.path == entry.first; });
  });
  return prefabs;
}

void EntityBrowserView::watchPrefabDirectory(const std::filesystem::path& directory)
{
  // Until the prefab directory exists, watch its parent to learn when it is created.
  const auto directoryPath = pathAsQString(directory);
  const auto parentPath = pathAsQString(directory.parent_path());
  const auto pathToWatch = QFileInfo{directoryPath}.isDir() ? directoryPath
                           : QFileInfo{parentPath}.isDir()  ? parentPath
                                                            : QString{};

  const auto watchedPaths = m_prefabWatcher.directories();
  if (watchedPaths == QStringList{pathToWatch})
  {
    return;
  }

  if (!watchedPaths.isEmpty())
  {
    m_prefabWatcher.removePaths(watchedPaths);
  }
  if (!pathToWatch.isEmpty())
  {
    m_prefabWatcher.addPath(pathToWatch);
  }
}

const EntityBrowserView::PrefabContents& EntityBrowserView::prefabContents(
  const mdl::PrefabFile& prefab)
{
  auto error = std::error_code{};
  const auto modificationTime = std::filesystem::last_write_time(prefab.path, error);

  if (const auto it = m_prefabContents.find(prefab.path);
      it != m_prefabContents.end() && it->second.modificationTime == modificationTime)
  {
    return it->second;
  }

  auto contents = PrefabContents{modificationTime, {}, {}};

  auto& map = m_document.map();
  mdl::readPrefab(map, prefab.path) | kdl::transform([&](auto nodes) {
    for (auto* node : nodes)
    {
      node->accept(kdl::overload(
        [](auto&& thisLambda, mdl::WorldNode& worldNode) {
          worldNode.visitChildren(thisLambda);
        },
        [](auto&& thisLambda, mdl::LayerNode& layerNode) {
          layerNode.visitChildren(thisLambda);
        },
        [](auto&& thisLambda, mdl::GroupNode& groupNode) {
          groupNode.visitChildren(thisLambda);
        },
        [&](auto&& thisLambda, mdl::EntityNode& entityNode) {
          if (entityNode.hasChildren())
          {
            entityNode.visitChildren(thisLambda);
          }
          else
          {
            contents.pointEntities.push_back(entityNode.entity());
          }
        },
        [&](mdl::BrushNode& brushNode) {
          for (const auto* edge : brushNode.brush().edges())
          {
            contents.brushEdges.push_back(edge->firstVertex()->position());
            contents.brushEdges.push_back(edge->secondVertex()->position());
          }
        },
        [&](mdl::PatchNode& patchNode) {
          patchNode.logicalBounds().for_each_edge([&](const auto& v1, const auto& v2) {
            contents.brushEdges.push_back(v1);
            contents.brushEdges.push_back(v2);
          });
        }));
    }
    kdl::vec_clear_and_delete(nodes);
  }) | kdl::transform_error([&](const auto& e) { map.logger().warn() << e.msg; });

  // An unreadable prefab is cached too, so that it is only reported once.
  return m_prefabContents.insert_or_assign(prefab.path, std::move(contents))
    .first->second;
}

void EntityBrowserView::addPrefabToLayout(
  Layout& layout, const mdl::PrefabFile& prefab, const gl::FontDescriptor& font)
{
  const auto& contents = prefabContents(prefab);

  auto& map = m_document.map();
  const auto& entityDefinitionManager = map.entityDefinitionManager();
  const auto& entityModelManager = map.entityModelManager();

  auto boundsBuilder = vm::bbox3d::builder{};
  auto edgeVertices = std::vector<PrefabEdgeVertex>{};
  auto models = std::vector<PrefabModelData>{};

  const auto addEdge = [&](const vm::vec3d& v1, const vm::vec3d& v2, const Color& color) {
    const auto rgba = color.to<RgbaF>().toVec();
    edgeVertices.push_back({vm::vec3f{v1}, rgba});
    edgeVertices.push_back({vm::vec3f{v2}, rgba});
    boundsBuilder.add(v1);
    boundsBuilder.add(v2);
  };

  const auto& edgeColor = pref(Preferences::EdgeColor);
  for (size_t i = 0; i + 1 < contents.brushEdges.size(); i += 2)
  {
    addEdge(contents.brushEdges[i], contents.brushEdges[i + 1], edgeColor);
  }

  for (auto entity : contents.pointEntities)
  {
    const auto* definition = entityDefinitionManager.definition(entity.classname());
    entity.setDefinition(definition);

    if (const auto spec = entity.modelSpecification(); spec.is_success())
    {
      const auto* model = entityModelManager.model(spec.value().path);
      const auto* modelData = model ? model->data() : nullptr;
      const auto* modelFrame =
        modelData ? modelData->frame(spec.value().frameIndex) : nullptr;
      auto* modelRenderer =
        modelFrame ? entityModelManager.renderer(spec.value()) : nullptr;
      if (modelRenderer)
      {
        const auto& transform = entity.modelTransformation(m_defaultScaleModelExpression);
        models.push_back(
          {modelRenderer, modelData->orientation(), vm::mat4x4f{transform}});
        boundsBuilder.add(vm::bbox3d{modelFrame->bounds()}.transform(transform));
        continue;
      }
    }

    const auto& color =
      definition ? definition->color : pref(Preferences::UndefinedEntityColor);
    entity.definitionBounds()
      .translate(entity.origin())
      .for_each_edge([&](const auto& v1, const auto& v2) { addEdge(v1, v2, color); });
  }

  // an empty or unreadable prefab still gets a cell so that it can be renamed or deleted
  const auto bounds =
    boundsBuilder.initialized() ? vm::bbox3f{boundsBuilder.bounds()} : vm::bbox3f{16.0f};

  // Rotate the prefab about its center and move that to the origin, since a prefab can be
  // far from the world origin, which would put it beyond the browser's depth range.
  const auto transform =
    vm::rotation_matrix(m_rotation) * vm::translation_matrix(-bounds.center());
  const auto rotatedBoundsSize = bounds.transform(transform).size();

  const auto& name = prefab.name;
  const auto actualFont =
    fontManager().selectFontSize(font, name, layout.maxCellWidth(), 5);
  const auto actualSize = fontManager().font(actualFont).measure(name);

  layout.addItem(
    PrefabCellData{prefab, std::move(edgeVertices), std::move(models), bounds, transform},
    name,
    rotatedBoundsSize.y(),
    rotatedBoundsSize.z(),
    actualSize.x(),
    static_cast<float>(font.size()) + 2.0f);
}

void EntityBrowserView::doClear() {}

void EntityBrowserView::doRender(
  gl::Gl& gl, Layout& layout, const float y, const float height)
{
  const auto viewLeft = static_cast<float>(0);
  const auto viewTop = static_cast<float>(size().height());
  const auto viewRight = static_cast<float>(size().width());
  const auto viewBottom = static_cast<float>(0);

  const auto projection =
    vm::ortho_matrix(-1024.0f, 1024.0f, viewLeft, viewTop, viewRight, viewBottom);
  const auto view =
    vm::view_matrix(CameraDirection, CameraUp) * vm::translation_matrix(CameraPosition);
  auto transformation = render::Transformation{gl, projection, view};

  renderBounds(gl, layout, y, height);
  renderPrefabEdges(gl, layout, y, height);
  renderModels(gl, layout, y, height, transformation);
}

bool EntityBrowserView::shouldRenderFocusIndicator() const
{
  return false;
}

const Color& EntityBrowserView::getBackgroundColor()
{
  return pref(Preferences::BrowserBackgroundColor);
}

void EntityBrowserView::renderBounds(
  gl::Gl& gl, Layout& layout, const float y, const float height)
{
  using BoundsVertex = gl::VertexTypes::P3C4::Vertex;
  auto vertices = std::vector<BoundsVertex>{};

  for (const auto& group : layout.groups())
  {
    if (group.intersectsY(y, height))
    {
      for (const auto& row : group.rows())
      {
        if (row.intersectsY(y, height))
        {
          for (const auto& cell : row.cells())
          {
            const auto* cellData = entityCellData(cell);
            if (cellData && cellData->modelRenderer == nullptr)
            {
              const auto& definition = cellData->entityDefinition;
              const auto& pointEntityDefinition = *definition.pointEntityDefinition;
              const auto itemTrans = itemTransformation(
                cell, cellData->bounds, cellData->transform, y, height);
              const auto& color = definition.color;
              vm::bbox3f{pointEntityDefinition.bounds}.for_each_edge(
                [&](const vm::vec3f& v1, const vm::vec3f& v2) {
                  vertices.emplace_back(itemTrans * v1, color.to<RgbaF>().toVec());
                  vertices.emplace_back(itemTrans * v2, color.to<RgbaF>().toVec());
                });
            }
          }
        }
      }
    }
  }

  auto shader = gl::ActiveShader{gl, shaderManager(), gl::Shaders::VaryingPCShader};
  auto vertexArray = gl::VertexArray::move(std::move(vertices));

  vertexArray.prepare(gl, vboManager());
  if (vertexArray.setup(gl, shader.program()))
  {
    vertexArray.render(gl, gl::PrimType::Lines);
    vertexArray.cleanup(gl, shader.program());
  }
}

void EntityBrowserView::renderPrefabEdges(
  gl::Gl& gl, Layout& layout, const float y, const float height)
{
  using EdgeVertex = gl::VertexTypes::P3C4::Vertex;
  auto vertices = std::vector<EdgeVertex>{};

  for (const auto& group : layout.groups())
  {
    if (group.intersectsY(y, height))
    {
      for (const auto& row : group.rows())
      {
        if (row.intersectsY(y, height))
        {
          for (const auto& cell : row.cells())
          {
            if (const auto* cellData = prefabCellData(cell))
            {
              const auto itemTrans = itemTransformation(
                cell, cellData->bounds, cellData->transform, y, height);
              for (const auto& vertex : cellData->edgeVertices)
              {
                vertices.emplace_back(itemTrans * vertex.position, vertex.color);
              }
            }
          }
        }
      }
    }
  }

  if (vertices.empty())
  {
    return;
  }

  auto shader = gl::ActiveShader{gl, shaderManager(), gl::Shaders::VaryingPCShader};
  auto vertexArray = gl::VertexArray::move(std::move(vertices));

  vertexArray.prepare(gl, vboManager());
  if (vertexArray.setup(gl, shader.program()))
  {
    vertexArray.render(gl, gl::PrimType::Lines);
    vertexArray.cleanup(gl, shader.program());
  }
}

void EntityBrowserView::renderModels(
  gl::Gl& gl,
  Layout& layout,
  const float y,
  const float height,
  render::Transformation& transformation)
{
  gl.frontFace(GL_CW);

  auto& entityModelManager = m_document.map().entityModelManager();
  entityModelManager.prepare(gl, vboManager());

  auto shader = gl::ActiveShader{gl, shaderManager(), gl::Shaders::EntityModelShader};
  shader.set("ApplyTinting", false);
  shader.set("Brightness", pref(Preferences::Brightness));
  shader.set("GrayScale", false);

  shader.set("CameraPosition", CameraPosition);
  shader.set("CameraDirection", CameraDirection);
  shader.set("CameraRight", vm::cross(CameraDirection, CameraUp));
  shader.set("CameraUp", CameraUp);
  shader.set("ViewMatrix", transformation.viewMatrix());

  const auto renderModel = [&](
                             gl::MaterialRenderer& modelRenderer,
                             const mdl::Orientation modelOrientation,
                             const vm::mat4x4f& modelMatrix) {
    shader.set("Orientation", static_cast<int>(modelOrientation));
    shader.set("ModelMatrix", modelMatrix);

    const auto multMatrix = render::MultiplyModelMatrix{transformation, modelMatrix};

    auto renderFunc = gl::AlphaTestedMaterialRenderFunc{
      shader, pref(Preferences::TextureMinFilter), pref(Preferences::TextureMagFilter)};
    modelRenderer.render(gl, shader.program(), renderFunc);
  };

  for (const auto& group : layout.groups())
  {
    if (group.intersectsY(y, height))
    {
      for (const auto& row : group.rows())
      {
        if (row.intersectsY(y, height))
        {
          for (const auto& cell : row.cells())
          {
            if (const auto* cellData = entityCellData(cell))
            {
              if (auto* modelRenderer = cellData->modelRenderer)
              {
                const auto itemTrans = itemTransformation(
                  cell, cellData->bounds, cellData->transform, y, height);
                renderModel(*modelRenderer, cellData->modelOrientation, itemTrans);
              }
            }
            else if (const auto* prefabData = prefabCellData(cell))
            {
              const auto itemTrans = itemTransformation(
                cell, prefabData->bounds, prefabData->transform, y, height);
              for (const auto& model : prefabData->models)
              {
                renderModel(
                  *model.modelRenderer,
                  model.modelOrientation,
                  itemTrans * model.transform);
              }
            }
          }
        }
      }
    }
  }
}

vm::mat4x4f EntityBrowserView::itemTransformation(
  const Cell& cell,
  const vm::bbox3f& bounds,
  const vm::mat4x4f& transform,
  const float y,
  const float height) const
{
  const auto offset =
    vm::vec3f{0.0f, cell.itemBounds().left(), height - (cell.itemBounds().bottom() - y)};
  const auto scaling = cell.scale();
  const auto& rotatedBounds = bounds.transform(transform);
  const auto rotationOffset =
    vm::vec3f{0.0f, -rotatedBounds.min.y(), -rotatedBounds.min.z()};

  return vm::translation_matrix(offset) * vm::scaling_matrix(vm::vec3f::fill(scaling))
         * vm::translation_matrix(rotationOffset) * transform;
}

QString EntityBrowserView::tooltip(const Cell& cell)
{
  if (const auto* prefabData = prefabCellData(cell))
  {
    return tr("Prefab %1\n%2\n\nDrag it into a map to place a copy.")
      .arg(QString::fromStdString(prefabData->prefab.name))
      .arg(pathAsQString(prefabData->prefab.path));
  }
  return QString::fromStdString(entityCellData(cell)->entityDefinition.name);
}

void EntityBrowserView::doContextMenu(
  Layout& layout, const float x, const float y, QContextMenuEvent* event)
{
  const auto* cell = layout.cellAt(x, y);
  const auto* prefabData = cell ? prefabCellData(*cell) : nullptr;
  if (!prefabData)
  {
    return;
  }

  // the layout is rebuilt when the prefab changes, so keep a copy
  const auto prefab = prefabData->prefab;
  const auto refresh = [&]() {
    invalidate();
    update();
  };

  auto menu = QMenu{this};
  menu.addAction(tr("Rename Prefab..."), this, [&]() {
    if (renamePrefab(this, prefab))
    {
      refresh();
    }
  });
  menu.addAction(tr("Delete Prefab..."), this, [&]() {
    if (deletePrefab(this, prefab))
    {
      refresh();
    }
  });
  menu.addSeparator();
  menu.addAction(tr("Show in File Browser"), this, [&]() {
    QDesktopServices::openUrl(
      QUrl::fromLocalFile(pathAsQString(prefab.path.parent_path())));
  });

  menu.exec(event->globalPos());
}

const EntityCellData* EntityBrowserView::entityCellData(const Cell& cell) const
{
  return std::any_cast<EntityCellData>(&cell.item());
}

const PrefabCellData* EntityBrowserView::prefabCellData(const Cell& cell) const
{
  return std::any_cast<PrefabCellData>(&cell.item());
}

} // namespace tb::ui
