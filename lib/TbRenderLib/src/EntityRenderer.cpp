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

#include "render/EntityRenderer.h"

#include "gl/Camera.h"
#include "gl/PrimType.h"
#include "gl/VertexType.h"
#include "mdl/AreaLight.h"
#include "mdl/EditorContext.h"
#include "mdl/Entity.h"
#include "mdl/EntityDefinition.h"
#include "mdl/EntityModelManager.h"
#include "mdl/EntityNode.h"
#include "mdl/ImageProjector.h"
#include "render/RenderBatch.h"
#include "render/RenderContext.h"
#include "render/RenderService.h"
#include "render/TextAnchor.h"

#include "kd/ranges/to.h"

#include "vm/mat.h"
#include "vm/mat_ext.h"
#include "vm/vec.h"

#include <array>
#include <cmath>
#include <ranges>
#include <vector>

namespace tb::render
{
namespace
{

class EntityClassnameAnchor : public TextAnchor3D
{
private:
  const mdl::EntityNode& m_entityNode;

public:
  explicit EntityClassnameAnchor(const mdl::EntityNode& entityNode)
    : m_entityNode{entityNode}
  {
  }

private:
  vm::vec3f basePosition() const override
  {
    return vm::vec3f{
      m_entityNode.logicalBounds().center().xy(),
      m_entityNode.logicalBounds().max.z() + 2.0};
  }

  TextAlignment::Type alignment() const override { return TextAlignment::Bottom; }
};

} // namespace

EntityRenderer::EntityRenderer(
  Logger& logger,
  mdl::EntityModelManager& entityModelManager,
  const mdl::EditorContext& editorContext)
  : m_entityModelManager{entityModelManager}
  , m_editorContext{editorContext}
  , m_modelRenderer{logger, m_entityModelManager, m_editorContext}
{
}

void EntityRenderer::invalidate()
{
  invalidateBounds();
  reloadModels();
}

void EntityRenderer::clear()
{
  m_entities.clear();
  m_pointEntityWireframeBoundsRenderer = DirectEdgeRenderer();
  m_brushEntityWireframeBoundsRenderer = DirectEdgeRenderer();
  m_solidBoundsRenderer = TriangleRenderer();
  m_modelRenderer.clear();
  m_paths.clear();
}

void EntityRenderer::reloadModels()
{
  m_modelRenderer.updateEntities(std::begin(m_entities), std::end(m_entities));
}

void EntityRenderer::addEntity(const mdl::EntityNode& entityNode)
{
  if (m_entities.insert(&entityNode).second)
  {
    m_modelRenderer.addEntity(entityNode);
    invalidateBounds();
  }
}

void EntityRenderer::removeEntity(const mdl::EntityNode& entityNode)
{
  if (auto it = m_entities.find(&entityNode); it != std::end(m_entities))
  {
    m_entities.erase(it);
    m_modelRenderer.removeEntity(entityNode);
    invalidateBounds();
  }
}

void EntityRenderer::invalidateEntity(const mdl::EntityNode& entityNode)
{
  m_modelRenderer.updateEntity(entityNode);
  invalidateBounds();
}

void EntityRenderer::invalidateEntityModels(
  const std::vector<const mdl::EntityModel*>& entityModels)
{
  const auto entityModelSet =
    std::unordered_set<const mdl::EntityModel*>{entityModels.begin(), entityModels.end()};
  for (const auto* entityNode : m_entities)
  {
    if (entityModelSet.contains(entityNode->entity().model()))
    {
      invalidateEntity(*entityNode);
    }
  }
}

void EntityRenderer::setShowOverlays(const bool showOverlays)
{
  m_showOverlays = showOverlays;
}

void EntityRenderer::setOverlayTextColor(const Color& overlayTextColor)
{
  m_overlayTextColor = overlayTextColor;
}

void EntityRenderer::setOverlayBackgroundColor(const Color& overlayBackgroundColor)
{
  m_overlayBackgroundColor = overlayBackgroundColor;
}

void EntityRenderer::setShowOccludedOverlays(const bool showOccludedOverlays)
{
  m_showOccludedOverlays = showOccludedOverlays;
}

void EntityRenderer::setTint(const bool tint)
{
  m_tint = tint;
}

void EntityRenderer::setTintColor(const Color& tintColor)
{
  m_tintColor = tintColor;
}

void EntityRenderer::setOverrideBoundsColor(const bool overrideBoundsColor)
{
  m_overrideBoundsColor = overrideBoundsColor;
}

void EntityRenderer::setBoundsColor(const Color& boundsColor)
{
  m_boundsColor = boundsColor;
}

void EntityRenderer::setShowOccludedBounds(const bool showOccludedBounds)
{
  m_showOccludedBounds = showOccludedBounds;
}

void EntityRenderer::setOccludedBoundsColor(const Color& occludedBoundsColor)
{
  m_occludedBoundsColor = occludedBoundsColor;
}

void EntityRenderer::setShowAngles(const bool showAngles)
{
  m_showAngles = showAngles;
}

void EntityRenderer::setAngleColor(const Color& angleColor)
{
  m_angleColor = angleColor;
}

void EntityRenderer::setShowHiddenEntities(const bool showHiddenEntities)
{
  m_showHiddenEntities = showHiddenEntities;
}

void EntityRenderer::renderOpaque(RenderContext& renderContext, RenderBatch& renderBatch)
{
  if (!m_entities.empty())
  {
    renderBounds(renderContext, renderBatch);
    renderModels(renderContext, renderBatch);
    renderClassnames(renderContext, renderBatch);
    renderAngles(renderContext, renderBatch);
    renderMuzzles(renderContext, renderBatch);
    renderPaths(renderContext, renderBatch);
  }
}

void EntityRenderer::renderTransparent(
  RenderContext& renderContext, RenderBatch& renderBatch)
{
  if (!m_entities.empty())
  {
    renderTransparentModels(renderContext, renderBatch);
  }
}

void EntityRenderer::renderPaths(RenderContext& renderContext, RenderBatch& renderBatch)
{
  if (m_paths.empty() || (!m_showHiddenEntities && !renderContext.showPointEntities()))
  {
    return;
  }
  auto service = RenderService{renderContext, renderBatch};
  for (const auto& [entityNode, geometry] : m_paths)
  {
    if (
      (!m_showHiddenEntities && !m_editorContext.visible(*entityNode))
      || geometry.nodes.empty())
    {
      continue;
    }
    const auto selected = entityNode->transitivelySelected();
    const auto color = m_overrideBoundsColor ? m_boundsColor : boundsColor(*entityNode);
    service.setForegroundColor(color);
    service.setLineWidth(selected ? 2.0f : 1.0f);
    if (selected)
    {
      service.setShowOccludedObjectsTransparent();
    }
    else
    {
      service.setHideOccludedObjects();
    }
    if (geometry.curve.size() >= 2)
    {
      service.renderLineStrip(geometry.curve);
    }
    if (!geometry.arrows.empty())
    {
      service.renderLines(geometry.arrows);
    }
    if (!geometry.rollMarkers.empty())
    {
      service.renderLines(geometry.rollMarkers);
    }
    service.renderHandles(geometry.nodes);
    // The first node identifies the start, including the seam of a closed path.
    service.renderHandleHighlight(geometry.nodes.front());
    if (selected)
    {
      service.renderString(
        geometry.closed ? "Closed path" : "Path start", geometry.nodes.front());
      service.setLineWidth(1.0f);
      if (!geometry.handleLines.empty())
      {
        service.renderLines(geometry.handleLines);
        service.renderHandles(geometry.handles);
      }
    }
  }
}

void EntityRenderer::renderBounds(RenderContext& renderContext, RenderBatch& renderBatch)
{
  if (!m_boundsValid)
  {
    validateBounds();
  }

  if (renderContext.showPointEntityBounds())
  {
    renderPointEntityWireframeBounds(renderBatch);
  }

  if (renderContext.showBrushEntityBounds())
  {
    renderBrushEntityWireframeBounds(renderBatch);
  }

  if (m_showHiddenEntities || renderContext.showPointEntities())
  {
    renderSolidBounds(renderBatch);
  }
}

void EntityRenderer::renderPointEntityWireframeBounds(RenderBatch& renderBatch)
{
  if (m_showOccludedBounds)
  {
    m_pointEntityWireframeBoundsRenderer.renderOnTop(
      renderBatch, m_overrideBoundsColor, m_occludedBoundsColor);
  }

  m_pointEntityWireframeBoundsRenderer.render(
    renderBatch, m_overrideBoundsColor, m_boundsColor);
}

void EntityRenderer::renderBrushEntityWireframeBounds(RenderBatch& renderBatch)
{
  if (m_showOccludedBounds)
  {
    m_brushEntityWireframeBoundsRenderer.renderOnTop(
      renderBatch, m_overrideBoundsColor, m_occludedBoundsColor);
  }

  m_brushEntityWireframeBoundsRenderer.render(
    renderBatch, m_overrideBoundsColor, m_boundsColor);
}

void EntityRenderer::renderSolidBounds(RenderBatch& renderBatch)
{
  m_solidBoundsRenderer.setApplyTinting(m_tint);
  m_solidBoundsRenderer.setTintColor(m_tintColor);
  renderBatch.add(&m_solidBoundsRenderer);
}

void EntityRenderer::renderModels(RenderContext& renderContext, RenderBatch& renderBatch)
{
  if (
    m_showHiddenEntities
    || (renderContext.showPointEntities() && renderContext.showPointEntityModels()))
  {
    m_modelRenderer.setApplyTinting(m_tint);
    m_modelRenderer.setTintColor(m_tintColor);
    m_modelRenderer.setShowHiddenEntities(m_showHiddenEntities);
    m_modelRenderer.renderOpaque(renderBatch);
  }
}

void EntityRenderer::renderTransparentModels(
  RenderContext& renderContext, RenderBatch& renderBatch)
{
  if (
    m_showHiddenEntities
    || (renderContext.showPointEntities() && renderContext.showPointEntityModels()))
  {
    m_modelRenderer.setApplyTinting(m_tint);
    m_modelRenderer.setTintColor(m_tintColor);
    m_modelRenderer.setShowHiddenEntities(m_showHiddenEntities);
    m_modelRenderer.renderTransparent(renderBatch);
  }
}

void EntityRenderer::renderClassnames(
  RenderContext& renderContext, RenderBatch& renderBatch)
{
  if (m_showOverlays && renderContext.showEntityClassnames())
  {
    auto renderService = render::RenderService{renderContext, renderBatch};
    renderService.setForegroundColor(m_overlayTextColor);
    renderService.setBackgroundColor(m_overlayBackgroundColor);

    for (const auto* entityNode : m_entities)
    {
      if (m_showHiddenEntities || m_editorContext.visible(*entityNode))
      {
        if (
          !entityNode->containingGroup()
          || entityNode->containingGroup() == m_editorContext.currentGroup())
        {
          if (m_showOccludedOverlays)
          {
            renderService.setShowOccludedObjects();
          }
          else
          {
            renderService.setHideOccludedObjects();
          }

          renderService.renderString(
            entityString(*entityNode), EntityClassnameAnchor{*entityNode});
        }
      }
    }
  }
}

void EntityRenderer::renderAngles(RenderContext& renderContext, RenderBatch& renderBatch)
{
  static constexpr auto maxDistance2 = 500.0f * 500.0f;

  if (m_showAngles)
  {
    const auto arrow = arrowHead(9.0f, 6.0f);

    auto renderService = RenderService{renderContext, renderBatch};
    renderService.setShowOccludedObjectsTransparent();
    renderService.setForegroundColor(m_angleColor);

    for (const auto* entityNode : m_entities)
    {
      if (!m_showHiddenEntities && !m_editorContext.visible(*entityNode))
      {
        continue;
      }

      const auto rotation = vm::mat4x4f{entityNode->entity().rotation()};
      const auto direction = rotation * vm::vec3f{1, 0, 0};
      const auto center = vm::vec3f{entityNode->logicalBounds().center()};

      const auto toCam = renderContext.camera().position() - center;
      // only distance cull for perspective camera, since the 2D one is always very far
      // from the level
      if (
        renderContext.camera().perspectiveProjection()
        && vm::squared_length(toCam) > maxDistance2)
      {
        continue;
      }

      auto onPlane = toCam - vm::dot(toCam, direction) * direction;
      if (vm::is_zero(onPlane, vm::Cf::almost_zero()))
      {
        continue;
      }

      onPlane = vm::normalize(onPlane);

      const auto rotZ = rotation * vm::vec3f{0, 0, 1};
      const auto angle = -vm::measure_angle(rotZ, onPlane, direction);
      const auto matrix = vm::translation_matrix(center)
                          * vm::rotation_matrix(direction, angle) * rotation
                          * vm::translation_matrix(16.0f * vm::vec3f{0, 0, 1});


      const auto vertices =
        arrow | std::views::transform([&](const auto& x) { return matrix * x; })
        | kdl::ranges::to<std::vector>();
      renderService.renderPolygonOutline(vertices);
    }
  }
}

void EntityRenderer::renderMuzzles(RenderContext& renderContext, RenderBatch& renderBatch)
{
  // An entity whose "muzzle" property names another entity fires from that entity
  // instead, which marks its own muzzle if its class has one.
  static constexpr auto MuzzleOverrideKey = "muzzle";
  // Lengths in map units: the arrow out of the muzzle, its head, and the aim line
  // running on from its tip.
  static constexpr auto ArrowLength = 14.0;
  static constexpr auto ArrowHeadLength = 4.0;
  static constexpr auto ArrowHeadWidth = 1.5;
  static constexpr auto AimLength = 128.0;

  if (!m_showHiddenEntities && !renderContext.showPointEntities())
  {
    return;
  }

  const auto arrowColor = Color{RgbaF{1.0f, 0.72f, 0.1f, 1.0f}};
  const auto aimColor = Color{RgbaF{1.0f, 0.72f, 0.1f, 0.4f}};

  auto renderService = RenderService{renderContext, renderBatch};
  // The muzzle sits on the model's surface, so it shows through the model.
  renderService.setShowOccludedObjectsTransparent();

  for (const auto* entityNode : m_entities)
  {
    if (!m_showHiddenEntities && !m_editorContext.visible(*entityNode))
    {
      continue;
    }

    const auto& entity = entityNode->entity();
    const auto* pointDefinition = mdl::getPointEntityDefinition(entity.definition());
    if (!pointDefinition || !pointDefinition->muzzle)
    {
      continue;
    }
    if (const auto* target = entity.property(MuzzleOverrideKey);
        target && !target->empty())
    {
      continue;
    }

    const auto& transformation = entity.modelTransformation(
      entityNode->entityPropertyConfig().defaultModelScaleExpression);
    const auto position = transformation * pointDefinition->muzzle->position;
    const auto scaledDirection =
      vm::strip_translation(transformation) * pointDefinition->muzzle->direction;
    if (vm::is_zero(scaledDirection, vm::Cd::almost_zero()))
    {
      continue;
    }
    const auto direction = vm::normalize(scaledDirection);
    const auto side = vm::normalize(vm::cross(
      direction,
      std::abs(direction.z()) < 0.9 ? vm::vec3d{0, 0, 1} : vm::vec3d{1, 0, 0}));
    const auto up = vm::cross(side, direction);

    const auto tip = position + direction * ArrowLength;
    const auto headBase = tip - direction * ArrowHeadLength;
    const auto arrow = std::vector{
      vm::vec3f{position},
      vm::vec3f{tip},
      vm::vec3f{tip},
      vm::vec3f{headBase + side * ArrowHeadWidth},
      vm::vec3f{tip},
      vm::vec3f{headBase - side * ArrowHeadWidth},
      vm::vec3f{tip},
      vm::vec3f{headBase + up * ArrowHeadWidth},
      vm::vec3f{tip},
      vm::vec3f{headBase - up * ArrowHeadWidth},
    };

    renderService.setLineWidth(entityNode->transitivelySelected() ? 2.0f : 1.0f);
    renderService.setForegroundColor(arrowColor);
    renderService.renderLines(arrow);
    renderService.renderHandle(vm::vec3f{position});
    renderService.setForegroundColor(aimColor);
    renderService.renderLine(vm::vec3f{tip}, vm::vec3f{tip + direction * AimLength});
  }
}

std::vector<vm::vec3f> EntityRenderer::arrowHead(
  const float length, const float width) const
{
  // clockwise winding
  return std::vector{
    vm::vec3f{0.0f, width / 2.0f, 0.0f},
    vm::vec3f{length, 0.0f, 0.0f},
    vm::vec3f{0.0f, -width / 2.0f, 0.0f},
  };
}

void EntityRenderer::invalidateBounds()
{
  m_boundsValid = false;
}

namespace
{

auto makeWireFrameBoundsVertexBuilder(std::vector<gl::VertexTypes::P3::Vertex>& vertices)
{
  return [&](const vm::vec3d& v1, const vm::vec3d& v2) {
    vertices.emplace_back(vm::vec3f{v1});
    vertices.emplace_back(vm::vec3f{v2});
  };
}

auto makeColoredWireFrameBoundsVertexBuilder(
  std::vector<gl::VertexTypes::P3C4::Vertex>& vertices, const Color& color)
{
  return [&](const vm::vec3d& v1, const vm::vec3d& v2) {
    vertices.emplace_back(vm::vec3f{v1}, color.to<RgbaF>().toVec());
    vertices.emplace_back(vm::vec3f{v2}, color.to<RgbaF>().toVec());
  };
}

/**
 * Fills an area light's panel: the light's color on the side it shines from, dark grey
 * behind. Map views cull back faces, so each side shows only from its front.
 */
void addAreaLightPanel(
  std::vector<gl::VertexTypes::P3NC4::Vertex>& vertices, const mdl::AreaLight& light)
{
  static const auto backColor = vm::vec4f{0.15f, 0.15f, 0.15f, 1.0f};
  const auto rgb = light.color.toVec();
  const auto frontColor = vm::vec4f{rgb.x(), rgb.y(), rgb.z(), 1.0f};
  const auto normal = vm::vec3f{light.normal()};
  const auto corners = light.corners();

  // corners() runs counterclockwise seen from the front, and front faces wind
  // clockwise.
  for (const auto& corner : corners | std::views::reverse)
  {
    vertices.emplace_back(vm::vec3f{corner}, normal, frontColor);
  }
  for (const auto& corner : corners)
  {
    vertices.emplace_back(vm::vec3f{corner}, -normal, backColor);
  }
}

auto makeColoredSolidBoundsVertexBuilder(
  std::vector<gl::VertexTypes::P3NC4::Vertex>& vertices, const Color& color)
{
  return [&](
           const vm::vec3d& v1,
           const vm::vec3d& v2,
           const vm::vec3d& v3,
           const vm::vec3d& v4,
           const vm::vec3d& n) {
    vertices.emplace_back(vm::vec3f{v1}, vm::vec3f{n}, color.to<RgbaF>().toVec());
    vertices.emplace_back(vm::vec3f{v2}, vm::vec3f{n}, color.to<RgbaF>().toVec());
    vertices.emplace_back(vm::vec3f{v3}, vm::vec3f{n}, color.to<RgbaF>().toVec());
    vertices.emplace_back(vm::vec3f{v4}, vm::vec3f{n}, color.to<RgbaF>().toVec());
  };
}

} // namespace

void EntityRenderer::validateBounds()
{
  m_paths.clear();
  for (const auto* entityNode : m_entities)
  {
    if (!entityNode->hasChildren())
    {
      if (auto geometry = makePathRenderData(entityNode->entity()))
      {
        m_paths.emplace_back(entityNode, std::move(*geometry));
      }
    }
  }
  auto solidVertices = std::vector<gl::VertexTypes::P3NC4::Vertex>{};
  solidVertices.reserve(36 * m_entities.size());

  if (m_overrideBoundsColor)
  {
    using Vertex = gl::VertexTypes::P3::Vertex;
    auto pointEntityWireframeVertices = std::vector<Vertex>{};
    auto brushEntityWireframeVertices = std::vector<Vertex>{};

    pointEntityWireframeVertices.reserve(24 * m_entities.size());
    brushEntityWireframeVertices.reserve(24 * m_entities.size());

    for (const auto* entityNode : m_entities)
    {
      if (m_editorContext.visible(*entityNode))
      {
        const auto pointEntity = !entityNode->hasChildren();
        if (pointEntity)
        {
          for (const auto& vertex : entityNode->boundsEdgeVertices())
          {
            pointEntityWireframeVertices.emplace_back(vm::vec3f{vertex});
          }

          if (const auto projector = mdl::imageProjector(entityNode->entity()))
          {
            const auto depth = projector->localBounds.size().z();
            const auto tip = vm::vec3d{0, 0, projector->localBounds.min.z()};
            const auto arrow = std::array{
              vm::vec3d{0, 0, 0},
              tip,
              tip,
              vm::vec3d{depth * 0.12, 0, -depth * 0.3},
              tip,
              vm::vec3d{-depth * 0.12, 0, -depth * 0.3},
              tip,
              vm::vec3d{0, depth * 0.12, -depth * 0.3},
              tip,
              vm::vec3d{0, -depth * 0.12, -depth * 0.3}};
            for (const auto& vertex : arrow)
            {
              pointEntityWireframeVertices.emplace_back(
                vm::vec3f{projector->transformation * vertex});
            }
          }

          const auto hasModel =
            entityNode->entity().model() && entityNode->entity().model()->data();
          if (const auto light = mdl::areaLight(entityNode->entity()))
          {
            addAreaLightPanel(solidVertices, *light);
          }
          else if (!hasModel && !mdl::imageProjector(entityNode->entity()))
          {
            entityNode->logicalBounds().for_each_face(
              makeColoredSolidBoundsVertexBuilder(solidVertices, m_boundsColor));
          }
        }
        else
        {
          entityNode->logicalBounds().for_each_edge(
            makeWireFrameBoundsVertexBuilder(brushEntityWireframeVertices));
        }
      }
    }

    m_pointEntityWireframeBoundsRenderer = DirectEdgeRenderer{
      gl::VertexArray::move(std::move(pointEntityWireframeVertices)),
      gl::PrimType::Lines};
    m_brushEntityWireframeBoundsRenderer = DirectEdgeRenderer{
      gl::VertexArray::move(std::move(brushEntityWireframeVertices)),
      gl::PrimType::Lines};
  }
  else
  {
    using Vertex = gl::VertexTypes::P3C4::Vertex;
    auto pointEntityWireframeVertices = std::vector<Vertex>{};
    auto brushEntityWireframeVertices = std::vector<Vertex>{};

    pointEntityWireframeVertices.reserve(24 * m_entities.size());
    brushEntityWireframeVertices.reserve(24 * m_entities.size());

    for (auto* entityNode : m_entities)
    {
      if (m_editorContext.visible(*entityNode))
      {
        const auto isPointEntity = !entityNode->hasChildren();
        if (isPointEntity)
        {
          for (const auto& vertex : entityNode->boundsEdgeVertices())
          {
            pointEntityWireframeVertices.emplace_back(
              vm::vec3f{vertex}, boundsColor(*entityNode).to<RgbaF>().toVec());
          }

          if (const auto projector = mdl::imageProjector(entityNode->entity()))
          {
            const auto depth = projector->localBounds.size().z();
            const auto tip = vm::vec3d{0, 0, projector->localBounds.min.z()};
            const auto arrow = std::array{
              vm::vec3d{0, 0, 0},
              tip,
              tip,
              vm::vec3d{depth * 0.12, 0, -depth * 0.3},
              tip,
              vm::vec3d{-depth * 0.12, 0, -depth * 0.3},
              tip,
              vm::vec3d{0, depth * 0.12, -depth * 0.3},
              tip,
              vm::vec3d{0, -depth * 0.12, -depth * 0.3}};
            const auto color = boundsColor(*entityNode).to<RgbaF>().toVec();
            for (const auto& vertex : arrow)
            {
              pointEntityWireframeVertices.emplace_back(
                vm::vec3f{projector->transformation * vertex}, color);
            }
          }

          const auto hasModel =
            entityNode->entity().model() && entityNode->entity().model()->data();
          if (const auto light = mdl::areaLight(entityNode->entity()))
          {
            addAreaLightPanel(solidVertices, *light);
          }
          else if (!hasModel && !mdl::imageProjector(entityNode->entity()))
          {
            entityNode->logicalBounds().for_each_face(makeColoredSolidBoundsVertexBuilder(
              solidVertices, boundsColor(*entityNode)));
          }
        }
        else
        {
          entityNode->logicalBounds().for_each_edge(
            makeColoredWireFrameBoundsVertexBuilder(
              brushEntityWireframeVertices, boundsColor(*entityNode)));
        }
      }
    }

    m_pointEntityWireframeBoundsRenderer = DirectEdgeRenderer(
      gl::VertexArray::move(std::move(pointEntityWireframeVertices)),
      gl::PrimType::Lines);
    m_brushEntityWireframeBoundsRenderer = DirectEdgeRenderer(
      gl::VertexArray::move(std::move(brushEntityWireframeVertices)),
      gl::PrimType::Lines);
  }

  m_solidBoundsRenderer = TriangleRenderer{
    gl::VertexArray::move(std::move(solidVertices)), gl::PrimType::Quads};
  m_boundsValid = true;
}

gl::AttrString EntityRenderer::entityString(const mdl::EntityNode& entityNode) const
{
  const auto& classname = entityNode.entity().classname();
  // const mdl::AttributeValue& targetname =
  // entity->attribute(mdl::AttributeNames::Targetname);

  auto str = gl::AttrString{};
  str.appendCentered(classname);
  // if (!targetname.empty())
  // str.appendCentered(targetname);
  return str;
}

const Color& EntityRenderer::boundsColor(const mdl::EntityNode& entityNode) const
{
  if (const auto* definition = entityNode.entity().definition())
  {
    return definition->color;
  }
  return m_boundsColor;
}

} // namespace tb::render
