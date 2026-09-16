/*
 Copyright (C) 2026 Kristian Duske

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

#include "mdl/Map_Patches.h"

#include "mdl/ApplyAndSwap.h"
#include "mdl/BezierPatch.h"
#include "mdl/ControlPointCommand.h"
#include "mdl/Map.h"
#include "mdl/Map_Groups.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Selection.h"
#include "mdl/ModelUtils.h"
#include "mdl/PatchNode.h"
#include "mdl/PatchUtils.h"
#include "mdl/PathEntity.h"
#include "mdl/Selection.h"
#include "mdl/Transaction.h"

#include "kd/overload.h"
#include "kd/ranges/to.h"
#include "kd/string_format.h"
#include "kd/vector_utils.h"

#include "vm/mat_ext.h"

#include <ranges>

namespace tb::mdl
{

void convertSelectionToPatches(
  Map& map, const size_t pointRowCount, const size_t pointColumnCount)
{
  const auto& selection = map.selection();
  contract_assert(selection.hasAnyBrushFaces());

  auto patchNodes =
    selection.allBrushFaces() | std::views::transform([&](const auto& faceHandle) {
      return createPatch(faceHandle.face(), pointRowCount, pointColumnCount);
    })
    | std::views::join | std::views::transform([](auto patch) {
        return static_cast<Node*>(
          std::make_unique<PatchNode>(std::move(patch)).release());
      })
    | kdl::ranges::to<std::vector>();

  auto nodesToRemove = selection.allBrushFaces()
                       | std::views::transform([](const auto& faceHandle) {
                           return static_cast<Node*>(faceHandle.node());
                         })
                       | kdl::ranges::to<std::vector>();
  kdl::vec_sort_and_remove_duplicates(nodesToRemove);

  auto transaction = Transaction{map, "Convert Selection to Patches"};

  auto addedNodes = addNodes(map, {{&parentForNodes(map), patchNodes}});
  deselectAll(map);
  removeNodes(map, nodesToRemove);
  selectNodes(map, addedNodes);

  transaction.commit();
}

bool resamplePatches(Map& map, const size_t pointRowCount, const size_t pointColumnCount)
{
  const auto allSelectedHandlePositions =
    map.nodeHandles().selectedHandles<ControlPointHandle>()
    | std::views::transform([](const auto& handle) { return handle.position; })
    | kdl::ranges::to<std::vector>();

  auto newNodes = applyToNodeContents(
    map.selection().patches,
    kdl::overload(
      [](Layer&) { return true; },
      [](Group&) { return true; },
      [](Entity&) { return true; },
      [](Brush&) { return true; },
      [&](BezierPatch& patch) {
        patch = resamplePatch(patch, pointRowCount, pointColumnCount);
        return true;
      }));

  if (!newNodes)
  {
    return false;
  }

  auto transaction = Transaction{map, "Resample Patch"};

  const auto changedLinkedGroups = collectContainingGroups(
    *newNodes | std::views::keys | kdl::ranges::to<std::vector>());

  auto command = std::make_unique<ControlPointCommand>(
    "Resample Patch",
    std::move(*newNodes),
    allSelectedHandlePositions,
    std::vector<ControlPointHandle::Position>{});

  if (!map.executeAndStore(std::move(command)))
  {
    transaction.cancel();
    return false;
  }

  setHasPendingChanges(changedLinkedGroups, true);

  return transaction.commit();
}

bool transformControlPoints(
  Map& map,
  const std::vector<vm::vec3d>& controlPointPositions,
  const vm::mat4x4d& transform)
{
  const auto hasPathEntity = std::ranges::any_of(
    map.selection().allEntities(),
    [](const auto* entityNode) { return readPath(entityNode->entity()).is_success(); });
  auto transformPathNodes = [&]() {
    const auto positions =
      std::set<vm::vec3d>{controlPointPositions.begin(), controlPointPositions.end()};
    return applyToNodeContents(
      map.selection().allEntities(),
      kdl::overload(
        [&](Entity& entity) {
          const auto pathResult = readPath(entity);
          if (!pathResult)
          {
            return true;
          }
          auto path = pathResult.value();
          const auto entityTransform =
            vm::translation_matrix(entity.origin()) * entity.rotation();
          const auto inverseTransform = vm::invert(entityTransform);
          if (!inverseTransform)
          {
            return false;
          }
          if (path.kind == PathKind::Bezier)
          {
            for (size_t i = 0; i < path.segmentCount(); ++i)
            {
              const auto controls = path.segmentControls(i);
              const auto updateHandle = [&](const size_t nodeIndex,
                                            std::optional<vm::vec3d>& handle,
                                            const vm::vec3d& worldPosition) {
                  if (positions.contains(worldPosition))
                  {
                    handle = *inverseTransform * (transform * worldPosition);
                    path.nodes[nodeIndex].handleMode = PathHandleMode::Free;
                  }
                };
              updateHandle(i, path.nodes[i].handleOut, entityTransform * controls[1]);
              const auto next = (i + 1) % path.nodes.size();
              updateHandle(next, path.nodes[next].handleIn, entityTransform * controls[2]);
            }
          }
          for (auto& node : path.nodes)
          {
            const auto worldPosition = entityTransform * node.position;
            if (positions.contains(worldPosition))
            {
              node.position = *inverseTransform * (transform * worldPosition);
            }
          }
          return writePath(entity, path).is_success();
        },
        [](Layer&) { return true; },
        [](Group&) { return true; },
        [](Brush&) { return true; },
        [](BezierPatch&) { return true; }));
  };

  const auto controlPointPositionSet =
    std::set<vm::vec3d>{controlPointPositions.begin(), controlPointPositions.end()};

  auto newNodes = hasPathEntity ? transformPathNodes()
                                : applyToNodeContents(
                                    map.selection().patches,
                                    kdl::overload(
                                      [](Layer&) { return true; },
                                      [](Group&) { return true; },
                                      [](Entity&) { return true; },
                                      [](Brush&) { return true; },
                                      [&](BezierPatch& patch) {
                                        patch.transformControlPoints(
                                          controlPointPositionSet, transform);
                                        return true;
                                      }));

  if (!newNodes)
  {
    return false;
  }

  auto newControlPointPositions = transform * controlPointPositions;
  kdl::vec_sort_and_remove_duplicates(newControlPointPositions);

  auto commandName = kdl::str_plural(
    controlPointPositions.size(), "Move Control Point", "Move Control Points");
  auto transaction = Transaction{map, commandName};

  const auto changedLinkedGroups = collectContainingGroups(
    *newNodes | std::views::keys | kdl::ranges::to<std::vector>());

  auto command = std::make_unique<ControlPointCommand>(
    std::move(commandName),
    std::move(*newNodes),
    controlPointPositions,
    newControlPointPositions);

  if (!map.executeAndStore(std::move(command)))
  {
    transaction.cancel();
    return false;
  }

  setHasPendingChanges(changedLinkedGroups, true);

  return transaction.commit();
}

bool setPatchMaterial(Map& map, const std::string& materialName)
{
  const auto patchNodes = map.selection().patches;
  return applyAndSwap(
    map,
    "Set Material",
    patchNodes,
    collectContainingGroups(kdl::vec_static_cast<Node*>(patchNodes)),
    kdl::overload(
      [](Layer&) { return true; },
      [](Group&) { return true; },
      [](Entity&) { return true; },
      [](Brush&) { return true; },
      [&](BezierPatch& patch) {
        patch.setMaterialName(materialName);
        return true;
      }));
}

} // namespace tb::mdl
