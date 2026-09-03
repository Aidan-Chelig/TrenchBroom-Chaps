/* Copyright (C) 2026 TrenchBroom Authors */
#include "mdl/EntityInterfaceDefinition.h"

#include "kd/reflection_impl.h"

#include <algorithm>

namespace tb::mdl
{
kdl_reflect_impl(EntityInterfaceEndpointDefinition);
kdl_reflect_impl(EntityInterfaceDefinition);

const EntityInterfaceDefinition* getEntityInterfaceDefinition(
  const std::vector<EntityInterfaceDefinition>& interfaces, const std::string_view name)
{
  const auto it = std::ranges::find(interfaces, name, &EntityInterfaceDefinition::name);
  return it != interfaces.end() ? &*it : nullptr;
}

const EntityInterfaceEndpointDefinition* getEntityInterfaceEndpointDefinition(
  const EntityInterfaceDefinition& interfaceDefinition, const std::string_view name)
{
  const auto it = std::ranges::find(
    interfaceDefinition.endpoints, name, &EntityInterfaceEndpointDefinition::name);
  return it != interfaceDefinition.endpoints.end() ? &*it : nullptr;
}
} // namespace tb::mdl
