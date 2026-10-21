/**
 * @file    MapViewDevicePropsPanel.cpp
 * @layer   GUI
 * @brief   Real-time robot status display and MQTT telemetry data monitor implementation.
 * @deps    MapViewDevicePropsPanel.h, AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewDevicePropsPanel.h"
#include "app/AppConfig.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <cmath>

MapViewDevicePropsPanel::MapViewDevicePropsPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

QGroupBox* MapViewDevicePropsPanel::mkGroup(const QString &title)
{
    auto *g = new QGroupBox(title, this);
    g->setStyleSheet(
        "QGroupBox{border:1px solid #2d3340; border-radius:4px;"
        " margin-top:14px; color:#8898a8; font-size:12px; font-weight:600;}"
        "QGroupBox::title{subcontrol-origin:margin; left:8px; top:-2px; padding:0 4px;}");
    return g;
}

QLabel* MapViewDevicePropsPanel::mkInfoLabel(const QString &val)
{
    auto *l = new QLabel(val, this);
    l->setStyleSheet("color:#c8d0d8; font-size:12px;");
    return l;
}

void MapViewDevicePropsPanel::setupUI()
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);

    m_carGroup = mkGroup("设备状态");
    auto *carForm = new QFormLayout(m_carGroup);
    carForm->setSpacing(6);
    carForm->setLabelAlignment(Qt::AlignRight);

    m_carNameLabel      = mkInfoLabel();
    m_deviceStatusLabel = mkInfoLabel();
    m_batteryLabel      = mkInfoLabel();
    m_speedLabel        = mkInfoLabel();
    m_covLabel          = mkInfoLabel();
    m_posLabel          = mkInfoLabel();

    carForm->addRow("设备:", m_carNameLabel);
    carForm->addRow("状态:", m_deviceStatusLabel);
    carForm->addRow("电量:", m_batteryLabel);
    carForm->addRow("速度:", m_speedLabel);
    carForm->addRow("置信度:", m_covLabel);
    carForm->addRow("坐标:", m_posLabel);
    lay->addWidget(m_carGroup);
    m_deviceGroups.append(m_carGroup);
    m_deviceHasData.insert(m_carGroup);

    auto mkValRow = [&](QFormLayout *f, const QString &label, QLabel *&outVal) {
        outVal = mkInfoLabel();
        f->addRow(label, outVal);
    };

    auto setupSection = [&](const QString &title, QWidget *&grp, auto buildFn) {
        grp = mkGroup(title);
        auto *f = new QFormLayout(grp);
        f->setSpacing(6);
        f->setLabelAlignment(Qt::AlignRight);
        buildFn(f);
        lay->addWidget(grp);
        m_deviceGroups.append(grp);
        grp->setVisible(false);
    };

    setupSection("控制屏", m_ctlGroup, [&](QFormLayout *f) {
        mkValRow(f, "电量:", m_ctlBatteryVal);
        mkValRow(f, "实际速度:", m_ctlActualVal);
        mkValRow(f, "接收速度:", m_ctlCmdVal);
    });

    setupSection("里程", m_mileageGroup, [&](QFormLayout *f) {
        mkValRow(f, "累计里程:", m_mileageTotalVal);
        mkValRow(f, "今日里程:", m_mileageTodayVal);
    });

    setupSection("运行时间", m_runtimeGroup, [&](QFormLayout *f) {
        mkValRow(f, "本次运行:", m_runtimeCurVal);
        mkValRow(f, "累计运行:", m_runtimeTotalVal);
    });

    setupSection("障碍", m_obstacleGroup, [&](QFormLayout *f) {
        mkValRow(f, "类型:", m_obstacleTypeVal);
        mkValRow(f, "障碍点:", m_obstaclePointVal);
    });

    setupSection("阻挡", m_blockedGroup, [&](QFormLayout *f) {
        mkValRow(f, "状态:", m_blockedStateVal);
    });

    setupSection("位姿与机台", m_poseGroup, [&](QFormLayout *f) {
        mkValRow(f, "坐标:", m_posePosVal);
        mkValRow(f, "朝向:", m_poseYawVal);
        mkValRow(f, "机台角度:", m_poseThetaVal);
        mkValRow(f, "末端坐标:", m_poseEpcVal);
    });

    setupSection("机器人信息", m_robotInfoGroup, [&](QFormLayout *f) {
        mkValRow(f, "车辆ID:", m_robotCarIdVal);
        mkValRow(f, "地图ID:", m_robotMapIdVal);
        mkValRow(f, "任务ID:", m_robotTaskIdVal);
        mkValRow(f, "状态:", m_robotStatusVal);
    });
}

void MapViewDevicePropsPanel::updateCarInfo(const MapViewRobotState &state, const RobotInfo &info)
{
    if (!state.carName.isEmpty()) {
        m_carNameLabel->setText(state.carName);
    } else {
        QString cid = state.carId.isEmpty() ? AppConfig::instance().agvId() : state.carId;
        if (cid.isEmpty()) cid = "1";
        m_carNameLabel->setText(QString("AGV-%1").arg(cid));
    }

    if (m_deviceStatusLabel) {
        m_deviceStatusLabel->setText(info.statusText());
        m_deviceStatusLabel->setStyleSheet(QString("color:%1; font-size:12px; font-weight:600;").arg(info.statusColor()));
    }

    if (!state.posValid) {
        m_posLabel->setText("未定位");
        m_covLabel->setText("未定位");
        m_covLabel->setStyleSheet("color:#e05555; font-size:12px;");
        return;
    }

    double bat = state.battery;
    if (bat < 0) {
        m_batteryLabel->setText("充电中");
        m_batteryLabel->setStyleSheet("color:#5cb85c; font-size:12px;");
    } else {
        int batPct = (bat >= 0.0 && bat <= 1.0) ? qRound(bat * 100.0) : qRound(bat);
        m_batteryLabel->setText(QString::number(batPct) + "%");
        QColor c = batPct < 20 ? QColor(0xe0,0x55,0x55) :
                   batPct < 40 ? QColor(0xf0,0xa0,0x30) : QColor(0x5c,0xb8,0x5c);
        m_batteryLabel->setStyleSheet(QString("color:%1; font-size:12px;").arg(c.name()));
    }

    m_speedLabel->setText(QString::number(state.speed, 'f', 2) + " m/s");
    m_posLabel->setText(QString("%1, %2").arg(state.wx, 0, 'f', 2).arg(state.wy, 0, 'f', 2));

    if (state.covScore >= 0) {
        double cs = (state.covScore > 1.0) ? state.covScore / 100.0 : state.covScore;
        m_covLabel->setText(QString::number(cs, 'f', 2));
        m_covLabel->setStyleSheet(cs <= 0.4 ? "color:#e05555; font-size:12px;" : "color:#c8d0d8; font-size:12px;");
    } else {
        m_covLabel->setText("未知");
        m_covLabel->setStyleSheet("color:#8898a8; font-size:12px;");
    }
}

void MapViewDevicePropsPanel::updateDeviceMonitor(const RobotInfo &info, const QString &mapName)
{
    if (m_ctlGroup) {
        auto velText = [](const VelocityInfo &v) {
            return QString("%1 m/s  %2 m/s  %3 °/s")
                .arg(v.vx, 0, 'f', 3).arg(v.vy, 0, 'f', 3)
                .arg(v.wz * 180.0 / M_PI, 0, 'f', 3);
        };
        bool has = (info.battery >= 0.0) || info.actual_vel.valid || info.cmd_vel.valid;
        if (m_ctlBatteryVal) m_ctlBatteryVal->setText(info.battery >= 0.0 ? QString("%1%").arg(info.batteryPercent()) : "--");
        if (m_ctlActualVal)  m_ctlActualVal->setText(info.actual_vel.valid ? velText(info.actual_vel) : "--");
        if (m_ctlCmdVal)     m_ctlCmdVal->setText(info.cmd_vel.valid ? velText(info.cmd_vel) : "--");
        if (has) m_deviceHasData.insert(m_ctlGroup); else m_deviceHasData.remove(m_ctlGroup);
    }

    if (m_mileageGroup) {
        if (info.mileage.valid) {
            if (m_mileageTotalVal) m_mileageTotalVal->setText(QString("%1 m").arg(info.mileage.total_distance_m, 0, 'f', 2));
            if (m_mileageTodayVal) m_mileageTodayVal->setText(QString("%1 m").arg(info.mileage.today_distance_m, 0, 'f', 2));
            m_deviceHasData.insert(m_mileageGroup);
        } else {
            m_deviceHasData.remove(m_mileageGroup);
        }
    }

    if (m_runtimeGroup) {
        if (info.runtime.valid) {
            if (m_runtimeCurVal)   m_runtimeCurVal->setText(RobotInfoManager::formatDuration(info.runtime.current_runtime_sec));
            if (m_runtimeTotalVal) m_runtimeTotalVal->setText(RobotInfoManager::formatDuration(info.runtime.total_runtime_sec));
            m_deviceHasData.insert(m_runtimeGroup);
        } else {
            m_deviceHasData.remove(m_runtimeGroup);
        }
    }

    if (m_obstacleGroup) {
        if (info.obstacle.valid) {
            if (m_obstacleTypeVal) m_obstacleTypeVal->setText(info.obstacle.blocked ? "已检测到障碍" : "无障碍");
            if (m_obstaclePointVal) {
                m_obstaclePointVal->setText(info.obstacle.hasPoint
                    ? QString("(%1, %2) m").arg(info.obstacle.point.x, 0, 'f', 3).arg(info.obstacle.point.y, 0, 'f', 3)
                    : "无");
            }
            m_deviceHasData.insert(m_obstacleGroup);
        } else {
            m_deviceHasData.remove(m_obstacleGroup);
        }
    }
    if (m_blockedGroup) {
        if (info.obstacle.valid) {
            if (m_blockedStateVal) m_blockedStateVal->setText(info.obstacle.blocked ? "已阻挡" : "未阻挡");
            m_deviceHasData.insert(m_blockedGroup);
        } else {
            m_deviceHasData.remove(m_blockedGroup);
        }
    }

    if (m_poseGroup) {
        if (info.hasPose && m_posePosVal) {
            m_posePosVal->setText(QString("(%1, %2, %3) m")
                .arg(info.car_x, 0, 'f', 3).arg(info.car_y, 0, 'f', 3).arg(info.car_z, 0, 'f', 3));
        } else if (m_posePosVal) {
            m_posePosVal->setText("--");
        }
        if (m_poseYawVal) {
            m_poseYawVal->setText(info.hasPose ? QString("%1°").arg(info.car_yaw * 180.0 / M_PI, 0, 'f', 1) : "--");
        }
        if (m_poseThetaVal) m_poseThetaVal->setText(info.hasTheta ? QString("%1°").arg(info.theta, 0, 'f', 1) : "--");
        if (m_poseEpcVal) {
            m_poseEpcVal->setText(info.hasEpc
                ? QString("(%1, %2) m, %3°")
                      .arg(info.epc_x, 0, 'f', 3).arg(info.epc_y, 0, 'f', 3).arg(info.epc_theta * 180.0 / M_PI, 0, 'f', 1)
                : "--");
        }
        if (info.hasPose || info.hasTheta || info.hasEpc) m_deviceHasData.insert(m_poseGroup);
        else m_deviceHasData.remove(m_poseGroup);
    }

    if (m_robotInfoGroup) {
        bool has = (info.car_id != 0) || !info.status.isEmpty() || (info.map_id != 0) || (info.task_id >= 0);
        if (m_robotCarIdVal)  m_robotCarIdVal->setText(info.car_id != 0 ? QString::number(info.car_id) : "--");
        if (m_robotMapIdVal)  m_robotMapIdVal->setText(!mapName.isEmpty() ? mapName : (info.map_id != 0 ? QString::number(info.map_id) : "--"));
        if (m_robotTaskIdVal) m_robotTaskIdVal->setText(info.task_id >= 0 ? QString::number(info.task_id) : "无");
        if (m_robotStatusVal) {
            m_robotStatusVal->setText(info.statusText());
            m_robotStatusVal->setStyleSheet(QString("color:%1; font-size:12px; font-weight:600;").arg(info.statusColor()));
        }
        if (has) m_deviceHasData.insert(m_robotInfoGroup); else m_deviceHasData.remove(m_robotInfoGroup);
    }
}

void MapViewDevicePropsPanel::setViewVisible(bool visible)
{
    for (QWidget *g : m_deviceGroups) {
        if (!g) continue;
        g->setVisible(visible && m_deviceHasData.contains(g));
    }
}
