/**
 * @file    MapStationPropsPanel.cpp
 * @layer   GUI
 * @brief   Implementation of station/charging/high-precision property editing panel.
 * @deps    MapStationPropsPanel.h, MapPropRowHelper.h, MapEditDataModel.h, MapDataSerializer.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapStationPropsPanel.h"
#include "MapPropRowHelper.h"
#include "MapEditTypes.h"
#include "MapDataSerializer.h"
#include "MapEditDataModel.h"

#include <QComboBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>

using namespace MapPropRowHelper;
using namespace MapEdit;

MapStationPropsPanel::MapStationPropsPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void MapStationPropsPanel::setupUI()
{
    setStyleSheet("background:transparent; border:none;");
    auto *sl = new QVBoxLayout(this);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(4);

    // 基础信息
    m_stationTypeCombo = new QComboBox;
    m_stationTypeCombo->addItem("普通站点 (LM)", "point");
    m_stationTypeCombo->addItem("充电桩 (CP)", "charge");
    m_stationTypeCombo->addItem("高精定位点 (TL)", "highPrecision");
    sl->addWidget(mkCheckableRow(m_chkStationType, "站点类型:", m_stationTypeCombo));

    m_stationIdEdit = new QLineEdit;
    m_stationIdEdit->setReadOnly(true);
    m_stationIdEdit->setStyleSheet("background:#12151e; color:#627284; border:1px solid #232b3b; border-radius:3px; padding:2px 5px; font-size:11px;");
    sl->addWidget(mkFormRow("站点编号:", m_stationIdEdit));

    m_stationLabelEdit = new QLineEdit;
    sl->addWidget(mkCheckableRow(m_chkStationLabel, "站点名称:", m_stationLabelEdit));

    // 空间坐标
    m_stationXSpin = new QDoubleSpinBox;
    m_stationXSpin->setRange(-99999.0, 99999.0);
    m_stationXSpin->setDecimals(3);
    sl->addWidget(mkCheckableRow(m_chkStationX, "X 坐标:", m_stationXSpin, "m"));

    m_stationYSpin = new QDoubleSpinBox;
    m_stationYSpin->setRange(-99999.0, 99999.0);
    m_stationYSpin->setDecimals(3);
    sl->addWidget(mkCheckableRow(m_chkStationY, "Y 坐标:", m_stationYSpin, "m"));

    m_stationYawSpin = new QDoubleSpinBox;
    m_stationYawSpin->setRange(-360.0, 360.0);
    m_stationYawSpin->setDecimals(1);
    sl->addWidget(mkCheckableRow(m_chkStationYaw, "朝向角度:", m_stationYawSpin, "°"));

    // 站点特性复选框
    m_stationDockableChk = new QCheckBox("启用");
    sl->addWidget(mkCheckableRow(m_chkStationDockable, "允许停靠:", m_stationDockableChk));

    m_stationRotateChk = new QCheckBox("启用");
    sl->addWidget(mkCheckableRow(m_chkStationRotate, "允许转身:", m_stationRotateChk));

    m_stationPgvChk = new QCheckBox("启用");
    sl->addWidget(mkCheckableRow(m_chkStationPgv, "使用下视PGV:", m_stationPgvChk));

    // EPC 相对定位数据
    m_stationEpcGroup = new QWidget;
    m_stationEpcGroup->setStyleSheet("background:#141a26; border:1px solid #222b3c; border-radius:4px;");
    auto *el = new QVBoxLayout(m_stationEpcGroup);
    el->setContentsMargins(6, 5, 6, 5);
    el->setSpacing(3);
    auto *epcTitle = new QLabel("设备相对校准 (EPC)");
    epcTitle->setStyleSheet("color:#627e9e; font-size:10px; font-weight:bold; border:none;");
    el->addWidget(epcTitle);
    m_stationEpcXLabel = new QLabel("--");
    m_stationEpcXLabel->setStyleSheet("color:#79a2c9; font-size:11px; border:none;");
    m_stationEpcYLabel = new QLabel("--");
    m_stationEpcYLabel->setStyleSheet("color:#79a2c9; font-size:11px; border:none;");
    el->addWidget(mkFormRow("epc X:", m_stationEpcXLabel));
    el->addWidget(mkFormRow("epc Y:", m_stationEpcYLabel));
    sl->addWidget(m_stationEpcGroup);
    m_stationEpcGroup->setVisible(false);
}

void MapStationPropsPanel::populate(const QJsonObject &obj, const QString &type, const QString &id)
{
    int idx = m_stationTypeCombo->findData(type);
    if (idx >= 0) m_stationTypeCombo->setCurrentIndex(idx);
    m_stationIdEdit->setText(id);
    m_stationLabelEdit->setText(obj.value("name").toString());
    m_stationXSpin->setValue(obj.value("x").toDouble());
    m_stationYSpin->setValue(obj.value("y").toDouble());

    double yawVal = 0.0;
    if (obj.contains("yaw")) yawVal = obj.value("yaw").toDouble();
    else if (obj.contains("robotYaw")) yawVal = obj.value("robotYaw").toDouble();
    else if (obj.contains("theta")) yawVal = obj.value("theta").toDouble() * kDegToRad;
    m_stationYawSpin->setValue(yawVal * kRadToDeg);

    m_stationDockableChk->setChecked(obj.value("rdsDockable").toBool(true));
    m_stationRotateChk->setChecked(obj.value("allowRotate").toBool(true));
    m_stationPgvChk->setChecked(obj.value("useDownPGV").toBool(false));

    bool isDevicePoint = (type == "charge" || type == "highPrecision");
    m_stationXSpin->setReadOnly(false);
    m_stationYSpin->setReadOnly(false);
    m_stationYawSpin->setReadOnly(false);

    QString rwStyle = "background:#131722; color:#cfddea; border:1px solid #2b384e; border-radius:4px; padding:2px 6px; font-size:12px;";
    m_stationXSpin->setStyleSheet(rwStyle);
    m_stationYSpin->setStyleSheet(rwStyle);
    m_stationYawSpin->setStyleSheet(rwStyle);

    if (isDevicePoint) {
        double epcX = obj.contains("epc_x") ? obj.value("epc_x").toDouble()
                                             : obj.value("epcX").toDouble();
        double epcY = obj.contains("epc_y") ? obj.value("epc_y").toDouble()
                                             : obj.value("epcY").toDouble();
        m_stationEpcXLabel->setText(QString::number(epcX, 'f', 4) + " m");
        m_stationEpcYLabel->setText(QString::number(epcY, 'f', 4) + " m");
        m_stationEpcGroup->setVisible(true);
    } else {
        m_stationEpcGroup->setVisible(false);
    }

    restoreCheckedProps(obj.value("checked_props").toArray(), {
        {m_chkStationType, "stationType"},
        {m_chkStationLabel, "name"},
        {m_chkStationX, "x"},
        {m_chkStationY, "y"},
        {m_chkStationYaw, "yaw"},
        {m_chkStationDockable, "rdsDockable"},
        {m_chkStationRotate, "allowRotate"},
        {m_chkStationPgv, "useDownPGV"}
    });
}

void MapStationPropsPanel::applyEdit(MapEditDataModel *model, QString &selType, QString &selId)
{
    if (!model) return;

    QString newName  = m_stationLabelEdit->text().trimmed();
    QString newKind  = m_stationTypeCombo->currentData().toString();
    double newX      = m_stationXSpin->value();
    double newY      = m_stationYSpin->value();
    double newYawDeg = m_stationYawSpin->value();
    double newYawRad = newYawDeg * kDegToRad;
    bool dockable    = m_stationDockableChk->isChecked();
    bool rotate      = m_stationRotateChk->isChecked();
    bool pgv         = m_stationPgvChk->isChecked();

    const QList<QPair<QCheckBox*, QString>> stationChkMapping = {
        {m_chkStationType, "stationType"},
        {m_chkStationLabel, "name"},
        {m_chkStationX, "x"},
        {m_chkStationY, "y"},
        {m_chkStationYaw, "yaw"},
        {m_chkStationDockable, "rdsDockable"},
        {m_chkStationRotate, "allowRotate"},
        {m_chkStationPgv, "useDownPGV"}
    };

    bool typeChecked = m_chkStationType && m_chkStationType->isChecked();
    if (typeChecked && newKind != selType) {
        QJsonObject pointObj = model->findFeature(selType, selId);
        model->removeFeature(selType, selId);

        if (!pointObj.isEmpty()) {
            QString newId, defaultName;
            if (newKind == "point") {
                int nn = MapDataSerializer::nextIdNum(model->points(), "p_");
                newId = QString("p_%1").arg(nn);  defaultName = QString("LM%1").arg(nn);
            } else if (newKind == "charge") {
                int nn = MapDataSerializer::nextIdNum(model->charges(), "c_");
                newId = QString("c_%1").arg(nn);  defaultName = QString("CP%1").arg(nn);
            } else {
                int nn = MapDataSerializer::nextIdNum(model->highPrecisionPoints(), "hp_");
                newId = QString("hp_%1").arg(nn); defaultName = QString("TL%1").arg(nn);
            }

            pointObj["id"] = newId;
            if (m_chkStationLabel && m_chkStationLabel->isChecked()) pointObj["name"] = newName.isEmpty() ? defaultName : newName;
            if (m_chkStationX && m_chkStationX->isChecked()) pointObj["x"] = newX;
            if (m_chkStationY && m_chkStationY->isChecked()) pointObj["y"] = newY;
            if (m_chkStationYaw && m_chkStationYaw->isChecked()) {
                pointObj["yaw"] = newYawRad;
                pointObj["theta"] = newYawDeg;
                pointObj["robotYaw"] = 0.0;
            }
            if (m_chkStationDockable && m_chkStationDockable->isChecked()) pointObj["rdsDockable"] = dockable;
            if (m_chkStationRotate && m_chkStationRotate->isChecked()) pointObj["allowRotate"] = rotate;
            if (m_chkStationPgv && m_chkStationPgv->isChecked()) pointObj["useDownPGV"] = pgv;
            pointObj["checked_props"] = collectCheckedProps(stationChkMapping);

            model->addFeature(newKind, pointObj);
            selType = newKind;
            selId = newId;
        }
    } else {
        QJsonObject o = model->findFeature(selType, selId);
        if (!o.isEmpty()) {
            if (m_chkStationLabel && m_chkStationLabel->isChecked()) o["name"] = newName;
            bool xChecked = m_chkStationX && m_chkStationX->isChecked();
            bool yChecked = m_chkStationY && m_chkStationY->isChecked();
            if (xChecked) o["x"] = newX;
            if (yChecked) o["y"] = newY;
            if (m_chkStationYaw && m_chkStationYaw->isChecked()) {
                o["yaw"] = newYawRad;
                o["theta"] = newYawDeg;
                o["robotYaw"] = 0.0;
            }
            if (m_chkStationDockable && m_chkStationDockable->isChecked()) o["rdsDockable"] = dockable;
            if (m_chkStationRotate && m_chkStationRotate->isChecked()) o["allowRotate"] = rotate;
            if (m_chkStationPgv && m_chkStationPgv->isChecked()) o["useDownPGV"] = pgv;
            o["checked_props"] = collectCheckedProps(stationChkMapping);

            if (xChecked || yChecked) {
                model->updateConnectedRoutesPos(selId, newX, newY, xChecked, yChecked);
            }
            model->updateFeature(selType, selId, o);
        }
    }
}

void MapStationPropsPanel::clear()
{
    m_stationIdEdit->clear();
    m_stationLabelEdit->clear();
    m_stationXSpin->setValue(0.0);
    m_stationYSpin->setValue(0.0);
    m_stationYawSpin->setValue(0.0);
    m_stationDockableChk->setChecked(true);
    m_stationRotateChk->setChecked(true);
    m_stationPgvChk->setChecked(false);
    m_stationEpcGroup->setVisible(false);
}
