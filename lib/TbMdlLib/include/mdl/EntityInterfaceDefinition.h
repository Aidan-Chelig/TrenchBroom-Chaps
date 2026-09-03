/* Copyright (C) 2026 TrenchBroom Authors */
#pragma once

#include "kd/reflection_decl.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tb::mdl
{
struct EntityInterfaceEndpointDefinition
{
  std::string name;
  std::optional<std::string> displayName;
  std::optional<std::string> description;

  kdl_reflect_decl(EntityInterfaceEndpointDefinition, name, displayName, description);
};

struct EntityInterfaceDefinition
{
  std::string name;
  std::optional<std::string> displayName;
  std::optional<std::string> description;
  std::vector<EntityInterfaceEndpointDefinition> endpoints;

  kdl_reflect_decl(EntityInterfaceDefinition, name, displayName, description, endpoints);
};

const EntityInterfaceDefinition* getEntityInterfaceDefinition(
  const std::vector<EntityInterfaceDefinition>& interfaces, std::string_view name);
const EntityInterfaceEndpointDefinition* getEntityInterfaceEndpointDefinition(
  const EntityInterfaceDefinition& interfaceDefinition, std::string_view name);
} // namespace tb::mdl
