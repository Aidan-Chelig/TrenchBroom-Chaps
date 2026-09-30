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

#include "mdl/PropertyDefinition.h"

#include "kd/optional_utils.h"
#include "kd/overload.h"
#include "kd/reflection_impl.h"
#include "kd/string_compare.h"
#include "kd/string_format.h"
#include "kd/string_utils.h"

#include <fmt/format.h>

#include <algorithm>
#include <string>

namespace tb::mdl
{
namespace PropertyValueTypes
{

kdl_reflect_impl(LinkSource);
kdl_reflect_impl(LinkTarget);
kdl_reflect_impl(String);
kdl_reflect_impl(ModelPath);
kdl_reflect_impl(ParticlePath);
kdl_reflect_impl(EntityReference);
kdl_reflect_impl(EndpointReference);
kdl_reflect_impl(Boolean);
kdl_reflect_impl(Integer);
kdl_reflect_impl(Float);
kdl_reflect_impl(ChoiceOption);
kdl_reflect_impl(Choice);
kdl_reflect_impl(Flag);
kdl_reflect_impl(Flags);
kdl_reflect_impl(Origin);
kdl_reflect_impl(Input);
kdl_reflect_impl(Output);
kdl_reflect_impl(Unknown);

const Flag* Flags::flag(const int flagValue) const
{
  const auto iFlag = std::ranges::find_if(
    flags, [&](const auto& flag) { return flag.value == flagValue; });
  return iFlag != flags.end() ? &*iFlag : nullptr;
}

bool Flags::isDefault(const int flagValue) const
{
  return (defaultValue & flagValue) != 0;
}

std::ostream& operator<<(std::ostream& lhs, IOParameterType rhs)
{
  switch (rhs)
  {
  case IOParameterType::Void:
    lhs << "Void";
    break;
  case IOParameterType::String:
    lhs << "String";
    break;
  case IOParameterType::Integer:
    lhs << "Integer";
    break;
  case IOParameterType::Float:
    lhs << "Float";
    break;
  case IOParameterType::Boolean:
    lhs << "Boolean";
    break;
  case IOParameterType::EHandle:
    lhs << "EHandle";
    break;
  }
  return lhs;
}

} // namespace PropertyValueTypes

std::ostream& operator<<(std::ostream& lhs, const PropertyValueType& rhs)
{
  std::visit([&](const auto& x) { lhs << x; }, rhs);
  return lhs;
}

kdl_reflect_impl(PropertyCondition);

std::optional<PropertyCondition> parsePropertyCondition(const std::string_view str)
{
  auto condition = PropertyCondition{};
  auto rest = kdl::str_trim(str);
  if (rest.starts_with('!'))
  {
    condition.negated = true;
    rest = kdl::str_trim(std::string_view{rest}.substr(1));
  }

  if (const auto equals = rest.find('='); equals != std::string::npos)
  {
    auto key = std::string_view{rest}.substr(0, equals);
    if (key.ends_with('!'))
    {
      condition.negated = !condition.negated;
      key.remove_suffix(1);
    }
    condition.key = kdl::str_trim(key);
    for (const auto& value :
         kdl::str_split(std::string_view{rest}.substr(equals + 1), "|"))
    {
      if (auto trimmed = kdl::str_trim(value); !trimmed.empty())
      {
        condition.values.push_back(std::move(trimmed));
      }
    }
    if (condition.values.empty())
    {
      return std::nullopt;
    }
  }
  else
  {
    condition.key = std::move(rest);
  }

  if (condition.key.empty() || condition.key.find_first_of(" \t!=|") != std::string::npos)
  {
    return std::nullopt;
  }
  return condition;
}

bool conditionHolds(const PropertyCondition& condition, const std::string_view value)
{
  const auto trimmed = kdl::str_trim(value);
  const auto isTrue = [&]() {
    return !trimmed.empty() && trimmed != "0" && !kdl::ci::str_is_equal(trimmed, "false")
           && !kdl::ci::str_is_equal(trimmed, "no")
           && !kdl::ci::str_is_equal(trimmed, "off");
  };
  const auto matches =
    condition.values.empty()
      ? isTrue()
      : std::ranges::any_of(condition.values, [&](const auto& expected) {
          return kdl::ci::str_is_equal(expected, trimmed);
        });
  return matches != condition.negated;
}

kdl_reflect_impl(PropertyDefinition);

std::optional<std::string> PropertyDefinition::defaultValue(
  const PropertyDefinition& definition)
{
  using namespace std::string_literals;

  return std::visit(
    kdl::overload(
      [](const PropertyValueTypes::LinkTarget&) -> std::optional<std::string> {
        return std::nullopt;
      },
      [](const PropertyValueTypes::LinkSource&) -> std::optional<std::string> {
        return std::nullopt;
      },
      [](const PropertyValueTypes::String& value) { return value.defaultValue; },
      [](const PropertyValueTypes::ModelPath& value) { return value.defaultValue; },
      [](const PropertyValueTypes::ParticlePath& value) { return value.defaultValue; },
      [](const PropertyValueTypes::EntityReference& value) { return value.defaultValue; },
      [](const PropertyValueTypes::EndpointReference& value) {
        return value.defaultValue;
      },
      [](const PropertyValueTypes::Boolean& value) {
        return value.defaultValue | kdl::optional_transform([](const auto b) {
                 return b ? "true"s : "false"s;
               });
      },
      [](const PropertyValueTypes::Integer& value) {
        return value.defaultValue
               | kdl::optional_transform([](const auto i) { return std::to_string(i); });
      },
      [](const PropertyValueTypes::Float& value) {
        return value.defaultValue
               | kdl::optional_transform([](const auto f) { return std::to_string(f); });
      },
      [](const PropertyValueTypes::Choice& value) { return value.defaultValue; },
      [](const PropertyValueTypes::Flags& value) {
        return value.defaultValue != 0 ? std::optional{std::to_string(value.defaultValue)}
                                       : std::nullopt;
      },
      [](const PropertyValueTypes::Origin& value) { return value.defaultValue; },
      [](const PropertyValueTypes::Input&) -> std::optional<std::string> {
        return std::nullopt;
      },
      [](const PropertyValueTypes::Output&) -> std::optional<std::string> {
        return std::nullopt;
      },
      []<typename T>(const PropertyValueTypes::Color<T>& value) {
        return value.defaultValue;
      },
      [](const PropertyValueTypes::Unknown& value) { return value.defaultValue; }),
    definition.valueType);
}

} // namespace tb::mdl
