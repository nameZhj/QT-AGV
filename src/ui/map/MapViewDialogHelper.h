/**
 * @file    MapViewDialogHelper.h
 * @layer   GUI
 * @brief   Dialog popup helpers and interaction routing for MapView.
 * @deps    QWidget, QString, QJsonArray, QJsonObject
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <functional>
#include "MapViewTelemetryController.h"

class QDialog;
class RobotStatusDialog;
class EditConfigWidget;
class MapViewRelocController;
class MapViewNavInteraction;
class MapCanvas;
class QGraphicsScene;
struct MapViewToolbarButtons;

class MapViewDialogHelper
{
public:
    static void showUploadMap(QWidget *parent, std::function<void()> onSuccess);
    static void showUploadSmap(QWidget *parent, std::function<void()> onSuccess);
    static void showSwitchMap(QWidget *parent, const QString &mapId, std::function<void(const QString &newMapId)> onSwitched);
    static void showRobotStatus(QWidget *parent, RobotStatusDialog *&dlg, const QString &mapId, const QString &mapName, const MapViewRobotState &st);
    static void showConfig(QWidget *parent, QDialog *&configDlg, EditConfigWidget *&editWidget, std::function<void()> onUpdate);
    static void showDownloadSource(const QString &mapId);

    static void unpackRouteJson(const QString &jsonStr, QJsonArray &pts, QJsonArray &lns, QJsonArray &crvs,
                                QJsonArray &chgs, QJsonArray &hps, QJsonArray &areas);
    static QString findStationName(const QString &id, const QJsonArray &pts, const QJsonArray &chgs, const QJsonArray &hps);
    static QString formatRouteName(const QJsonObject &obj, const std::function<QString(const QString&)> &nameLookup);

    static void handleRelocalize(MapViewRelocController *relocCtrl, MapViewNavInteraction *navInteraction,
                                 MapCanvas *canvas, QGraphicsScene *scene, MapViewToolbarButtons &btns,
                                 std::function<void(const QString&, const QString&)> setStatus);

    static void handleToggleNavMode(MapViewNavInteraction *navInteraction, MapViewRelocController *relocCtrl,
                                    MapCanvas *canvas, QGraphicsScene *scene, MapViewToolbarButtons &btns,
                                    std::function<void(const QString&, const QString&)> setStatus);
};
