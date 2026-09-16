#pragma once

#include "ui/ControlPointTool.h"

namespace tb::ui
{
class PathTool final : public ControlPointTool
{
public:
  explicit PathTool(MapDocument& document);

  std::string actionName() const override;
};
} // namespace tb::ui
