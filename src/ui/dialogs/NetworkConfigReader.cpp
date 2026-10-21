/**
 * @file    NetworkConfigReader.cpp
 * @layer   GUILayer
 * @brief   Remote execution routine for reading Ubuntu network parameters via SSH.
 * @deps    NetworkConfigReader.h, NetworkConfigDialog.h, NetworkConfigSshRunner.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "NetworkConfigReader.h"
#include "NetworkConfigDialog.h"
#include "NetworkConfigSshRunner.h"

#include <QApplication>
#include <QMessageBox>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace {

const char *kPyInspectScript = R"py(
import socket, struct, subprocess, re, json

def get_default_iface():
    try:
        out = subprocess.check_output("ip route", shell=True).decode()
        for line in out.splitlines():
            if line.startswith("default"):
                parts = line.split()
                if "dev" in parts:
                    return parts[parts.index("dev") + 1]
    except: pass
    return ""

def get_ifaces():
    try:
        out = subprocess.check_output("ip -o link show", shell=True).decode()
        names = []
        for line in out.splitlines():
            m = re.search(r"^[0-9]+:\s+([^:@]+)", line)
            if m and m.group(1) != "lo":
                names.append(m.group(1).strip())
        return names
    except: return []

ifaces = get_ifaces()
def_iface = get_default_iface() or (ifaces[0] if ifaces else "eth0")

ip, mask, gw = "", "255.255.255.0", ""
try:
    addr_out = subprocess.check_output(f"ip -4 addr show {def_iface}", shell=True).decode()
    m = re.search(r"inet\s+([0-9.]+)/([0-9]+)", addr_out)
    if m:
        ip = m.group(1)
        prefix = int(m.group(2))
        mask_int = (0xffffffff << (32 - prefix)) & 0xffffffff
        mask = socket.inet_ntoa(struct.pack("!I", mask_int))
except: pass

try:
    out = subprocess.check_output("ip route", shell=True).decode()
    for line in out.splitlines():
        if line.startswith("default") and def_iface in line:
            parts = line.split()
            if "via" in parts:
                gw = parts[parts.index("via") + 1]
                break
        elif line.startswith("default") and not gw:
            parts = line.split()
            if "via" in parts:
                gw = parts[parts.index("via") + 1]
except: pass

res = {"status": "success", "iface": def_iface, "ifaces": ifaces, "ip": ip, "mask": mask, "gateway": gw}
print("JSON_START" + json.dumps(res) + "JSON_END")
)py";

} // namespace

void NetworkConfigReader::readConfig(NetworkConfigDialog *dlg) {
  if (dlg->m_isBusy) return;

  QString host = dlg->m_hostEdit->text().trimmed();
  if (!NetworkConfigSshRunner::validateIPv4(dlg, host, "SSH 主机 IP")) return;

  int port = dlg->m_portEdit->text().trimmed().toInt();
  if (port <= 0 || port > 65535) port = 22;

  QString user = dlg->m_userEdit->text().trimmed();
  QString pwd = dlg->m_pwdEdit->text();

  if (user.isEmpty()) {
    QMessageBox::warning(dlg, "输入错误", "请输入 SSH 用户名 (如 tuju)");
    return;
  }

  dlg->m_isBusy = true;
  dlg->m_readBtn->setEnabled(false);
  dlg->m_applyBtn->setEnabled(false);
  dlg->m_statusLabel->setText("正在连接 AGV 并读取网络状态...");
  dlg->m_statusLabel->setStyleSheet("color:#63b3ed; font-weight:600;");

  dlg->appendLog(QString(">>> 开始连接 SSH: %1@%2:%3 ...").arg(user, host).arg(port), "#63b3ed");

  QString b64 = QString::fromUtf8(kPyInspectScript).toUtf8().toBase64();
  QString remoteCmd = QString("python3 -c \"import base64; "
                              "exec(base64.b64decode('%1').decode('utf-8'))\"").arg(b64);

  qApp->processEvents();

  QString stdOut, stdErr;
  bool ok = NetworkConfigSshRunner::runSshCommand(host, port, user, pwd, remoteCmd, stdOut, stdErr, 12000);

  dlg->m_isBusy = false;
  dlg->m_readBtn->setEnabled(true);
  dlg->m_applyBtn->setEnabled(true);

  if (!ok) {
    dlg->appendLog("SSH 执行失败: " + (stdErr.isEmpty() ? "未知错误，请检查网络和账号密码" : stdErr), "#f56565");
    dlg->m_statusLabel->setText("连接失败");
    dlg->m_statusLabel->setStyleSheet("color:#f56565; font-weight:600;");
    QMessageBox::critical(
        dlg, "SSH 连接失败",
        QString("无法连接至 AGV (%1:%2)\n\n错误信息: %3\n\n请检查：\n1. 目标 IP 与 SSH 端口是否连通\n2. 用户名或密码是否正确")
            .arg(host).arg(port).arg(stdErr));
    return;
  }

  int startIdx = stdOut.indexOf("JSON_START");
  int endIdx = stdOut.indexOf("JSON_END");

  if (startIdx >= 0 && endIdx > startIdx) {
    QString jsonStr = stdOut.mid(startIdx + 10, endIdx - (startIdx + 10));
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    QJsonObject obj = doc.object();

    QString curIface = obj.value("iface").toString();
    QJsonArray ifacesArr = obj.value("ifaces").toArray();
    QString curIp = obj.value("ip").toString();
    QString curMask = obj.value("mask").toString();
    QString curGw = obj.value("gateway").toString();

    dlg->m_ifaceCombo->clear();
    for (const auto &v : ifacesArr) {
      dlg->m_ifaceCombo->addItem(v.toString());
    }
    if (dlg->m_ifaceCombo->findText(curIface) >= 0) {
      dlg->m_ifaceCombo->setCurrentText(curIface);
    } else if (!curIface.isEmpty()) {
      dlg->m_ifaceCombo->insertItem(0, curIface);
      dlg->m_ifaceCombo->setCurrentIndex(0);
    }

    if (!curIp.isEmpty()) dlg->m_ipEdit->setText(curIp);
    if (!curMask.isEmpty()) dlg->m_maskEdit->setText(curMask);
    if (!curGw.isEmpty()) dlg->m_gwEdit->setText(curGw);

    dlg->appendLog(QString("✓ 成功读取 Ubuntu 网络状态:\n  • 网卡: %1\n  • 当前 IP: "
                          "%2\n  • 子网掩码: %3\n  • 默认网关: %4")
                      .arg(curIface, curIp, curMask, curGw), "#48bb78");

    dlg->m_statusLabel->setText(QString("已成功连接至 %1 (%2)").arg(host, curIface));
    dlg->m_statusLabel->setStyleSheet("color:#48bb78; font-weight:600;");

    dlg->m_appliedUser = user;
    dlg->m_appliedPwd = pwd;
    dlg->m_appliedPort = port;
    emit dlg->sshCredentialsUpdated(user, pwd, port);
  } else {
    dlg->appendLog("读取成功但解析回显异常: " + stdOut, "#f6ad55");
    dlg->m_statusLabel->setText("回显解析异常");
  }
}
