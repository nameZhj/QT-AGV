/**
 * @file    DeviceCardProbe.cpp
 * @layer   GUILayer
 * @brief   Implementation of network latency probing and telemetry polling for DeviceCard.
 * @deps    DeviceCardProbe.h, DeviceCard.h, AppConfig.h, RobotInfoManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "DeviceCardProbe.h"
#include "DeviceCard.h"
#include "AppConfig.h"
#include "RobotInfoManager.h"
#include <QTcpSocket>
#include <QPointer>
#include <QTimer>
#include <QDateTime>

void DeviceCardProbe::testLatency(DeviceCard *card, const QString &ip)
{
    if (!card || ip.isEmpty() || !card->m_isOnline || (AppConfig::instance().currentDeviceIndex() != card->m_index)) {
        return;
    }
    if (card->m_isProbingLatency) {
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (card->m_latency >= 0 && (now - card->m_lastLatencyProbeMs < 3000)) {
        return;
    }
    card->m_lastLatencyProbeMs = now;
    card->m_isProbingLatency = true;

    auto *socket = new QTcpSocket(card);
    const qint64 startTime = now;
    QPointer<DeviceCard> pCard(card);

    QTimer::singleShot(1500, socket, [socket, pCard]() {
        if (socket && socket->state() != QAbstractSocket::ConnectedState) {
            if (pCard) pCard->m_isProbingLatency = false;
            socket->abort();
            socket->deleteLater();
        }
    });

    QObject::connect(socket, &QTcpSocket::connected, socket, [socket, pCard, startTime]() {
        const int elapsed = static_cast<int>(QDateTime::currentMSecsSinceEpoch() - startTime);
        if (pCard && pCard->m_isOnline && (AppConfig::instance().currentDeviceIndex() == pCard->m_index)) {
            pCard->m_latency = qMax(1, elapsed);
            pCard->updateCardDisplay();
        }
        if (pCard) pCard->m_isProbingLatency = false;
        socket->disconnectFromHost();
        socket->deleteLater();
    });

    QObject::connect(socket, &QTcpSocket::errorOccurred, socket, [socket, pCard](QAbstractSocket::SocketError) {
        if (pCard) pCard->m_isProbingLatency = false;
        socket->deleteLater();
    });

    socket->connectToHost(ip, 8048);
}

void DeviceCardProbe::probeDeviceStatus(DeviceCard *card, const QString &ip)
{
    if (!card) return;
    if (ip.isEmpty()) {
        card->disconnectDevice();
        return;
    }

    bool isCurrent = (AppConfig::instance().currentDeviceIndex() == card->m_index);

    if (isCurrent) {
        card->m_isBusy = false;
        card->setOnlineUI(true);
        const RobotInfo &info = RobotInfoManager::instance().currentInfo();
        if (info.isOnline()) {
            card->m_battery = info.batteryPercent();
            card->m_confidence = info.covScore;
            card->m_statusText = info.statusText();
            if (info.latency >= 0) {
                card->m_latency = info.latency;
            }
        } else {
            if (card->m_statusText.isEmpty() || card->m_statusText == "未连接") {
                card->m_statusText = "已连接";
            }
        }
        card->updateCardDisplay();

        if (!card->m_latencyTimer) {
            card->m_latencyTimer = new QTimer(card);
            QObject::connect(card->m_latencyTimer, &QTimer::timeout, card, [card]() {
                if (card && card->m_isOnline && (AppConfig::instance().currentDeviceIndex() == card->m_index) && !card->m_profile.ip.isEmpty()) {
                    DeviceCardProbe::testLatency(card, card->m_profile.ip);
                }
            });
        }
        if (!card->m_latencyTimer->isActive()) {
            card->m_latencyTimer->start(3000);
        }

        if (card->m_latency < 0) {
            DeviceCardProbe::testLatency(card, ip);
        }
        return;
    }

    if (card->m_isBusy) {
        return;
    }

    card->disconnectDevice();
}
