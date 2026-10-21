/**
 * @file    DeviceManagerConnectionHandler.cpp
 * @layer   GUILayer
 * @brief   Implementation of connection actions and SSH config for DeviceManagerDialog.
 * @deps    DeviceManagerConnectionHandler.h, DeviceManagerDialog.h, NetworkConfigDialog.h, MqttService.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "DeviceManagerConnectionHandler.h"
#include "DeviceManagerDialog.h"
#include "DeviceCard.h"
#include "NetworkConfigDialog.h"
#include "AppConfig.h"
#include "MqttService.h"
#include "RobotInfoManager.h"
#include "Toast.h"
#include <QDateTime>

void DeviceManagerConnectionHandler::onConnectCard(DeviceManagerDialog *dlg, int index)
{
    if (!dlg) return;
    const QList<DeviceProfile> list = AppConfig::instance().deviceList();
    if (index < 0 || index >= list.size()) return;

    int oldIdx = AppConfig::instance().currentDeviceIndex();
    if (oldIdx >= 0 && oldIdx != index) {
        if (oldIdx < dlg->m_cards.size() && dlg->m_cards[oldIdx]) {
            dlg->m_cards[oldIdx]->disconnectDevice();
        }
    }

    AppConfig::instance().setCurrentDeviceIndex(index);
    AppConfig::instance().setServerIp(list[index].ip);
    QString effectiveCarId = list[index].carId.trimmed().isEmpty() ? "1" : list[index].carId.trimmed();
    AppConfig::instance().setAgvId(effectiveCarId);
    RobotInfoManager::instance().resetForCar(effectiveCarId);
    RobotInfoManager::instance().init();

    for (int i = 0; i < dlg->m_cards.size(); ++i) {
        if (!dlg->m_cards[i]) continue;
        if (i == index) {
            dlg->m_cards[i]->probeDeviceStatus(list[index].ip);
        } else {
            dlg->m_cards[i]->disconnectDevice();
        }
    }

    MqttService::instance().connectToHost(list[index].ip, 1883);

    QList<DeviceProfile> curList = AppConfig::instance().deviceList();
    if (index < curList.size()) {
        curList[index].lastConnected = QDateTime::currentDateTime();
        curList[index].linkStatus = "1";
        AppConfig::instance().saveDevice(curList[index]);
    }

    QString devName = list[index].name.isEmpty() ? list[index].ip : list[index].name;
    dlg->setStatus(QString("已连接设备 [%1] (%2)").arg(devName, list[index].ip), "#22c55e");
    Toast::success(dlg, QString("已连接设备 [%1]").arg(devName));
}

void DeviceManagerConnectionHandler::onDisconnectCard(DeviceManagerDialog *dlg, int index)
{
    if (!dlg) return;
    const QList<DeviceProfile> list = AppConfig::instance().deviceList();
    if (index < 0 || index >= list.size()) return;

    QString devName = list[index].name.isEmpty() ? list[index].ip : list[index].name;

    // 1. 重置当前活动设备索引为 -1，断开全局连接服务
    AppConfig::instance().setCurrentDeviceIndex(-1);
    MqttService::instance().disconnectFromHost();

    for (auto *c : dlg->m_cards) {
        if (c) {
            c->disconnectDevice();
        }
    }

    QList<DeviceProfile> curList = AppConfig::instance().deviceList();
    if (index < curList.size()) {
        curList[index].linkStatus = "0";
        AppConfig::instance().updateDevice(index, curList[index]);
    }

    dlg->setStatus(QString("设备 [%1] 已断开连接").arg(devName), "#e05555");
    Toast::info(dlg, QString("设备 [%1] 已断开连接").arg(devName));
}

void DeviceManagerConnectionHandler::onRemoveCard(DeviceManagerDialog *dlg, int index)
{
    if (!dlg) return;
    AppConfig::instance().removeDevice(index);
    dlg->refreshCards();
    dlg->setStatus("设备已移除");
    Toast::success(dlg, "设备已移除");
}

void DeviceManagerConnectionHandler::connectDevice(DeviceManagerDialog *dlg, int index)
{
    if (!dlg) return;
    const QList<DeviceProfile> list = AppConfig::instance().deviceList();
    if (index < 0 || index >= list.size()) return;

    if (AppConfig::instance().currentDeviceIndex() != index || !MqttService::instance().isConnected()) {
        onConnectCard(dlg, index);
    }
    dlg->accept();
}

void DeviceManagerConnectionHandler::onNetworkConfig(DeviceManagerDialog *dlg, int index)
{
    if (!dlg) return;
    const QList<DeviceProfile> list = AppConfig::instance().deviceList();
    if (index < 0 || index >= list.size()) return;
    DeviceProfile profile = list[index];

    NetworkConfigDialog configDlg(profile.ip,
                                  profile.name.isEmpty() ? profile.carId : profile.name,
                                  profile.sshUser,
                                  profile.sshPwd,
                                  profile.sshPort,
                                  dlg);

    QObject::connect(&configDlg, &NetworkConfigDialog::sshCredentialsUpdated, dlg, [index](const QString &user, const QString &pwd, int port) {
        QList<DeviceProfile> curList = AppConfig::instance().deviceList();
        if (index >= 0 && index < curList.size()) {
            curList[index].sshUser = user;
            curList[index].sshPwd  = pwd;
            curList[index].sshPort = port;
            AppConfig::instance().updateDevice(index, curList[index]);
        }
    });

    if (configDlg.exec() == QDialog::Accepted) {
        profile.sshUser = configDlg.sshUser();
        profile.sshPwd  = configDlg.sshPwd();
        profile.sshPort = configDlg.sshPort();

        if (configDlg.isIpChanged()) {
            QString oldIp = profile.ip;
            QString newIp = configDlg.newIp().trimmed();
            if (!newIp.isEmpty() && newIp != oldIp) {
                profile.ip = newIp;
                AppConfig::instance().updateDevice(index, profile);
                dlg->setStatus(QString("已更新设备 [%1] 的 IP 地址为 %2").arg(profile.name.isEmpty() ? profile.carId : profile.name, newIp), "#4caf7a");
                dlg->refreshCards();
                return;
            }
        }
        AppConfig::instance().updateDevice(index, profile);
    }
}
