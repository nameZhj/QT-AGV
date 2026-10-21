/**
 * @file    MapPropsManager.cpp
 * @layer   GUI
 * @brief   Implementation of composite property manager widget.
 * @deps    MapPropsManager.h, MapPropRowHelper.h, MapStationPropsPanel.h, MapLinePropsPanel.h, MapAreaPropsPanel.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapPropsManager.h"
#include "MapPropRowHelper.h"
#include "MapStationPropsPanel.h"
#include "MapLinePropsPanel.h"
#include "MapAreaPropsPanel.h"
#include "MapEditDataModel.h"

#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

using namespace MapPropRowHelper;

MapPropsManager::MapPropsManager(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void MapPropsManager::setupUI()
{
    setStyleSheet("background:transparent; border:none;");
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto [card, pl] = mkCardContainer("要素属性");

    // 顶栏: 标题 + Badge
    auto *headerBar = new QWidget;
    headerBar->setStyleSheet("background:transparent; border:none;");
    auto *hbl = new QHBoxLayout(headerBar);
    hbl->setContentsMargins(0, 0, 0, 4);
    m_propHeaderTitle = new QLabel("图元属性");
    m_propHeaderTitle->setStyleSheet("color:#e2ecf7; font-size:13px; font-weight:bold; background:transparent; border:none;");
    m_propHeaderBadge = new QLabel("LM");
    m_propHeaderBadge->setStyleSheet("background:#1e3a5f; color:#60a5fa; border:1px solid #2563eb; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;");
    hbl->addWidget(m_propHeaderTitle);
    hbl->addStretch();
    hbl->addWidget(m_propHeaderBadge);
    pl->addWidget(headerBar);

    // 三大子面板
    m_stationProps = new MapStationPropsPanel(this);
    m_lineProps = new MapLinePropsPanel(this);
    m_areaProps = new MapAreaPropsPanel(this);
    pl->addWidget(m_stationProps);
    pl->addWidget(m_lineProps);
    pl->addWidget(m_areaProps);
    m_stationProps->setVisible(false);
    m_lineProps->setVisible(false);
    m_areaProps->setVisible(false);

    // 操作按钮栏 (应用修改 / 删除图元)
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);
    m_propApplyBtn = new QPushButton("应用修改");
    m_propDeleteBtn = new QPushButton("删除图元");
    m_propApplyBtn->setCursor(Qt::PointingHandCursor);
    m_propDeleteBtn->setCursor(Qt::PointingHandCursor);
    m_propApplyBtn->setStyleSheet(
        "QPushButton{background:#1296DB; color:#fff; font-weight:bold; border:none; border-radius:4px; padding:6px 12px; font-size:12px;}"
        "QPushButton:hover{background:#23a2e6;}");
    m_propDeleteBtn->setStyleSheet(
        "QPushButton{background:#c0392b; color:#fff; font-weight:bold; border:none; border-radius:4px; padding:6px 12px; font-size:12px;}"
        "QPushButton:hover{background:#d9534f;}");

    connect(m_propApplyBtn, &QPushButton::clicked, this, &MapPropsManager::applyClicked);
    connect(m_propDeleteBtn, &QPushButton::clicked, this, &MapPropsManager::deleteClicked);
    btnRow->addWidget(m_propApplyBtn, 1);
    btnRow->addWidget(m_propDeleteBtn, 1);
    pl->addLayout(btnRow);

    root->addWidget(card);
    setVisible(false);
}

void MapPropsManager::updateBadge(const QString &type)
{
    if (type == "highLevelArea") {
        m_propHeaderTitle->setText("高级区域属性");
        m_propHeaderBadge->setText("AA");
        m_propHeaderBadge->setStyleSheet("background:#3b1d5c; color:#c084fc; border:1px solid #7c3aed; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;");
    } else if (type == "line" || type == "curve") {
        m_propHeaderTitle->setText("线路属性");
        m_propHeaderBadge->setText(type == "line" ? "直线" : "贝塞尔曲线");
        m_propHeaderBadge->setStyleSheet("background:#17344f; color:#38bdf8; border:1px solid #0284c7; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;");
    } else {
        m_propHeaderTitle->setText("站点属性");
        QString badgeText = (type == "point") ? "LM" : ((type == "charge") ? "CP" : "TL");
        QString badgeStyle = (type == "point")
            ? "background:#1e3a5f; color:#60a5fa; border:1px solid #2563eb; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;"
            : ((type == "charge")
                ? "background:#4d3800; color:#fbbf24; border:1px solid #d97706; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;"
                : "background:#4d1414; color:#f87171; border:1px solid #dc2626; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;");
        m_propHeaderBadge->setText(badgeText);
        m_propHeaderBadge->setStyleSheet(badgeStyle);
    }
}

void MapPropsManager::showProperties(const QString &type, const QString &id, const QJsonObject &obj)
{
    m_selType = type;
    m_selId = id;
    updateBadge(type);

    if (type == "highLevelArea") {
        m_stationProps->setVisible(false);
        m_lineProps->setVisible(false);
        m_areaProps->setVisible(true);
        m_areaProps->populate(obj);
    } else if (type == "point" || type == "charge" || type == "highPrecision") {
        m_stationProps->setVisible(true);
        m_lineProps->setVisible(false);
        m_areaProps->setVisible(false);
        m_stationProps->populate(obj, type, id);
    } else {
        m_stationProps->setVisible(false);
        m_lineProps->setVisible(true);
        m_areaProps->setVisible(false);
        m_lineProps->populate(obj, type, id);
    }

    setVisible(true);
}

void MapPropsManager::applyEdit(MapEditDataModel *model)
{
    if (m_selType.isEmpty() || !model) return;

    if (m_selType == "highLevelArea") {
        m_areaProps->applyEdit(model, m_selId);
    } else if (m_selType == "point" || m_selType == "charge" || m_selType == "highPrecision") {
        m_stationProps->applyEdit(model, m_selType, m_selId);
    } else {
        m_lineProps->applyEdit(model, m_selType, m_selId);
    }

    updateBadge(m_selType);
    emit featureUpdated(m_selType, m_selId);
}

void MapPropsManager::syncMotionSpinBoxRanges()
{
    if (m_lineProps) m_lineProps->syncMotionSpinBoxRanges();
    if (m_areaProps) m_areaProps->syncMotionSpinBoxRanges();
}

void MapPropsManager::clearSelection()
{
    m_selType.clear();
    m_selId.clear();
    setVisible(false);
}
