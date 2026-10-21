/**
 * @file    MapViewDataLoader.h
 * @layer   GUI
 * @brief   Data loader and network synchronization manager for maps and route files.
 * @deps    QObject, QPixmap, QJsonObject, QMap
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QPixmap>
#include <QJsonObject>
#include <QMap>
#include <functional>

class MapViewDataLoader : public QObject
{
    Q_OBJECT
public:
    explicit MapViewDataLoader(QObject *parent = nullptr);
    ~MapViewDataLoader() override = default;

    void fetchMapNameList(std::function<void()> onDone = nullptr);
    QString lookupMapName(int mapId, const QString &currentMapId = QString(), const QString &currentMapName = QString());

    void loadCurrentCarMap(std::function<void(const QString &carName, const QString &mapId)> onDone,
                           std::function<void(const QString &msg, const QString &color)> setStatus);

    bool findLocalMapFiles(const QString &dirPath, QString &outYamlPath, QString &outPgmPath,
                           const QString &preferredMapName = QString(),
                           const QString &preferredYamlName = QString(),
                           const QString &preferredPgmName = QString());

    bool loadMapFromLocalFiles(const QString &yamlPath, const QString &pgmPath,
                               QPixmap &outPix, double &worldL, double &worldB, double &worldR, double &worldT);

    void loadMapById(const QString &mapId, const QString &mapName,
                     QWidget *parentWidget,
                     std::function<void(const QString &msg, const QString &color)> setStatus,
                     std::function<void(const QString &finalMapName, const QPixmap &pix,
                                        double wL, double wB, double wR, double wT)> onMapLoaded);

    void loadRoute(const QString &mapId, const QString &mapName,
                   std::function<void(bool ok, const QString &jsonStr)> onLoaded);

    void saveRouteToServer(const QString &mapId, const QString &mapName,
                           const QJsonObject &geojson,
                           QWidget *parentWidget,
                           std::function<void(bool ok)> onDone);

    void clearLoadingState() { m_isLoadingCar = false; m_loadingMapId.clear(); }

private:
    QMap<int, QString> m_mapIdToName;
    bool               m_isFetchingMapList = false;
    bool               m_isLoadingCar      = false;
    QString            m_loadingMapId;
};
