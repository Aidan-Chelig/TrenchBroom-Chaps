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

#include "base/Color.h"

#include "kd/reflection_impl.h"

#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace tb::mdl
{

namespace PropertyValueTypes
{
struct LinkSource
{
  kdl_reflect_decl_empty(LinkSource);
};

struct LinkTarget
{
  kdl_reflect_decl_empty(LinkTarget);
};

struct String
{
  std::optional<std::string> defaultValue = std::nullopt;

  kdl_reflect_decl(String, defaultValue);
};

struct ModelPath
{
  std::optional<std::string> defaultValue = std::nullopt;

  kdl_reflect_decl(ModelPath, defaultValue);
};

// An asset-relative path to a particle effect under particles/, picked from a browser.
struct ParticlePath
{
  std::optional<std::string> defaultValue = std::nullopt;

  kdl_reflect_decl(ParticlePath, defaultValue);
};

struct EntityReference
{
  std::optional<std::string> defaultValue = std::nullopt;
  std::optional<std::string> requiredInterface = std::nullopt;
  std::optional<Color> linkColor = std::nullopt;

  kdl_reflect_decl(EntityReference, defaultValue, requiredInterface, linkColor);
};

struct EndpointReference
{
  std::optional<std::string> defaultValue = std::nullopt;
  std::string entityProperty;
  std::string interfaceName;

  kdl_reflect_decl(EndpointReference, defaultValue, entityProperty, interfaceName);
};

struct Boolean
{
  std::optional<bool> defaultValue = std::nullopt;

  kdl_reflect_decl(Boolean, defaultValue);
};

struct Integer
{
  std::optional<int> defaultValue = std::nullopt;

  kdl_reflect_decl(Integer, defaultValue);
};

struct Float
{
  std::optional<float> defaultValue = std::nullopt;

  kdl_reflect_decl(Float, defaultValue);
};

struct ChoiceOption
{
  std::string value;
  std::string description;

  kdl_reflect_decl(ChoiceOption, value, description);
};

struct Choice
{
  std::vector<ChoiceOption> options;
  std::optional<std::string> defaultValue = std::nullopt;

  kdl_reflect_decl(Choice, options, defaultValue);
};

struct Flag
{
  int value;
  std::string shortDescription;
  std::string longDescription;

  kdl_reflect_decl(Flag, value, shortDescription, longDescription);
};

struct Flags
{
  std::vector<Flag> flags;
  int defaultValue = 0;

  const Flag* flag(int flagValue) const;
  bool isDefault(int flagValue) const;

  kdl_reflect_decl(Flags, defaultValue, flags);
};

struct Origin
{
  std::optional<std::string> defaultValue = std::nullopt;

  kdl_reflect_decl(Origin, defaultValue);
};

enum class IOParameterType
{
  Void,
  String,
  Integer,
  Float,
  Boolean,
  EHandle,
};

std::ostream& operator<<(std::ostream& lhs, IOParameterType rhs);

struct Input
{
  IOParameterType parameterType;

  kdl_reflect_decl(Input, parameterType);
};

struct Output
{
  IOParameterType parameterType;

  kdl_reflect_decl(Output, parameterType);
};

template <typename T>
struct Color
{
  using Type = T;

  std::optional<std::string> defaultValue = std::nullopt;

  kdl_reflect_inline(Color, defaultValue);
};

struct Unknown
{
  std::optional<std::string> defaultValue = std::nullopt;

  kdl_reflect_decl(Unknown, defaultValue);
};

} // namespace PropertyValueTypes

using PropertyValueType = std::variant<
  PropertyValueTypes::LinkTarget,
  PropertyValueTypes::LinkSource,
  PropertyValueTypes::String,
  PropertyValueTypes::ModelPath,
  PropertyValueTypes::ParticlePath,
  PropertyValueTypes::EntityReference,
  PropertyValueTypes::EndpointReference,
  PropertyValueTypes::Boolean,
  PropertyValueTypes::Integer,
  PropertyValueTypes::Float,
  PropertyValueTypes::Choice,
  PropertyValueTypes::Flags,
  PropertyValueTypes::Origin,
  PropertyValueTypes::Input,
  PropertyValueTypes::Output,
  PropertyValueTypes::Color<RgbF>,
  PropertyValueTypes::Color<RgbB>,
  PropertyValueTypes::Color<Rgb>,
  PropertyValueTypes::Unknown>;

std::ostream& operator<<(std::ostream& lhs, const PropertyValueType& rhs);

/**
 * A condition on another property of the same entity, which decides whether the entity
 * inspector shows a property. Written in the FGD as the `show_if` type parameter: `key`
 * (that property is true), `!key` (it is false), `key=a|b` (it is one of these values)
 * or `key!=a|b` (it is none of them). Caution Tape groups optional settings under a
 * toggle this way, so a group only takes up rows while it is switched on.
 */
struct PropertyCondition
{
  std::string key;
  // Values to compare the property with, ignoring case. Empty tests whether it is true.
  std::vector<std::string> values;
  bool negated = false;

  kdl_reflect_decl(PropertyCondition, key, values, negated);
};

/**
 * Parses a `show_if` condition, or returns nothing if it is not one.
 */
std::optional<PropertyCondition> parsePropertyCondition(std::string_view str);

/**
 * Whether the condition holds when its property has the given value. A property is true
 * unless it is empty, 0, false, no or off.
 */
bool conditionHolds(const PropertyCondition& condition, std::string_view value);

struct PropertyDefinition
{
  std::string key;
  PropertyValueType valueType;
  std::string shortDescription;
  std::string longDescription;
  bool readOnly = false;
  bool requiresUniqueName = false;
  // Shown in the entity inspector only while this holds; always when unset.
  std::optional<PropertyCondition> showIf = std::nullopt;

  static std::optional<std::string> defaultValue(const PropertyDefinition& definition);

  kdl_reflect_decl(
    PropertyDefinition,
    key,
    valueType,
    shortDescription,
    longDescription,
    readOnly,
    requiresUniqueName,
    showIf);
};

} // namespace tb::mdl
