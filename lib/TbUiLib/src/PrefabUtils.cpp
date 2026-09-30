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

#include "ui/PrefabUtils.h"

#include <QFile>
#include <QInputDialog>
#include <QMessageBox>
#include <QObject>
#include <QString>
#include <QWidget>

#include "base/Logger.h"
#include "fs/DiskIO.h"
#include "fs/PathInfo.h"
#include "mdl/GroupNode.h"
#include "mdl/Map.h"
#include "mdl/Prefab.h"
#include "ui/QPathUtils.h"

#include "kd/string_format.h"

#include <optional>
#include <string>

namespace tb::ui
{
namespace
{

std::optional<std::string> queryPrefabName(
  QWidget* parent, const QString& title, const QString& label, std::string suggestion)
{
  while (true)
  {
    auto ok = false;
    const auto name = kdl::str_trim(
      QInputDialog::getText(
        parent, title, label, QLineEdit::Normal, QString::fromStdString(suggestion), &ok)
        .toStdString());

    if (!ok)
    {
      return std::nullopt;
    }

    const auto valid =
      mdl::validatePrefabName(name) | kdl::transform([]() { return true; })
      | kdl::transform_error([&](const auto& e) {
          QMessageBox::warning(parent, title, QString::fromStdString(e.msg));
          return false;
        })
      | kdl::value();
    if (valid)
    {
      return name;
    }

    suggestion = name;
  }
}

std::string suggestPrefabName(const mdl::Map& map)
{
  const auto& nodes = map.selection().nodes;
  if (nodes.size() == 1u)
  {
    if (const auto* groupNode = dynamic_cast<const mdl::GroupNode*>(nodes.front()))
    {
      return groupNode->name();
    }
  }
  return "";
}

} // namespace

bool canCreatePrefabFromSelection(const mdl::Map& map)
{
  return map.selection().hasNodes();
}

bool createPrefabFromSelection(QWidget* parent, mdl::Map& map)
{
  const auto title = QObject::tr("Create Entity from Selection");
  const auto directory = mdl::prefabDirectory(map);
  const auto label =
    QObject::tr(
      "The selection is grouped and saved as a prefab in\n%1\n\nDrag it into any map "
      "from the Prefabs section of the entity browser.\n\nName:")
      .arg(pathAsQString(directory));

  auto suggestion = suggestPrefabName(map);
  while (true)
  {
    const auto name = queryPrefabName(parent, title, label, suggestion);
    if (!name)
    {
      return false;
    }

    const auto path = mdl::prefabPath(directory, *name);
    if (
      fs::Disk::pathInfo(path) == fs::PathInfo::File
      && QMessageBox::question(
           parent,
           title,
           QObject::tr("A prefab named '%1' already exists. Do you want to replace it?")
             .arg(QString::fromStdString(*name)),
           QMessageBox::Yes | QMessageBox::No,
           QMessageBox::No)
           != QMessageBox::Yes)
    {
      suggestion = *name;
      continue;
    }

    return mdl::createPrefabFromSelection(map, *name, path) | kdl::transform([&](auto) {
             map.logger().info() << "Saved prefab '" << *name << "' to " << path;
             return true;
           })
           | kdl::transform_error([&](const auto& e) {
               QMessageBox::critical(parent, title, QString::fromStdString(e.msg));
               return false;
             })
           | kdl::value();
  }
}

bool renamePrefab(QWidget* parent, const mdl::PrefabFile& prefab)
{
  const auto title = QObject::tr("Rename Prefab");

  auto suggestion = prefab.name;
  while (true)
  {
    const auto name = queryPrefabName(parent, title, QObject::tr("Name:"), suggestion);
    if (!name || *name == prefab.name)
    {
      return false;
    }

    const auto renamed =
      mdl::renamePrefab(prefab.path, *name)
      | kdl::transform([](const auto&) { return true; })
      | kdl::transform_error([&](const auto& e) {
          QMessageBox::warning(parent, title, QString::fromStdString(e.msg));
          return false;
        })
      | kdl::value();
    if (renamed)
    {
      return true;
    }

    suggestion = *name;
  }
}

bool deletePrefab(QWidget* parent, const mdl::PrefabFile& prefab)
{
  const auto title = QObject::tr("Delete Prefab");
  if (
    QMessageBox::question(
      parent,
      title,
      QObject::tr(
        "Move the prefab '%1' to the trash?\n\nMaps that already contain it keep their "
        "copies.")
        .arg(QString::fromStdString(prefab.name)),
      QMessageBox::Yes | QMessageBox::No,
      QMessageBox::No)
    != QMessageBox::Yes)
  {
    return false;
  }

  if (!QFile::moveToTrash(pathAsQString(prefab.path)))
  {
    QMessageBox::warning(
      parent,
      title,
      QObject::tr("Could not move %1 to the trash.").arg(pathAsQString(prefab.path)));
    return false;
  }

  return true;
}

} // namespace tb::ui
