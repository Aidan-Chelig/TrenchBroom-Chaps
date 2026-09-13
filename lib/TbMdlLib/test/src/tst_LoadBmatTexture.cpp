/*
 Copyright (C) 2026 TrenchBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
*/

#include "TestEnvironment.h"
#include "fs/DiskFileSystem.h"
#include "gl/Texture.h"
#include "mdl/CatchConfig.h"
#include "mdl/LoadBmatTexture.h"
#include "mdl/TestUtils.h"

#include "kd/result.h"

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
namespace
{

auto openFixture(const std::string& name)
{
  const auto bmatPath = getFixtureRoot() / "test/mdl/LoadBmatTexture/";
  auto diskFS = fs::DiskFileSystem{bmatPath};
  return diskFS.openFile(name) | kdl::value();
}

} // namespace

TEST_CASE("loadBmatTexture")
{
  SECTION("loads an opaque texture")
  {
    const auto file = openFixture("bmat_opaque.bmat");
    auto reader = file->reader().buffer();

    const auto bmatTexture = loadBmatTexture(reader) | kdl::value();

    CHECK(bmatTexture.alphaMode == BmatAlphaMode::Opaque);
    CHECK(bmatTexture.texture.width() == 4);
    CHECK(bmatTexture.texture.height() == 4);
    CHECK(bmatTexture.texture.format() == GL_RGBA);
    CHECK(bmatTexture.texture.alphaDomain() == img::ImageAlphaDomain::Opaque);
    // Solid red. A regression guard for reading the pixel buffer after it was moved
    // into the texture, which yielded a null buffer and crashed.
    CHECK(bmatTexture.texture.averageColor() == Color{RgbaF{1.0f, 0.0f, 0.0f, 1.0f}});
  }

  SECTION("loads a texture with graduated alpha")
  {
    const auto file = openFixture("bmat_graduated.bmat");
    auto reader = file->reader().buffer();

    const auto bmatTexture = loadBmatTexture(reader) | kdl::value();

    CHECK(bmatTexture.alphaMode == BmatAlphaMode::Blend);
    CHECK(bmatTexture.texture.width() == 4);
    CHECK(bmatTexture.texture.height() == 4);
    CHECK(bmatTexture.texture.alphaDomain() == img::ImageAlphaDomain::Graduated);
  }

  SECTION("rejects a file that is not a BMAT archive")
  {
    const auto file = openFixture("bmat_opaque.bmat");
    auto reader = file->reader().subReaderFromBegin(0, 16).buffer();

    CHECK(loadBmatTexture(reader).is_error());
  }
}

TEST_CASE("loadBmatAlphaMode")
{
  SECTION("reads the alpha mode without decoding the texture")
  {
    const auto opaque = openFixture("bmat_opaque.bmat");
    auto opaqueReader = opaque->reader().buffer();
    CHECK((loadBmatAlphaMode(opaqueReader) | kdl::value()) == BmatAlphaMode::Opaque);

    const auto graduated = openFixture("bmat_graduated.bmat");
    auto graduatedReader = graduated->reader().buffer();
    CHECK((loadBmatAlphaMode(graduatedReader) | kdl::value()) == BmatAlphaMode::Blend);
  }
}

} // namespace tb::mdl
