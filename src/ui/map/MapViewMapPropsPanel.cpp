/**
 * @file    MapViewMapPropsPanel.cpp
 * @layer   GUI
 * @brief   Map metadata, canvas resolution, feature counts, and shortcut guidance card layout.
 * @deps    MapViewMapPropsPanel.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewMapPropsPanel.h"
#include <QVBoxLayout>
#include <QFormLayout>

MapViewMapPropsPanel::MapViewMapPropsPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void MapViewMapPropsPanel::setupUI()
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0); lay->setSpacing(10);

    auto mkGrp = [](const QString &t) {
        auto *g = new QGroupBox(t);
        g->setStyleSheet("QGroupBox{border:1px solid #2d3340; border-radius:4px; margin-top:14px; color:#8898a8; font-size:12px; font-weight:600;} QGroupBox::title{subcontrol-origin:margin; left:8px; top:-2px; padding:0 4px;}");
        return g;
    };
    auto mkLbl = [](const QString &v = "--") {
        auto *l = new QLabel(v);
        l->setStyleSheet("color:#c8d0d8; font-size:12px;");
        return l;
    };

    m_mapPropsGroup = mkGrp("地图属性");
    auto *infF = new QFormLayout(m_mapPropsGroup);
    infF->setSpacing(6); infF->setLabelAlignment(Qt::AlignRight);
    m_infoNameLabel   = mkLbl(); m_mapPropIdVal    = mkLbl();
    m_infoSizeLabel   = mkLbl(); m_infoResLabel    = mkLbl();
    m_mapPropAreaVal  = mkLbl(); m_infoNameLabel->setWordWrap(true);
    infF->addRow("地图名称:", m_infoNameLabel);
    infF->addRow("地图编号:", m_mapPropIdVal);
    infF->addRow("尺寸:",     m_infoSizeLabel);
    infF->addRow("分辨率:",   m_infoResLabel);
    infF->addRow("面积:",     m_mapPropAreaVal);
    lay->addWidget(m_mapPropsGroup);

    m_mapCountGroup = mkGrp("图元数量");
    auto *cntF = new QFormLayout(m_mapCountGroup);
    cntF->setSpacing(6); cntF->setLabelAlignment(Qt::AlignRight);
    m_mapCountPointVal  = mkLbl("0"); m_mapCountHpVal     = mkLbl("0");
    m_mapCountChargeVal = mkLbl("0"); m_mapCountLineVal   = mkLbl("0");
    m_mapCountCurveVal  = mkLbl("0"); m_mapCountAreaVal   = mkLbl("0");
    cntF->addRow("普通点:",   m_mapCountPointVal);
    cntF->addRow("高精定位点:", m_mapCountHpVal);
    cntF->addRow("充电桩:",   m_mapCountChargeVal);
    cntF->addRow("路线:",     m_mapCountLineVal);
    cntF->addRow("曲线:",     m_mapCountCurveVal);
    cntF->addRow("高级区域:", m_mapCountAreaVal);
    lay->addWidget(m_mapCountGroup);

    m_hintLabel = new QLabel(
        "<small style='color:#556677;'>"
        "左键点击点位/线条: 查看属性<br>"
        "双击点位/线条: 放大并居中<br>"
        "路径导航模式: 点击点位导航<br>"
        "Alt+拖拽 / 中键: 平移<br>"
        "滚轮/触控板双指: 缩放<br>"
        "Ctrl+滚轮/触控板: 旋转"
        "</small>", this);
    m_hintLabel->setTextFormat(Qt::RichText);
    m_hintLabel->setWordWrap(true);
    lay->addWidget(m_hintLabel);
}

void MapViewMapPropsPanel::updateMapProps(const QString &mapId, const QString &mapName,
                                          double imgW, double imgH,
                                          double wL, double wB, double wR, double wT,
                                          int ptCount, int hpCount, int chgCount,
                                          int lineCount, int curveCount, int areaCount)
{
    m_infoNameLabel->setText(mapName.isEmpty() ? "--" : mapName);
    m_mapPropIdVal->setText(mapId.isEmpty() ? "--" : mapId);
    m_infoSizeLabel->setText(QString("%1 × %2 px").arg((int)imgW).arg((int)imgH));
    double rx = (imgW > 0) ? (wR - wL) / imgW : 0.0;
    m_infoResLabel->setText(QString("%1 m/px").arg(rx, 0, 'f', 4));
    double w = wR - wL, h = wT - wB;
    m_mapPropAreaVal->setText((w > 0 && h > 0) ? QString("%1 × %2 m (%3 m²)").arg(w, 0, 'f', 2).arg(h, 0, 'f', 2).arg(w * h, 0, 'f', 2) : "--");

    m_mapCountPointVal->setText(QString::number(ptCount));
    m_mapCountHpVal->setText(QString::number(hpCount));
    m_mapCountChargeVal->setText(QString::number(chgCount));
    m_mapCountLineVal->setText(QString::number(lineCount));
    m_mapCountCurveVal->setText(QString::number(curveCount));
    m_mapCountAreaVal->setText(QString::number(areaCount));
}
