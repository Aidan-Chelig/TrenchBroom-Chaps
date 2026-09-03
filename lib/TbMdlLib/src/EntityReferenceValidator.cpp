/* Copyright (C) 2026 TrenchBroom Authors */
#include "mdl/EntityReferenceValidator.h"

#include "mdl/EntityDefinition.h"
#include "mdl/EntityNode.h"
#include "mdl/EntityProperties.h"
#include "mdl/Issue.h"
#include "mdl/Map.h"
#include "mdl/NodeQueries.h"
#include "mdl/PropertyDefinition.h"

#include <fmt/format.h>

namespace tb::mdl
{
namespace
{
const auto Type = freeIssueType();

std::vector<const EntityNode*> findTargets(Map& map, const std::string_view name)
{
  auto result = std::vector<const EntityNode*>{};
  for (const auto* node : collectDescendants(std::vector<Node*>{&map.worldNode()}))
  {
    if (const auto* entityNode = dynamic_cast<const EntityNode*>(node))
    {
      const auto* targetname =
        entityNode->entity().property(EntityPropertyKeys::Targetname);
      if (targetname && *targetname == name)
      {
        result.push_back(entityNode);
      }
    }
  }
  return result;
}
} // namespace

EntityReferenceValidator::EntityReferenceValidator(Map& map)
  : Validator{Type, "Invalid entity interface reference"}
  , m_map{map}
{
}

void EntityReferenceValidator::doValidate(
  EntityNodeBase& entityNode, std::vector<std::unique_ptr<Issue>>& issues) const
{
  const auto* definition = entityNode.entity().definition();
  if (!definition)
  {
    return;
  }

  const auto addIssue = [&](const auto& key, auto description) {
    issues.push_back(
      std::make_unique<EntityPropertyIssue>(
        Type, entityNode, key, std::move(description)));
  };

  for (const auto& propertyDefinition : definition->propertyDefinitions)
  {
    const auto* value = entityNode.entity().property(propertyDefinition.key);
    if (!value || value->empty())
    {
      continue;
    }

    if (
      const auto* reference =
        std::get_if<PropertyValueTypes::EntityReference>(&propertyDefinition.valueType))
    {
      const auto targets = findTargets(m_map, *value);
      if (targets.empty())
      {
        addIssue(
          propertyDefinition.key,
          fmt::format(
            "Property '{}' references missing entity '{}'",
            propertyDefinition.key,
            *value));
      }
      else if (targets.size() > 1u)
      {
        addIssue(
          propertyDefinition.key,
          fmt::format(
            "Property '{}' references duplicate targetname '{}'",
            propertyDefinition.key,
            *value));
      }
      else if (
        reference->requiredInterface
        && (!targets.front()->entity().definition()
            || !getEntityInterfaceDefinition(
              targets.front()->entity().definition()->interfaces,
              *reference->requiredInterface)))
      {
        addIssue(
          propertyDefinition.key,
          fmt::format(
            "Referenced entity '{}' does not provide interface '{}'",
            *value,
            *reference->requiredInterface));
      }
    }
    else if (
      const auto* endpointReference =
        std::get_if<PropertyValueTypes::EndpointReference>(&propertyDefinition.valueType))
    {
      const auto* targetname =
        entityNode.entity().property(endpointReference->entityProperty);
      const auto targets =
        targetname ? findTargets(m_map, *targetname) : std::vector<const EntityNode*>{};
      if (targets.size() != 1u || !targets.front()->entity().definition())
      {
        addIssue(
          propertyDefinition.key,
          fmt::format(
            "Cannot resolve entity property '{}'", endpointReference->entityProperty));
        continue;
      }
      const auto* interfaceDefinition = getEntityInterfaceDefinition(
        targets.front()->entity().definition()->interfaces,
        endpointReference->interfaceName);
      if (!interfaceDefinition)
      {
        addIssue(
          propertyDefinition.key,
          fmt::format(
            "Referenced entity does not provide interface '{}'",
            endpointReference->interfaceName));
      }
      else if (!getEntityInterfaceEndpointDefinition(*interfaceDefinition, *value))
      {
        addIssue(
          propertyDefinition.key,
          fmt::format(
            "Interface '{}' has no endpoint '{}'",
            endpointReference->interfaceName,
            *value));
      }
    }
  }
}
} // namespace tb::mdl
