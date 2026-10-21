/**
 * @file    MotionDialogController.cpp
 * @layer   GUILayer
 * @brief   Navigation and rotation command dispatcher and status observer for MotionDialog.
 * @deps    MotionDialogController.h, MotionDialog.h, ApiService.h, RobotInfoManager.h, MotionConfigManager.h, TaskChainManager.h, TeleopController.h
 * @note    Refactored for AI readability; API and motion semantics invariant.
 */

#include "MotionDialogController.h"
#include "MotionDialog.h"
#include "network/ApiService.h"
#include "core/RobotInfoManager.h"
#include "core/MotionConfigManager.h"
#include "core/TaskChainManager.h"
#include "core/TeleopController.h"

#include <QPointer>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QTabWidget>
#include <QJsonObject>
#include <QDebug>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void MotionDialogController::onSendClicked(MotionDialog *dlg) {
  if (TaskChainManager::instance().isWorking()) {
    dlg->m_statusLabel->setText("任务链执行中，不可下发移动指令");
    dlg->m_statusLabel->setStyleSheet("color: #ef4444; font-weight: bold; font-size: 14px;");
    return;
  }

  const int currentTab = dlg->m_tabWidget->currentIndex();

  if (currentTab == 0) {
    const double dist = dlg->m_transDistSpin->value();
    const double rawVx = dlg->m_transVxSpin->value();
    const double rawVy = dlg->m_transVySpin->value();
    double vx = MotionConfigManager::instance().clampSignedLinearSpeed(rawVx);
    double vy = MotionConfigManager::instance().clampSignedLinearSpeed(rawVy);

    if (dist <= 0.0001) {
      dlg->m_statusLabel->setText("距离应大于 0m");
      dlg->m_statusLabel->setStyleSheet("color: #ef4444; font-weight: bold; font-size: 14px;");
      return;
    }
    if (std::abs(vx) < 1e-4 && std::abs(vy) < 1e-4) {
      dlg->m_statusLabel->setText("速度 vx 或 vy 绝对值应大于 0");
      dlg->m_statusLabel->setStyleSheet("color: #ef4444; font-weight: bold; font-size: 14px;");
      return;
    }
  } else {
    const double angle = dlg->m_rotAngleSpin->value();
    const double rawSpeed = dlg->m_rotSpeedSpin->value();
    double speed = MotionConfigManager::instance().clampSignedAngularSpeedDeg(rawSpeed);

    if (angle <= 1e-4) {
      dlg->m_statusLabel->setText("旋转角度应大于 0°");
      dlg->m_statusLabel->setStyleSheet("color: #ef4444; font-weight: bold; font-size: 14px;");
      return;
    }
    if (std::abs(speed) < 1e-4) {
      dlg->m_statusLabel->setText("角速度绝对值应大于 0°/s");
      dlg->m_statusLabel->setStyleSheet("color: #ef4444; font-weight: bold; font-size: 14px;");
      return;
    }
  }

  const RobotInfo &info = RobotInfoManager::instance().currentInfo();
  dlg->m_startX = info.car_x;
  dlg->m_startY = info.car_y;
  dlg->m_startAngle = info.car_yaw * 180.0 / M_PI;
  dlg->m_hasStartPose = info.hasPose;

  dlg->m_sendBtn->setEnabled(false);
  dlg->m_statusLabel->setText("正在下发移动指令...");
  dlg->m_statusLabel->setStyleSheet("color: #3b82f6; font-weight: bold; font-size: 14px;");

  QPointer<MotionDialog> self = dlg;

  if (currentTab == 0) {
    self->m_motionType = 1;
    const double dist = dlg->m_transDistSpin->value();
    const double rawVx = dlg->m_transVxSpin->value();
    const double rawVy = dlg->m_transVySpin->value();
    double vx = MotionConfigManager::instance().clampSignedLinearSpeed(rawVx);
    double vy = MotionConfigManager::instance().clampSignedLinearSpeed(rawVy);
    self->m_targetDistance = std::abs(dist);

    double maxV = std::max(std::abs(vx), std::abs(vy));
    double durationSec = (maxV > 1e-3) ? ((std::abs(dist) / maxV) + 1.5) : 4.0;

    emit self->statusHintRequested(QString("正在下发平动指令(距离: %1m)...").arg(std::abs(dist), 0, 'f', 2), "#00bcd4");

    ApiService::instance().robotNavigationMoveLinear(vx, vy, dist, [self, dist, durationSec](int code, const QJsonObject &resp) {
      if (!self) return;
      qDebug() << "[MotionDialog] 平动指令响应:" << code << resp;

      const QString msg = resp.value("msg").toString();
      const bool ok = (code == 200 || msg.compare("ok", Qt::CaseInsensitive) == 0 || resp.value("code").toInt() == 200);
      if (!ok) {
        qWarning() << "[MotionDialog] 平动指令下发失败, code:" << code << resp;
        self->m_sendBtn->setEnabled(true);
        QString errMsg = msg.isEmpty() ? QString("错误码 %1").arg(code) : msg;
        self->m_statusLabel->setText(QString("平动指令失败: %1").arg(errMsg));
        self->m_statusLabel->setStyleSheet("color: #ef4444; font-weight: bold; font-size: 14px;");
        emit self->statusHintRequested(QString("平动指令下发失败: %1").arg(errMsg), "#ef4444");
        return;
      }

      self->setRunningState(true, false);
      emit self->motionStarted(1, std::abs(dist), QString("平动 %1m").arg(std::abs(dist), 0, 'f', 2));
      emit self->statusHintRequested(QString("正在平动中(目标距离: %1m)...").arg(std::abs(dist), 0, 'f', 2), "#00e676");
      int watchdogMs = static_cast<int>(std::clamp(durationSec, 2.0, 120.0) * 1000.0);
      self->m_motionWatchdog.start(watchdogMs);
    });
  } else {
    self->m_motionType = 2;
    const double angle = dlg->m_rotAngleSpin->value();
    const double rawSpeed = dlg->m_rotSpeedSpin->value();
    double speed = MotionConfigManager::instance().clampSignedAngularSpeedDeg(rawSpeed);
    self->m_targetAngle = std::abs(angle);

    double durationSec = (std::abs(speed) > 1e-3) ? ((std::abs(angle) / std::abs(speed)) + 1.5) : 4.0;

    emit self->statusHintRequested(QString("正在下发转动指令(角度: %1°)...").arg(std::abs(angle), 0, 'f', 1), "#00bcd4");

    ApiService::instance().robotNavigationMoveRotate(angle, speed, [self, angle, durationSec](int code, const QJsonObject &resp) {
      if (!self) return;
      qDebug() << "[MotionDialog] 转动指令响应:" << code << resp;

      const QString msg = resp.value("msg").toString();
      const bool ok = (code == 200 || msg.compare("ok", Qt::CaseInsensitive) == 0 || resp.value("code").toInt() == 200);
      if (!ok) {
        qWarning() << "[MotionDialog] 转动指令下发失败, code:" << code << resp;
        self->m_sendBtn->setEnabled(true);
        QString errMsg = msg.isEmpty() ? QString("错误码 %1").arg(code) : msg;
        self->m_statusLabel->setText(QString("转动指令失败: %1").arg(errMsg));
        self->m_statusLabel->setStyleSheet("color: #ef4444; font-weight: bold; font-size: 14px;");
        emit self->statusHintRequested(QString("转动指令下发失败: %1").arg(errMsg), "#ef4444");
        return;
      }

      self->setRunningState(true, false);
      emit self->motionStarted(2, std::abs(angle), QString("转动 %1°").arg(std::abs(angle), 0, 'f', 1));
      emit self->statusHintRequested(QString("正在转动中(目标角度: %1°)...").arg(std::abs(angle), 0, 'f', 1), "#00e676");
      int watchdogMs = static_cast<int>(std::clamp(durationSec, 2.0, 120.0) * 1000.0);
      self->m_motionWatchdog.start(watchdogMs);
    });
  }
}

void MotionDialogController::onCancelNavClicked(MotionDialog *dlg) {
  ApiService::instance().controlNavigation(QStringLiteral("stop navigation"), [](int, const QJsonObject &) {});
  dlg->setCanceledState();
  emit dlg->statusHintRequested("移动已取消", "#f59e0b");
}

void MotionDialogController::onPauseNavClicked(MotionDialog *dlg) {
  const RobotInfo &info = RobotInfoManager::instance().currentInfo();
  const bool isCurrentlyPaused = (dlg->m_isPaused || info.status == "2" || info.status == "5");

  if (isCurrentlyPaused) {
    ApiService::instance().controlNavigation(QStringLiteral("continue navigation"), [](int, const QJsonObject &) {});
    dlg->setRunningState(true, false);
    emit dlg->motionPaused(false);
    emit dlg->statusHintRequested("已恢复移动", "#00e676");
  } else {
    ApiService::instance().controlNavigation(QStringLiteral("pause navigation"), [](int, const QJsonObject &) {});
    dlg->setRunningState(true, true);
    emit dlg->motionPaused(true);
    emit dlg->statusHintRequested("移动已暂停", "#f59e0b");
  }
}

void MotionDialogController::onCurrentRobotInfoUpdated(MotionDialog *dlg, const RobotInfo &info) {
  const QString st = info.status;
  const bool isPaused = (st == "2" || st == "5" || (st != "0" && st != "1" && dlg->m_isPaused));

  if (dlg->m_statusLabel) {
    if (st == "2" || st == "5") {
      dlg->m_statusLabel->setText("暂停导航");
      dlg->m_statusLabel->setStyleSheet("color: #f59e0b; font-weight: bold; font-size: 14px;");
    } else if (st == "1") {
      dlg->m_statusLabel->setText("导航中");
      dlg->m_statusLabel->setStyleSheet("color: #0ea5e9; font-weight: bold; font-size: 14px;");
    } else if (st == "0") {
      dlg->m_statusLabel->setText("就绪");
      dlg->m_statusLabel->setStyleSheet("color: #22c55e; font-weight: bold; font-size: 14px;");
    } else {
      dlg->m_statusLabel->setText(info.statusText());
      dlg->m_statusLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 14px;").arg(info.statusColor()));
    }
  }

  const bool isNav = (st == "1" || st == "2" || st == "5" || st == "3");
  const bool isUserPreempted = TeleopController::instance().isUserPreempted() || TeleopController::instance().isActive();
  const bool canControlNav = isNav && !isUserPreempted;

  if (dlg->m_cancelBtn) dlg->m_cancelBtn->setEnabled(canControlNav);
  if (dlg->m_pauseBtn) {
    dlg->m_pauseBtn->setEnabled(canControlNav);
    if (isPaused) dlg->m_pauseBtn->setText("继续导航");
    else if (isNav) dlg->m_pauseBtn->setText("暂停导航");
    else dlg->m_pauseBtn->setText("暂无");
  }

  if (st == "2" || st == "5") dlg->m_isPaused = true;
  else if (st == "1") dlg->m_isPaused = false;

  if (!isNav && st == "0") {
    if (dlg->m_isRunning) {
      dlg->m_isRunning = false;
      dlg->m_isPaused = false;
      dlg->m_motionWatchdog.stop();
    }
    if (dlg->m_sendBtn) dlg->m_sendBtn->setEnabled(true);
  }

  if (!dlg->m_isRunning || dlg->m_isPaused) return;

  if (dlg->m_hasStartPose && info.hasPose) {
    if (dlg->m_motionType == 1) {
      double dx = info.car_x - dlg->m_startX;
      double dy = info.car_y - dlg->m_startY;
      double moved = std::hypot(dx, dy);
      bool speedZero = (std::abs(info.actual_vel.vx) < 0.02 && std::abs(info.actual_vel.vy) < 0.02);

      if (moved >= dlg->m_targetDistance * 0.96 && speedZero) {
        dlg->setCompletedState();
        return;
      }
    } else if (dlg->m_motionType == 2) {
      double currentDeg = info.car_yaw * 180.0 / M_PI;
      double dAngle = std::abs(currentDeg - dlg->m_startAngle);
      while (dAngle > 180.0) dAngle = std::abs(360.0 - dAngle);
      bool speedZero = (std::abs(info.actual_vel.wz) < 0.03);

      if (dAngle >= dlg->m_targetAngle * 0.95 && speedZero) {
        dlg->setCompletedState();
        return;
      }
    }
  }
}

void MotionDialogController::onMotionTimeout(MotionDialog *dlg) {
  if (dlg->m_isRunning) {
    dlg->setCompletedState();
  }
}
