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

#include "ui/SmartParticleEditor.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include "fs/File.h"
#include "fs/FileSystem.h"
#include "fs/PathInfo.h"
#include "fs/PathMatcher.h"
#include "fs/Reader.h"
#include "fs/TraversalMode.h"
#include "mdl/EntityNodeBase.h"
#include "mdl/GameFileSystem.h"
#include "mdl/Map.h"
#include "mdl/Map_Entities.h"
#include "ui/MapDocument.h"
#include "ui/QPathUtils.h"
#include "ui/QStringUtils.h"
#include "ui/SearchBox.h"
#include "ui/ViewConstants.h"

#include "kd/contracts.h"
#include "kd/result.h"
#include "kd/set_temp.h"

#include <algorithm>

namespace tb::ui
{
namespace
{
// Where the game keeps its Sprinkles effects, and where the Sprinkles editor saves them
// when the Caution Tape launcher starts it.
const auto ParticlesFolder = std::filesystem::path{"particles"};
constexpr auto ThumbnailSize = 64;

/**
 * An image saved beside the effect with the same name (campfire.png for campfire.ron),
 * or a null pixmap when there is none.
 */
QPixmap loadThumbnail(const fs::FileSystem& fs, const std::filesystem::path& effectPath)
{
  for (const auto* extension : {".png", ".jpg", ".jpeg"})
  {
    const auto imagePath = std::filesystem::path{effectPath}.replace_extension(extension);
    if (fs.pathInfo(imagePath) != fs::PathInfo::File)
    {
      continue;
    }

    auto pixmap = fs.openFile(imagePath) | kdl::transform([](auto file) {
                    const auto buffer = file->reader().buffer();
                    const auto data = buffer.stringView();
                    auto loaded = QPixmap{};
                    loaded.loadFromData(
                      reinterpret_cast<const uchar*>(data.data()),
                      static_cast<uint>(data.size()));
                    return loaded;
                  })
                  | kdl::value_or(QPixmap{});
    if (!pixmap.isNull())
    {
      return pixmap.scaled(
        ThumbnailSize, ThumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
  }
  return {};
}

class ParticleBrowserDialog : public QDialog
{
private:
  QListWidget* m_list = nullptr;
  QPushButton* m_okButton = nullptr;

public:
  ParticleBrowserDialog(
    const fs::FileSystem& fs, const QString& currentPath, QWidget* parent)
    : QDialog{parent}
  {
    setWindowTitle(tr("Select Particle Effect"));
    resize(560, 520);

    auto* search = createSearchBox();
    search->setPlaceholderText(tr("Search particle effects"));

    m_list = new QListWidget{};
    m_list->setIconSize(QSize{ThumbnailSize, ThumbnailSize});
    m_list->setUniformItemSizes(true);

    auto placeholder = QPixmap{ThumbnailSize, ThumbnailSize};
    placeholder.fill(Qt::transparent);
    for (const auto& path : findParticlePaths(fs))
    {
      const auto thumbnail = loadThumbnail(fs, path);
      auto* item = new QListWidgetItem{
        QIcon{thumbnail.isNull() ? placeholder : thumbnail}, pathAsGenericQString(path)};
      m_list->addItem(item);
      if (item->text() == currentPath)
      {
        m_list->setCurrentItem(item);
      }
    }

    auto* buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
    m_okButton = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(search, &QLineEdit::textEdited, this, [this](const QString& text) {
      for (auto row = 0; row < m_list->count(); ++row)
      {
        auto* item = m_list->item(row);
        item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
      }
    });
    connect(m_list, &QListWidget::currentItemChanged, this, [this] { updateOkButton(); });
    connect(m_list, &QListWidget::itemActivated, this, &QDialog::accept);

    auto* layout = new QVBoxLayout{};
    layout->addWidget(search);
    layout->addWidget(m_list, 1);
    layout->addWidget(new QLabel{
      m_list->count() == 0
        ? tr("No particle effects under particles/ yet. Make one in Sprinkles, from "
             "the launcher, and save it there.")
        : tr("Effects are the .ron files under particles/, saved by Sprinkles.")});
    layout->addWidget(buttons);
    setLayout(layout);

    updateOkButton();
  }

  QString selectedPath() const
  {
    const auto* item = m_list->currentItem();
    return item ? item->text() : QString{};
  }

private:
  void updateOkButton() { m_okButton->setEnabled(m_list->currentItem() != nullptr); }
};
} // namespace

std::vector<std::filesystem::path> findParticlePaths(const fs::FileSystem& fs)
{
  if (fs.pathInfo(ParticlesFolder) != fs::PathInfo::Directory)
  {
    return {};
  }

  auto paths = fs.find(
                 ParticlesFolder,
                 fs::TraversalMode::Recursive,
                 fs::makeExtensionPathMatcher({".ron"}))
               | kdl::value_or(std::vector<std::filesystem::path>{});
  std::ranges::sort(paths);
  return paths;
}

SmartParticleEditor::SmartParticleEditor(MapDocument& document, QWidget* parent)
  : SmartPropertyEditor{document, parent}
{
  createGui();
}

void SmartParticleEditor::createGui()
{
  m_pathEditor = new QLineEdit{};
  connect(
    m_pathEditor, &QLineEdit::editingFinished, this, &SmartParticleEditor::commitPath);
  auto* chooseButton = new QPushButton{tr("...")};
  chooseButton->setToolTip(tr("Browse particle effects"));
  chooseButton->setMaximumWidth(40);
  connect(
    chooseButton, &QPushButton::clicked, this, &SmartParticleEditor::chooseParticle);

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
  layout->addWidget(new QLabel{tr("Particle effect asset path:")});
  layout->addLayout(pathLayout);
  layout->addStretch(1);
  setLayout(layout);
}

void SmartParticleEditor::chooseParticle()
{
  auto dialog =
    ParticleBrowserDialog{document().map().gameFileSystem(), m_pathEditor->text(), this};
  if (dialog.exec() == QDialog::Accepted && !dialog.selectedPath().isEmpty())
  {
    m_pathEditor->setText(dialog.selectedPath());
    commitPath();
  }
}

void SmartParticleEditor::commitPath()
{
  if (!m_updating)
  {
    auto& map = document().map();
    addOrUpdateProperty(mapStringFromUnicode(map.encoding(), m_pathEditor->text()));
  }
}

void SmartParticleEditor::doUpdateVisual(const std::vector<mdl::EntityNodeBase*>& nodes)
{
  contract_pre(m_pathEditor != nullptr);
  const auto updating = kdl::set_temp{m_updating, true};
  const auto& map = document().map();
  m_pathEditor->setText(
    mapStringToUnicode(map.encoding(), mdl::selectPropertyValue(propertyKey(), nodes)));
  m_pathEditor->setEnabled(!nodes.empty());
}

} // namespace tb::ui
