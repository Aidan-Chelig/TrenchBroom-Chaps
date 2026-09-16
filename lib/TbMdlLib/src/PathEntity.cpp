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

#include "mdl/PathEntity.h"

#include "mdl/Entity.h"
#include "mdl/EntityProperties.h"

#include <fmt/format.h>

#include <charconv>
#include <cmath>
#include <locale>
#include <set>
#include <sstream>
#include <string_view>

namespace tb::mdl
{
namespace
{
constexpr size_t MaxPathNodes = 65536;

std::optional<size_t> parseIndex(const std::string_view value)
{
  auto result = size_t{0};
  const auto [end, error] =
    std::from_chars(value.data(), value.data() + value.size(), result);
  if (value.empty() || error != std::errc{} || end != value.data() + value.size())
  {
    return std::nullopt;
  }
  return result;
}

bool isPointProperty(const std::string_view key)
{
  if (!key.starts_with("point_"))
  {
    return false;
  }
  const auto tail = key.substr(6);
  const auto end = tail.find_first_not_of("0123456789");
  if (tail.empty() || end == 0)
  {
    return false;
  }
  const auto suffix =
    end == std::string_view::npos ? std::string_view{} : tail.substr(end);
  return suffix.empty() || suffix == "_in" || suffix == "_out" || suffix == "_mode"
         || suffix == "_roll";
}

bool isPathProperty(const std::string_view key)
{
  return key == "path_version" || key == "path_type" || key == "closed"
         || key == "point_count" || isPointProperty(key);
}

bool finite(const vm::vec3d& value)
{
  return std::isfinite(value.x()) && std::isfinite(value.y()) && std::isfinite(value.z());
}

std::optional<vm::vec3d> parsePosition(const std::string* value)
{
  if (!value)
  {
    return std::nullopt;
  }
  auto input = std::istringstream{*value};
  input.imbue(std::locale::classic());
  auto result = vm::vec3d{};
  if (!(input >> result[0] >> result[1] >> result[2]) || !finite(result))
  {
    return std::nullopt;
  }
  input >> std::ws;
  return input.eof() ? std::optional{result} : std::nullopt;
}

std::string positionString(const vm::vec3d& position)
{
  return fmt::format("{:.17g} {:.17g} {:.17g}", position.x(), position.y(), position.z());
}
} // namespace

Result<Path> readPath(const Entity& entity)
{
  if (!entity.hasProperty("path_version", "1"))
  {
    return Error{"Missing or unsupported path_version (expected 1)"};
  }
  auto result = Path{};
  const auto* kind = entity.property("path_type");
  if (!kind)
  {
    return Error{"Missing path_type"};
  }
  if (*kind == "linear")
  {
    result.kind = PathKind::Linear;
  }
  else if (*kind == "catmull_rom")
  {
    result.kind = PathKind::CatmullRom;
  }
  else if (*kind == "bezier")
  {
    result.kind = PathKind::Bezier;
  }
  else
  {
    return Error{"Unknown path_type"};
  }
  if (entity.hasProperty("closed", "1") || entity.hasProperty("closed", "true"))
  {
    result.closed = true;
  }
  else if (!entity.hasProperty("closed", "0") && !entity.hasProperty("closed", "false"))
  {
    return Error{"Expected closed to be 0 or 1"};
  }
  const auto* countString = entity.property("point_count");
  const auto count = countString ? parseIndex(*countString) : std::nullopt;
  if (!count || *count > MaxPathNodes)
  {
    return Error{"Invalid point_count (maximum 65536)"};
  }

  auto seen = std::set<std::string>{};
  for (const auto& property : entity.properties())
  {
    if (isPathProperty(property.key()) && !seen.insert(property.key()).second)
    {
      return Error{fmt::format("Duplicate path property '{}'", property.key())};
    }
    if (isPointProperty(property.key()))
    {
      const auto tail = std::string_view{property.key()}.substr(6);
      const auto indexText = tail.substr(0, tail.find('_'));
      const auto index = parseIndex(indexText);
      if (!index || *index >= *count || indexText != std::to_string(*index))
      {
        return Error{fmt::format("Invalid path point index in '{}'", property.key())};
      }
    }
  }
  result.nodes.reserve(*count);
  for (size_t i = 0; i < *count; ++i)
  {
    const auto key = fmt::format("point_{}", i);
    const auto position = parsePosition(entity.property(key));
    if (!position)
    {
      return Error{fmt::format("Missing or invalid '{}'", key)};
    }
    auto node = PathNode{};
    node.position = *position;
    if (const auto* roll = entity.property(key + "_roll"))
    {
      auto input = std::istringstream{*roll};
      input.imbue(std::locale::classic());
      if (!(input >> node.roll) || !std::isfinite(node.roll))
      {
        return Error{fmt::format("Invalid '{}_roll'", key)};
      }
      input >> std::ws;
      if (!input.eof())
      {
        return Error{fmt::format("Invalid '{}_roll'", key)};
      }
    }
    for (const auto& [suffix, handle] :
         {std::pair{"_in", &node.handleIn}, std::pair{"_out", &node.handleOut}})
    {
      if (const auto* value = entity.property(key + suffix))
      {
        *handle = parsePosition(value);
        if (!*handle)
        {
          return Error{fmt::format("Invalid '{}{}'", key, suffix)};
        }
      }
    }
    if (const auto* mode = entity.property(key + "_mode"))
    {
      if (*mode == "auto")
      {
        node.handleMode = PathHandleMode::Auto;
      }
      else if (*mode == "aligned")
      {
        node.handleMode = PathHandleMode::Aligned;
      }
      else if (*mode == "free")
      {
        node.handleMode = PathHandleMode::Free;
      }
      else
      {
        return Error{fmt::format("Unknown handle mode for '{}'", key)};
      }
    }
    result.nodes.push_back(std::move(node));
  }
  return result;
}

Result<void> writePath(Entity& entity, const Path& path)
{
  if (path.nodes.size() > MaxPathNodes)
  {
    return Error{"Too many path nodes (maximum 65536)"};
  }
  auto kind = std::string{};
  switch (path.kind)
  {
  case PathKind::Linear:
    kind = "linear";
    break;
  case PathKind::CatmullRom:
    kind = "catmull_rom";
    break;
  case PathKind::Bezier:
    kind = "bezier";
    break;
  default:
    return Error{"Unknown path kind"};
  }
  auto properties = std::vector<EntityProperty>{};
  for (const auto& property : entity.properties())
  {
    if (!isPathProperty(property.key()))
    {
      properties.push_back(property);
    }
  }
  properties.emplace_back("path_version", "1");
  properties.emplace_back("path_type", kind);
  properties.emplace_back("closed", path.closed ? "1" : "0");
  properties.emplace_back("point_count", std::to_string(path.nodes.size()));
  for (size_t i = 0; i < path.nodes.size(); ++i)
  {
    const auto& node = path.nodes[i];
    if (
      !finite(node.position) || (node.handleIn && !finite(*node.handleIn))
      || (node.handleOut && !finite(*node.handleOut)) || !std::isfinite(node.roll))
    {
      return Error{"Path positions and handles must be finite"};
    }
    const auto key = fmt::format("point_{}", i);
    properties.emplace_back(key, positionString(node.position));
    if (node.roll != 0.0)
    {
      properties.emplace_back(key + "_roll", fmt::format("{:.17g}", node.roll));
    }
    if (node.handleIn)
    {
      properties.emplace_back(key + "_in", positionString(*node.handleIn));
    }
    if (node.handleOut)
    {
      properties.emplace_back(key + "_out", positionString(*node.handleOut));
    }
    auto mode = std::string{};
    switch (node.handleMode)
    {
    case PathHandleMode::Auto:
      mode = "auto";
      break;
    case PathHandleMode::Aligned:
      mode = "aligned";
      break;
    case PathHandleMode::Free:
      mode = "free";
      break;
    default:
      return Error{"Unknown path handle mode"};
    }
    properties.emplace_back(key + "_mode", mode);
  }
  entity.setProperties(std::move(properties));
  return {};
}

} // namespace tb::mdl
