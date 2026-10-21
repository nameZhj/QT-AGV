/**
 * @file    MapEditDataLoader.cpp
 * @layer   GUI
 * @brief   Raster map loading, route loading, and async save pipeline implementation.
 * @deps    MapEditDataLoader.h, ApiService.h, AppConfig.h, MapPgmUtil.h, MapDataSerializer.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapEditDataLoader.h"
#include "ApiService.h"
#include "AppConfig.h"
#include "MapPgmUtil.h"
#include "MapDataSerializer.h"
#include "MapEditDataModel.h"
#include "MapCurveHandleManager.h"
#include "RouteNetworkValidator.h"
#include "MotionConfigManager.h"
#include "Toast.h"
#include "LoadingDialog.h"
#include "MapCheckResultDialog.h"

#include <QCoreApplication>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonDocument>
#include <memory>

namespace MapEditDataLoader {

namespace {

bool loadLocalMap(const QString &mapId, MapRasterData &out)
{
    QString localDir = QCoreApplication::applicationDirPath() + "/maps/" + mapId;
    QDirIterator yIt(localDir, QStringList() << "*.yaml" << "*.yml", QDir::Files, QDirIterator::Subdirectories);
    QDirIterator pIt(localDir, QStringList() << "*.pgm", QDir::Files, QDirIterator::Subdirectories);
    QString yPath, pPath;
    while (yIt.hasNext()) {
        QString f = yIt.next();
        if (f.endsWith(".map.yaml", Qt::CaseInsensitive) || f.contains("carto_map", Qt::CaseInsensitive) || yPath.isEmpty())
            yPath = f;
    }
    while (pIt.hasNext()) {
        QString f = pIt.next();
        if (f.endsWith(".map.pgm", Qt::CaseInsensitive) || f.contains("carto_map", Qt::CaseInsensitive) || pPath.isEmpty())
            pPath = f;
    }
    if (yPath.isEmpty() || pPath.isEmpty() || QFileInfo(yPath).size() == 0 || QFileInfo(pPath).size() == 0) return false;

    QFile yf(yPath);
    if (!yf.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QString yamlText = QString::fromUtf8(yf.readAll());
    yf.close();

    double resolution = 0, originX = 0, originY = 0; bool negate = false;
    if (!MapPgmUtil::parseMapYaml(yamlText, resolution, originX, originY, negate) || resolution <= 0) return false;

    QFile pf(pPath);
    if (!pf.open(QIODevice::ReadOnly)) return false;
    QImage img = MapPgmUtil::pgmToImage(pf.readAll(), negate);
    pf.close();
    if (img.isNull()) return false;

    out.ok = true; out.image = img.convertToFormat(QImage::Format_RGB32);
    out.imgW = img.width(); out.imgH = img.height();
    out.worldL = originX; out.worldB = originY;
    out.worldR = out.worldL + img.width() * resolution; out.worldT = out.worldB + img.height() * resolution;

    QFile metaFile(localDir + "/meta.json");
    if (metaFile.open(QIODevice::ReadOnly)) {
        out.mapName = QJsonDocument::fromJson(metaFile.readAll()).object().value("mapName").toString();
    }
    if (out.mapName.isEmpty()) {
        QString base = QFileInfo(yPath).completeBaseName();
        if (base.endsWith(".map", Qt::CaseInsensitive)) base.chop(4);
        out.mapName = base;
    }
    return true;
}

} // namespace

void loadRasterMap(const QString &mapId, std::function<void(const MapRasterData &data)> onComplete)
{
    MapRasterData localData;
    if (loadLocalMap(mapId, localData)) { onComplete(localData); return; }

    ApiService::instance().getMap(mapId, [mapId, onComplete](int code, const QJsonObject &data) {
        if (code != 200) { onComplete(MapRasterData{}); return; }
        QJsonObject info = data.value("data").toObject(data);
        QString mapName = info.value("mapName").toString();

        auto tryDbl = [&](const QStringList &keys) {
            for (const QString &k : keys) {
                QJsonValue v = info.value(k);
                if (!v.isUndefined() && !v.isNull()) return v.toDouble(0.0);
            }
            return 0.0;
        };
        double apiPhysW = tryDbl({"mapWidth","map_width","width"}), apiPhysH = tryDbl({"mapHeight","map_height","height"});
        auto getOrigin = [&](const QStringList &keys) {
            for (const QString &k : keys) {
                QJsonValue v = info.value(k);
                if (!v.isUndefined() && !v.isNull()) return -v.toDouble();
            }
            return 0.0;
        };
        double apiOriginX = getOrigin({"offsetX","mapOffsetX","offset_x"}), apiOriginY = getOrigin({"offsetY","mapOffsetY","offset_y"});

        QString tilemapPath = info.value("tilemapPath").toString();
        QString thumbUrl = tilemapPath.isEmpty() ? QString() : QString("%1%2_%3/%4/thumbnail.png").arg(AppConfig::instance().mapHost(), mapName, mapId, tilemapPath);

        auto fallback = [thumbUrl, mapName, apiOriginX, apiOriginY, apiPhysW, apiPhysH, onComplete]() {
            if (thumbUrl.isEmpty()) { onComplete(MapRasterData{}); return; }
            ApiService::instance().downloadFile(thumbUrl, [mapName, apiOriginX, apiOriginY, apiPhysW, apiPhysH, onComplete](bool ok, const QByteArray &raw) {
                if (!ok) { onComplete(MapRasterData{}); return; }
                QImage img; img.loadFromData(raw);
                if (img.isNull()) { onComplete(MapRasterData{}); return; }
                MapRasterData res; res.ok = true; res.mapName = mapName; res.image = img.convertToFormat(QImage::Format_RGB32);
                res.imgW = img.width(); res.imgH = img.height(); res.worldL = apiOriginX; res.worldB = apiOriginY;
                res.worldR = apiOriginX + apiPhysW; res.worldT = apiOriginY + apiPhysH;
                onComplete(res);
            });
        };

        QString resBase = AppConfig::instance().agvMapResourceHost() + mapName + "_" + mapId + "/";
        ApiService::instance().downloadFile(resBase + "carto_map.yaml", [resBase, mapName, fallback, onComplete](bool yOk, const QByteArray &yRaw) {
            if (!yOk) { fallback(); return; }
            double resolution = 0, originX = 0, originY = 0; bool negate = false;
            if (!MapPgmUtil::parseMapYaml(QString::fromUtf8(yRaw), resolution, originX, originY, negate) || resolution <= 0) { fallback(); return; }
            ApiService::instance().downloadFile(resBase + "carto_map.pgm", [resolution, originX, originY, negate, mapName, fallback, onComplete](bool pOk, const QByteArray &pRaw) {
                if (!pOk) { fallback(); return; }
                QImage img = MapPgmUtil::pgmToImage(pRaw, negate);
                if (img.isNull()) { fallback(); return; }
                MapRasterData res; res.ok = true; res.mapName = mapName; res.image = img.convertToFormat(QImage::Format_RGB32);
                res.imgW = img.width(); res.imgH = img.height(); res.worldL = originX; res.worldB = originY;
                res.worldR = res.worldL + img.width() * resolution; res.worldT = res.worldB + img.height() * resolution;
                onComplete(res);
            });
        });
    });
}

void loadRouteData(MapEditDataModel *model, std::function<void(bool ok)> onDone)
{
    if (model) model->setHighLevelAreas(QJsonArray());
    ApiService::instance().loadEditedMap([model, onDone](int code, const QJsonObject &data) {
        if (code != 200) { if (onDone) onDone(false); return; }
        QString jsonStr = data.value("json").toString();
        if (jsonStr.isEmpty()) {
            if (model) model->setHighLevelAreas(QJsonArray());
            if (onDone) onDone(true);
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (!doc.isObject()) { if (onDone) onDone(false); return; }
        MapDataSerializer::parseRouteGeoJson(doc.object(), model);
        if (onDone) onDone(true);
    });
}

void loadReflectorData(const QString &mapId, MapEditDataModel *model, std::function<void(bool ok)> onDone)
{
    ApiService::instance().getReflectorColumnInfo(mapId, [model, onDone](int code, const QJsonObject &data) {
        if (code != 200) { if (onDone) onDone(false); return; }
        model->setReflectorPoles(MapDataSerializer::parseReflectorJson(data));
        if (onDone) onDone(true);
    });
}

void saveMap(const QString &mapId, const QJsonObject &geojson, const QByteArray &pgmData,
             std::function<void(bool routeOk, const QString &routeErr, bool pgmOk, const QString &pgmErr)> onComplete)
{
    struct State { bool routeDone = false, routeOk = false; QString routeErr; bool pgmDone = false, pgmOk = false; QString pgmErr; };
    auto s = std::make_shared<State>();
    if (pgmData.isEmpty()) { s->pgmDone = true; s->pgmOk = true; }

    auto checkDone = [s, onComplete]() {
        if (s->routeDone && s->pgmDone) onComplete(s->routeOk, s->routeErr, s->pgmOk, s->pgmErr);
    };

    ApiService::instance().saveEditedMap(geojson, [s, checkDone](int code, const QJsonObject &resp) {
        s->routeDone = true; s->routeOk = (code == 200);
        if (code != 200) {
            QString err = resp.value("msg").toString(resp.value("message").toString());
            s->routeErr = err.isEmpty() ? QString("HTTP %1").arg(code) : err;
        }
        checkDone();
    });

    if (!pgmData.isEmpty()) {
        ApiService::instance().uploadMapPgm(mapId, pgmData, [s, checkDone](int code, const QJsonObject &resp) {
            s->pgmDone = true; s->pgmOk = (code == 200);
            if (code != 200) {
                QString err = resp.value("msg").toString(resp.value("message").toString());
                s->pgmErr = err.isEmpty() ? QString("HTTP %1").arg(code) : err;
            }
            checkDone();
        });
    }
}

void executeSavePipeline(const QString &mapId, MapEditDataModel *model, MapCurveHandleManager *handleMgr,
                         const QImage &mapImage, const QImage &origImage, bool hasPaintUndo,
                         QWidget *parent, std::function<void(bool success)> onFinished)
{
    model->syncLineObstaclesToEndpoints();
    QList<CheckResultItem> results;
    bool passed = RouteNetworkValidator::validate(model->highLevelAreas(), model->lines(), model->curves(),
                                                 MotionConfigManager::instance(), results);
    if (!results.isEmpty()) {
        auto *dlg = new MapCheckResultDialog(parent);
        dlg->setResults(results); dlg->show();
        if (!passed) { Toast::error(parent, "路网校验未通过，请检查高级区域参数冲突"); if (onFinished) onFinished(false); return; }
    }
    if (handleMgr) handleMgr->flushCurveHandlesToJson();
    QJsonObject geo = MapDataSerializer::buildRouteGeoJson(mapId, model);
    QByteArray pgmData;
    if (!mapImage.isNull() && (mapImage != origImage || hasPaintUndo)) pgmData = MapPgmUtil::imageToPgm(mapImage, false);

    auto *loading = new LoadingDialog("正在保存地图与路网...", parent ? parent->window() : nullptr, Qt::ApplicationModal);
    loading->show();
    saveMap(mapId, geo, pgmData, [parent, loading, onFinished](bool rOk, const QString &rErr, bool pOk, const QString &pErr) {
        loading->close(); loading->deleteLater();
        if (rOk && pOk) {
            Toast::success(parent, "地图与路网保存成功");
            if (onFinished) onFinished(true);
        } else {
            Toast::error(parent, QString("保存失败: 路网(%1), 栅格(%2)").arg(rErr, pErr));
            if (onFinished) onFinished(false);
        }
    });
}

} // namespace MapEditDataLoader
