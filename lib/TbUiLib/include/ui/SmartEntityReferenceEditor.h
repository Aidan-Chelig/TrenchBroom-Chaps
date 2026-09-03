/* Copyright (C) 2026 TrenchBroom Authors */
#pragma once

#include "ui/SmartPropertyEditor.h"

class QComboBox;
class QLabel;

namespace tb::ui
{
class SmartEntityReferenceEditor : public SmartPropertyEditor
{
  Q_OBJECT
private:
  QComboBox* m_entities = nullptr;
  QLabel* m_status = nullptr;
  bool m_updating = false;

public:
  SmartEntityReferenceEditor(MapDocument& document, QWidget* parent = nullptr);

private:
  void commitValue();
  void jumpToEntity();
  void doUpdateVisual(const std::vector<mdl::EntityNodeBase*>& nodes) override;
};
} // namespace tb::ui
