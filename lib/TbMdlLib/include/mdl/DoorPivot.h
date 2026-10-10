#pragma once

#include "mdl/Entity.h"
#include "mdl/EntityNodeBase.h"

#include "vm/vec_io.h"

namespace tb::mdl
{
inline bool isDoorWithPivot(const Entity& entity)
{
  return entity.classname() == "func_door";
}

inline vm::vec3d doorPivotOffset(const Entity& entity)
{
  if (const auto* value = entity.property("pivot_offset"))
  {
    return vm::parse<double, 3>(*value).value_or(vm::vec3d{});
  }
  return vm::vec3d{};
}

inline vm::vec3d doorPivotPosition(const EntityNodeBase& node)
{
  return node.logicalBounds().center()
         + node.entity().rotation() * doorPivotOffset(node.entity());
}
} // namespace tb::mdl
