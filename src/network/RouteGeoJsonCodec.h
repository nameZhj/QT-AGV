/**
 * @file    RouteGeoJsonCodec.h
 * @layer   DataComm
 * @brief   GeoJSON feature parser and generator for RouteNetwork models.
 * @deps    RouteNetworkApi.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include "RouteNetworkApi.h"

class RouteGeoJsonCodec final
{
public:
    RouteGeoJsonCodec() = delete;
    ~RouteGeoJsonCodec() = delete;

    struct ObstacleTuple {
        double stopLength  = 0.0;
        double stopWidth   = 0.0;
        double decelLength = 0.0;
        double decelWidth  = 0.0;
    };

    static RouteNetwork fromGeoJson(const QJsonObject &root);
    static QJsonObject toGeoJson(const RouteNetwork &network);

    static void parsePointFeature(const QJsonObject &properties,
                                  const QJsonArray &coordinates,
                                  RouteNetwork &outNetwork);

    static void parseLineFeature(const QJsonObject &properties,
                                 const QJsonArray &coordinates,
                                 RouteNetwork &outNetwork);

    static void parseCurveFeature(const QJsonObject &properties,
                                  const QJsonArray &coordinates,
                                  RouteNetwork &outNetwork);

    static void collectStationObstacleOverrides(const RouteNetwork &network,
                                                const QSet<QString> &specialIds,
                                                QMap<QString, ObstacleTuple> &stationObstacles);

    static QJsonObject buildPointFeature(RoutePoint point,
                                         bool isSpecial,
                                         const QSet<QString> &specialIds,
                                         const QMap<QString, ObstacleTuple> &stationObstacles);

    static QJsonObject buildLineFeature(RouteSegment line);

    static QJsonObject buildCurveFeature(RouteSegment curve);
};
