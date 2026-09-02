/*
 Copyright (C) 2010 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ui/SmartModelEditor.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

#include "mdl/EntityNodeBase.h"
#include "mdl/Map.h"
#include "mdl/Map_Entities.h"
#include "ui/MapDocument.h"
#include "ui/ModelBrowserView.h"
#include "ui/QStringUtils.h"
#include "ui/SearchBox.h"
#include "ui/ViewConstants.h"

#include "kd/contracts.h"
#include "kd/set_temp.h"

namespace tb::ui
{
namespace
{
class ModelBrowserDialog : public QDialog
{
private:
  QString m_selectedPath;

public:
  ModelBrowserDialog(AppController& appController, MapDocument& document, QWidget* parent)
    : QDialog{parent}
  {
    setWindowTitle(tr("Select Model"));
    resize(760, 560);

    auto* scrollBar = new QScrollBar{Qt::Vertical};
    auto* view = new ModelBrowserView{appController, scrollBar, document};
    auto* browserLayout = new QHBoxLayout{};
    browserLayout->setContentsMargins(0, 0, 0, 0);
    browserLayout->setSpacing(0);
    browserLayout->addWidget(view, 1);
    browserLayout->addWidget(scrollBar);
    auto* browser = new QWidget{};
    browser->setLayout(browserLayout);

    auto* search = createSearchBox();
    search->setPlaceholderText(tr("Search models"));
    connect(search, &QLineEdit::textEdited, this, [=](const QString& text) {
      view->setFilterText(text.toStdString());
    });

    auto* buttons = new QDialogButtonBox{QDialogButtonBox::Cancel};
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(view, &ModelBrowserView::modelSelected, this, [this](const QString& path) {
      m_selectedPath = path;
      accept();
    });

    auto* layout = new QVBoxLayout{};
    layout->addWidget(search);
    layout->addWidget(browser, 1);
    layout->addWidget(new QLabel{tr("Click a thumbnail to select its asset path.")});
    layout->addWidget(buttons);
    setLayout(layout);
  }

  const QString& selectedPath() const { return m_selectedPath; }
};
} // namespace

SmartModelEditor::SmartModelEditor(
  AppController& appController, MapDocument& document, QWidget* parent)
  : SmartPropertyEditor{document, parent}
  , m_appController{appController}
{
  createGui();
}

void SmartModelEditor::createGui()
{
  m_pathEditor = new QLineEdit{};
  connect(m_pathEditor, &QLineEdit::editingFinished, this, &SmartModelEditor::commitPath);
  auto* chooseButton = new QPushButton{tr("...")};
  chooseButton->setToolTip(tr("Browse models"));
  chooseButton->setMaximumWidth(40);
  connect(chooseButton, &QPushButton::clicked, this, &SmartModelEditor::chooseModel);

  auto* pathLayout = new QHBoxLayout{};
  pathLayout->setContentsMargins(0, 0, 0, 0);
  pathLayout->addWidget(m_pathEditor, 1);
  pathLayout->addWidget(chooseButton);
  auto* layout = new QVBoxLayout{};
  layout->setContentsMargins(
    LayoutConstants::WideHMargin,
    LayoutConstants::WideVMargin,
    LayoutConstants::WideHMargin,
    LayoutConstants::WideVMargin);
  layout->setSpacing(LayoutConstants::NarrowVMargin);
  layout->addWidget(new QLabel{tr("Model asset path:")});
  layout->addLayout(pathLayout);
  layout->addStretch(1);
  setLayout(layout);
}

void SmartModelEditor::chooseModel()
{
  auto dialog = ModelBrowserDialog{m_appController, document(), this};
  if (dialog.exec() == QDialog::Accepted)
  {
    m_pathEditor->setText(dialog.selectedPath());
    commitPath();
  }
}

void SmartModelEditor::commitPath()
{
  if (!m_updating)
  {
    auto& map = document().map();
    addOrUpdateProperty(mapStringFromUnicode(map.encoding(), m_pathEditor->text()));
  }
}

void SmartModelEditor::doUpdateVisual(const std::vector<mdl::EntityNodeBase*>& nodes)
{
  contract_pre(m_pathEditor != nullptr);
  const auto updating = kdl::set_temp{m_updating, true};
  const auto& map = document().map();
  m_pathEditor->setText(
    mapStringToUnicode(map.encoding(), mdl::selectPropertyValue(propertyKey(), nodes)));
  m_pathEditor->setEnabled(!nodes.empty());
}
} // namespace tb::ui
