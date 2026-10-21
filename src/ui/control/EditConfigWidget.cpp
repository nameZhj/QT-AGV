/**
 * @file    EditConfigWidget.cpp
 * @layer   GUILayer
 * @brief   Implementation of hierarchical tree widget for system configuration.
 * @deps    EditConfigWidget.h, EditConfigStyle.h, EditConfigDataLoader.h, EditConfigRouteSync.h, EditConfigTableBuilder.h, EditConfigParamEditor.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "EditConfigWidget.h"
#include "EditConfigStyle.h"
#include "EditConfigDataLoader.h"
#include "EditConfigRouteSync.h"
#include "EditConfigTableBuilder.h"
#include "EditConfigParamEditor.h"
#include "ConfigMetadataHelper.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QLabel>
#include <QHeaderView>

using namespace EditConfigStyle;

EditConfigWidget::EditConfigWidget(QWidget *parent) : QWidget(parent) {
  setupUI();
  loadAll();
}

void EditConfigWidget::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  QHBoxLayout *toolbar = new QHBoxLayout;

  auto addToolBtn = [&](const QString &iconName, const QString &text) {
    auto *b = new QToolButton;
    b->setText(text);
    b->setIcon(makeCfgToolIcon(iconName));
    b->setIconSize(QSize(20, 20));
    b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    b->setAutoRaise(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(kToolBtnStyle);
    return b;
  };

  m_saveBtn = addToolBtn("save", "保存");
  m_saveBtn->setStyleSheet(kToolBtnPrimary);
  m_downloadBtn = addToolBtn("download", "下载参数文件");
  m_uploadBtn = addToolBtn("upload", "上传参数文件");

  m_filterEdit = new QLineEdit;
  m_filterEdit->setPlaceholderText("搜索参数...");

  connect(m_saveBtn, &QToolButton::clicked, this, &EditConfigWidget::onSave);
  connect(m_downloadBtn, &QToolButton::clicked, this, &EditConfigWidget::onDownload);
  connect(m_uploadBtn, &QToolButton::clicked, this, &EditConfigWidget::onUpload);
  connect(m_filterEdit, &QLineEdit::textChanged, this, &EditConfigWidget::onFilterChanged);

  toolbar->addWidget(m_saveBtn);
  toolbar->addWidget(m_downloadBtn);
  toolbar->addWidget(m_uploadBtn);
  toolbar->addStretch();
  toolbar->addWidget(new QLabel("搜索:"));
  toolbar->addWidget(m_filterEdit);
  mainLayout->addLayout(toolbar);

  QSplitter *splitter = new QSplitter(Qt::Horizontal);

  m_groupTree = new QTreeWidget;
  m_groupTree->setHeaderLabel("配置分组");
  m_groupTree->setMaximumWidth(250);
  connect(m_groupTree, &QTreeWidget::itemClicked, this, &EditConfigWidget::onGroupItemClicked);
  connect(m_groupTree, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem *current, QTreeWidgetItem *previous) {
            Q_UNUSED(previous)
            if (current) onGroupItemClicked(current, 0);
          });
  splitter->addWidget(m_groupTree);

  m_paramTable = new QTableWidget;
  m_paramTable->setColumnCount(4);
  m_paramTable->setHorizontalHeaderLabels({"参数名", "值", "默认值", "说明"});
  m_paramTable->horizontalHeader()->setStretchLastSection(true);
  m_paramTable->setColumnWidth(0, 220);
  m_paramTable->setColumnWidth(1, 260);
  m_paramTable->setColumnWidth(2, 200);
  m_paramTable->setItemDelegateForColumn(1, new ParamItemDelegate(m_paramTable));
  m_paramTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
  m_paramTable->verticalHeader()->setDefaultSectionSize(40);
  m_paramTable->verticalHeader()->setMinimumSectionSize(40);
  m_paramTable->setStyleSheet(EditConfigStyle::kParamTableStyle);
  connect(m_paramTable, &QTableWidget::cellChanged, this, &EditConfigWidget::onParamCellChanged);
  splitter->addWidget(m_paramTable);

  splitter->setSizes({250, 800});
  mainLayout->addWidget(splitter);
}

void EditConfigWidget::refreshData() { loadAll(); }

void EditConfigWidget::loadAll() {
  EditConfigDataLoader::loadAll(this);
}

void EditConfigWidget::refreshGroups() {
  m_groupTree->clear();
  QMap<QString, QList<int>> groupIndices;
  QStringList profileOrder;

  for (int i = 0; i < m_configs.size(); ++i) {
    QJsonObject entry = m_configs[i].toObject();
    QString profileKey = ConfigMetadataHelper::determineProfile(entry);
    if (!groupIndices.contains(profileKey)) {
      profileOrder.append(profileKey);
    }
    groupIndices[profileKey].append(i);
  }

  for (const QString &profileKey : profileOrder) {
    QString profileName = ConfigMetadataHelper::getProfileName(profileKey);
    const QList<int> &indices = groupIndices[profileKey];

    QTreeWidgetItem *topItem = new QTreeWidgetItem;
    topItem->setText(0, profileName);
    topItem->setData(0, Qt::UserRole, profileKey);

    if (indices.size() == 1) {
      int configIdx = indices.first();
      topItem->setData(0, Qt::UserRole + 1, configIdx);
      m_groupTree->addTopLevelItem(topItem);
    } else {
      m_groupTree->addTopLevelItem(topItem);
      for (int idx : indices) {
        QJsonObject entry = m_configs[idx].toObject();
        QTreeWidgetItem *child = new QTreeWidgetItem;
        child->setText(0, ConfigMetadataHelper::getChildDisplayName(entry));
        child->setData(0, Qt::UserRole, profileKey);
        child->setData(0, Qt::UserRole + 1, idx);
        topItem->addChild(child);
      }
    }
  }

  m_groupTree->expandAll();
  if (m_groupTree->topLevelItemCount() > 0) {
    QTreeWidgetItem *firstItem = m_groupTree->topLevelItem(0);
    m_groupTree->setCurrentItem(firstItem);
    onGroupItemClicked(firstItem, 0);
  }
}

void EditConfigWidget::refreshParams(const QString &groupKey) {
  EditConfigTableBuilder::refreshParams(this, groupKey);
}

void EditConfigWidget::refreshParamsForConfig(int configIdx) {
  EditConfigTableBuilder::refreshParamsForConfig(this, configIdx);
}

void EditConfigWidget::addParamRows(const QJsonObject &params, int configIdx,
                                    const QString &prefix, const QString &filter) {
  EditConfigTableBuilder::addParamRows(this, params, configIdx, prefix, filter);
}

void EditConfigWidget::parseMetadata(const QByteArray &rawJson) {
  EditConfigDataLoader::parseMetadata(this, rawJson);
}

void EditConfigWidget::updateDefaultValuesInTable() {
  EditConfigTableBuilder::updateDefaultValuesInTable(this);
}

void EditConfigWidget::syncObstacleParamsToRouteNetwork(double stopDx, double stopDy,
                                                       double decelDx, double decelDy,
                                                       std::function<void(bool ok)> onDone) {
  EditConfigRouteSync::syncObstacleParamsToRouteNetwork(this, stopDx, stopDy, decelDx, decelDy, onDone);
}

void EditConfigWidget::onSave() {
  EditConfigDataLoader::onSave(this);
}

void EditConfigWidget::onDownload() {
  EditConfigDataLoader::onDownload(this);
}

void EditConfigWidget::onUpload() {
  EditConfigDataLoader::onUpload(this);
}

void EditConfigWidget::onFilterChanged(const QString &text) {
  Q_UNUSED(text)
  if (m_currentConfigIdx >= 0)
    refreshParamsForConfig(m_currentConfigIdx);
  else if (!m_currentGroupKey.isEmpty())
    refreshParams(m_currentGroupKey);
}

void EditConfigWidget::onGroupItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item) return;
  QString key = item->data(0, Qt::UserRole).toString();
  if (key.isEmpty()) return;

  bool ok = false;
  int configIdx = item->data(0, Qt::UserRole + 1).toInt(&ok);
  if (!ok) configIdx = -1;

  if (configIdx >= 0 && configIdx < m_configs.size()) {
    m_currentGroupKey = key;
    m_currentConfigIdx = configIdx;
    refreshParamsForConfig(configIdx);
  } else {
    m_currentGroupKey = key;
    m_currentConfigIdx = -1;
    refreshParams(key);
  }
}

void EditConfigWidget::onParamCellChanged(int row, int col) {
  EditConfigParamEditor::onParamCellChanged(this, row, col);
}
