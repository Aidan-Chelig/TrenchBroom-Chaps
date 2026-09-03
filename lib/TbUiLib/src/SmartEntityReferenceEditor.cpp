/* Copyright (C) 2026 TrenchBroom Authors */
#include "ui/SmartEntityReferenceEditor.h"

#include <QComboBox>
#include <QCompleter>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "mdl/EntityDefinition.h"
#include "mdl/EntityDefinitionUtils.h"
#include "mdl/EntityNode.h"
#include "mdl/EntityProperties.h"
#include "mdl/Map.h"
#include "mdl/Map_Selection.h"
#include "mdl/NodeQueries.h"
#include "mdl/PropertyDefinition.h"
#include "ui/MapDocument.h"
#include "ui/QStringUtils.h"
#include "ui/ViewConstants.h"

#include "kd/set_temp.h"

#include <ranges>

namespace tb::ui
{
namespace
{
std::vector<std::pair<mdl::EntityNode*, std::string_view>> namedEntities(mdl::Map& map)
{
  auto result = std::vector<std::pair<mdl::EntityNode*, std::string_view>>{};
  for (auto* node : mdl::collectDescendants(std::vector<mdl::Node*>{&map.worldNode()}))
  {
    if (auto* entityNode = dynamic_cast<mdl::EntityNode*>(node))
    {
      for (const auto* propertyDefinition :
           mdl::getLinkTargetPropertyDefinitions(entityNode->entity().definition()))
      {
        if (const auto* name = entityNode->entity().property(propertyDefinition->key);
            name && !name->empty())
        {
          result.emplace_back(entityNode, *name);
        }
      }
    }
  }
  return result;
}
} // namespace

SmartEntityReferenceEditor::SmartEntityReferenceEditor(
  MapDocument& document, QWidget* parent)
  : SmartPropertyEditor{document, parent}
{
  m_entities = new QComboBox{};
  m_entities->setEditable(true);
  m_entities->setInsertPolicy(QComboBox::NoInsert);
  m_entities->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  m_entities->setMinimumContentsLength(24);
  m_entities->completer()->setCaseSensitivity(Qt::CaseInsensitive);
  m_entities->completer()->setFilterMode(Qt::MatchContains);
  connect(m_entities, &QComboBox::activated, this, [this] { commitValue(); });
  connect(
    m_entities->lineEdit(), &QLineEdit::editingFinished, this, [this] { commitValue(); });

  auto* jumpButton = new QPushButton{tr("Select")};
  jumpButton->setToolTip(tr("Select the referenced entity in the map"));
  connect(
    jumpButton, &QPushButton::clicked, this, &SmartEntityReferenceEditor::jumpToEntity);

  auto* row = new QHBoxLayout{};
  row->setContentsMargins(0, 0, 0, 0);
  row->addWidget(m_entities, 1);
  row->addWidget(jumpButton);

  m_status = new QLabel{};
  m_status->setStyleSheet("color: #d05050");
  auto* layout = new QVBoxLayout{};
  layout->setContentsMargins(
    LayoutConstants::WideHMargin,
    LayoutConstants::WideVMargin,
    LayoutConstants::WideHMargin,
    LayoutConstants::WideVMargin);
  layout->addWidget(new QLabel{tr("Referenced entity:")});
  layout->addLayout(row);
  layout->addWidget(m_status);
  layout->addStretch(1);
  setLayout(layout);
}

void SmartEntityReferenceEditor::commitValue()
{
  if (!m_updating)
  {
    const auto index = m_entities->currentIndex();
    const auto value =
      index >= 0 ? m_entities->itemData(index).toString() : m_entities->currentText();
    addOrUpdateProperty(mapStringFromUnicode(document().map().encoding(), value));
  }
}

void SmartEntityReferenceEditor::jumpToEntity()
{
  auto& map = document().map();
  const auto index = m_entities->currentIndex();
  const auto text =
    index >= 0 ? m_entities->itemData(index).toString() : m_entities->currentText();
  const auto value = mapStringFromUnicode(map.encoding(), text);
  auto matches = std::vector<mdl::Node*>{};
  for (const auto& [node, name] : namedEntities(map))
  {
    if (name == value)
    {
      matches.push_back(node);
    }
  }
  if (!matches.empty())
  {
    mdl::deselectAll(map);
    mdl::selectNodes(map, matches);
  }
}

void SmartEntityReferenceEditor::doUpdateVisual(
  const std::vector<mdl::EntityNodeBase*>& nodes)
{
  const auto updating = kdl::set_temp{m_updating, true};
  auto& map = document().map();
  const auto current = mdl::selectPropertyValue(propertyKey(), nodes);
  const auto* propertyDefinition = mdl::selectPropertyDefinition(propertyKey(), nodes);
  const auto* reference = propertyDefinition
                            ? std::get_if<mdl::PropertyValueTypes::EntityReference>(
                                &propertyDefinition->valueType)
                            : nullptr;

  m_entities->clear();
  auto matchCount = 0u;
  for (const auto& [node, targetname] : namedEntities(map))
  {
    const auto* definition = node->entity().definition();
    if (
      reference && reference->requiredInterface
      && (!definition
          || !mdl::getEntityInterfaceDefinition(
            definition->interfaces, *reference->requiredInterface)))
    {
      continue;
    }
    if (targetname == current)
    {
      ++matchCount;
    }
    m_entities->addItem(
      mapStringToUnicode(
        map.encoding(), std::string{targetname} + " — " + node->entity().classname()),
      mapStringToUnicode(map.encoding(), std::string{targetname}));
  }

  const auto currentText = mapStringToUnicode(map.encoding(), current);
  const auto currentIndex = m_entities->findData(currentText);
  if (!current.empty() && currentIndex < 0)
  {
    m_entities->insertItem(0, tr("%1 (invalid)").arg(currentText), currentText);
    m_entities->setCurrentIndex(0);
    m_status->setText(
      tr("The referenced entity is missing or does not provide the required interface."));
  }
  else
  {
    m_entities->setCurrentIndex(currentIndex);
    m_entities->setEditText(currentText);
    m_status->setText(
      matchCount > 1 ? tr("More than one entity has this targetname.") : QString{});
  }
  m_entities->setEnabled(!nodes.empty());
}
} // namespace tb::ui
