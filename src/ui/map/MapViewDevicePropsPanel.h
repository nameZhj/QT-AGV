/**
 * @file    MapViewDevicePropsPanel.h
 * @layer   GUI
 * @brief   Property panel displaying real-time robot metrics, telemetry, and MQTT device data.
 * @deps    QWidget, RobotInfoManager.h, MapViewTelemetryController.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QLabel>
#include <QGroupBox>
#include <QSet>
#include <QList>
#include "MapViewTelemetryController.h"

class MapViewDevicePropsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MapViewDevicePropsPanel(QWidget *parent = nullptr);
    ~MapViewDevicePropsPanel() override = default;

    void updateCarInfo(const MapViewRobotState &state, const RobotInfo &info);
    void updateDeviceMonitor(const RobotInfo &info, const QString &mapName = QString());
    void setViewVisible(bool visible);

private:
    void setupUI();
    QGroupBox* mkGroup(const QString &title);
    QLabel* mkInfoLabel(const QString &val = "--");

    QList<QWidget*> m_deviceGroups;
    QSet<QWidget*>  m_deviceHasData;

    // Car Status
    QGroupBox *m_carGroup          = nullptr;
    QLabel    *m_carNameLabel      = nullptr;
    QLabel    *m_deviceStatusLabel = nullptr;
    QLabel    *m_batteryLabel      = nullptr;
    QLabel    *m_speedLabel        = nullptr;
    QLabel    *m_covLabel          = nullptr;
    QLabel    *m_posLabel          = nullptr;

    // Telemetry Monitors
    QWidget *m_ctlGroup        = nullptr;
    QLabel  *m_ctlBatteryVal   = nullptr;
    QLabel  *m_ctlActualVal    = nullptr;
    QLabel  *m_ctlCmdVal       = nullptr;
    QWidget *m_mileageGroup    = nullptr;
    QLabel  *m_mileageTotalVal = nullptr;
    QLabel  *m_mileageTodayVal = nullptr;
    QWidget *m_runtimeGroup    = nullptr;
    QLabel  *m_runtimeCurVal   = nullptr;
    QLabel  *m_runtimeTotalVal = nullptr;
    QWidget *m_obstacleGroup   = nullptr;
    QLabel  *m_obstacleTypeVal = nullptr;
    QLabel  *m_obstaclePointVal= nullptr;
    QWidget *m_blockedGroup    = nullptr;
    QLabel  *m_blockedStateVal = nullptr;
    QWidget *m_poseGroup       = nullptr;
    QLabel  *m_posePosVal      = nullptr;
    QLabel  *m_poseYawVal      = nullptr;
    QLabel  *m_poseThetaVal    = nullptr;
    QLabel  *m_poseEpcVal      = nullptr;
    QWidget *m_robotInfoGroup  = nullptr;
    QLabel  *m_robotCarIdVal   = nullptr;
    QLabel  *m_robotMapIdVal   = nullptr;
    QLabel  *m_robotTaskIdVal  = nullptr;
    QLabel  *m_robotStatusVal  = nullptr;
};
