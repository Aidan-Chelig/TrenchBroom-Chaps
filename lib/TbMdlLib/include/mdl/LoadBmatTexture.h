/*
 Copyright (C) 2026 TrenchBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
*/

#pragma once

#include "base/Result.h"
#include "gl/Texture.h"

namespace tb::fs
{
class Reader;
}

namespace tb::mdl
{

enum class BmatAlphaMode
{
  Opaque,
  Mask,
  Blend,
};

struct BmatTexture
{
  gl::Texture texture;
  BmatAlphaMode alphaMode;
};

Result<BmatTexture> loadBmatTexture(fs::Reader& reader);
Result<BmatAlphaMode> loadBmatAlphaMode(fs::Reader& reader);

} // namespace tb::mdl
