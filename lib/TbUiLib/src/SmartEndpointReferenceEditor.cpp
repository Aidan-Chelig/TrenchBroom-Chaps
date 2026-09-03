/* Copyright (C) 2026 TrenchBroom Authors */
#include "ui/SmartEndpointReferenceEditor.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include "mdl/EntityDefinition.h"
#include "mdl/EntityDefinitionUtils.h"
#include "mdl/EntityNode.h"
#include "mdl/EntityProperties.h"
#include "mdl/Map.h"
#include "mdl/NodeQueries.h"
#include "mdl/PropertyDefinition.h"
#include "ui/MapDocument.h"
#include "ui/QStringUtils.h"
#include "ui/ViewConstants.h"

#include "kd/set_temp.h"

namespace tb::ui
{
SmartEndpointReferenceEditor::SmartEndpointReferenceEditor(
  MapDocument& document, QWidget* parent)
  : SmartPropertyEditor{document, parent}
{
  m_endpoints = new QComboBox{};
  m_endpoints->setEditable(true);
  m_endpoints->setInsertPolicy(QComboBox::NoInsert);
  connect(m_endpoints, &QComboBox::activated, this, [this] { commitValue(); });
  connect(m_endpoints->lineEdit(), &QLineEdit::editingFinished, this, [this] {
    commitValue();
  });
  m_status = new QLabel{};
  m_status->setStyleSheet("color: #d05050");

  auto* layout = new QVBoxLayout{};
  layout->setContentsMargins(
    LayoutConstants::WideHMargin,
    LayoutConstants::WideVMargin,
    LayoutConstants::WideHMargin,
    LayoutConstants::WideVMargin);
  layout->addWidget(new QLabel{tr("Interface endpoint:")});
  layout->addWidget(m_endpoints);
  layout->addWidget(m_status);
  layout->addStretch(1);
  setLayout(layout);
}

void SmartEndpointReferenceEditor::commitValue()
{
  if (!m_updating)
  {
    const auto index = m_endpoints->currentIndex();
    const auto value =
      index >= 0 ? m_endpoints->itemData(index).toString() : m_endpoints->currentText();
    addOrUpdateProperty(mapStringFromUnicode(document().map().encoding(), value));
  }
}

void SmartEndpointReferenceEditor::doUpdateVisual(
  const std::vector<mdl::EntityNodeBase*>& nodes)
{
  const auto updating = kdl::set_temp{m_updating, true};
  auto& map = document().map();
  const auto current = mdl::selectPropertyValue(propertyKey(), nodes);
  const auto* propertyDefinition = mdl::selectPropertyDefinition(propertyKey(), nodes);
  const auto* reference = propertyDefinition
                            ? std::get_if<mdl::PropertyValueTypes::EndpointReference>(
                                &propertyDefinition->valueType)
                            : nullptr;
  m_endpoints->clear();

  const mdl::EntityInterfaceDefinition* interfaceDefinition = nullptr;
  auto targetCount = 0u;
  if (reference)
  {
    const auto targetname = mdl::selectPropertyValue(reference->entityProperty, nodes);
    for (auto* node : mdl::collectDescendants(std::vector<mdl::Node*>{&map.worldNode()}))
    {
      const auto* entityNode = dynamic_cast<mdl::EntityNode*>(node);
      if (!entityNode)
      {
        continue;
      }
      const auto nameMatches = std::ranges::any_of(
        mdl::getLinkTargetPropertyDefinitions(entityNode->entity().definition()),
        [&](const auto* linkTargetDefinition) {
          const auto* name = entityNode->entity().property(linkTargetDefinition->key);
          return name && *name == targetname;
        });
      if (!nameMatches)
      {
        continue;
      }
      ++targetCount;
      if (const auto* definition = entityNode->entity().definition())
      {
        interfaceDefinition = mdl::getEntityInterfaceDefinition(
          definition->interfaces, reference->interfaceName);
      }
    }
  }

  if (interfaceDefinition)
  {
    for (const auto& endpoint : interfaceDefinition->endpoints)
    {
      const auto label = endpoint.displayName.value_or(endpoint.name);
      m_endpoints->addItem(
        mapStringToUnicode(map.encoding(), label),
        mapStringToUnicode(map.encoding(), endpoint.name));
    }
  }

  const auto currentText = mapStringToUnicode(map.encoding(), current);
  const auto currentIndex = m_endpoints->findData(currentText);
  if (!current.empty() && currentIndex < 0)
  {
    m_endpoints->insertItem(0, tr("%1 (invalid)").arg(currentText), currentText);
    m_endpoints->setCurrentIndex(0);
  }
  else
  {
    m_endpoints->setCurrentIndex(currentIndex);
    m_endpoints->setEditText(currentText);
  }

  if (targetCount == 0)
  {
    m_status->setText(tr("The referenced entity does not exist."));
  }
  else if (targetCount > 1)
  {
    m_status->setText(tr("More than one entity has the referenced targetname."));
  }
  else if (!interfaceDefinition)
  {
    m_status->setText(tr("The referenced entity does not provide this interface."));
  }
  else if (!current.empty() && currentIndex < 0)
  {
    m_status->setText(tr("This endpoint is not provided by the referenced entity."));
  }
  else
  {
    m_status->clear();
  }
  m_endpoints->setEnabled(!nodes.empty());
}
} // namespace tb::ui
