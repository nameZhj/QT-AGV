/**
 * @file    MapDataSerializer.h
 * @layer   GUI
 * @brief   GeoJSON and route data parser and serializer for topological map features.
 * @deps    QJsonObject, QJsonArray, MapEditDataModel
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>

class MapEditDataModel;

class MapDataSerializer
{
public:
    MapDataSerializer() = delete;

    // === [Section: GeoJSON Serializer] ===
    static QJsonObject buildRouteGeoJson(const QString &mapId, const MapEditDataModel *model);

    // === [Section: GeoJSON Parsers] ===
    static bool parseRouteGeoJson(const QJsonObject &root, MapEditDataModel *model);
    static QJsonArray parseReflectorJson(const QJsonObject &data);

    // === [Section: ID Sequence Generator] ===
    static int nextIdNum(const QJsonArray &arr, const QString &prefix);
};
