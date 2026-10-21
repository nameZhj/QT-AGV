/**
 * @file    MapFeatureFactory.h
 * @layer   GUI
 * @brief   Factory functions constructing standardized GeoJSON feature objects for map editor.
 * @deps    QJsonObject, QPointF, QList, QString, MapEditDataModel
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QJsonObject>
#include <QPointF>
#include <QList>
#include <QString>

class MapEditDataModel;

namespace MapFeatureFactory {

void initDefaultMotionProps(QJsonObject &obj);
void initDeviceEndpointProps(QJsonObject &obj, double epcX, double epcY, double epcTheta);

QString addPoint(MapEditDataModel *model, const QPointF &worldPos, double yaw = 0.0, double theta = 0.0);
QString addCharge(MapEditDataModel *model, const QPointF &worldPos, double epcX, double epcY, double epcTheta);
QString addHighPrecisionPoint(MapEditDataModel *model, const QPointF &worldPos, double epcX, double epcY, double epcTheta);
QString addLine(MapEditDataModel *model, const QPointF &wa, const QPointF &wb, const QString &startId, const QString &endId);
QString addBezierCurve(MapEditDataModel *model, const QList<QPointF> &worldPts, const QString &startId, const QString &endId);
QString addHighLevelArea(MapEditDataModel *model, double minX, double maxX, double minY, double maxY);

} // namespace MapFeatureFactory
