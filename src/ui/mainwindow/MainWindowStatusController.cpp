/**
 * @file    MainWindowStatusController.cpp
 * @layer   GUI
 * @brief   Implementation of status bar synchronization, telemetry labels, and latency probes.
 * @deps    MainWindowStatusController.h, MainWindow.h, AppConfig.h, MqttService.h, RobotInfoManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#include "MainWindowStatusController.h"
#include "MainWindow.h"
#include "app/AppConfig.h"
#include "network/MqttService.h"
#include "core/RobotInfoManager.h"
#include "core/TeleopController.h"
#include "ui/map/MapViewWidget.h"
#include "ui/common/Toast.h"
#include "MainWindowStyle.h"
#include <QStatusBar>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QTcpSocket>
#include <QPointer>
#include <QDateTime>
#include <QStyle>
#include <QEvent>

MainWindowStatusController::MainWindowStatusController(MainWindow *window)
    : QObject(window)
    , m_window(window)
{
}

void MainWindowStatusController::setupStatusBar()
{
    if (!m_window) return;
    QStatusBar *sb = m_window->statusBar();

    DeviceProfile dev = AppConfig::instance().currentDevice();

    m_sbDevice = new QLabel(QString("设备: %1").arg(dev.name.isEmpty() ? "未连接" : dev.name));
    m_sbCarId  = new QLabel(QString("CarID: %1").arg(dev.carId.isEmpty() ? "--" : dev.carId));
    m_sbConn   = new QLabel("● MQTT: 连接中...");
    m_sbConn->setCursor(Qt::PointingHandCursor);
    m_sbConn->setToolTip("点击重新连接 MQTT");
    m_sbConn->installEventFilter(m_window);

    m_sbCov       = new QLabel("置信度: --");
    m_sbLatency   = new QLabel("网络延迟: --");
    m_sbTeleop    = new QLabel("无人抢占控制权");
    m_sbLocStatus = new QLabel("定位状态: 定位失败");
    m_sbMapStatus = new QLabel("地图加载: 未加载");

    sb->addWidget(m_sbDevice);
    sb->addWidget(m_sbCarId);
    sb->addWidget(m_sbConn);
    sb->addWidget(m_sbCov);
    sb->addWidget(m_sbLatency);
    sb->addWidget(m_sbTeleop);
    sb->addWidget(m_sbLocStatus);
    sb->addWidget(m_sbMapStatus);

    QString ipText = dev.ip.isEmpty() ? "--" : dev.ip;
    sb->addPermanentWidget(new QLabel(QString("IP: %1").arg(ipText)));

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindowStatusController::updateStatusBar);
    m_statusTimer->start(3000);

    connect(&MqttService::instance(), &MqttService::connected,    this, &MainWindowStatusController::updateStatusBar);
    connect(&MqttService::instance(), &MqttService::disconnected, this, &MainWindowStatusController::updateStatusBar);
    connect(&MqttService::instance(), &MqttService::stateChanged, this, &MainWindowStatusController::updateStatusBar);
    connect(&RobotInfoManager::instance(), &RobotInfoManager::currentRobotInfoUpdated,
            this, &MainWindowStatusController::updateStatusBar);

    updateStatusBar();

    connect(&TeleopController::instance(), &TeleopController::activeChanged,
            this, &MainWindowStatusController::onTeleopActiveChanged);
}

void MainWindowStatusController::updateDeviceProfile()
{
    const DeviceProfile dev = AppConfig::instance().currentDevice();
    if (m_sbDevice) m_sbDevice->setText(QString("设备: %1").arg(dev.name));
    if (m_sbCarId)  m_sbCarId->setText(QString("CarID: %1").arg(dev.carId));
}

void MainWindowStatusController::updateStatusBar()
{
    updateMqttStatusLabel();
    updateTelemetryLabels(RobotInfoManager::instance().currentInfo());
    testNetworkLatency();
}

void MainWindowStatusController::onMqttStatusClicked()
{
    updateStatusBar();
    if (MqttService::instance().isConnected()) {
        Toast::info(m_window, "MQTT 已连接");
        return;
    }

    const QString ip = AppConfig::instance().serverIp();
    if (ip.isEmpty()) {
        Toast::error(m_window, "未配置服务器地址，无法重连");
        return;
    }

    if (m_window) {
        m_window->connectServices();
    }
    updateStatusBar();
    Toast::info(m_window, "正在重新连接 MQTT...");
}

void MainWindowStatusController::onTeleopActiveChanged(bool active)
{
    if (m_teleopBtn) {
        m_teleopBtn->setObjectName(active ? "teleopBtnActive" : "");
        m_teleopBtn->style()->unpolish(m_teleopBtn);
        m_teleopBtn->style()->polish(m_teleopBtn);
    }
}

bool MainWindowStatusController::handleEventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_sbConn && event->type() == QEvent::MouseButtonRelease) {
        onMqttStatusClicked();
        return true;
    }
    return false;
}

void MainWindowStatusController::updateMqttStatusLabel()
{
    if (!m_sbConn) return;
    const QMqttClient::ClientState st = MqttService::instance().state();
    if (st == QMqttClient::Connected) {
        m_sbConn->setText("● MQTT: 已连接");
        m_sbConn->setStyleSheet(MainWindowStyle::kMqttConnected);
    } else if (st == QMqttClient::Connecting) {
        m_sbConn->setText("● MQTT: 连接中...");
        m_sbConn->setStyleSheet(MainWindowStyle::kMqttConnecting);
    } else {
        m_sbConn->setText("○ MQTT: 未连接");
        m_sbConn->setStyleSheet(MainWindowStyle::kMqttDisconnected);
    }
}

void MainWindowStatusController::updateTelemetryLabels(const RobotInfo &info)
{
    if (m_sbCov) {
        if (info.covScore > 0.0) {
            const double cs = (info.covScore > 1.0) ? info.covScore / 100.0 : info.covScore;
            int pct = qRound(cs * 100.0);
            if (pct <= 0) pct = 1;
            m_sbCov->setText(QString("置信度: <span style='color:#22c55e;'>%1%</span>").arg(pct));
        } else if (info.covScore == 0.0) {
            m_sbCov->setText("置信度: <span style='color:#ef4444;'>0%</span>");
        } else {
            m_sbCov->setText("置信度: <span style='color:#8898a8;'>--</span>");
        }
    }

    if (m_sbLatency) {
        const bool isConnected = MqttService::instance().isConnected();
        const int lat = (isConnected && info.latency >= 0) ? info.latency : (isConnected ? m_networkLatency : -1);
        if (lat >= 0) {
            const QString col = (lat < 60) ? "#22c55e" : ((lat < 150) ? "#eab308" : "#ef4444");
            m_sbLatency->setText(QString("网络延迟: <span style='color:%1;'>%2 ms</span>").arg(col).arg(lat));
        } else {
            m_sbLatency->setText("网络延迟: <span style='color:#8898a8;'>--</span>");
        }
    }

    if (m_sbTeleop) {
        const bool isTeleop = TeleopController::instance().isActive();
        m_sbTeleop->setText(isTeleop ? "<span style='color:#f87171;'>抢占控制权</span>"
                                     : "<span style='color:#38bdf8;'>无人抢占控制权</span>");
    }

    if (m_sbLocStatus) {
        const bool locSuccess = (info.covScore > 0.0);
        m_sbLocStatus->setText(locSuccess ? "定位状态: <span style='color:#22c55e;'>定位成功</span>"
                                          : "定位状态: <span style='color:#ef4444;'>定位失败</span>");
    }

    if (m_sbMapStatus) {
        auto *mapPage = m_window ? m_window->mapPage() : nullptr;
        if (mapPage) {
            m_sbMapStatus->setText(QString("地图加载: <span style='color:%1;'>%2</span>")
                                      .arg(mapPage->mapStatusColor(), mapPage->mapStatusText()));
        } else {
            m_sbMapStatus->setText("地图加载: <span style='color:#8898a8;'>未加载</span>");
        }
    }
}

void MainWindowStatusController::testNetworkLatency()
{
    if (!MqttService::instance().isConnected()) {
        m_networkLatency = -1;
        if (m_sbLatency) {
            m_sbLatency->setText("网络延迟: <span style='color:#8898a8;'>--</span>");
        }
        return;
    }

    const RobotInfo &info = RobotInfoManager::instance().currentInfo();
    if (info.latency >= 0) {
        m_networkLatency = info.latency;
        return;
    }

    const QString ip = AppConfig::instance().serverIp();
    if (ip.isEmpty()) return;

    static qint64 s_lastProbeMs = 0;
    static bool s_probing = false;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (s_probing || (m_networkLatency >= 0 && (now - s_lastProbeMs < 3000))) {
        return;
    }
    s_lastProbeMs = now;
    s_probing = true;

    auto *socket = new QTcpSocket(this);
    const qint64 startTime = now;
    QPointer<MainWindowStatusController> self(this);

    // 设定连接超时定时器 (1500ms)，防止挂起
    QTimer::singleShot(1500, socket, [socket]() {
        if (socket && socket->state() != QAbstractSocket::ConnectedState) {
            s_probing = false;
            socket->abort();
            socket->deleteLater();
        }
    });

    connect(socket, &QTcpSocket::connected, this, [self, socket, startTime]() {
        const int elapsed = static_cast<int>(QDateTime::currentMSecsSinceEpoch() - startTime);
        s_probing = false;
        if (self) {
            self->m_networkLatency = qMax(1, elapsed);
            if (self->m_sbLatency) {
                const QString col = (self->m_networkLatency < 60) ? "#22c55e" : ((self->m_networkLatency < 150) ? "#eab308" : "#ef4444");
                self->m_sbLatency->setText(QString("网络延迟: <span style='color:%1;'>%2 ms</span>").arg(col).arg(self->m_networkLatency));
            }
        }
        socket->disconnectFromHost();
        socket->deleteLater();
    });

    connect(socket, &QTcpSocket::errorOccurred, this, [socket](QAbstractSocket::SocketError) {
        s_probing = false;
        socket->deleteLater();
    });

    socket->connectToHost(ip, 8048);
}
