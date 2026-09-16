#include "ui/PathTool.h"

namespace tb::ui
{
PathTool::PathTool(MapDocument& document)
  : ControlPointTool{document}
{
}

std::string PathTool::actionName() const
{
  return "Move Path Control Point";
}
} // namespace tb::ui
