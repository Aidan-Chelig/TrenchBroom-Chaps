/*
 Copyright (C) 2026 TrenchBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
*/

#pragma once

#include "vm/bbox.h"
#include "vm/mat.h"

#include <optional>
#include <string>

namespace tb::mdl
{
class Entity;

struct ImageProjector
{
  vm::bbox3d localBounds;
  vm::mat4x4d transformation;
  std::string materialName;
};

/** Returns the visualization data for an image_projector point entity. */
std::optional<ImageProjector> imageProjector(const Entity& entity);

} // namespace tb::mdl
