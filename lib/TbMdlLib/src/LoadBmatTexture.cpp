/*
 Copyright (C) 2026 TrenchBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
*/

#include "mdl/LoadBmatTexture.h"

#include "fs/Reader.h"
#include "gl/TextureBuffer.h"
#include "img/ImageAlphaDomain.h"
#include "mdl/LoadImageTexture.h"
#include "mdl/MaterialUtils.h"

#include "kd/result.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

namespace tb::mdl
{
namespace
{

constexpr auto Ktx2Identifier = std::array<uint8_t, 12>{
  0xAB, 'K', 'T', 'X', ' ', '2', '0', 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

uint32_t readU32(const std::span<const uint8_t> bytes, const size_t offset)
{
  return uint32_t(bytes[offset]) | uint32_t(bytes[offset + 1]) << 8u
         | uint32_t(bytes[offset + 2]) << 16u | uint32_t(bytes[offset + 3]) << 24u;
}

uint64_t readU64(const std::span<const uint8_t> bytes, const size_t offset)
{
  return uint64_t(readU32(bytes, offset)) | uint64_t(readU32(bytes, offset + 4u)) << 32u;
}

uint8_t srgbToLinear(const uint8_t value)
{
  const auto encoded = float(value) / 255.0f;
  const auto linear =
    encoded <= 0.04045f ? encoded / 12.92f : std::pow((encoded + 0.055f) / 1.055f, 2.4f);
  return static_cast<uint8_t>(std::clamp(std::lround(linear * 255.0f), 0l, 255l));
}

Result<size_t> readTarSize(const std::span<const uint8_t> bytes)
{
  const auto end = std::ranges::find(bytes, uint8_t{0});
  const auto value =
    std::string{reinterpret_cast<const char*>(bytes.data()), size_t(end - bytes.begin())};
  try
  {
    return std::stoull(value, nullptr, 8);
  }
  catch (const std::exception& e)
  {
    return Error{fmt::format("Invalid BMAT tar entry size '{}': {}", value, e.what())};
  }
}

Result<std::span<const uint8_t>> findTarEntry(
  const std::span<const uint8_t> archive, const std::string_view wantedName)
{
  auto offset = size_t{0};
  while (offset + 512u <= archive.size())
  {
    const auto header = archive.subspan(offset, 512u);
    if (std::ranges::all_of(header, [](const auto byte) { return byte == 0; }))
    {
      break;
    }

    const auto nameBytes = header.first<100>();
    const auto nameEnd = std::ranges::find(nameBytes, uint8_t{0});
    const auto name = std::string_view{
      reinterpret_cast<const char*>(nameBytes.data()),
      size_t(nameEnd - nameBytes.begin())};

    const auto entrySizeResult = readTarSize(header.subspan(124u, 12u));
    if (entrySizeResult.is_error())
    {
      return Error{fmt::format("Invalid BMAT tar entry size for '{}'", name)};
    }
    const auto entrySize = entrySizeResult.value();
    const auto dataOffset = offset + 512u;
    if (entrySize > archive.size() - dataOffset)
    {
      return Error{fmt::format("BMAT tar entry '{}' extends past archive", name)};
    }
    if (name == wantedName)
    {
      return archive.subspan(dataOffset, entrySize);
    }

    const auto paddedSize = (entrySize + 511u) / 512u * 512u;
    if (paddedSize > archive.size() - dataOffset)
    {
      return Error{"BMAT tar entry offset overflow"};
    }
    offset = dataOffset + paddedSize;
  }
  return Error{fmt::format("BMAT archive is missing '{}'", wantedName)};
}

Result<std::string> manifestString(
  const std::string_view manifest, const std::string_view field)
{
  const auto fieldPos = manifest.find(field);
  const auto colonPos = manifest.find(':', fieldPos);
  const auto somePos = manifest.find("Some", colonPos);
  const auto commaPos = manifest.find(',', colonPos);
  const auto quoteBegin = manifest.find('"', somePos);
  const auto quoteEnd = manifest.find('"', quoteBegin + 1u);
  if (
    fieldPos == std::string_view::npos || colonPos == std::string_view::npos
    || somePos == std::string_view::npos
    || (commaPos != std::string_view::npos && somePos > commaPos)
    || quoteBegin == std::string_view::npos || quoteEnd == std::string_view::npos)
  {
    return Error{fmt::format("BMAT manifest has invalid '{}'", field)};
  }
  return std::string{manifest.substr(quoteBegin + 1u, quoteEnd - quoteBegin - 1u)};
}

Result<BmatAlphaMode> parseManifestAlphaMode(const std::string_view manifest)
{
  const auto fieldPos = manifest.find("alpha_mode");
  if (fieldPos == std::string_view::npos)
  {
    return BmatAlphaMode::Mask;
  }
  const auto commaPos = manifest.find(',', fieldPos);
  const auto value = manifest.substr(fieldPos, commaPos - fieldPos);
  if (value.find("Mask") != std::string_view::npos)
  {
    return BmatAlphaMode::Mask;
  }
  if (value.find("Opaque") != std::string_view::npos)
  {
    return BmatAlphaMode::Opaque;
  }
  if (value.find("Blend") != std::string_view::npos)
  {
    return BmatAlphaMode::Blend;
  }
  return Error{"BMAT manifest has invalid 'alpha_mode'"};
}

bool hasSupportedManifestVersion(const std::string_view manifest)
{
  const auto fieldPos = manifest.find("version");
  if (fieldPos == std::string_view::npos)
  {
    return false;
  }
  const auto colonPos = manifest.find(':', fieldPos);
  const auto commaPos = manifest.find(',', colonPos);
  if (colonPos == std::string_view::npos)
  {
    return false;
  }
  const auto value = manifest.substr(colonPos + 1u, commaPos - colonPos - 1u);
  const auto first = value.find_first_not_of(" \t\r\n");
  const auto last = value.find_last_not_of(" \t\r\n");
  return first != std::string_view::npos && value.substr(first, last - first + 1u) == "1";
}

Result<gl::Texture> decodeKtx2(const std::span<const uint8_t> bytes)
{
  constexpr auto headerSize = size_t{80};
  constexpr auto levelIndexSize = size_t{24};
  if (bytes.size() < headerSize + levelIndexSize)
  {
    return Error{"KTX2 texture is truncated"};
  }
  if (!std::ranges::equal(Ktx2Identifier, bytes.first(Ktx2Identifier.size())))
  {
    return Error{"Invalid KTX2 identifier"};
  }

  const auto vkFormat = readU32(bytes, 12u);
  const auto srgb = vkFormat == 29u || vkFormat == 43u;
  const auto width = size_t{readU32(bytes, 20u)};
  const auto height = size_t{readU32(bytes, 24u)};
  const auto depth = readU32(bytes, 28u);
  const auto layerCount = readU32(bytes, 32u);
  const auto faceCount = readU32(bytes, 36u);
  const auto levelCount = readU32(bytes, 40u);
  const auto supercompression = readU32(bytes, 44u);
  if (
    width == 0u || height == 0u || !checkTextureDimensions(width, height) || depth != 0u
    || layerCount != 0u || faceCount != 1u || levelCount == 0u || supercompression != 0u)
  {
    return Error{fmt::format("Unsupported KTX2 texture layout: {}*{}", width, height)};
  }

  // VkFormat values emitted by bmat's converter.
  const auto channels = vkFormat == 9u                       ? size_t{1}
                        : vkFormat == 23u || vkFormat == 29u ? size_t{3}
                        : vkFormat == 37u || vkFormat == 43u ? size_t{4}
                                                             : size_t{0};
  if (channels == 0u)
  {
    return Error{fmt::format("Unsupported BMAT KTX2 VkFormat {}", vkFormat)};
  }

  const auto dataOffset64 = readU64(bytes, headerSize);
  const auto dataLength64 = readU64(bytes, headerSize + 8u);
  if (
    dataOffset64 > std::numeric_limits<size_t>::max()
    || dataLength64 > std::numeric_limits<size_t>::max())
  {
    return Error{"KTX2 level offset overflow"};
  }
  const auto dataOffset = size_t{dataOffset64};
  const auto dataLength = size_t{dataLength64};
  if (width > std::numeric_limits<size_t>::max() / height)
  {
    return Error{"KTX2 pixel count overflow"};
  }
  const auto pixelCount = width * height;
  if (
    pixelCount > std::numeric_limits<size_t>::max() / channels
    || dataLength < pixelCount * channels || dataOffset > bytes.size()
    || dataLength > bytes.size() - dataOffset)
  {
    return Error{"KTX2 level data is truncated"};
  }

  auto buffers = gl::TextureBufferList{1u};
  setMipBufferSize(buffers, 1u, width, height, GL_RGBA);
  auto* output = buffers.front().data();
  const auto* input = bytes.data() + dataOffset;
  auto alphaDomain = img::ImageAlphaDomain::Opaque;
  for (auto i = size_t{0}; i < pixelCount; ++i)
  {
    const auto red = input[i * channels];
    const auto green = channels == 1u ? red : input[i * channels + 1u];
    const auto blue = channels == 1u ? red : input[i * channels + 2u];
    output[i * 4u] = srgb ? srgbToLinear(red) : red;
    output[i * 4u + 1u] = srgb ? srgbToLinear(green) : green;
    output[i * 4u + 2u] = srgb ? srgbToLinear(blue) : blue;
    output[i * 4u + 3u] = channels == 4u ? input[i * channels + 3u] : uint8_t{255};
    const auto alpha = output[i * 4u + 3u];
    if (alpha > 0u && alpha < 255u)
    {
      alphaDomain = img::ImageAlphaDomain::Graduated;
    }
    else if (alpha == 0u && alphaDomain == img::ImageAlphaDomain::Opaque)
    {
      alphaDomain = img::ImageAlphaDomain::Binary;
    }
  }

  auto texture = gl::Texture{
    width,
    height,
    getAverageColor(buffers.front(), GL_RGBA),
    GL_RGBA,
    gl::NoEmbeddedDefaults{},
    std::move(buffers)};
  texture.setAlphaDomain(alphaDomain);
  return texture;
}

} // namespace

Result<BmatTexture> loadBmatTexture(fs::Reader& reader)
{
  const auto bufferedReader = reader.buffer();
  const auto archive = std::span{
    reinterpret_cast<const uint8_t*>(bufferedReader.begin()),
    size_t(bufferedReader.end() - bufferedReader.begin())};

  return findTarEntry(archive, "manifest.ron")
         | kdl::and_then([&](const auto manifestBytes) {
             const auto manifest = std::string_view{
               reinterpret_cast<const char*>(manifestBytes.data()), manifestBytes.size()};
             if (!hasSupportedManifestVersion(manifest))
             {
               return Result<BmatTexture>{Error{"Unsupported BMAT manifest version"}};
             }
             return manifestString(manifest, "base_color_texture")
                    | kdl::join(parseManifestAlphaMode(manifest))
                    | kdl::and_then([&](const auto& texturePath, const auto alphaMode) {
                        return findTarEntry(archive, texturePath)
                               | kdl::and_then(decodeKtx2)
                               | kdl::transform([&](auto texture) {
                                   return BmatTexture{std::move(texture), alphaMode};
                                 });
                      });
           });
}

Result<BmatAlphaMode> loadBmatAlphaMode(fs::Reader& reader)
{
  const auto bufferedReader = reader.buffer();
  const auto archive = std::span{
    reinterpret_cast<const uint8_t*>(bufferedReader.begin()),
    size_t(bufferedReader.end() - bufferedReader.begin())};

  return findTarEntry(archive, "manifest.ron")
         | kdl::and_then([](const auto manifestBytes) {
             const auto manifest = std::string_view{
               reinterpret_cast<const char*>(manifestBytes.data()), manifestBytes.size()};
             if (!hasSupportedManifestVersion(manifest))
             {
               return Result<BmatAlphaMode>{Error{"Unsupported BMAT manifest version"}};
             }
             return parseManifestAlphaMode(manifest);
           });
}

} // namespace tb::mdl
