/*
 Copyright (C) 2026 TrenchBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
*/

#pragma once

#include "ui/ToolController.h"

#include <memory>

namespace tb
{
namespace render
{
class RenderBatch;
class RenderContext;
} // namespace render

namespace ui
{
class AreaLightTool;

/**
 * Drags the handles of a selected area light's panel in the 2D and 3D views. Hold Alt
 * to resize around the panel's center.
 */
class AreaLightToolController : public ToolController
{
private:
  AreaLightTool& m_tool;

public:
  explicit AreaLightToolController(AreaLightTool& tool);
  ~AreaLightToolController() override;

private:
  Tool& tool() override;
  const Tool& tool() const override;

  void pick(const InputState& inputState, mdl::PickResult& pickResult) override;
  void mouseMove(const InputState& inputState) override;
  std::unique_ptr<GestureTracker> acceptMouseDrag(const InputState& inputState) override;

  void render(
    const InputState& inputState,
    render::RenderContext& renderContext,
    render::RenderBatch& renderBatch) override;

  bool cancel() override;
};

} // namespace ui
} // namespace tb
