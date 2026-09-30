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

#include "mdl/Prefab.h"

#include "base/SimpleParserStatus.h"
#include "base/Uuid.h"
#include "fs/DiskIO.h"
#include "fs/PathInfo.h"
#include "fs/TraversalMode.h"
#include "mdl/GameInfo.h"
#include "mdl/GroupNode.h"
#include "mdl/Map.h"
#include "mdl/MapHeader.h"
#include "mdl/Map_CopyPaste.h"
#include "mdl/Map_Groups.h"
#include "mdl/ModelUtils.h"
#include "mdl/NodeReader.h"
#include "mdl/NodeWriter.h"
#include "mdl/Transaction.h"
#include "mdl/WorldNode.h"

#include "kd/path_utils.h"
#include "kd/string_compare.h"
#include "kd/string_utils.h"
#include "kd/vector_utils.h"

#include <fmt/format.h>
#include <fmt/std.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <sstream>
#include <unordered_map>

namespace tb::mdl
{
namespace
{

constexpr auto PrefabExtension = ".map";

bool isReservedFileName(const std::string_view name)
{
  static const auto reservedNames = std::array<std::string_view, 22>{
    "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
    "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
    "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

  return std::ranges::any_of(reservedNames, [&](const auto& reserved) {
    return kdl::ci::str_is_equal(name, reserved);
  });
}

Result<void> writePrefab(
  Map& map, const std::vector<Node*>& nodes, const std::filesystem::path& path)
{
  const auto directory = path.parent_path();

  // Write a temporary file and move it over the prefab so that a failed write leaves the
  // old prefab intact and a prefab that is hard linked elsewhere is replaced rather than
  // changed in place.
  const auto tempPath = kdl::path_add_extension(path, ".tmp");

  return fs::Disk::createDirectory(directory) | kdl::and_then([&](auto) {
           return fs::Disk::withOutputStream(tempPath, [&](auto& stream) {
             writeMapHeader(
               stream,
               map.gameInfo().gameConfig.name,
               map.worldNode().mapFormat(),
               "CautionTrench prefab");

             auto writer = NodeWriter{map.worldNode(), stream};
             writer.writeNodes(nodes, map.taskManager());
           });
         })
         | kdl::and_then([&]() { return fs::Disk::moveFile(tempPath, path); })
         | kdl::or_else([&](auto e) -> Result<void> {
             auto error = std::error_code{};
             std::filesystem::remove(tempPath, error);
             return Error{fmt::format("Could not save prefab {}: {}", path, e.msg)};
           });
}

std::string pathToUtf8(const std::filesystem::path& path)
{
  const auto str = path.u8string();
  return std::string{str.begin(), str.end()};
}

Result<std::string> readPrefabFile(const std::filesystem::path& path)
{
  return fs::Disk::withInputStream(path, [](auto& stream) {
    return std::string{
      std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
  });
}

void replaceGroupLinkIds(const std::vector<Node*>& nodes)
{
  auto newLinkIds = std::unordered_map<std::string, std::string>{};
  for (auto* groupNode : collectGroups(nodes))
  {
    auto [it, inserted] = newLinkIds.try_emplace(groupNode->linkId());
    if (inserted)
    {
      it->second = generateUuid();
    }
    groupNode->setLinkId(it->second);
  }
}

} // namespace

std::filesystem::path prefabDirectory(const Map& map)
{
  const auto& gameConfig = map.gameInfo().gameConfig;
  if (const auto& gamePath = map.gamePath(); !gamePath.empty())
  {
    return gamePath / gameConfig.fileSystemConfig.searchPath / "prefabs";
  }
  return gameConfig.path.parent_path() / "prefabs";
}

std::filesystem::path prefabPath(
  const std::filesystem::path& directory, const std::string_view name)
{
  return directory / kdl::parse_utf8_path(std::string{name} + PrefabExtension, false);
}

Result<void> validatePrefabName(const std::string_view name)
{
  if (kdl::str_is_blank(name))
  {
    return Error{"Prefab names cannot be blank."};
  }

  static constexpr auto invalidChars = std::string_view{R"(<>:"/\|?*)"};
  if (std::ranges::any_of(name, [](const auto c) {
        return invalidChars.find(c) != std::string_view::npos
               || static_cast<unsigned char>(c) < 32u;
      }))
  {
    return Error{fmt::format(
      "Prefab names cannot contain control characters or any of {}", invalidChars)};
  }

  if (name.back() == '.' || name.back() == ' ')
  {
    return Error{"Prefab names cannot end with a dot or a space."};
  }

  if (isReservedFileName(name))
  {
    return Error{fmt::format("'{}' is a reserved file name.", name)};
  }

  return kdl::void_success;
}

std::vector<PrefabFile> findPrefabs(const std::filesystem::path& directory)
{
  if (fs::Disk::pathInfo(directory) != fs::PathInfo::Directory)
  {
    return {};
  }

  return fs::Disk::find(
           directory,
           fs::TraversalMode::Flat,
           [](const auto& path, const auto& getPathInfo) {
             return getPathInfo(path) == fs::PathInfo::File
                    && kdl::path_to_lower(path.extension()) == PrefabExtension;
           })
         | kdl::transform([](const auto& paths) {
             auto prefabs = std::vector<PrefabFile>{};
             for (const auto& path : paths)
             {
               prefabs.push_back({pathToUtf8(path.stem()), path});
             }
             std::ranges::sort(prefabs, [](const auto& lhs, const auto& rhs) {
               return kdl::ci::str_compare(lhs.name, rhs.name) < 0;
             });
             return prefabs;
           })
         | kdl::value_or(std::vector<PrefabFile>{});
}

Result<GroupNode*> createPrefabFromSelection(
  Map& map, const std::string& name, const std::filesystem::path& path)
{
  const auto& selectedNodes = map.selection().nodes;
  if (selectedNodes.empty())
  {
    return Error{"Nothing is selected"};
  }

  auto transaction = Transaction{map, "Create Prefab"};

  auto* groupNode = static_cast<GroupNode*>(nullptr);
  if (selectedNodes.size() == 1u)
  {
    groupNode = dynamic_cast<GroupNode*>(selectedNodes.front());
  }

  if (groupNode)
  {
    renameSelectedGroups(map, name);
  }
  else
  {
    groupNode = groupSelectedNodes(map, name);
  }

  if (!groupNode)
  {
    transaction.cancel();
    return Error{"Could not group the selected objects"};
  }

  return writePrefab(map, {groupNode}, path) | kdl::transform([&]() {
           transaction.commit();
           return groupNode;
         })
         | kdl::or_else([&](auto e) -> Result<GroupNode*> {
             transaction.cancel();
             return e;
           });
}

Result<std::vector<Node*>> readPrefab(Map& map, const std::filesystem::path& path)
{
  return readPrefabFile(path) | kdl::and_then([&](const auto& str) {
           auto parserStatus = SimpleParserStatus{map.logger()};
           return NodeReader::read(
             str,
             map.worldNode().mapFormat(),
             map.worldBounds(),
             map.worldNode().entityPropertyConfig(),
             parserStatus,
             map.taskManager());
         })
         | kdl::transform([](auto nodes) {
             replaceGroupLinkIds(nodes);
             return nodes;
           })
         | kdl::or_else([&](auto e) -> Result<std::vector<Node*>> {
             return Error{fmt::format("Could not read prefab {}: {}", path, e.msg)};
           });
}

Result<void> placePrefab(Map& map, const std::filesystem::path& path)
{
  return readPrefab(map, path) | kdl::and_then([&](auto nodes) -> Result<void> {
           if (nodes.empty())
           {
             return Error{fmt::format("Prefab {} is empty", path)};
           }
           if (!pasteNodes(map, nodes))
           {
             return Error{fmt::format("Could not place prefab {}", path)};
           }
           return kdl::void_success;
         });
}

Result<std::filesystem::path> renamePrefab(
  const std::filesystem::path& path, const std::string_view newName)
{
  return validatePrefabName(newName) | kdl::and_then([&]() {
           const auto newPath = prefabPath(path.parent_path(), newName);

           // Where file names are case insensitive, a new name that differs only in case
           // denotes the same file, and renaming it changes the case. fs::Disk is not
           // used here because it resolves such paths to the existing file's name.
           auto error = std::error_code{};
           if (
             std::filesystem::exists(newPath, error)
             && !std::filesystem::equivalent(path, newPath, error))
           {
             return Result<std::filesystem::path>{
               Error{fmt::format("A prefab named '{}' already exists", newName)}};
           }

           std::filesystem::rename(path, newPath, error);
           if (error)
           {
             return Result<std::filesystem::path>{Error{fmt::format(
               "Could not rename {} to {}: {}", path, newPath, error.message())}};
           }
           return Result<std::filesystem::path>{newPath};
         });
}

} // namespace tb::mdl
