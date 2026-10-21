/**
 * @file    MapEditDataLoader.h
 * @layer   GUI
 * @brief   Raster map, YAML/PGM download and save pipeline coordinator.
 * @deps    QString, QImage, QJsonObject, QByteArray, functional
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QImage>
#include <QJsonObject>
#include <QByteArray>
#include <functional>

class QWidget;
class MapEditDataModel;
class MapCurveHandleManager;

struct MapRasterData {
    bool    ok = false;
    QImage  image;
    QString mapName;
    double  worldL = 0.0;
    double  worldB = 0.0;
    double  worldR = 0.0;
    double  worldT = 0.0;
    int     imgW = 0;
    int     imgH = 0;
};

namespace MapEditDataLoader {

void loadRasterMap(const QString &mapId,
                   std::function<void(const MapRasterData &data)> onComplete);

void loadRouteData(MapEditDataModel *model, std::function<void(bool ok)> onDone);

void loadReflectorData(const QString &mapId, MapEditDataModel *model, std::function<void(bool ok)> onDone);

void saveMap(const QString &mapId,
             const QJsonObject &geojson,
             const QByteArray &pgmData,
             std::function<void(bool routeOk, const QString &routeErr,
                                bool pgmOk, const QString &pgmErr)> onComplete);

void executeSavePipeline(const QString &mapId, MapEditDataModel *model,
                         MapCurveHandleManager *handleMgr,
                         const QImage &mapImage, const QImage &origImage,
                         bool hasPaintUndo, QWidget *parent,
                         std::function<void(bool success)> onFinished);

} // namespace MapEditDataLoader
