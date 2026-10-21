/**
 * @file    MapFeatureFactory.cpp
 * @layer   GUI
 * @brief   Implementation of map feature construction factory.
 * @deps    MapFeatureFactory.h, MapEditDataModel.h, MapDataSerializer.h, MotionConfigManager.h, MapEditTypes.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapFeatureFactory.h"
#include "MapEditDataModel.h"
#include "MapDataSerializer.h"
#include "MapEditTypes.h"
#include "core/MotionConfigManager.h"
#include <QDateTime>

namespace MapFeatureFactory {

void initDefaultMotionProps(QJsonObject &obj)
{
    const auto &mc = MotionConfigManager::instance();
    obj["maxspeed"] = mc.maxSpeed();
    obj["max_acceleration"] = mc.maxAcceleration();
    obj["maxacc"] = mc.maxAcceleration();
    obj["max_deceleration"] = mc.maxDeceleration();
    obj["maxdec"] = mc.maxDeceleration();
    obj["max_angular_speed"] = mc.maxAngularSpeedDeg();
    obj["maxrot"] = mc.maxAngularSpeedDeg();
    obj["max_angular_acceleration"] = mc.maxAngularAccelerationDeg();
    obj["maxrotacc"] = mc.maxAngularAccelerationDeg();
    obj["reachdist"] = mc.arrivalDistanceTolerance();
    obj["reachangle"] = mc.arrivalAngleToleranceDeg();
    obj["width"] = mc.routeWidth();
    obj["intrusion_stop_length"] = mc.defaultObsStopLength();
    obj["intrusion_stop_width"] = mc.defaultObsStopWidth();
    obj["intrusion_deceleration_length"] = mc.defaultObsDecelLength();
    obj["intrusion_deceleration_width"] = mc.defaultObsDecelWidth();
    obj["is_custom_obstacle"] = false;
}

void initDeviceEndpointProps(QJsonObject &obj, double epcX, double epcY, double epcTheta)
{
    obj["epc_x"] = epcX; obj["epc_y"] = epcY; obj["epc_theta"] = epcTheta;
    obj["epcX"] = epcX; obj["epcY"] = epcY; obj["epcTheta"] = epcTheta;
    obj["epc_Yaw"] = epcTheta; obj["epc_yaw"] = epcTheta;
    obj["yaw"] = 0.0; obj["theta"] = 0.0; obj["robotYaw"] = 0.0;
    obj["rdsDockable"] = true; obj["allowRotate"] = true; obj["useDownPGV"] = false;
    obj["intrusion_stop_length"] = 0.0; obj["intrusion_stop_width"] = 0.0;
    obj["intrusion_deceleration_length"] = 0.0; obj["intrusion_deceleration_width"] = 0.0;
    obj["is_custom_obstacle"] = false;
}

QString addPoint(MapEditDataModel *model, const QPointF &w, double yaw, double theta)
{
    if (!model) return {};
    int n = MapDataSerializer::nextIdNum(model->points(), "p_");
    QJsonObject pt;
    pt["id"] = QString("p_%1").arg(n); pt["name"] = QString("LM%1").arg(n); pt["typeName"] = "LandMark";
    pt["x"] = w.x(); pt["y"] = w.y(); pt["yaw"] = yaw; pt["theta"] = theta; pt["robotYaw"] = 0.0;
    pt["rdsDockable"] = true; pt["allowRotate"] = true; pt["useDownPGV"] = false;
    const auto &mc = MotionConfigManager::instance();
    pt["intrusion_stop_length"] = mc.defaultObsStopLength(); pt["intrusion_stop_width"] = mc.defaultObsStopWidth();
    pt["intrusion_deceleration_length"] = mc.defaultObsDecelLength(); pt["intrusion_deceleration_width"] = mc.defaultObsDecelWidth();
    pt["is_custom_obstacle"] = false;
    model->addFeature("point", pt);
    return pt["id"].toString();
}

QString addCharge(MapEditDataModel *model, const QPointF &w, double epcX, double epcY, double epcTheta)
{
    if (!model) return {};
    int n = MapDataSerializer::nextIdNum(model->charges(), "c_");
    QJsonObject ch;
    ch["id"] = QString("c_%1").arg(n); ch["name"] = QString("CP_%1").arg(n); ch["typeName"] = "ChargePoint";
    ch["x"] = w.x(); ch["y"] = w.y();
    initDeviceEndpointProps(ch, epcX, epcY, epcTheta);
    model->addFeature("charge", ch);
    return ch["id"].toString();
}

QString addHighPrecisionPoint(MapEditDataModel *model, const QPointF &w, double epcX, double epcY, double epcTheta)
{
    if (!model) return {};
    int n = MapDataSerializer::nextIdNum(model->highPrecisionPoints(), "hp_");
    QJsonObject hp;
    hp["id"] = QString("hp_%1").arg(n); hp["name"] = QString("AP%1").arg(n); hp["typeName"] = "ActionPoint";
    hp["x"] = w.x(); hp["y"] = w.y();
    initDeviceEndpointProps(hp, epcX, epcY, epcTheta);
    model->addFeature("highPrecision", hp);
    return hp["id"].toString();
}

QString addLine(MapEditDataModel *model, const QPointF &wa, const QPointF &wb, const QString &startId, const QString &endId)
{
    if (!model) return {};
    int n = MapDataSerializer::nextIdNum(model->lines(), "l_");
    QJsonObject ln;
    ln["id"] = QString("l_%1").arg(n); ln["name"] = QString("线%1").arg(n);
    ln["x1"] = wa.x(); ln["y1"] = wa.y(); ln["x2"] = wb.x(); ln["y2"] = wb.y();
    ln["direct"] = MapEdit::kDirectionBidirectional; ln["startid"] = startId; ln["endid"] = endId;
    initDefaultMotionProps(ln);
    model->addFeature("line", ln);
    return ln["id"].toString();
}

QString addBezierCurve(MapEditDataModel *model, const QList<QPointF> &worldPts, const QString &startId, const QString &endId)
{
    if (!model || worldPts.size() < 2) return {};
    int n = MapDataSerializer::nextIdNum(model->curves(), "b_");
    QJsonObject cv;
    cv["id"] = QString("b_%1").arg(n); cv["name"] = QString("曲%1").arg(n);
    cv["direct"] = MapEdit::kDirectionBidirectional;
    cv["x1"] = worldPts.first().x(); cv["y1"] = worldPts.first().y();
    cv["x2"] = worldPts.last().x();  cv["y2"] = worldPts.last().y();
    cv["startid"] = startId; cv["endid"] = endId;
    initDefaultMotionProps(cv);

    QJsonArray ctrlArr;
    for (const QPointF &wp : worldPts) {
        QJsonObject pt; pt["x"] = wp.x(); pt["y"] = wp.y();
        ctrlArr.append(pt);
    }
    cv["controlPoints"] = ctrlArr;
    model->addFeature("curve", cv);
    return cv["id"].toString();
}

QString addHighLevelArea(MapEditDataModel *model, double minX, double maxX, double minY, double maxY)
{
    if (!model) return {};
    QJsonObject newArea;
    QString newId = QString("highLevelArea_%1").arg(QDateTime::currentMSecsSinceEpoch());
    newArea["id"] = newId;
    newArea["name"] = QString("AdvancedArea %1").arg(model->highLevelAreas().size() + 1);

    QJsonArray coords, pTL, pBR;
    pTL.append(minX); pTL.append(maxY);
    pBR.append(maxX); pBR.append(minY);
    coords.append(pTL); coords.append(pBR);
    newArea["coordinates"] = coords;
    initDefaultMotionProps(newArea);
    newArea["checked_props"] = QJsonArray();

    model->addFeature("highLevelArea", newArea);
    return newId;
}

} // namespace MapFeatureFactory
