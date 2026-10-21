/**
 * @file    MapViewStationPropsPanel.cpp
 * @layer   GUI
 * @brief   Station inspection card layout and EPC pose binding implementation.
 * @deps    MapViewStationPropsPanel.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewStationPropsPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QJsonArray>
#include <QSet>
#include <cmath>

QWidget* MapViewStationPropsPanel::mkCheckRow(QCheckBox *&chk, const QString &text, QLabel *valLbl)
{
    auto *w = new QWidget;
    w->setStyleSheet("background:transparent; border:none;");
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 2, 2, 2); h->setSpacing(6);

    auto *leftCont = new QWidget;
    leftCont->setFixedWidth(110);
    leftCont->setStyleSheet("background:transparent; border:none;");
    auto *lh = new QHBoxLayout(leftCont);
    lh->setContentsMargins(0, 0, 0, 0); lh->setSpacing(4);

    chk = new QCheckBox;
    chk->setEnabled(false);
    chk->setFixedSize(16, 16);
    chk->setStyleSheet(
        "QCheckBox { background: transparent; }"
        "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #3d4a5d; border-radius: 2px; background: #171c26; }"
        "QCheckBox::indicator:disabled { border: 1px solid #3d4a5d; background: #171c26; }"
        "QCheckBox::indicator:checked:disabled { background: #1e6fd4; border-color: #1e6fd4; image: url(:/check.svg); }");
    lh->addWidget(chk, 0, Qt::AlignVCenter);

    auto *lbl = new QLabel(text);
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lbl->setStyleSheet("color: #8fa6bf; font-size: 11px; background: transparent; border: none;");
    lh->addWidget(lbl, 1, Qt::AlignVCenter);

    h->addWidget(leftCont, 0, Qt::AlignLeft | Qt::AlignVCenter);
    valLbl->setStyleSheet("color: #c8d0d8; font-size: 11px; background: transparent; border: none;");
    h->addWidget(valLbl, 1, Qt::AlignVCenter);
    return w;
}

MapViewStationPropsPanel::MapViewStationPropsPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void MapViewStationPropsPanel::setupUI()
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0); lay->setSpacing(0);

    m_stationGroup = new QGroupBox("站点属性", this);
    m_stationGroup->setStyleSheet(
        "QGroupBox{border:1px solid #2d3340; border-radius:4px; margin-top:14px; color:#8898a8; font-size:12px; font-weight:600;}"
        "QGroupBox::title{subcontrol-origin:margin; left:8px; top:-2px; padding:0 4px;}");
    auto *stMainLay = new QVBoxLayout(m_stationGroup);
    stMainLay->setContentsMargins(8, 12, 8, 8); stMainLay->setSpacing(6);

    auto *bRow = new QHBoxLayout;
    bRow->addStretch();
    m_stationBadge = new QLabel(m_stationGroup);
    m_stationBadge->setStyleSheet("border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;");
    m_stationBadge->setVisible(false);
    bRow->addWidget(m_stationBadge);
    stMainLay->addLayout(bRow);

    m_stationEmptyHint = new QLabel("点击地图上的点位\n查看站点信息", m_stationGroup);
    m_stationEmptyHint->setAlignment(Qt::AlignCenter);
    m_stationEmptyHint->setStyleSheet("color:#556677; font-size:12px; padding:14px 0;");
    stMainLay->addWidget(m_stationEmptyHint);

    m_stationContentWidget = new QWidget(m_stationGroup);
    m_stationContentWidget->setStyleSheet("background:transparent; border:none;");
    auto *stLay = new QVBoxLayout(m_stationContentWidget);
    stLay->setContentsMargins(0, 0, 0, 0); stLay->setSpacing(4);

    auto mkLbl = []() { auto *l = new QLabel("--"); l->setStyleSheet("color:#c8d0d8; font-size:12px;"); return l; };
    m_stationTypeVal   = mkLbl(); m_stationIdVal     = mkLbl();
    m_stationNameVal   = mkLbl(); m_stationPosVal    = mkLbl();
    m_stationYawVal    = mkLbl(); m_stationDockVal   = mkLbl();
    m_stationRotateVal = mkLbl(); m_stationPgvVal    = mkLbl();

    stLay->addWidget(mkCheckRow(m_chkStationType,   "类型:", m_stationTypeVal));
    stLay->addWidget(mkCheckRow(m_chkStationId,     "编号:", m_stationIdVal));
    stLay->addWidget(mkCheckRow(m_chkStationName,   "名称:", m_stationNameVal));
    stLay->addWidget(mkCheckRow(m_chkStationPos,    "坐标:", m_stationPosVal));
    stLay->addWidget(mkCheckRow(m_chkStationYaw,    "朝向:", m_stationYawVal));
    stLay->addWidget(mkCheckRow(m_chkStationDock,   "停靠:", m_stationDockVal));
    stLay->addWidget(mkCheckRow(m_chkStationRotate, "转身:", m_stationRotateVal));
    stLay->addWidget(mkCheckRow(m_chkStationPgv,    "PGV:",  m_stationPgvVal));

    m_stationEpcGroup = new QWidget(m_stationContentWidget);
    m_stationEpcGroup->setStyleSheet("background:#141a26; border:1px solid #222b3c; border-radius:4px; margin-top:4px;");
    auto *epcL = new QVBoxLayout(m_stationEpcGroup);
    epcL->setContentsMargins(6, 6, 6, 6); epcL->setSpacing(3);
    auto *epcT = new QLabel("设备相对校准 (EPC)");
    epcT->setStyleSheet("color:#627e9e; font-size:10px; font-weight:bold; border:none;");
    epcL->addWidget(epcT);
    auto *epcF = new QFormLayout;
    epcF->setContentsMargins(0, 0, 0, 0); epcF->setSpacing(3); epcF->setLabelAlignment(Qt::AlignRight);
    m_stationEpcPosVal = mkLbl(); m_stationEpcYawVal = mkLbl();
    epcF->addRow("相对坐标:", m_stationEpcPosVal);
    epcF->addRow("相对偏角:", m_stationEpcYawVal);
    epcL->addLayout(epcF);
    stLay->addWidget(m_stationEpcGroup);
    m_stationEpcGroup->setVisible(false);

    stMainLay->addWidget(m_stationContentWidget);
    m_stationContentWidget->setVisible(false);
    lay->addWidget(m_stationGroup);
}

void MapViewStationPropsPanel::showStationDetails(const QJsonObject &pt, const QString &type)
{
    QString id = pt.value("id").toString();
    QString name = pt.value("name").toString(id);
    QString typeText = "普通站点 (LM)", badgeText = "LM";
    QString badgeStyle = "background:#1e3a5f; color:#60a5fa; border:1px solid #2563eb; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;";
    if (type == "charge" || type == "chargeEpc") {
        typeText = "充电桩 (CP)"; badgeText = "CP";
        badgeStyle = "background:#4d3800; color:#fbbf24; border:1px solid #d97706; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;";
    } else if (type == "highPrecision" || type == "highPrecisionEpc") {
        typeText = "高精定位点 (TL)"; badgeText = "TL";
        badgeStyle = "background:#4d1414; color:#f87171; border:1px solid #dc2626; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;";
    }
    m_stationBadge->setText(badgeText);
    m_stationBadge->setStyleSheet(badgeStyle);
    m_stationBadge->setVisible(true);

    QSet<QString> chkSet;
    for (const auto &v : pt.value("checked_props").toArray()) chkSet.insert(v.toString());
    m_chkStationType->setChecked(chkSet.contains("stationType")); m_stationTypeVal->setText(typeText);
    m_chkStationId->setChecked(chkSet.contains("id"));           m_stationIdVal->setText(id);
    m_chkStationName->setChecked(chkSet.contains("name"));       m_stationNameVal->setText(name);
    m_chkStationPos->setChecked(chkSet.contains("x") || chkSet.contains("y"));
    m_stationPosVal->setText(QString("(%1, %2) m").arg(pt.value("x").toDouble(), 0, 'f', 3).arg(pt.value("y").toDouble(), 0, 'f', 3));

    double yawVal = pt.contains("yaw") ? pt.value("yaw").toVariant().toDouble()
                  : (pt.contains("theta") ? pt.value("theta").toVariant().toDouble() * M_PI / 180.0
                  : pt.value("robotYaw").toVariant().toDouble());
    m_chkStationYaw->setChecked(chkSet.contains("yaw"));
    m_stationYawVal->setText(QString("%1°").arg(yawVal * 180.0 / M_PI, 0, 'f', 1));
    m_chkStationDock->setChecked(chkSet.contains("rdsDockable"));
    m_stationDockVal->setText(pt.value("rdsDockable").toBool(true) ? "允许停靠" : "禁止停靠");
    m_chkStationRotate->setChecked(chkSet.contains("allowRotate"));
    m_stationRotateVal->setText(pt.value("allowRotate").toBool(true) ? "允许转身" : "禁止转身");
    m_chkStationPgv->setChecked(chkSet.contains("useDownPGV"));
    m_stationPgvVal->setText(pt.value("useDownPGV").toBool(false) ? "使用PGV" : "不使用PGV");

    bool isDev = (type == "charge" || type == "highPrecision" || type == "chargeEpc" || type == "highPrecisionEpc");
    if (isDev && (pt.contains("epc_x") || pt.contains("epcX") || pt.contains("epc_y") || pt.contains("epcY"))) {
        double ex = pt.contains("epc_x") ? pt.value("epc_x").toDouble() : pt.value("epcX").toDouble();
        double ey = pt.contains("epc_y") ? pt.value("epc_y").toDouble() : pt.value("epcY").toDouble();
        double et = pt.contains("epc_theta") ? pt.value("epc_theta").toDouble() : pt.value("epcTheta").toDouble();
        m_stationEpcPosVal->setText(QString("(%1, %2) m").arg(ex, 0, 'f', 3).arg(ey, 0, 'f', 3));
        m_stationEpcYawVal->setText(QString("%1°").arg(et * 180.0 / M_PI, 0, 'f', 1));
        m_stationEpcGroup->setVisible(true);
    } else {
        m_stationEpcGroup->setVisible(false);
    }
    m_stationEmptyHint->setVisible(false);
    m_stationContentWidget->setVisible(true);
}

void MapViewStationPropsPanel::clearStationDetails()
{
    m_stationBadge->setText(""); m_stationBadge->setVisible(false);
    m_chkStationType->setChecked(false); m_chkStationId->setChecked(false);
    m_chkStationName->setChecked(false); m_chkStationPos->setChecked(false);
    m_chkStationYaw->setChecked(false);  m_chkStationDock->setChecked(false);
    m_chkStationRotate->setChecked(false); m_chkStationPgv->setChecked(false);
    m_stationContentWidget->setVisible(false); m_stationEmptyHint->setVisible(true);
}
