/**
 * @file    RobotStatusDialog.cpp
 * @layer   GUILayer
 * @brief   Implementation of real-time hardware status inspection dialog.
 * @deps    RobotStatusDialog.h, RobotStatusStyle.h, RobotStatusUiFactory.h, RobotStatusNetworkProbe.h, RobotStatusTelemetrySync.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "RobotStatusDialog.h"
#include "RobotStatusStyle.h"
#include "RobotStatusUiFactory.h"
#include "RobotStatusNetworkProbe.h"
#include "RobotStatusTelemetrySync.h"
#include "core/RobotInfoManager.h"
#include "network/MqttService.h"

#include <QScrollArea>
#include <QTabWidget>
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QTimer>

RobotStatusDialog::RobotStatusDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    m_startTimeMs = QDateTime::currentMSecsSinceEpoch();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &RobotStatusDialog::onPeriodicRefresh);
    m_timer->start(2000);

    initMqttSubscriptions();
    refreshNetworkAndWifiInfo();

    connect(&RobotInfoManager::instance(), &RobotInfoManager::currentRobotInfoUpdated,
            this, &RobotStatusDialog::updateFromRobotInfo);
    updateFromRobotInfo(RobotInfoManager::instance().currentInfo());
}

RobotStatusDialog::~RobotStatusDialog()
{
    if (m_timer) {
        m_timer->stop();
    }
}

void RobotStatusDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    updateFromRobotInfo(RobotInfoManager::instance().currentInfo());
    refreshNetworkAndWifiInfo();
}

void RobotStatusDialog::setupUi()
{
    setWindowTitle("运行状态");
    setMinimumSize(980, 680);
    resize(1060, 740);
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    setStyleSheet(RobotStatusStyle::kDialogStyle);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 10, 12, 12);
    rootLayout->setSpacing(8);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createBasicInfoTab(), "基本信息");
    m_tabWidget->addTab(RobotStatusUiFactory::createPlaceholderTab("运行曲线"), "运行曲线");
    m_tabWidget->addTab(RobotStatusUiFactory::createPlaceholderTab("PGV"), "PGV");
    m_tabWidget->addTab(RobotStatusUiFactory::createPlaceholderTab("定位信息"), "定位信息");
    m_tabWidget->addTab(RobotStatusUiFactory::createPlaceholderTab("所有网卡信息"), "所有网卡信息");
    m_tabWidget->addTab(RobotStatusUiFactory::createPlaceholderTab("Driver info"), "Driver info");
    m_tabWidget->addTab(RobotStatusUiFactory::createPlaceholderTab("所有数据"), "所有数据");

    rootLayout->addWidget(m_tabWidget);
}

#include "RobotStatusBasicInfoPanel.h"

QWidget *RobotStatusDialog::createBasicInfoTab()
{
    return RobotStatusBasicInfoPanel::createBasicInfoTab(this);
}

void RobotStatusDialog::initMqttSubscriptions()
{
    MqttService::instance().subscribeTopic(QStringLiteral("/rob/1/info"), 0,
        [this](const QString &topic, const QByteArray &payload) {
            onMqttMessage(topic, payload);
        });
}

void RobotStatusDialog::setMapInfo(const QString &mapId, const QString &mapName)
{
    m_mapId = mapId;
    m_mapName = mapName;
    if (m_valCurMap) {
        QString disp = m_mapName.isEmpty() ? (m_mapId.isEmpty() ? "--" : m_mapId) : m_mapName;
        m_valCurMap->setText(disp);
    }
    if (m_valMapLoadStatus) {
        m_valMapLoadStatus->setText(mapId.isEmpty() ? "未加载地图" : "加载地图成功");
    }
}

void RobotStatusDialog::updateFromRobotInfo(const RobotInfo &info)
{
    RobotStatusTelemetrySync::updateFromRobotInfo(this, info);
}

void RobotStatusDialog::updateFromRobotState(const QString &carId, double x, double y, double yawRad,
                                            double speed, double battery, double covScore, bool posValid)
{
    RobotStatusTelemetrySync::updateFromRobotState(this, carId, x, y, yawRad, speed, battery, covScore, posValid);
}

void RobotStatusDialog::onMqttMessage(const QString &topic, const QByteArray &payload)
{
    RobotStatusTelemetrySync::onMqttMessage(this, topic, payload);
}

void RobotStatusDialog::refreshNetworkAndWifiInfo()
{
    RobotStatusNetworkProbe::refreshNetworkAndWifiInfo(this);
}

void RobotStatusDialog::onPeriodicRefresh()
{
    RobotStatusTelemetrySync::onPeriodicRefresh(this);
}
