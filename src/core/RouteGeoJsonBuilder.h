/**
 * @file    RouteGeoJsonBuilder.h
 * @layer   CoreScheduling
 * @brief   Builds standard GeoJSON FeatureCollection from route network primitives.
 * @deps    QJsonObject, QJsonArray, QString
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>

namespace RouteGeoJsonBuilder {

QJsonObject buildRouteGeoJson(const QString &mapId,
                              const QJsonArray &points,
                              const QJsonArray &lines,
                              const QJsonArray &curves,
                              const QJsonArray &charges,
                              const QJsonArray &highPrecisionPoints,
                              QJsonArray &highLevelAreas);

} // namespace RouteGeoJsonBuilder
