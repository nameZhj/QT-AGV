/**
 * @file    RobotStatusTelemetrySync.cpp
 * @layer   GUILayer
 * @brief   Telemetry, MQTT payload parsing, and RobotInfo mapper for RobotStatusDialog.
 * @deps    RobotStatusTelemetrySync.h, RobotStatusDialog.h, RobotStatusUiFactory.h, RobotInfoManager.h, AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & telemetry mappings invariant.
 */

#include "RobotStatusTelemetrySync.h"
#include "RobotStatusDialog.h"
#include "RobotStatusUiFactory.h"
#include "core/RobotInfoManager.h"
#include "app/AppConfig.h"

#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void RobotStatusTelemetrySync::updateFromRobotInfo(RobotStatusDialog *dlg, const RobotInfo &info) {
  dlg->m_hasMqttData = true;

  // 1. 机器人运行状态
  if (dlg->m_valRunMode) {
    if (!info.status.isEmpty()) {
      dlg->m_valRunMode->setText(info.statusText());
      dlg->m_valRunMode->setStyleSheet(QString("color:%1;").arg(info.statusColor()));
    } else {
      dlg->m_valRunMode->setText("--");
      dlg->m_valRunMode->setStyleSheet("color:#94a3b8;");
    }
  }
  if (dlg->m_valTotalDist) {
    dlg->m_valTotalDist->setText(info.mileage.valid ? QString("%1 m").arg(info.mileage.total_distance_m, 0, 'f', 2) : "--");
  }
  if (dlg->m_valTodayDist) {
    dlg->m_valTodayDist->setText(info.mileage.valid ? QString("%1 m").arg(info.mileage.today_distance_m, 0, 'f', 2) : "--");
  }
  if (dlg->m_valTotalTime) {
    dlg->m_valTotalTime->setText(info.runtime.valid ? RobotInfoManager::formatDuration(info.runtime.total_runtime_sec) : "--");
  }
  if (dlg->m_valThisTime) {
    dlg->m_valThisTime->setText(info.runtime.valid ? RobotInfoManager::formatDuration(info.runtime.current_runtime_sec) : "--");
  }
  if (info.battery >= 0.0 && dlg->m_valBatteryPct) {
    dlg->m_valBatteryPct->setText(QString("%1 %").arg(info.batteryPercent()));
  }

  // 2. 机器人运动状态
  double yawDeg = info.car_yaw * 180.0 / M_PI;
  if (dlg->m_valPosition) {
    if (info.hasPose) {
      dlg->m_valPosition->setText(QString("%1 m, %2 m, %3 °")
          .arg(info.car_x, 0, 'f', 3).arg(info.car_y, 0, 'f', 3).arg(yawDeg, 0, 'f', 3));
    } else {
      dlg->m_valPosition->setText("--");
    }
  }

  if (dlg->m_valActualSpeed) {
    if (info.actual_vel.valid) {
      double wDeg = info.actual_vel.wz * 180.0 / M_PI;
      dlg->m_valActualSpeed->setText(QString("%1 m/s, %2 m/s, %3 °/s")
          .arg(info.actual_vel.vx, 0, 'f', 3).arg(info.actual_vel.vy, 0, 'f', 3).arg(wDeg, 0, 'f', 3));

      if (dlg->m_badgeStopped) {
        bool stopped = (std::abs(info.actual_vel.vx) < 0.005 &&
                        std::abs(info.actual_vel.vy) < 0.005 &&
                        std::abs(info.actual_vel.wz) < 0.005);
        RobotStatusUiFactory::updateBadge(dlg->m_badgeStopped, stopped ? "是" : "否",
                                          stopped ? RobotStatusBadgeType::Green : RobotStatusBadgeType::Orange);
      }
    } else {
      dlg->m_valActualSpeed->setText("--");
    }
  }

  if (dlg->m_valTargetSpeed) {
    if (info.cmd_vel.valid) {
      double cmdWDeg = info.cmd_vel.wz * 180.0 / M_PI;
      dlg->m_valTargetSpeed->setText(QString("%1 m/s, %2 m/s, %3 °/s")
          .arg(info.cmd_vel.vx, 0, 'f', 3).arg(info.cmd_vel.vy, 0, 'f', 3).arg(cmdWDeg, 0, 'f', 3));
    } else {
      dlg->m_valTargetSpeed->setText("--");
    }
  }

  if (info.obstacle.valid) {
    if (dlg->m_badgeBlocked) {
      RobotStatusUiFactory::updateBadge(dlg->m_badgeBlocked, info.obstacle.blocked ? "遮挡" : "未遮挡",
                                        info.obstacle.blocked ? RobotStatusBadgeType::Orange : RobotStatusBadgeType::Green);
    }
    if (info.obstacle.hasPoint) {
      if (dlg->m_valNearestObs) {
        dlg->m_valNearestObs->setText(QString("%1 m, %2 m")
            .arg(info.obstacle.point.x, 0, 'f', 3).arg(info.obstacle.point.y, 0, 'f', 3));
      }
      if (dlg->m_valDynObs) {
        dlg->m_valDynObs->setText(QString("x:%1m y:%2m")
            .arg(info.obstacle.point.x, 0, 'f', 4).arg(info.obstacle.point.y, 0, 'f', 4));
      }
    }
  }

  if (info.covScore >= 0.0 && dlg->m_badgeConfidence) {
    double cs = (info.covScore > 1.0) ? info.covScore / 100.0 : info.covScore;
    RobotStatusBadgeType bt = (cs > 0.0) ? RobotStatusBadgeType::Green : RobotStatusBadgeType::Red;
    RobotStatusUiFactory::updateBadge(dlg->m_badgeConfidence, QString::number(cs, 'f', 2), bt);
    if (dlg->m_valLocStatus) {
      dlg->m_valLocStatus->setText(cs > 0.0 ? "<font color='#22c55e'>定位成功</font>" : "<font color='#ef4444'>定位失败</font>");
    }
  }

  // 3. 机器人导航状态
  if (info.map_id != 0 && dlg->m_valCurMap) {
    if (!dlg->m_mapName.isEmpty() && (dlg->m_mapId.isEmpty() || dlg->m_mapId.toInt() == info.map_id)) {
      dlg->m_valCurMap->setText(dlg->m_mapName);
    } else {
      dlg->m_valCurMap->setText(QString::number(info.map_id));
    }
  }
  if (dlg->m_valNavStatus) {
    dlg->m_valNavStatus->setText(info.navStatusText());
  }

  // 4. 机器人基本信息
  if (info.car_id != 0 && dlg->m_valRobotId) {
    dlg->m_valRobotId->setText(QString::number(info.car_id));
  }
  if (!info.carName.isEmpty() && dlg->m_valRobotName) {
    dlg->m_valRobotName->setText(info.carName);
  }
  if (!info.remark.isEmpty() && dlg->m_valRobotRemark) {
    dlg->m_valRobotRemark->setText(info.remark);
  }
  if (!info.fwVersion.isEmpty() && dlg->m_valFirmwareVer) {
    dlg->m_valFirmwareVer->setText(info.fwVersion);
  }
}

void RobotStatusTelemetrySync::updateFromRobotState(RobotStatusDialog *dlg, const QString &carId,
                                                    double x, double y, double yawRad,
                                                    double speed, double battery, double covScore,
                                                    bool posValid) {
  double yawDeg = yawRad * 180.0 / M_PI;
  if (dlg->m_valPosition && !RobotInfoManager::instance().currentInfo().hasPose) {
    dlg->m_valPosition->setText(QString("%1 m, %2 m, %3 °").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3).arg(yawDeg, 0, 'f', 3));
  }
  if (dlg->m_valActualSpeed && !RobotInfoManager::instance().currentInfo().actual_vel.valid) {
    dlg->m_valActualSpeed->setText(QString("%1 m/s, 0.000 m/s, 0.000 rad/s (0.000 °/s)").arg(speed, 0, 'f', 3));
  }
  if (dlg->m_badgeConfidence && covScore >= 0) {
    double cs = (covScore > 1.0) ? covScore / 100.0 : covScore;
    RobotStatusBadgeType bt = cs >= 0.7 ? RobotStatusBadgeType::Green : (cs >= 0.3 ? RobotStatusBadgeType::Orange : RobotStatusBadgeType::Red);
    RobotStatusUiFactory::updateBadge(dlg->m_badgeConfidence, QString::number(cs, 'f', 2), bt);
  }
  if (dlg->m_valBatteryPct && battery >= 0) {
    double bat = (battery <= 1.0 && battery > 0) ? battery * 100.0 : battery;
    dlg->m_valBatteryPct->setText(QString::number(bat, 'f', 1) + " %");
  }
  if (dlg->m_valLocStatus) {
    if (covScore > 0.0) dlg->m_valLocStatus->setText("<font color='#22c55e'>定位成功</font>");
    else if (posValid) dlg->m_valLocStatus->setText("<font color='#ef4444'>定位失败</font>");
    else dlg->m_valLocStatus->setText("<font color='#ef4444'>未定位</font>");
  }
  if (dlg->m_badgeStopped) {
    bool stopped = std::abs(speed) < 0.01;
    RobotStatusUiFactory::updateBadge(dlg->m_badgeStopped, stopped ? "是" : "否",
                                      stopped ? RobotStatusBadgeType::Green : RobotStatusBadgeType::Orange);
  }
  if (dlg->m_valRobotId && !carId.isEmpty()) {
    dlg->m_valRobotId->setText(carId);
  }
}

QByteArray RobotStatusTelemetrySync::sanitizeJsonPayload(const QByteArray &payload) {
  QByteArray clean = payload.trimmed();
  bool inQuote = false, escape = false;
  for (int i = 0; i < clean.size(); ++i) {
    char c = clean.at(i);
    if (escape) { escape = false; continue; }
    if (c == '\\') { if (inQuote) escape = true; continue; }
    if (c == '"') { inQuote = !inQuote; continue; }
    if (!inQuote) {
      if (c == '(') clean[i] = '{';
      else if (c == ')') clean[i] = '}';
    }
  }
  return clean;
}

void RobotStatusTelemetrySync::onMqttMessage(RobotStatusDialog *dlg, const QString &topic,
                                            const QByteArray &payload) {
  QByteArray cleanPayload = sanitizeJsonPayload(payload);
  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(cleanPayload, &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
  if (!topic.contains("info")) return;

  RobotInfoManager::instance().updateFromPayload(topic, cleanPayload);
  Q_UNUSED(dlg)
}

void RobotStatusTelemetrySync::onPeriodicRefresh(RobotStatusDialog *dlg) {
  const RobotInfo &info = RobotInfoManager::instance().currentInfo();
  if (dlg->m_valThisTime) {
    if (info.runtime.valid) {
      dlg->m_valThisTime->setText(RobotInfoManager::formatDuration(info.runtime.current_runtime_sec));
    } else if (!dlg->m_hasMqttData) {
      dlg->m_valThisTime->setText("--");
    }
  }
  if (dlg->m_valTotalTime) {
    if (info.runtime.valid) {
      dlg->m_valTotalTime->setText(RobotInfoManager::formatDuration(info.runtime.total_runtime_sec));
    } else if (!dlg->m_hasMqttData) {
      dlg->m_valTotalTime->setText("--");
    }
  }

  if (dlg->m_lblDetail) {
    QString serverIp = AppConfig::instance().serverIp();
    if (serverIp.isEmpty()) serverIp = "192.168.10.241";
    dlg->m_lblDetail->setText(QString(
        "IP:        %1\n"
        "port:      8048\n"
        "type:      TCP/MQTT\n"
        "nick_name: %2\n"
        "time_t:    %3\n"
        "desc:      上位机控制终端"
    ).arg(serverIp,
          AppConfig::instance().currentDevice().name.isEmpty() ? "AGV-01" : AppConfig::instance().currentDevice().name,
          QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")));
  }
}
