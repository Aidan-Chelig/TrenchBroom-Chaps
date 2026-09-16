#pragma once

#include <QWidget>

#include "base/NotifierConnection.h"

class QDoubleSpinBox;
class QSpinBox;

namespace tb::ui
{
class MapDocument;

class PathToolPage : public QWidget
{
private:
  MapDocument& m_document;
  QSpinBox* m_point = nullptr;
  QDoubleSpinBox* m_roll = nullptr;
  NotifierConnection m_connections;

public:
  explicit PathToolPage(MapDocument& document, QWidget* parent = nullptr);

  void updateControls();

private:
  void setRoll(double degrees);
};
} // namespace tb::ui
