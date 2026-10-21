/**
 * @file    MapViewRoutePropsPanel.cpp
 * @layer   GUI
 * @brief   Route and advanced area attribute forms and motion constraint displays.
 * @deps    MapViewRoutePropsPanel.h, MotionConfigManager.h, MapPgmUtil.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewRoutePropsPanel.h"
#include "core/MotionConfigManager.h"
#include "utils/MapPgmUtil.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QSet>
#include <cmath>

QWidget* MapViewRoutePropsPanel::mkCheckRow(QCheckBox *&chk, const QString &text, QLabel *valLbl)
{
    auto *w = new QWidget;
    w->setStyleSheet("background:transparent; border:none;");
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 2, 2, 2); h->setSpacing(6);

    auto *leftCont = new QWidget;
    leftCont->setFixedWidth(110); leftCont->setStyleSheet("background:transparent; border:none;");
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

MapViewRoutePropsPanel::MapViewRoutePropsPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void MapViewRoutePropsPanel::setupUI()
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0); lay->setSpacing(0);

    auto mkGrp = [](const QString &t) {
        auto *g = new QGroupBox(t);
        g->setStyleSheet("QGroupBox{border:1px solid #2d3340; border-radius:4px; margin-top:14px; color:#8898a8; font-size:12px; font-weight:600;} QGroupBox::title{subcontrol-origin:margin; left:8px; top:-2px; padding:0 4px;}");
        return g;
    };
    auto mkLbl = []() { auto *l = new QLabel("--"); l->setStyleSheet("color:#c8d0d8; font-size:12px;"); return l; };

    // 1. Route Group
    m_routeGroup = mkGrp("路线属性");
    auto *rtMainLay = new QVBoxLayout(m_routeGroup);
    rtMainLay->setContentsMargins(8, 12, 8, 8); rtMainLay->setSpacing(6);
    auto *rtBRow = new QHBoxLayout;
    rtBRow->addStretch();
    m_routeBadge = new QLabel(m_routeGroup);
    m_routeBadge->setStyleSheet("border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;");
    m_routeBadge->setVisible(false);
    rtBRow->addWidget(m_routeBadge);
    rtMainLay->addLayout(rtBRow);

    m_routeEmptyHint = new QLabel("点击地图上的线条\n查看路线信息", m_routeGroup);
    m_routeEmptyHint->setAlignment(Qt::AlignCenter);
    m_routeEmptyHint->setStyleSheet("color:#556677; font-size:12px; padding:14px 0;");
    rtMainLay->addWidget(m_routeEmptyHint);

    m_routeContentWidget = new QWidget(m_routeGroup);
    m_routeContentWidget->setStyleSheet("background:transparent; border:none;");
    auto *rtLay = new QVBoxLayout(m_routeContentWidget);
    rtLay->setContentsMargins(0, 0, 0, 0); rtLay->setSpacing(4);
    m_routeTypeVal       = mkLbl(); m_routeIdVal         = mkLbl();
    m_routeNameVal       = mkLbl(); m_routeStartVal      = mkLbl();
    m_routeEndVal        = mkLbl(); m_routeDirectVal     = mkLbl();
    m_routeMaxSpeedVal   = mkLbl(); m_routeMaxAccVal     = mkLbl();
    m_routeMaxDecVal     = mkLbl(); m_routeMaxRotVal     = mkLbl();
    m_routeMaxRotAccVal  = mkLbl(); m_routeReachDistVal  = mkLbl();
    m_routeReachAngleVal = mkLbl(); m_routeWidthVal      = mkLbl();
    m_routeObsRangeVal   = mkLbl();

    rtLay->addWidget(mkCheckRow(m_chkRouteType,       "线路类型:",     m_routeTypeVal));
    rtLay->addWidget(mkCheckRow(m_chkRouteId,         "线路编号:",     m_routeIdVal));
    rtLay->addWidget(mkCheckRow(m_chkRouteName,       "线路名称:",     m_routeNameVal));
    rtLay->addWidget(mkCheckRow(m_chkRouteStart,      "起点:",         m_routeStartVal));
    rtLay->addWidget(mkCheckRow(m_chkRouteEnd,        "终点:",         m_routeEndVal));
    rtLay->addWidget(mkCheckRow(m_chkRouteDirect,     "运动方向:",     m_routeDirectVal));
    rtLay->addWidget(mkCheckRow(m_chkRouteMaxSpeed,   "最大速度:",     m_routeMaxSpeedVal));
    rtLay->addWidget(mkCheckRow(m_chkRouteMaxAcc,     "最大加速度:",   m_routeMaxAccVal));
    rtLay->addWidget(mkCheckRow(m_chkRouteMaxRot,     "最大角速度:",   m_routeMaxRotVal));
    rtLay->addWidget(mkCheckRow(m_chkRouteMaxRotAcc,  "最大角加减:",   m_routeMaxRotAccVal));
    rtLay->addWidget(mkCheckRow(m_chkRouteReachDist,  "到达距离精度:", m_routeReachDistVal));
    rtLay->addWidget(mkCheckRow(m_chkRouteReachAngle, "到达角度精度:", m_routeReachAngleVal));
    rtLay->addWidget(mkCheckRow(m_chkRouteWidth,      "路线宽度:",     m_routeWidthVal));
    rtLay->addWidget(mkCheckRow(m_chkRouteObsRange,   "避障范围:",     m_routeObsRangeVal));
    rtMainLay->addWidget(m_routeContentWidget);
    m_routeContentWidget->setVisible(false);
    lay->addWidget(m_routeGroup);

    // 2. Area Group
    m_areaGroup = mkGrp("高级区域属性");
    auto *arMainLay = new QVBoxLayout(m_areaGroup);
    arMainLay->setContentsMargins(8, 12, 8, 8); arMainLay->setSpacing(6);
    auto *arBRow = new QHBoxLayout;
    arBRow->addStretch();
    m_areaBadge = new QLabel("高级区域", m_areaGroup);
    m_areaBadge->setStyleSheet("background:#3b1d5c; color:#c084fc; border:1px solid #7c3aed; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;");
    m_areaBadge->setVisible(false);
    arBRow->addWidget(m_areaBadge);
    arMainLay->addLayout(arBRow);

    m_areaEmptyHint = new QLabel("点击地图上的高级区域\n查看区域属性", m_areaGroup);
    m_areaEmptyHint->setAlignment(Qt::AlignCenter);
    m_areaEmptyHint->setStyleSheet("color:#556677; font-size:12px; padding:14px 0;");
    arMainLay->addWidget(m_areaEmptyHint);

    m_areaContentWidget = new QWidget(m_areaGroup);
    m_areaContentWidget->setStyleSheet("background:transparent; border:none;");
    auto *arLay = new QVBoxLayout(m_areaContentWidget);
    arLay->setContentsMargins(0, 0, 0, 0); arLay->setSpacing(4);
    m_areaNameVal        = mkLbl(); m_areaBoundsVal      = mkLbl();
    m_areaMaxSpeedVal    = mkLbl(); m_areaMaxAccVal      = mkLbl();
    m_areaMaxRotVal      = mkLbl(); m_areaMaxRotAccVal   = mkLbl();
    m_areaReachDistVal   = mkLbl(); m_areaReachAngleVal  = mkLbl();
    m_areaWidthVal       = mkLbl(); m_areaObsRangeVal    = mkLbl();

    arLay->addWidget(mkCheckRow(m_chkAreaName,       "区域名称:",     m_areaNameVal));
    auto *coordRow = new QHBoxLayout;
    coordRow->setContentsMargins(0, 0, 0, 0); coordRow->setSpacing(4);
    auto *bLbl = new QLabel("范围坐标:"); bLbl->setFixedWidth(90); bLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    bLbl->setStyleSheet("color: #8fa6bf; font-size: 11px; background: transparent; border: none;");
    m_areaBoundsVal->setStyleSheet("color: #c8d0d8; font-size: 11px; background: transparent; border: none;");
    coordRow->addWidget(bLbl); coordRow->addWidget(m_areaBoundsVal, 1);
    auto *coordW = new QWidget; coordW->setLayout(coordRow); coordW->setStyleSheet("background:transparent; border:none;");
    arLay->addWidget(coordW);

    arLay->addWidget(mkCheckRow(m_chkAreaMaxSpeed,   "最大速度:",     m_areaMaxSpeedVal));
    arLay->addWidget(mkCheckRow(m_chkAreaMaxAcc,     "最大加速度:",   m_areaMaxAccVal));
    arLay->addWidget(mkCheckRow(m_chkAreaMaxRot,     "最大角速度:",   m_areaMaxRotVal));
    arLay->addWidget(mkCheckRow(m_chkAreaMaxRotAcc,  "最大角加减:",   m_areaMaxRotAccVal));
    arLay->addWidget(mkCheckRow(m_chkAreaReachDist,  "到达距离精度:", m_areaReachDistVal));
    arLay->addWidget(mkCheckRow(m_chkAreaReachAngle, "到达角度精度:", m_areaReachAngleVal));
    arLay->addWidget(mkCheckRow(m_chkAreaWidth,      "路线宽度:",     m_areaWidthVal));
    arLay->addWidget(mkCheckRow(m_chkAreaObsRange,   "避障范围:",     m_areaObsRangeVal));
    arMainLay->addWidget(m_areaContentWidget);
    m_areaContentWidget->setVisible(false);
    lay->addWidget(m_areaGroup);

    showRouteView();
}

void MapViewRoutePropsPanel::showRouteView() { m_routeGroup->setVisible(true); m_areaGroup->setVisible(false); }
void MapViewRoutePropsPanel::showAreaView() { m_routeGroup->setVisible(false); m_areaGroup->setVisible(true); }

void MapViewRoutePropsPanel::showRouteDetails(const QJsonObject &obj, const QString &type,
                                             const std::function<QString(const QString &)> &lookup)
{
    showRouteView();
    bool isLine = (type == "line");
    m_routeBadge->setText(isLine ? "直线" : "贝塞尔曲线");
    m_routeBadge->setStyleSheet("background:#17344f; color:#38bdf8; border:1px solid #0284c7; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;");
    m_routeBadge->setVisible(true);

    QSet<QString> chk;
    for (const auto &v : obj.value("checked_props").toArray()) chk.insert(v.toString());
    m_chkRouteType->setChecked(chk.contains("routeType")); m_routeTypeVal->setText(isLine ? "直线" : "贝塞尔曲线");
    m_chkRouteId->setChecked(chk.contains("id"));         m_routeIdVal->setText(obj.value("id").toString());
    m_chkRouteName->setChecked(chk.contains("name"));     m_routeNameVal->setText(obj.value("name").toString("--"));
    QString sId = obj.value("startid").toString(), eId = obj.value("endid").toString();
    m_chkRouteStart->setChecked(chk.contains("startid")); m_routeStartVal->setText(sId.isEmpty() ? "--" : lookup(sId));
    m_chkRouteEnd->setChecked(chk.contains("endid"));     m_routeEndVal->setText(eId.isEmpty() ? "--" : lookup(eId));
    m_chkRouteDirect->setChecked(chk.contains("direct"));
    int d = obj.value("direct").toInt(10);
    m_routeDirectVal->setText((d == 10) ? "双向 ↔" : ((d == 0) ? "单向前进 →" : "单向后退 ←"));

    const auto &mc = MotionConfigManager::instance();
    m_chkRouteMaxSpeed->setChecked(chk.contains("maxspeed"));
    m_routeMaxSpeedVal->setText(QString("%1 m/s").arg(mc.clampLinearSpeed(obj.value("maxspeed").toDouble(mc.maxSpeed())), 0, 'f', 2));
    m_chkRouteMaxAcc->setChecked(chk.contains("maxacc") || chk.contains("max_acceleration"));
    double acc = obj.contains("max_acceleration") ? obj.value("max_acceleration").toDouble() : obj.value("maxacc").toDouble(mc.maxAcceleration());
    m_routeMaxAccVal->setText(QString("%1 m/s²").arg(qMin(acc, mc.maxAcceleration()), 0, 'f', 2));
    m_chkRouteMaxRot->setChecked(chk.contains("maxrot") || chk.contains("max_angular_speed"));
    double rot = obj.contains("max_angular_speed") ? obj.value("max_angular_speed").toDouble() : obj.value("maxrot").toDouble(mc.maxAngularSpeedDeg());
    m_routeMaxRotVal->setText(QString("%1 °/s").arg(qMin(rot, mc.maxAngularSpeedDeg()), 0, 'f', 1));
    m_chkRouteMaxRotAcc->setChecked(chk.contains("maxrotacc") || chk.contains("max_angular_acceleration"));
    double rAcc = obj.contains("max_angular_acceleration") ? obj.value("max_angular_acceleration").toDouble() : obj.value("maxrotacc").toDouble(mc.maxAngularAccelerationDeg());
    m_routeMaxRotAccVal->setText(QString("%1 °/s²").arg(qMin(rAcc, mc.maxAngularAccelerationDeg()), 0, 'f', 1));
    m_chkRouteReachDist->setChecked(chk.contains("reachdist"));
    m_routeReachDistVal->setText(QString("%1 m").arg(obj.value("reachdist").toDouble(mc.arrivalDistanceTolerance()), 0, 'f', 2));
    m_chkRouteReachAngle->setChecked(chk.contains("reachangle"));
    m_routeReachAngleVal->setText(QString("%1 °").arg(obj.value("reachangle").toDouble(mc.arrivalAngleToleranceDeg()), 0, 'f', 1));
    m_chkRouteWidth->setChecked(chk.contains("width"));
    m_routeWidthVal->setText(QString("%1 m").arg(obj.value("width").toDouble(mc.routeWidth()), 0, 'f', 2));
    bool hasObs = chk.contains("intrusion_stop_length") || chk.contains("intrusion_stop_width");
    m_chkRouteObsRange->setChecked(hasObs);
    m_routeObsRangeVal->setText(QString("[%1,%2,%3,%4]")
        .arg(obj.value("intrusion_stop_length").toDouble(0), 0, 'f', 1)
        .arg(obj.value("intrusion_stop_width").toDouble(0), 0, 'f', 1)
        .arg(obj.value("intrusion_deceleration_length").toDouble(0), 0, 'f', 1)
        .arg(obj.value("intrusion_deceleration_width").toDouble(0), 0, 'f', 1));

    m_routeEmptyHint->setVisible(false); m_routeContentWidget->setVisible(true);
}

void MapViewRoutePropsPanel::clearRouteDetails()
{
    m_routeBadge->setText(""); m_routeBadge->setVisible(false);
    m_chkRouteType->setChecked(false); m_chkRouteId->setChecked(false);
    m_chkRouteName->setChecked(false); m_chkRouteStart->setChecked(false);
    m_chkRouteEnd->setChecked(false);  m_chkRouteDirect->setChecked(false);
    m_chkRouteMaxSpeed->setChecked(false); m_chkRouteMaxAcc->setChecked(false);
    m_chkRouteMaxRot->setChecked(false); m_chkRouteMaxRotAcc->setChecked(false);
    m_chkRouteReachDist->setChecked(false); m_chkRouteReachAngle->setChecked(false);
    m_chkRouteWidth->setChecked(false); m_chkRouteObsRange->setChecked(false);
    m_routeContentWidget->setVisible(false); m_routeEmptyHint->setVisible(true);
}

void MapViewRoutePropsPanel::showAreaDetails(const QJsonObject &obj)
{
    showAreaView();
    m_areaBadge->setVisible(true);
    QSet<QString> chk;
    for (const auto &v : obj.value("checked_props").toArray()) chk.insert(v.toString());
    m_chkAreaName->setChecked(chk.contains("name"));
    m_areaNameVal->setText(obj.value("name").toString("AdvancedArea"));
    double minX = 0, maxX = 0, minY = 0, maxY = 0;
    MapPgmUtil::extractAreaBounds(obj, minX, maxX, minY, maxY);
    m_areaBoundsVal->setText(QString("[%1, %2] [%3, %4]").arg(minX, 0, 'f', 2).arg(maxX, 0, 'f', 2).arg(minY, 0, 'f', 2).arg(maxY, 0, 'f', 2));

    const auto &mc = MotionConfigManager::instance();
    m_chkAreaMaxSpeed->setChecked(chk.contains("maxspeed") || chk.contains("max_speed"));
    double spd = obj.contains("max_speed") ? obj.value("max_speed").toDouble() : obj.value("maxspeed").toDouble(mc.maxSpeed());
    m_areaMaxSpeedVal->setText(QString("%1 m/s").arg(spd, 0, 'f', 2));
    m_chkAreaMaxAcc->setChecked(chk.contains("maxacc") || chk.contains("max_acceleration"));
    double acc = obj.contains("max_acceleration") ? obj.value("max_acceleration").toDouble() : obj.value("maxacc").toDouble(mc.maxAcceleration());
    m_areaMaxAccVal->setText(QString("%1 m/s²").arg(acc, 0, 'f', 2));
    m_chkAreaMaxRot->setChecked(chk.contains("maxrot") || chk.contains("max_angular_speed"));
    double rot = obj.contains("max_angular_speed") ? obj.value("max_angular_speed").toDouble() : obj.value("maxrot").toDouble(mc.maxAngularSpeedDeg());
    m_areaMaxRotVal->setText(QString("%1 °/s").arg(rot, 0, 'f', 1));
    m_chkAreaMaxRotAcc->setChecked(chk.contains("maxrotacc") || chk.contains("max_angular_acceleration"));
    double rAcc = obj.contains("max_angular_acceleration") ? obj.value("max_angular_acceleration").toDouble() : obj.value("maxrotacc").toDouble(mc.maxAngularAccelerationDeg());
    m_areaMaxRotAccVal->setText(QString("%1 °/s²").arg(rAcc, 0, 'f', 1));
    m_chkAreaReachDist->setChecked(chk.contains("reachdist") || chk.contains("arrival_distance_tolerance"));
    m_areaReachDistVal->setText(QString("%1 m").arg(obj.value("reachdist").toDouble(mc.arrivalDistanceTolerance()), 0, 'f', 2));
    m_chkAreaReachAngle->setChecked(chk.contains("reachangle") || chk.contains("arrival_angle_tolerance"));
    m_areaReachAngleVal->setText(QString("%1 °").arg(obj.value("reachangle").toDouble(mc.arrivalAngleToleranceDeg()), 0, 'f', 1));
    m_chkAreaWidth->setChecked(chk.contains("width") || chk.contains("route_width"));
    m_areaWidthVal->setText(QString("%1 m").arg(obj.value("width").toDouble(mc.routeWidth()), 0, 'f', 2));
    m_chkAreaObsRange->setChecked(chk.contains("intrusion_stop_length"));
    m_areaObsRangeVal->setText(QString("[%1,%2,%3,%4]")
        .arg(obj.value("intrusion_stop_length").toDouble(0), 0, 'f', 1)
        .arg(obj.value("intrusion_stop_width").toDouble(0), 0, 'f', 1)
        .arg(obj.value("intrusion_deceleration_length").toDouble(0), 0, 'f', 1)
        .arg(obj.value("intrusion_deceleration_width").toDouble(0), 0, 'f', 1));

    m_areaEmptyHint->setVisible(false); m_areaContentWidget->setVisible(true);
}

void MapViewRoutePropsPanel::clearAreaDetails()
{
    m_areaBadge->setVisible(false); m_chkAreaName->setChecked(false);
    m_chkAreaMaxSpeed->setChecked(false); m_chkAreaMaxAcc->setChecked(false);
    m_chkAreaMaxRot->setChecked(false); m_chkAreaMaxRotAcc->setChecked(false);
    m_chkAreaReachDist->setChecked(false); m_chkAreaReachAngle->setChecked(false);
    m_chkAreaWidth->setChecked(false); m_chkAreaObsRange->setChecked(false);
    m_areaContentWidget->setVisible(false); m_areaEmptyHint->setVisible(true);
}

void MapViewRoutePropsPanel::clearAll()
{
    clearRouteDetails();
    clearAreaDetails();
}
