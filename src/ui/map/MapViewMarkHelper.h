/**
 * @file    MapViewMarkHelper.h
 * @layer   GUI
 * @brief   Helper for marking robot pose stations, workstations, and charging points.
 * @deps    QObject, QJsonObject, QJsonArray
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

class QWidget;
class QPoint;

class MapViewMarkHelper : public QObject
{
    Q_OBJECT
public:
    explicit MapViewMarkHelper(QWidget *parentWidget, QObject *parent = nullptr);
    ~MapViewMarkHelper() override = default;

    void showRobotContextMenu(const QPoint &globalPos,
                              const QString &mapId,
                              const QString &carId,
                              bool posValid,
                              double wx, double wy, double yawRad,
                              QJsonArray &points,
                              QJsonArray &charges,
                              QJsonArray &highPrecisionPoints,
                              std::function<void(std::function<void(bool)>)> saveRouteToServer);

    void markStation(const QString &mapId, bool posValid,
                     double wx, double wy, double yawRad,
                     QJsonArray &points,
                     std::function<void(std::function<void(bool)>)> saveRouteToServer);

    void markDevicePoint(const QString &type,
                         const QString &mapId,
                         const QString &carId,
                         bool posValid,
                         double wx, double wy, double yawRad,
                         QJsonArray &charges,
                         QJsonArray &highPrecisionPoints,
                         std::function<void(std::function<void(bool)>)> saveRouteToServer);

private:
    QWidget *m_parentWidget = nullptr;
    bool     m_devicePointPending = false;
};
