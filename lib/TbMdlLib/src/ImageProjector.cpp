/*
 Copyright (C) 2026 TrenchBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
*/

#include "mdl/ImageProjector.h"

#include "mdl/Entity.h"

#include "kd/string_utils.h"

#include "vm/mat.h"
#include "vm/mat_ext.h"

#include <cmath>

namespace tb::mdl
{
namespace
{
double dimension(const Entity& entity, const std::string& key)
{
  const auto* value = entity.property(key);
  const auto parsed = value ? kdl::str_to_double(*value) : std::optional<double>{64.0};
  return parsed && std::isfinite(*parsed) && *parsed > 0.0 ? *parsed : 64.0;
}
} // namespace

std::optional<ImageProjector> imageProjector(const Entity& entity)
{
  if (!entity.pointEntity() || entity.classname() != "image_projector")
  {
    return std::nullopt;
  }

  const auto size = vm::vec3d{
    dimension(entity, "width"), dimension(entity, "height"), dimension(entity, "depth")};
  const auto halfSize = size / 2.0;
  return ImageProjector{
    vm::bbox3d{-halfSize, halfSize},
    vm::translation_matrix(entity.origin()) * entity.rotation(),
    entity.property("material") ? *entity.property("material") : std::string{}};
}

} // namespace tb::mdl
