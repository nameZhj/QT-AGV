/**
 * @file    MapViewNavInteraction.cpp
 * @layer   GUI
 * @brief   Path navigation and motion modal interaction implementation.
 * @deps    MapViewNavInteraction.h, ApiService.h, TeleopController.h, MotionConfigManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewNavInteraction.h"
#include "app/AppConfig.h"
#include "network/ApiService.h"
#include "core/TeleopController.h"
#include "core/MotionConfigManager.h"
#include "core/RobotInfoManager.h"
#include "ui/dialogs/MotionDialog.h"
#include <QMessageBox>
#include <QPushButton>
#include <QDateTime>
#include <QDebug>

MapViewNavInteraction::MapViewNavInteraction(QObject *parent)
    : QObject(parent)
{
}

void MapViewNavInteraction::handleStationNavClick(const QString &ptId, const QString &ptType, const QString &ptName,
                                                 const QPointF &wPos, QWidget *parent)
{
    QString name = ptName.isEmpty() ? ptId : ptName;
    QString typeCN = (ptType == "charge") ? "充电点" : (ptType == "highPrecision" ? "精定位点" : "普通站点");

    QMessageBox navBox(parent);
    navBox.setWindowTitle("路径导航");
    navBox.setIcon(QMessageBox::Question);
    navBox.setText(QString("是否导航到 “%1”？").arg(name));
    navBox.setInformativeText(QString("点位类型: %1\n点位编号: %2\n目标坐标: (%3, %4)")
                              .arg(typeCN, ptId).arg(wPos.x(), 0, 'f', 2).arg(wPos.y(), 0, 'f', 2));
    QPushButton *startBtn = navBox.addButton("导航到 " + name, QMessageBox::AcceptRole);
    navBox.addButton("取消", QMessageBox::RejectRole);
    navBox.setDefaultButton(startBtn);

    navBox.exec();
    if (navBox.clickedButton() != startBtn) return;

    emit statusHint(QString("正在下发导航指令至点位 \"%1\"...").arg(name), "#00bcd4");
    m_navActive = true;
    m_navStartTime = QDateTime::currentMSecsSinceEpoch();
    m_currentOpType = OpNavigation;
    m_currentNavTargetName = name;
    emit navButtonsUpdateRequested();

    ApiService::instance().navigateTo(ptId, [this, name](int code, const QJsonObject &resp) {
        QString msg = resp.value("msg").toString();
        if (code == 200 || msg.compare("ok", Qt::CaseInsensitive) == 0 || resp.value("code").toInt() == 200) {
            m_navActive = true;
            m_navPaused = false;
            m_navEnteredRunning = false;
            m_navStartTime = QDateTime::currentMSecsSinceEpoch();
            m_currentOpType = OpNavigation;
            m_currentNavTargetName = name;
            emit navButtonsUpdateRequested();
            emit statusHint(QString("正在导航至点位 \"%1\"...").arg(name), "#00e676");
        } else {
            m_navActive = false;
            m_currentOpType = OpNone;
            m_currentNavTargetName.clear();
            emit navButtonsUpdateRequested();
            emit statusHint(QString("导航下发失败: %1").arg(msg.isEmpty() ? "请求失败" : msg), "#ef4444");
        }
    });
}

void MapViewNavInteraction::handleFreeMoveClick(const QPointF &worldPos, double curWx, double curWy, QWidget *parent)
{
    QMessageBox moveBox(parent);
    moveBox.setWindowTitle("自由移动");
    moveBox.setIcon(QMessageBox::Question);
    moveBox.setText(QString("是否移动至目标坐标 (%1, %2)？").arg(worldPos.x(), 0, 'f', 2).arg(worldPos.y(), 0, 'f', 2));
    QPushButton *startBtn = moveBox.addButton("开始移动", QMessageBox::AcceptRole);
    moveBox.addButton("取消", QMessageBox::RejectRole);
    moveBox.setDefaultButton(startBtn);

    moveBox.exec();
    if (moveBox.clickedButton() != startBtn) return;

    int actionId = static_cast<int>(QDateTime::currentMSecsSinceEpoch() % 1000000 + 1);
    double speed = TeleopController::instance().baseLinearSpeed();
    if (speed <= 0.01) speed = MotionConfigManager::instance().maxSpeed();
    speed = MotionConfigManager::instance().clampLinearSpeed(speed);

    const QString targetDesc = QString("自由移动至 (%1, %2)").arg(worldPos.x(), 0, 'f', 2).arg(worldPos.y(), 0, 'f', 2);
    emit statusHint(QString("正在下发移动指令至 (%1, %2)...").arg(worldPos.x(), 0, 'f', 2).arg(worldPos.y(), 0, 'f', 2), "#00bcd4");
    m_navActive = true;
    m_navStartTime = QDateTime::currentMSecsSinceEpoch();
    m_currentOpType = OpMotion;
    m_currentMotionDesc = targetDesc;
    emit navButtonsUpdateRequested();

    ApiService::instance().navigateToPose(curWx, curWy, worldPos.x(), worldPos.y(), speed, actionId, [this, worldPos, targetDesc](int code, const QJsonObject &resp) {
        if (code == 200) {
            QString msg = resp.value("data").toObject().value("msg").toString("ok");
            if (msg == "failed") {
                QString err = resp.value("data").toObject().value("error_info").toString();
                m_navActive = false;
                m_currentOpType = OpNone;
                m_currentMotionDesc.clear();
                emit navButtonsUpdateRequested();
                emit statusHint(QString("控制器拒绝移动: %1").arg(err), "#ef4444");
            } else {
                m_navActive = true;
                m_navPaused = false;
                m_navEnteredRunning = false;
                m_navStartTime = QDateTime::currentMSecsSinceEpoch();
                m_currentOpType = OpMotion;
                m_currentMotionDesc = targetDesc;
                emit navButtonsUpdateRequested();
                emit statusHint(QString("正在移动至 (%1, %2)...").arg(worldPos.x(), 0, 'f', 2).arg(worldPos.y(), 0, 'f', 2), "#00e676");
            }
        } else {
            m_navActive = false;
            m_currentOpType = OpNone;
            m_currentMotionDesc.clear();
            emit navButtonsUpdateRequested();
            emit statusHint(QString("移动请求失败: %1").arg(resp.value("msg").toString("控制器未响应")), "#ef4444");
        }
    });
}

void MapViewNavInteraction::cancelNavigation()
{
    ApiService::instance().controlNavigation(QStringLiteral("stop navigation"), [](int code, const QJsonObject &resp) {
        qDebug() << "[MapViewNavInteraction] controlNavigation stop navigation:" << code << resp;
    });

    m_navActive = false;
    m_navPaused = false;
    m_navEnteredRunning = false;
    emit navButtonsUpdateRequested();

    if (m_currentOpType == OpMotion) emit statusHint("移动已取消", "#f59e0b");
    else                             emit statusHint("已取消导航", "#f59e0b");

    m_currentOpType = OpNone;
    m_currentNavTargetName.clear();
    m_currentMotionDesc.clear();
}

void MapViewNavInteraction::pauseOrResumeNavigation()
{
    const RobotInfo &info = RobotInfoManager::instance().currentInfo();
    const bool isPaused = (m_navPaused || info.status == "2" || info.status == "5");

    if (isPaused) {
        ApiService::instance().controlNavigation(QStringLiteral("continue navigation"), [](int, const QJsonObject &) {});
        m_navPaused = false;
        emit navButtonsUpdateRequested();
        emit statusHint(m_currentOpType == OpMotion ? "已恢复移动" : "已继续导航", "#00e676");
    } else {
        ApiService::instance().controlNavigation(QStringLiteral("pause navigation"), [](int, const QJsonObject &) {});
        m_navPaused = true;
        emit navButtonsUpdateRequested();
        emit statusHint(m_currentOpType == OpMotion ? "移动已暂停" : "导航已暂停", "#f59e0b");
    }
}

void MapViewNavInteraction::toggleMoveMode(QWidget *parent)
{
    if (!m_motionDialog) {
        m_motionDialog = new MotionDialog(parent);
        connect(m_motionDialog, &MotionDialog::statusHintRequested, this, [this](const QString &msg, const QString &color) {
            emit statusHint(msg, color);
        });
        connect(m_motionDialog, &MotionDialog::motionStarted, this, [this](int, double, const QString &desc) {
            m_currentOpType = OpMotion;
            m_currentMotionDesc = desc;
            m_navActive = true;
            m_navStartTime = QDateTime::currentMSecsSinceEpoch();
            emit navButtonsUpdateRequested();
        });
        connect(m_motionDialog, &MotionDialog::motionCompleted, this, [this]() {
            emit statusHint(m_currentMotionDesc.isEmpty() ? "移动已完成，设备就绪" : QString("%1 已完成，设备就绪").arg(m_currentMotionDesc), "#00e676");
            m_currentOpType = OpNone;
            m_currentMotionDesc.clear();
            m_navActive = false;
            emit navButtonsUpdateRequested();
        });
        connect(m_motionDialog, &MotionDialog::motionCanceled, this, [this]() {
            emit statusHint("移动已取消", "#f59e0b");
            m_currentOpType = OpNone;
            m_currentMotionDesc.clear();
            m_navActive = false;
            emit navButtonsUpdateRequested();
        });
        connect(m_motionDialog, &MotionDialog::motionPaused, this, [this](bool paused) {
            m_navPaused = paused;
            emit statusHint(paused ? "移动已暂停" : "已恢复移动", paused ? "#f59e0b" : "#00e676");
            emit navButtonsUpdateRequested();
        });
    }
    m_motionDialog->show();
    m_motionDialog->raise();
    m_motionDialog->activateWindow();
}

void MapViewNavInteraction::updateRobotNavStatus(const QString &newStatus)
{
    const bool isRobotNavigating = (newStatus == "1" || newStatus == "2" || newStatus == "3" || newStatus == "5");
    if (m_navActive) {
        if (isRobotNavigating) {
            m_navEnteredRunning = true;
            m_navPaused = (newStatus == "2" || newStatus == "5");
        } else {
            qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_navStartTime;
            if ((m_navEnteredRunning && elapsed > 1500) || (!m_navEnteredRunning && elapsed > 15000)) {
                m_navActive = false;
                m_navEnteredRunning = false;
                m_navPaused = false;
            }
        }
    } else if (isRobotNavigating) {
        m_navActive = true;
        m_navEnteredRunning = true;
        m_navPaused = (newStatus == "2" || newStatus == "5");
    }

    if (m_lastRobotNavStatus != newStatus) {
        const QString oldStatus = m_lastRobotNavStatus;
        m_lastRobotNavStatus = newStatus;

        if (newStatus == "0") {
            if (oldStatus == "1" || oldStatus == "2" || oldStatus == "3" || oldStatus == "5") {
                if (m_currentOpType == OpNavigation) emit statusHint(m_currentNavTargetName.isEmpty() ? "导航已完成，设备就绪" : QString("已到达点位 \"%1\"，导航完成").arg(m_currentNavTargetName), "#00e676");
                else if (m_currentOpType == OpMotion) emit statusHint(m_currentMotionDesc.isEmpty() ? "移动已完成，设备就绪" : QString("%1 已完成，设备就绪").arg(m_currentMotionDesc), "#00e676");
                else emit statusHint("设备移动/导航已完成，就绪", "#00e676");
                m_currentOpType = OpNone;
                m_currentNavTargetName.clear();
                m_currentMotionDesc.clear();
            }
        } else if (newStatus == "1" || newStatus == "3") {
            if (oldStatus == "0" || oldStatus.isEmpty()) {
                if (m_currentOpType == OpNavigation) emit statusHint(QString("正在导航至点位 \"%1\"...").arg(m_currentNavTargetName.isEmpty() ? "目标点" : m_currentNavTargetName), "#00e676");
                else if (m_currentOpType == OpMotion) emit statusHint(QString("正在移动中 (%1)...").arg(m_currentMotionDesc.isEmpty() ? "移动" : m_currentMotionDesc), "#00e676");
                else emit statusHint("设备正在导航/移动中...", "#00e676");
            } else if (oldStatus == "2" || oldStatus == "5") {
                emit statusHint(m_currentOpType == OpNavigation ? "已继续导航" : (m_currentOpType == OpMotion ? "已恢复移动" : "已继续运行"), "#00e676");
            }
        } else if (newStatus == "2" || newStatus == "5") {
            emit statusHint(m_currentOpType == OpNavigation ? "导航已暂停" : (m_currentOpType == OpMotion ? "移动已暂停" : "设备运行已暂停"), "#f59e0b");
        } else if (newStatus == "4") {
            emit statusHint("停止导航，作业中", "#06b6d4");
        } else if (newStatus == "6") {
            emit statusHint("设备故障，请检查设备状态", "#ef4444");
        }
    }

    emit navButtonsUpdateRequested();
}
