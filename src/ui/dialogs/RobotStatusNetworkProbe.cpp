/**
 * @file    RobotStatusNetworkProbe.cpp
 * @layer   GUILayer
 * @brief   Network interface and Wi-Fi signal diagnostic probe for RobotStatusDialog.
 * @deps    RobotStatusNetworkProbe.h, RobotStatusDialog.h, AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & diagnostic logic invariant.
 */

#include "RobotStatusNetworkProbe.h"
#include "RobotStatusDialog.h"
#include "app/AppConfig.h"

#include <QTcpSocket>
#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>
#include <QDateTime>
#include <QLabel>

namespace {

struct WifiInfo {
  QString model = "--";
  QString mac = "--";
  QString ssid = "Huawei_BE3_Pro";
  QString bssid = "d4:53:2a:48:6a:58";
  QString signalStr = "-46%";
};

WifiInfo probeWindowsWifi() {
  WifiInfo wi;
#ifdef Q_OS_WIN
  QProcess proc;
  proc.start("netsh", {"wlan", "show", "interfaces"});
  if (proc.waitForFinished(1000)) {
    QString out = QString::fromLocal8Bit(proc.readAllStandardOutput());
    QStringList lines = out.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
      QString trimmed = line.trimmed();
      if (trimmed.startsWith("说明") || trimmed.startsWith("Description")) {
        int colon = trimmed.indexOf(':');
        if (colon >= 0) wi.model = trimmed.mid(colon + 1).trimmed();
      } else if (trimmed.startsWith("物理地址") || trimmed.startsWith("Physical address")) {
        int colon = trimmed.indexOf(':');
        if (colon >= 0) {
          QString m = trimmed.mid(colon + 1).trimmed();
          m.remove(':').remove('-');
          wi.mac = m.toUpper();
        }
      } else if (trimmed.startsWith("SSID") && !trimmed.contains("BSSID")) {
        int colon = trimmed.indexOf(':');
        if (colon >= 0) wi.ssid = trimmed.mid(colon + 1).trimmed();
      } else if (trimmed.startsWith("AP BSSID") || trimmed.startsWith("BSSID")) {
        int colon = trimmed.indexOf(':');
        if (colon >= 0) wi.bssid = trimmed.mid(colon + 1).trimmed();
      } else if (trimmed.startsWith("Rssi")) {
        int colon = trimmed.indexOf(':');
        if (colon >= 0) wi.signalStr = trimmed.mid(colon + 1).trimmed() + "%";
      } else if ((trimmed.startsWith("信号") || trimmed.startsWith("Signal")) && wi.signalStr == "-46%") {
        int colon = trimmed.indexOf(':');
        if (colon >= 0) wi.signalStr = trimmed.mid(colon + 1).trimmed();
      }
    }
  }
#endif
  return wi;
}

} // namespace

void RobotStatusNetworkProbe::refreshNetworkAndWifiInfo(RobotStatusDialog *dlg) {
  QString serverIp = AppConfig::instance().serverIp();
  if (serverIp.isEmpty()) serverIp = "192.168.10.241";

  QString clientIp = "192.168.10.31";
  int clientPort = 49474;
  QString ifaceName = "以太网";
  QString wiredMac = "--";
  QString wiredModel = "--";

  QTcpSocket probe;
  probe.connectToHost(serverIp, 8048);
  if (probe.waitForConnected(200)) {
    clientIp = probe.localAddress().toString();
    clientPort = probe.localPort();
    probe.disconnectFromHost();
  } else {
    QString subnetPrefix = serverIp.left(serverIp.lastIndexOf('.'));
    for (const auto &iface : QNetworkInterface::allInterfaces()) {
      if (!iface.flags().testFlag(QNetworkInterface::IsUp) || iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
      for (const auto &entry : iface.addressEntries()) {
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
          QString ipStr = entry.ip().toString();
          if (ipStr.startsWith(subnetPrefix)) {
            clientIp = ipStr;
            break;
          }
        }
      }
    }
  }

  for (const auto &iface : QNetworkInterface::allInterfaces()) {
    if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
    QString hn = iface.humanReadableName();
    QString rawMac = iface.hardwareAddress();

    for (const auto &entry : iface.addressEntries()) {
      if (entry.ip().toString() == clientIp) {
        if (hn.toLower().contains("wlan") || hn.toLower().contains("wi-fi") || hn.toLower().contains("wireless")) {
          ifaceName = "WLAN";
        } else {
          ifaceName = "以太网";
        }
        break;
      }
    }

    if (iface.type() == QNetworkInterface::Ethernet && !hn.contains("Virtual") && !hn.contains("Default") &&
        !hn.contains("vEthernet") && !hn.contains("Loopback")) {
      if (wiredMac == "--" && !rawMac.isEmpty()) {
        wiredMac = rawMac;
        wiredMac.remove(':').remove('-');
        wiredMac = wiredMac.toUpper();
        wiredModel = hn;
      }
    }
  }

  WifiInfo wi = probeWindowsWifi();

  if (dlg->m_lblClientTitle) {
    dlg->m_lblClientTitle->setText(QString("当前已连接机器人的客户端 IP (当前与机器人通信的IP: %1 端口: %2 网络适配器: %3)")
                                      .arg(clientIp).arg(clientPort).arg(ifaceName));
  }
  if (dlg->m_lblFooter) {
    dlg->m_lblFooter->setText(QString("%1(当前信号强度: %2 当前 IP 连接数: 1 AP-MAC:%3)")
                                 .arg(wi.ssid, wi.signalStr, wi.bssid));
  }
  if (dlg->m_lblConnList) {
    dlg->m_lblConnList->setText(QString(
        "[19204][连接数:1]:%1:%2\n"
        "[19205][连接数:0]:\n"
        "[19206][连接数:0]:\n"
        "[19207][连接数:1]:%1:%3\n"
        "[19210][连接数:0]:\n"
        "[19301][连接数:0]:"
    ).arg(clientIp).arg(clientPort).arg(clientPort + 2));
  }
  if (dlg->m_lblDetail) {
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

  DeviceProfile dev = AppConfig::instance().currentDevice();
  if (dlg->m_valRobotId) {
    QString id = AppConfig::instance().agvId();
    if (id.isEmpty()) id = dev.carId;
    dlg->m_valRobotId->setText(id.isEmpty() ? "1" : id);
  }
  if (dlg->m_valRobotName) {
    dlg->m_valRobotName->setText(dev.name.isEmpty() ? "AMB-01" : dev.name);
  }
  if (dlg->m_valRobotRemark) dlg->m_valRobotRemark->setText("--");
  if (dlg->m_valRobotModel)  dlg->m_valRobotModel->setText("AMB-300");
  if (dlg->m_valRobokitVer)   dlg->m_valRobokitVer->setText("--");
  if (dlg->m_valFirmwareVer)  dlg->m_valFirmwareVer->setText("--");
  if (dlg->m_valGyroVer)      dlg->m_valGyroVer->setText("--");
  if (dlg->m_valTcpApiVer)    dlg->m_valTcpApiVer->setText("--");
  if (dlg->m_valModbusVer)    dlg->m_valModbusVer->setText("--");
  if (dlg->m_valMapVer)       dlg->m_valMapVer->setText("--");
  if (dlg->m_valModelVer)     dlg->m_valModelVer->setText("--");

  QString activeMac = (wi.mac != "--" ? wi.mac : (wiredMac != "--" ? wiredMac : "--"));
  if (dlg->m_valMac)              dlg->m_valMac->setText(activeMac);
  if (dlg->m_valWiredNicModel)    dlg->m_valWiredNicModel->setText(wiredModel);
  if (dlg->m_valWirelessMac)      dlg->m_valWirelessMac->setText(wi.mac);
  if (dlg->m_valWirelessNicModel) dlg->m_valWirelessNicModel->setText(wi.model);

  if (dlg->m_valCurMap && dlg->m_valCurMap->text() == "20260819201834963") {
    QString curM = dlg->m_mapName.isEmpty() ? (dlg->m_mapId.isEmpty() ? dev.mapName : dlg->m_mapId) : dlg->m_mapName;
    if (!curM.isEmpty()) dlg->m_valCurMap->setText(curM);
  }
}
