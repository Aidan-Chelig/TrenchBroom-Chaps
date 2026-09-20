#include "ui/PathToolPage.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>

#include "mdl/EntityNode.h"
#include "mdl/Map.h"
#include "mdl/Map_Paths.h"
#include "mdl/PathEntity.h"
#include "mdl/Selection.h"
#include "ui/MapDocument.h"

namespace tb::ui
{
PathToolPage::PathToolPage(MapDocument& document, QWidget* parent)
  : QWidget{parent}
  , m_document{document}
  , m_count{new QSpinBox{this}}
  , m_point{new QSpinBox{this}}
  , m_roll{new QDoubleSpinBox{this}}
{
  m_count->setRange(2, 65536);
  m_point->setMinimum(0);
  m_roll->setRange(-36000.0, 36000.0);
  m_roll->setDecimals(2);
  m_roll->setSingleStep(5.0);
  m_roll->setSuffix(tr("°"));
  auto* layout = new QHBoxLayout{this};
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(new QLabel{tr("Points:")});
  layout->addWidget(m_count);
  layout->addWidget(new QLabel{tr("Point:")});
  layout->addWidget(m_point);
  layout->addWidget(new QLabel{tr("Roll:")});
  layout->addWidget(m_roll);
  layout->addStretch();

  connect(m_count, &QSpinBox::valueChanged, this, &PathToolPage::setPointCount);
  connect(m_point, &QSpinBox::valueChanged, this, [this](int) { updateControls(); });
  connect(m_roll, &QDoubleSpinBox::valueChanged, this, &PathToolPage::setRoll);
  m_connections +=
    m_document.documentDidChangeNotifier.connect([this]() { updateControls(); });
  m_connections += m_document.selectionDidChangeNotifier.connect(
    [this](const auto&) { updateControls(); });
  updateControls();
}

void PathToolPage::updateControls()
{
  const auto& entities = m_document.map().selection().entities;
  const auto parsed = entities.size() == 1 ? mdl::readPath(entities.front()->entity())
                                           : Result<mdl::Path>{Error{"No path selected"}};
  const auto validPath = parsed.is_success();
  const auto validPoint = validPath && !parsed.value().nodes.empty();
  const auto countBlocker = QSignalBlocker{m_count};
  const auto pointBlocker = QSignalBlocker{m_point};
  const auto rollBlocker = QSignalBlocker{m_roll};
  m_count->setEnabled(validPath);
  m_point->setEnabled(validPoint);
  m_roll->setEnabled(validPoint);
  if (validPath)
  {
    m_count->setValue(int(parsed.value().nodes.size()));
  }
  if (validPoint)
  {
    m_point->setMaximum(int(parsed.value().nodes.size() - 1));
    m_roll->setValue(parsed.value().nodes[size_t(m_point->value())].roll);
  }
  else
  {
    m_point->setMaximum(0);
    m_roll->setValue(0.0);
  }
}

void PathToolPage::setPointCount(const int count)
{
  const auto& entities = m_document.map().selection().entities;
  if (entities.size() == 1)
  {
    mdl::setPathNodeCount(m_document.map(), *entities.front(), size_t(count));
  }
}

void PathToolPage::setRoll(const double degrees)
{
  const auto& entities = m_document.map().selection().entities;
  if (entities.size() == 1)
  {
    mdl::setPathNodeRoll(
      m_document.map(), *entities.front(), size_t(m_point->value()), degrees);
  }
}
} // namespace tb::ui
