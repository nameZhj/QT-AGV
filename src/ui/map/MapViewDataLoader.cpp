/**
 * @file    MapViewDataLoader.cpp
 * @layer   GUI
 * @brief   Data loading and network map sync implementation.
 * @deps    MapViewDataLoader.h, ApiService.h, MapPgmUtil.h, ZipUtil.h, Toast.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewDataLoader.h"
#include "app/AppConfig.h"
#include "network/ApiService.h"
#include "utils/MapPgmUtil.h"
#include "utils/ZipUtil.h"
#include "ui/common/Toast.h"
#include <QDir>
#include <QDirIterator>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <algorithm>

MapViewDataLoader::MapViewDataLoader(QObject *parent) : QObject(parent) {}

void MapViewDataLoader::fetchMapNameList(std::function<void()> onDone) {
    if (m_isFetchingMapList) { if (onDone) onDone(); return; }
    m_isFetchingMapList = true;
    ApiService::instance().listMapAll(QJsonObject{{"pageNum", 1}, {"pageSize", 1000}}, [this, onDone](int code, const QJsonObject &data) {
        m_isFetchingMapList = false;
        if (code == 200) {
            for (const auto &v : data.value("rows").toArray()) {
                QJsonObject m = v.toObject();
                int mid = jsonIdStr(m.value("mapId")).toInt();
                if (mid == 0 && m.contains("mapId")) mid = m.value("mapId").toInt();
                QString mn = m.value("mapName").toString().trimmed();
                if (mid != 0 && !mn.isEmpty()) m_mapIdToName[mid] = mn;
            }
        }
        if (onDone) onDone();
    });
}

QString MapViewDataLoader::lookupMapName(int mapId, const QString &currentMapId, const QString &currentMapName) {
    if (mapId == 0) return QStringLiteral("--");
    if (m_mapIdToName.contains(mapId)) return m_mapIdToName.value(mapId);
    if (!currentMapId.isEmpty() && currentMapId.toInt() == mapId && !currentMapName.isEmpty()) {
        m_mapIdToName[mapId] = currentMapName;
        return currentMapName;
    }
    return QString::number(mapId);
}

void MapViewDataLoader::loadCurrentCarMap(std::function<void(const QString &carName, const QString &mapId)> onDone,
                                         std::function<void(const QString &msg, const QString &color)> setStatus) {
    if (m_isLoadingCar) return;
    m_isLoadingCar = true;
    if (setStatus) setStatus("正在获取设备信息...", "#8898a8");

    QString carId = AppConfig::instance().agvId().isEmpty() ? "1" : AppConfig::instance().agvId();
    ApiService::instance().getCar(carId, [this, carId, onDone, setStatus](int code, const QJsonObject &data) {
        m_isLoadingCar = false;
        if (code != 200) { if (setStatus) setStatus("获取设备信息失败", "#e05555"); return; }
        QJsonObject car = data.value("data").toObject(data);
        QString carName = car.value("carName").toString().trimmed();
        if (carName.isEmpty() || carName == "--") carName = QString("AGV-%1").arg(carId);

        QString mapId = jsonIdStr(car.value("mapId"));
        if (mapId.isEmpty() || mapId == "0") mapId = jsonIdStr(car.value("map_id"));
        if (mapId.isEmpty() || mapId == "0") mapId = jsonIdStr(data.value("mapId"));

        if (mapId.isEmpty() || mapId == "0") {
            ApiService::instance().listMap(QJsonObject{{"pageNum",1},{"pageSize",1}}, [onDone, carName, setStatus](int c2, const QJsonObject &d2) {
                if (c2 == 200 && !d2.value("rows").toArray().isEmpty()) {
                    if (onDone) onDone(carName, jsonIdStr(d2.value("rows").toArray().first().toObject().value("mapId")));
                } else if (setStatus) setStatus("无可用地图", "#e05555");
            });
            return;
        }
        if (onDone) onDone(carName, mapId);
    });
}

static QString findBestFile(const QStringList &files, const QString &prefExact, const QString &prefSub, const QString &fallbackSub) {
    if (!prefExact.isEmpty()) {
        for (const QString &f : files) if (QFileInfo(f).fileName().compare(prefExact, Qt::CaseInsensitive) == 0) return f;
    }
    if (!prefSub.isEmpty()) {
        for (const QString &f : files) if (QFileInfo(f).fileName().contains(prefSub, Qt::CaseInsensitive)) return f;
    }
    QStringList matches;
    if (!fallbackSub.isEmpty()) {
        for (const QString &f : files) if (f.contains(fallbackSub, Qt::CaseInsensitive)) matches << f;
    }
    QStringList sorted = matches.isEmpty() ? files : matches;
    if (sorted.isEmpty()) return QString();
    std::sort(sorted.begin(), sorted.end(), [](const QString &a, const QString &b) {
        QFileInfo fa(a), fb(b);
        return fa.lastModified() != fb.lastModified() ? fa.lastModified() > fb.lastModified() : fa.fileName() > fb.fileName();
    });
    return sorted.first();
}

bool MapViewDataLoader::findLocalMapFiles(const QString &dirPath, QString &outYamlPath, QString &outPgmPath,
                                         const QString &preferredMapName, const QString &preferredYamlName, const QString &preferredPgmName)
{
    if (!QDir(dirPath).exists()) return false;
    QStringList yList, pList;
    QDirIterator yIt(dirPath, QStringList() << "*.yaml" << "*.yml", QDir::Files, QDirIterator::Subdirectories);
    while (yIt.hasNext()) yList << yIt.next();
    QDirIterator pIt(dirPath, QStringList() << "*.pgm", QDir::Files, QDirIterator::Subdirectories);
    while (pIt.hasNext()) pList << pIt.next();
    if (yList.isEmpty() || pList.isEmpty()) return false;

    outYamlPath = findBestFile(yList, preferredYamlName, preferredMapName, ".map.yaml");
    if (outYamlPath.isEmpty()) outYamlPath = findBestFile(yList, QString(), QString(), QString());

    outPgmPath.clear();
    QFile yf(outYamlPath);
    if (yf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString txt = QString::fromUtf8(yf.readAll()); yf.close();
        double r = 0, ox = 0, oy = 0; bool neg = false; QString imgFile;
        if (MapPgmUtil::parseMapYaml(txt, r, ox, oy, neg, &imgFile) && !imgFile.isEmpty()) {
            QString cand = QFileInfo(QFileInfo(outYamlPath).dir(), imgFile).absoluteFilePath();
            if (QFile::exists(cand) && QFileInfo(cand).size() > 0) outPgmPath = cand;
        }
    }
    if (outPgmPath.isEmpty()) {
        QString base = QFileInfo(outYamlPath).completeBaseName();
        if (base.endsWith(".map", Qt::CaseInsensitive)) base.chop(4);
        outPgmPath = findBestFile(pList, preferredPgmName, base, QString());
    }
    return !outYamlPath.isEmpty() && !outPgmPath.isEmpty() && QFileInfo(outYamlPath).size() > 0 && QFileInfo(outPgmPath).size() > 0;
}

bool MapViewDataLoader::loadMapFromLocalFiles(const QString &yamlPath, const QString &pgmPath,
                                             QPixmap &outPix, double &worldL, double &worldB, double &worldR, double &worldT)
{
    QFile yf(yamlPath);
    if (!yf.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QString yamlText = QString::fromUtf8(yf.readAll()); yf.close();

    double resolution = 0, originX = 0, originY = 0; bool negate = false; QString imageRel;
    if (!MapPgmUtil::parseMapYaml(yamlText, resolution, originX, originY, negate, &imageRel) || resolution <= 0) return false;

    QString actualPgm = (!imageRel.isEmpty() && QFile::exists(QFileInfo(QFileInfo(yamlPath).dir(), imageRel).absoluteFilePath()))
                        ? QFileInfo(QFileInfo(yamlPath).dir(), imageRel).absoluteFilePath() : pgmPath;
    QFile pf(actualPgm);
    if (!pf.open(QIODevice::ReadOnly)) return false;
    QByteArray pgmData = pf.readAll(); pf.close();

    QImage img = MapPgmUtil::pgmToImage(pgmData, negate);
    if (img.isNull()) return false;

    outPix = QPixmap::fromImage(img);
    worldL = originX; worldB = originY;
    worldR = worldL + outPix.width() * resolution;
    worldT = worldB + outPix.height() * resolution;
    return true;
}

void MapViewDataLoader::loadMapById(const QString &mapId, const QString &mapName, QWidget *parentWidget,
                                   std::function<void(const QString &msg, const QString &color)> setStatus,
                                   std::function<void(const QString &finalMapName, const QPixmap &pix,
                                                      double wL, double wB, double wR, double wT)> onMapLoaded) {
    if (mapId.isEmpty() || m_loadingMapId == mapId) return;
    m_loadingMapId = mapId;
    if (setStatus) setStatus(QString("正在加载地图 %1...").arg(mapId), "#8898a8");

    QString localDir = QCoreApplication::applicationDirPath() + "/maps/" + mapId;
    QString metaMapName = mapName, metaYamlName, metaPgmName;
    QFile metaFile(localDir + "/meta.json");
    if (metaFile.open(QIODevice::ReadOnly)) {
        QJsonObject o = QJsonDocument::fromJson(metaFile.readAll()).object(); metaFile.close();
        if (metaMapName.isEmpty()) metaMapName = o.value("mapName").toString();
        metaYamlName = o.value("yamlName").toString(); metaPgmName = o.value("fileName").toString();
    }

    auto doLoadOrDownload = [this, mapId, localDir, parentWidget, setStatus, onMapLoaded](const QString &targetMapName, const QString &targetYamlName, const QString &targetPgmName) {
        QString yPath, pPath;
        if (findLocalMapFiles(localDir, yPath, pPath, targetMapName, targetYamlName, targetPgmName)) {
            QString base = QFileInfo(yPath).completeBaseName();
            if (base.endsWith(".map", Qt::CaseInsensitive)) base.chop(4);
            QString fName = !targetMapName.isEmpty() ? targetMapName : base;
            QPixmap pix; double wL = 0, wB = 0, wR = 0, wT = 0;
            if (loadMapFromLocalFiles(yPath, pPath, pix, wL, wB, wR, wT)) {
                m_loadingMapId.clear();
                if (setStatus) setStatus(QString("地图 %1 加载完成 (本地缓存)").arg(fName), "#8898a8");
                if (onMapLoaded) onMapLoaded(fName, pix, wL, wB, wR, wT);
                return;
            }
        }

        if (setStatus) setStatus(QString("正在下载地图资源包 (ID: %1)...").arg(mapId), "#8898a8");
        auto fail = [this, setStatus, parentWidget](const QString &msg) {
            m_loadingMapId.clear();
            if (setStatus) setStatus(msg, "#e05555");
            Toast::error(parentWidget, msg);
        };
        ApiService::instance().downloadMapResources(mapId, [this, mapId, localDir, targetMapName, targetYamlName, targetPgmName, parentWidget, setStatus, onMapLoaded, fail](bool ok, const QByteArray &zipData) {
            if (!ok || zipData.isEmpty()) { fail("下载地图资源包失败"); return; }
            QDir().mkpath(localDir);
            QString zipPath = localDir + "/" + mapId + ".zip";
            QFile zf(zipPath);
            if (!zf.open(QIODevice::WriteOnly)) { fail("保存地图资源包失败"); return; }
            zf.write(zipData); zf.close();
            if (!ZipUtil::extractZip(zipPath, localDir)) { fail("地图资源包解压失败"); return; }

            QString nyPath, npPath;
            if (!findLocalMapFiles(localDir, nyPath, npPath, targetMapName, targetYamlName, targetPgmName)) {
                fail("地图资源包中缺少 pgm 或 yaml 文件"); return;
            }
            QString base = QFileInfo(nyPath).completeBaseName();
            if (base.endsWith(".map", Qt::CaseInsensitive)) base.chop(4);
            QString fName = !targetMapName.isEmpty() ? targetMapName : base;

            QFile mf(localDir + "/meta.json");
            if (mf.open(QIODevice::WriteOnly)) {
                mf.write(QJsonDocument(QJsonObject{{"mapId", mapId}, {"mapName", fName},
                    {"yamlName", QFileInfo(nyPath).fileName()}, {"fileName", QFileInfo(npPath).fileName()}}).toJson());
                mf.close();
            }

            QPixmap pix; double wL = 0, wB = 0, wR = 0, wT = 0;
            if (!loadMapFromLocalFiles(nyPath, npPath, pix, wL, wB, wR, wT)) { fail("解析地图数据失败"); return; }
            m_loadingMapId.clear();
            if (setStatus) setStatus(QString("地图 %1 下载并加载完成").arg(fName), "#8898a8");
            Toast::success(parentWidget, "地图资源下载并加载成功");
            if (onMapLoaded) onMapLoaded(fName, pix, wL, wB, wR, wT);
        });
    };

    if (metaYamlName.isEmpty() || metaMapName.isEmpty()) {
        ApiService::instance().getMap(mapId, [metaMapName, doLoadOrDownload](int code, const QJsonObject &data) {
            QJsonObject d = (code == 200) ? data.value("data").toObject(data) : QJsonObject();
            QString aMap = metaMapName.isEmpty() ? d.value("mapName").toString() : metaMapName;
            doLoadOrDownload(aMap, d.value("yamlName").toString(), d.value("fileName").toString());
        });
    } else {
        doLoadOrDownload(metaMapName, metaYamlName, metaPgmName);
    }
}

void MapViewDataLoader::loadRoute(const QString &mapId, const QString &mapName,
                                 std::function<void(bool ok, const QString &jsonStr)> onLoaded)
{
    if (mapId.isEmpty()) { if (onLoaded) onLoaded(false, QString()); return; }

    auto readLocalCache = [mapId, mapName]() -> QString {
        const QStringList paths = {
            QCoreApplication::applicationDirPath() + "/maps/" + mapName + "_" + mapId + "/route.json",
            QCoreApplication::applicationDirPath() + "/maps/" + mapId + "/route.json"
        };
        for (const auto &p : paths) {
            QFile rf(p);
            if (rf.exists() && rf.open(QIODevice::ReadOnly)) {
                QString s = QString::fromUtf8(rf.readAll());
                rf.close();
                if (!s.trimmed().isEmpty()) return s;
            }
        }
        return QString();
    };

    ApiService::instance().loadEditedMap([mapId, mapName, onLoaded, readLocalCache](int code, const QJsonObject &data) {
        QString jsonStr = (code == 200) ? (data.contains("json") ? data.value("json").toString() : data.value("data").toObject().value("json").toString()) : QString();
        if (!jsonStr.isEmpty()) {
            QString dir = QCoreApplication::applicationDirPath() + "/maps/" + mapName + "_" + mapId;
            QDir().mkpath(dir);
            QFile rf(dir + "/route.json");
            if (rf.open(QIODevice::WriteOnly)) { rf.write(jsonStr.toUtf8()); rf.close(); }
        } else {
            jsonStr = readLocalCache();
        }
        if (onLoaded) onLoaded(!jsonStr.trimmed().isEmpty(), jsonStr);
    });
}

void MapViewDataLoader::saveRouteToServer(const QString &mapId, const QString &mapName,
                                         const QJsonObject &geojson, QWidget *parentWidget,
                                         std::function<void(bool ok)> onDone)
{
    if (mapId.isEmpty()) {
        Toast::info(parentWidget, "当前未加载地图，无法保存站点");
        if (onDone) onDone(false);
        return;
    }
    ApiService::instance().saveEditedMap(geojson, [mapId, mapName, geojson, parentWidget, onDone](int code, const QJsonObject &resp) {
        if (code == 200) {
            QString dir = QCoreApplication::applicationDirPath() + "/maps/" + mapName + "_" + mapId;
            QDir().mkpath(dir);
            QFile rf(dir + "/route.json");
            if (rf.open(QIODevice::WriteOnly)) { rf.write(QJsonDocument(geojson).toJson()); rf.close(); }
            if (onDone) onDone(true);
        } else {
            Toast::error(parentWidget, QString("保存路网失败: %1").arg(resp.value("msg").toString(resp.value("message").toString(QString("HTTP %1").arg(code)))));
            if (onDone) onDone(false);
        }
    });
}
