/* Copyright (C) 2026 TrenchBroom Authors */
#include "mdl/UniqueEntityNameValidator.h"

#include "mdl/EntityDefinition.h"
#include "mdl/EntityNode.h"
#include "mdl/Issue.h"
#include "mdl/Map.h"
#include "mdl/NodeQueries.h"

#include <fmt/format.h>

namespace tb::mdl
{
namespace
{
const auto Type = freeIssueType();
}

UniqueEntityNameValidator::UniqueEntityNameValidator(Map& map)
  : Validator{Type, "Duplicate unique entity name"}
  , m_map{map}
{
}

void UniqueEntityNameValidator::doValidate(
  EntityNodeBase& entityNode, std::vector<std::unique_ptr<Issue>>& issues) const
{
  const auto* definition = entityNode.entity().definition();
  if (!definition)
  {
    return;
  }

  for (const auto& propertyDefinition : definition->propertyDefinitions)
  {
    if (!propertyDefinition.requiresUniqueName)
    {
      continue;
    }

    const auto* value = entityNode.entity().property(propertyDefinition.key);
    if (!value || value->empty())
    {
      continue;
    }

    auto matchCount = size_t{0};
    for (const auto* node : collectDescendants(std::vector<Node*>{&m_map.worldNode()}))
    {
      const auto* otherEntityNode = dynamic_cast<const EntityNode*>(node);
      const auto* otherDefinition =
        otherEntityNode ? otherEntityNode->entity().definition() : nullptr;
      if (!otherDefinition)
      {
        continue;
      }

      for (const auto& otherPropertyDefinition : otherDefinition->propertyDefinitions)
      {
        if (
          otherPropertyDefinition.requiresUniqueName
          && otherEntityNode->entity().hasProperty(otherPropertyDefinition.key, *value))
        {
          ++matchCount;
        }
      }
    }

    if (matchCount > 1u)
    {
      issues.push_back(
        std::make_unique<EntityPropertyIssue>(
          Type,
          entityNode,
          propertyDefinition.key,
          fmt::format("Entity name '{}' must be unique", *value)));
    }
  }
}
} // namespace tb::mdl
