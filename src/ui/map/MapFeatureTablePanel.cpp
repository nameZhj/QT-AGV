/**
 * @file    MapFeatureTablePanel.cpp
 * @layer   GUI
 * @brief   Implementation of map feature table panel.
 * @deps    MapFeatureTablePanel.h, MapPropRowHelper.h, MapEditDataModel.h, MapEditStyle.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapFeatureTablePanel.h"
#include "MapPropRowHelper.h"
#include "MapEditDataModel.h"
#include "MapEditStyle.h"

#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QJsonObject>

using namespace MapPropRowHelper;

MapFeatureTablePanel::MapFeatureTablePanel(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void MapFeatureTablePanel::setupUI()
{
    setStyleSheet("background:transparent; border:none;");
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto [card, tl2] = mkCardContainer("要素列表");

    m_tableTypeCombo = new QComboBox;
    m_tableTypeCombo->addItem("普通站点", "point");
    m_tableTypeCombo->addItem("线段",     "line");
    m_tableTypeCombo->addItem("曲线",     "curve");
    m_tableTypeCombo->addItem("充电点",   "charge");
    m_tableTypeCombo->addItem("精定位点", "highPrecision");
    m_tableTypeCombo->setStyleSheet("background:#1e2d40; color:#b0c8e0; border:1px solid rgba(255,255,255,0.10); border-radius:3px; padding:2px 4px; font-size:12px;");
    tl2->addWidget(m_tableTypeCombo);

    m_featureTable = new QTableWidget;
    m_featureTable->setColumnCount(2);
    m_featureTable->setHorizontalHeaderLabels({"编号", "名称"});
    m_featureTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_featureTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_featureTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_featureTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_featureTable->setMaximumHeight(210);
    m_featureTable->setStyleSheet(MapEditStyle::S_TABLE);

    connect(m_featureTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        int row = m_featureTable->currentRow();
        if (row < 0) return;
        auto *it = m_featureTable->item(row, 0);
        if (it) emit featureSelected(currentTableType(), it->text());
    });
    tl2->addWidget(m_featureTable);

    m_delBtn = new QPushButton("删除选中");
    m_delBtn->setStyleSheet(
        "QPushButton{background:rgba(192,57,43,160);color:white;border:none;"
        "border-radius:4px;padding:5px;font-size:11px;}"
        "QPushButton:hover{background:rgba(231,76,60,190);}");
    connect(m_delBtn, &QPushButton::clicked, this, [this]() {
        int row = m_featureTable->currentRow();
        if (row < 0) return;
        auto *it = m_featureTable->item(row, 0);
        if (it) emit deleteRequested(currentTableType(), it->text());
    });
    tl2->addWidget(m_delBtn);

    root->addWidget(card);
}

QString MapFeatureTablePanel::currentTableType() const
{
    return m_tableTypeCombo->currentData().toString();
}

void MapFeatureTablePanel::setCurrentTableType(const QString &type)
{
    int idx = m_tableTypeCombo->findData(type);
    if (idx >= 0 && m_tableTypeCombo->currentIndex() != idx) {
        m_tableTypeCombo->setCurrentIndex(idx);
    }
}

void MapFeatureTablePanel::refresh(const MapEditDataModel *model)
{
    if (!model) return;
    const QJsonArray *arr = nullptr;
    QString cur = currentTableType();
    if      (cur == "point")         arr = &model->points();
    else if (cur == "line")          arr = &model->lines();
    else if (cur == "curve")         arr = &model->curves();
    else if (cur == "charge")        arr = &model->charges();
    else if (cur == "highPrecision") arr = &model->highPrecisionPoints();
    if (!arr) return;

    m_featureTable->blockSignals(true);
    m_featureTable->setRowCount(arr->size());
    for (int i = 0; i < arr->size(); ++i) {
        QJsonObject o = (*arr)[i].toObject();
        m_featureTable->setItem(i, 0, new QTableWidgetItem(o.value("id").toString()));
        m_featureTable->setItem(i, 1, new QTableWidgetItem(o.value("name").toString()));
    }
    m_featureTable->blockSignals(false);
}

void MapFeatureTablePanel::selectFeature(const QString &type, const QString &id)
{
    setCurrentTableType(type);
    for (int r = 0; r < m_featureTable->rowCount(); ++r) {
        auto *it = m_featureTable->item(r, 0);
        if (it && it->text() == id) {
            m_featureTable->blockSignals(true);
            m_featureTable->selectRow(r);
            m_featureTable->blockSignals(false);
            break;
        }
    }
}
