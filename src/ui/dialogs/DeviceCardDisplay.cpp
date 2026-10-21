/**
 * @file    DeviceCardDisplay.cpp
 * @layer   GUILayer
 * @brief   Implementation of DeviceCard visual styling and field synchronization.
 * @deps    DeviceCardDisplay.h, DeviceCard.h, DeviceCardWidgets.h, AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "DeviceCardDisplay.h"
#include "DeviceCard.h"
#include "DeviceCardWidgets.h"
#include "AppConfig.h"

void DeviceCardDisplay::setOnlineUI(DeviceCard *card, bool online)
{
    card->m_isOnline = online;
    if (online) {
        card->setStyleSheet(
            "QFrame#deviceCard {"
            "  background: #252a35;"
            "  border: 2px solid #0091da;"
            "  border-radius: 8px;"
            "}"
            "QFrame#deviceCard:hover {"
            "  background: #28303e;"
            "  border-color: #00b4d8;"
            "}");
        if (card->m_hdrWidget) {
            card->m_hdrWidget->setStyleSheet("background: #0091da; border-top-left-radius: 6px; border-top-right-radius: 6px;");
        }
        if (card->m_shadow) {
            card->m_shadow->setColor(QColor(0, 145, 218, 80));
            card->m_shadow->setBlurRadius(20);
            card->m_shadow->setOffset(0, 4);
        }
        if (card->m_onlineDot) {
            card->m_onlineDot->setStyleSheet("background: #44db22; border-radius: 5px;");
        }
        if (card->m_onlineLabel) {
            card->m_onlineLabel->setText(card->m_statusText.isEmpty() ? "已连接" : card->m_statusText);
            card->m_onlineLabel->setStyleSheet("color: #ffffff; font-size: 12px; font-weight: bold; background: transparent;");
        }
        if (card->m_robotIcon) {
            card->m_robotIcon->setPixmap(DeviceCardWidgets::makeRobotHeadIcon(26, QColor("#00e5ff")));
        }
        if (card->m_ipLabel) {
            card->m_ipLabel->setStyleSheet("color: #ffffff; font-size: 13px; font-weight: bold; font-family: Consolas,monospace; background: transparent;");
        }
        if (card->m_typeTag) {
            card->m_typeTag->setStyleSheet("color: #00e5ff; font-size: 11px; font-weight: bold; background: transparent; border: 1.2px solid #00e5ff; border-radius: 2px; padding: 1px 5px;");
        }
        if (card->m_baseIcon) {
            card->m_baseIcon->setPixmap(DeviceCardWidgets::makeBaseStationIcon(26, 20, QColor("#00e5ff")));
        }
    } else {
        card->setStyleSheet(
            "QFrame#deviceCard {"
            "  background: #1f232c;"
            "  border: 1.5px solid #475569;"
            "  border-radius: 8px;"
            "}"
            "QFrame#deviceCard:hover {"
            "  background: #242934;"
            "  border-color: #64748b;"
            "}");
        if (card->m_hdrWidget) {
            card->m_hdrWidget->setStyleSheet("background: #2d3544; border-top-left-radius: 6px; border-top-right-radius: 6px;");
        }
        if (card->m_shadow) {
            card->m_shadow->setColor(QColor(0, 0, 0, 160));
            card->m_shadow->setBlurRadius(16);
            card->m_shadow->setOffset(0, 4);
        }
        if (card->m_onlineDot) {
            card->m_onlineDot->setStyleSheet("background: #718096; border-radius: 5px;");
        }
        if (card->m_onlineLabel) {
            card->m_onlineLabel->setText(card->m_statusText.isEmpty() ? "未连接" : card->m_statusText);
            card->m_onlineLabel->setStyleSheet("color: #94a3b8; font-size: 12px; font-weight: bold; background: transparent;");
        }
        if (card->m_robotIcon) {
            card->m_robotIcon->setPixmap(DeviceCardWidgets::makeRobotHeadIcon(26, QColor("#718096")));
        }
        if (card->m_ipLabel) {
            card->m_ipLabel->setStyleSheet("color: #94a3b8; font-size: 13px; font-weight: bold; font-family: Consolas,monospace; background: transparent;");
        }
        if (card->m_typeTag) {
            card->m_typeTag->setStyleSheet("color: #718096; font-size: 11px; font-weight: bold; background: transparent; border: 1.2px solid #4a5568; border-radius: 2px; padding: 1px 5px;");
        }
        if (card->m_baseIcon) {
            card->m_baseIcon->setPixmap(DeviceCardWidgets::makeBaseStationIcon(26, 20, QColor("#4a5568")));
        }
        card->m_confidence = -1.0;
        card->m_battery = -1;
        card->m_latency = -1;
    }
    updateCardDisplay(card);
}

void DeviceCardDisplay::updateCardDisplay(DeviceCard *card)
{
    QString displayName = card->m_profile.name.trimmed();
    if (displayName.isEmpty()) {
        displayName = card->m_profile.carId.isEmpty() ? "未注册设备" : QString("AGV-%1").arg(card->m_profile.carId);
    }
    if (displayName.length() > 14) {
        card->m_nameLabel->setText(displayName.left(13) + "…");
        card->m_nameLabel->setToolTip(displayName);
    } else {
        card->m_nameLabel->setText(displayName);
        card->m_nameLabel->setToolTip(displayName);
    }

    QString mapName = card->m_profile.mapName.trimmed();
    if (mapName.startsWith("地图 ID:", Qt::CaseInsensitive)) {
        QString mid = mapName.mid(QString("地图 ID:").length()).trimmed();
        QString realName = DeviceCardWidgets::lookupMapName(mid);
        mapName = realName.isEmpty() ? mid : realName;
        card->m_profile.mapName = mapName;
    } else {
        bool ok = false;
        mapName.toInt(&ok);
        if (ok) {
            QString realName = DeviceCardWidgets::lookupMapName(mapName);
            if (!realName.isEmpty()) {
                mapName = realName;
                card->m_profile.mapName = realName;
            }
        }
    }

    if (mapName.isEmpty()) {
        card->m_mapLabel->setText("--");
    } else {
        if (mapName.length() > 14) {
            card->m_mapLabel->setText(mapName.left(13) + "…");
            card->m_mapLabel->setToolTip(mapName);
        } else {
            card->m_mapLabel->setText(mapName);
            card->m_mapLabel->setToolTip(mapName);
        }
    }

    if (card->m_remarkLabel) {
        card->m_remarkLabel->setText(card->m_remark.trimmed());
    }

    QString ver = card->m_profile.version.trimmed();
    if (ver.isEmpty() || ver == "未知" || ver.startsWith("v3.4.8")) {
        ver = "v2.1.1";
    }
    card->m_verLabel->setText(ver);

    QString fwVer = card->m_fwVersion.trimmed();
    if (fwVer.isEmpty() || fwVer == "未知" || fwVer.startsWith("v22A")) {
        fwVer = "v2.1.1";
    }
    card->m_fwVerLabel->setText(fwVer);

    if (card->m_typeTag) {
        card->m_typeTag->setText("潜伏底盘");
    }

    if (card->m_confBadge) {
        if (card->m_confidence >= 0.0) {
            card->m_confBadge->setText(QString::number(card->m_confidence, 'f', 2));
            card->m_confBadge->setStyleSheet("background:#ff6600; color:#ffffff; font-size:12px; font-weight:bold; border-radius:3px; padding:0 8px; min-width:42px;");
        } else {
            card->m_confBadge->setText("--");
            card->m_confBadge->setStyleSheet("background:#374151; color:#9ca3af; font-size:12px; font-weight:bold; border-radius:3px; padding:0 8px; min-width:42px;");
        }
    }

    if (card->m_latencyLabel) {
        if (card->m_latency >= 0) {
            QString latStr = QString("%1ms").arg(card->m_latency);
            if (card->m_latencyLabel->text() != latStr) {
                card->m_latencyLabel->setText(latStr);
                card->m_latencyLabel->setStyleSheet("color:#44db22; font-size:12px; font-weight:bold; font-family:Consolas,monospace; background:transparent;");
            }
        } else {
            if (card->m_latencyLabel->text() != "--") {
                card->m_latencyLabel->setText("--");
                card->m_latencyLabel->setStyleSheet("color:#64748b; font-size:12px; font-weight:bold; font-family:Consolas,monospace; background:transparent;");
            }
        }
    }

    if (card->m_batteryBadge) {
        card->m_batteryBadge->setBattery(card->m_battery);
    }

    bool isConnected = (AppConfig::instance().currentDeviceIndex() == card->m_index);
    if (card->m_syncBtn) {
        if (isConnected) {
            card->m_syncBtn->setText("✓ 已同步");
            card->m_syncBtn->setStyleSheet(
                "QPushButton{"
                "  background:#1e3a5f; color:#38bdf8; border:1px solid #0284c7;"
                "  border-radius:3px; padding:4px 10px; font-size:11px; font-weight:bold;"
                "}");
        } else {
            card->m_syncBtn->setText("未同步");
            card->m_syncBtn->setStyleSheet(
                "QPushButton{"
                "  background:#333842; color:#94a3b8; border:1px solid #4a5160;"
                "  border-radius:3px; padding:4px 10px; font-size:11px; font-weight:bold;"
                "}");
        }
    }

    if (card->m_connectBtn) {
        if (card->m_connectBtn->graphicsEffect()) {
            card->m_connectBtn->setGraphicsEffect(nullptr);
        }
        card->m_connectBtn->setVisible(true);
        card->m_connectBtn->setEnabled(true);
        if (isConnected) {
            card->m_connectBtn->setText("断开连接");
            card->m_connectBtn->setStyleSheet(
                "QPushButton#deviceCardConnectBtn{"
                "  background:#333842; color:#f87171; border:1px solid #7f1d1d;"
                "  border-radius:3px; padding:4px 12px; font-size:11px; font-weight:bold;"
                "}"
                "QPushButton#deviceCardConnectBtn:hover{background:#450a0a; color:#fca5a5;}"
                "QPushButton#deviceCardConnectBtn:pressed{background:#2b0505;}");
        } else {
            card->m_connectBtn->setText("连接");
            card->m_connectBtn->setStyleSheet(
                "QPushButton#deviceCardConnectBtn{"
                "  background:#0284c7; color:#ffffff; border:1px solid #0369a1;"
                "  border-radius:3px; padding:4px 14px; font-size:11px; font-weight:bold;"
                "}"
                "QPushButton#deviceCardConnectBtn:hover{background:#0ea5e9;}"
                "QPushButton#deviceCardConnectBtn:pressed{background:#0369a1;}");
        }
        card->m_connectBtn->update();
    }
}
