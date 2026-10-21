/**
 * @file    NetworkConfigApplier.cpp
 * @layer   GUILayer
 * @brief   Remote execution routine for applying and testing Ubuntu network configuration changes.
 * @deps    NetworkConfigApplier.h, NetworkConfigDialog.h, NetworkConfigSshRunner.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "NetworkConfigApplier.h"
#include "NetworkConfigDialog.h"
#include "NetworkConfigSshRunner.h"

#include <QApplication>
#include <QMessageBox>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QElapsedTimer>
#include <QThread>

namespace {

const char *kPyApplyTemplate = R"py(
import sys, os, subprocess, socket, struct, glob

iface = "%1"
ip = "%2"
mask = "%3"
gw = "%4"

def mask_to_prefix(netmask):
    try:
        return bin(struct.unpack("!I", socket.inet_aton(netmask))[0]).count("1")
    except:
        return 24

prefix = mask_to_prefix(mask)

# 1. 自愈修复：清除/恢复之前可能被错误写成 systemd-networkd 的 netplan 配置，确保桌面 NetworkManager 托管
netplan_dir = "/etc/netplan"
if os.path.exists(netplan_dir):
    try:
        for bak_f in glob.glob(os.path.join(netplan_dir, "*.bak")):
            orig_f = bak_f[:-4]
            if not os.path.exists(orig_f):
                try: os.rename(bak_f, orig_f)
                except: pass
            else:
                try: os.remove(bak_f)
                except: pass

        agv_yaml = os.path.join(netplan_dir, "01-agv-network.yaml")
        if os.path.exists(agv_yaml):
            try: os.remove(agv_yaml)
            except: pass

        modified_yaml = False
        for yf in glob.glob(os.path.join(netplan_dir, "*.yaml")):
            try:
                with open(yf, "r") as fp:
                    content = fp.read()
                if "renderer: networkd" in content or "renderer:  networkd" in content:
                    content = content.replace("renderer: networkd", "renderer: NetworkManager").replace("renderer:  networkd", "renderer: NetworkManager")
                    with open(yf, "w") as fp:
                        fp.write(content)
                    modified_yaml = True
            except: pass

        if not glob.glob(os.path.join(netplan_dir, "*.yaml")):
            nm_default = os.path.join(netplan_dir, "01-network-manager-all.yaml")
            with open(nm_default, "w") as fp:
                fp.write("# Let NetworkManager manage all devices on this system\nnetwork:\n  version: 2\n  renderer: NetworkManager\n")
            os.chmod(nm_default, 0o600)
            modified_yaml = True

        if modified_yaml:
            subprocess.run(["netplan", "apply"], timeout=8)
    except Exception as e:
        print("Netplan heal error:", e)

# 2. 核心使用 NetworkManager (nmcli) 进行网卡静态 IP 配置与即时生效
applied_nm = False
try:
    con_name = ""
    res = subprocess.run(["nmcli", "-t", "-f", "NAME,DEVICE", "connection", "show"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if res.returncode == 0:
        for line in res.stdout.decode().splitlines():
            parts = line.split(":")
            if len(parts) >= 2 and parts[1].strip() == iface:
                con_name = parts[0].strip()
                break

    if not con_name:
        res_all = subprocess.run(["nmcli", "-t", "-f", "NAME,DEVICE", "con", "show"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if res_all.returncode == 0:
            for line in res_all.stdout.decode().splitlines():
                parts = line.split(":")
                if len(parts) >= 1 and (parts[0].strip() == iface or (len(parts) >= 2 and parts[1].strip() == iface)):
                    con_name = parts[0].strip()
                    break

    if not con_name:
        con_name = f"Wired-{iface}"
        subprocess.run(["nmcli", "con", "add", "type", "ethernet", "ifname", iface, "con-name", con_name], timeout=8)

    cmd = ["nmcli", "con", "mod", con_name,
           "ipv4.method", "manual",
           "ipv4.addresses", f"{ip}/{prefix}"]
    if gw:
        cmd.extend(["ipv4.gateway", gw])
    else:
        cmd.extend(["ipv4.gateway", ""])
    cmd.extend(["ipv4.dns", "114.114.114.114 8.8.8.8"])

    subprocess.run(cmd, timeout=8)
    subprocess.run(["nmcli", "con", "up", con_name], timeout=8)
    subprocess.run(["nmcli", "device", "reapply", iface], timeout=5)
    applied_nm = True
except Exception as e:
    print("nmcli error:", e)

# 3. 兜底即时生效保障
os.system(f"ip addr replace {ip}/{prefix} dev {iface} 2>/dev/null")
if gw: os.system(f"ip route replace default via {gw} dev {iface} 2>/dev/null")

print("CONFIG_APPLY_SUCCESS")
)py";

bool testConnectivityWithRetries(NetworkConfigDialog *dlg, const QString &newIp, int port) {
  const int maxRetries = 6;
  for (int attempt = 1; attempt <= maxRetries; ++attempt) {
    dlg->appendLog(QString("正在测试目标 IP [%1] 连通性 (第 %2/%3 次尝试)...")
                      .arg(newIp).arg(attempt).arg(maxRetries), "#cbd5e1");
    qApp->processEvents();

    if (NetworkConfigSshRunner::checkIpConnectivity(newIp, port, 1500)) {
      return true;
    }

    if (attempt < maxRetries) {
      QElapsedTimer timer;
      timer.start();
      while (timer.elapsed() < 1000) {
        qApp->processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(50);
      }
    }
  }
  return false;
}

} // namespace

void NetworkConfigApplier::applyConfig(NetworkConfigDialog *dlg) {
  if (dlg->m_isBusy) return;

  QString host = dlg->m_hostEdit->text().trimmed();
  int port = dlg->m_portEdit->text().trimmed().toInt();
  if (port <= 0 || port > 65535) port = 22;
  QString user = dlg->m_userEdit->text().trimmed();
  QString pwd = dlg->m_pwdEdit->text();

  QString newIface = dlg->m_ifaceCombo->currentText().trimmed();
  QString newIp = dlg->m_ipEdit->text().trimmed();
  QString newMask = dlg->m_maskEdit->text().trimmed();
  QString newGw = dlg->m_gwEdit->text().trimmed();

  if (newIface.isEmpty()) {
    QMessageBox::warning(dlg, "输入错误", "请选择或指定网卡接口名称 (如 eth0)");
    return;
  }
  if (!NetworkConfigSshRunner::validateIPv4(dlg, newIp, "新 IP 地址")) return;
  if (!NetworkConfigSshRunner::validateIPv4(dlg, newMask, "子网掩码")) return;
  if (!newGw.isEmpty() && !NetworkConfigSshRunner::validateIPv4(dlg, newGw, "默认网关")) return;

  QString confirmMsg =
      QString("您确定要修改 AGV 机器人的网络配置吗？\n\n"
              "• 目标主机: %1 (接口: %2)\n"
              "• 新 IP 地址: %3\n"
              "• 子网掩码: %4\n"
              "• 默认网关: %5\n\n"
              "⚠ 注意：网络配置写入并生效后，原 IP 连线将立即断开！\n"
              "后续需要使用新 IP [%3] 连接设备。是否继续？")
          .arg(host, newIface, newIp, newMask, newGw.isEmpty() ? "(无)" : newGw);

  if (QMessageBox::question(dlg, "确认修改 AGV 网络配置", confirmMsg,
                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
    return;
  }

  dlg->m_isBusy = true;
  dlg->m_readBtn->setEnabled(false);
  dlg->m_applyBtn->setEnabled(false);
  dlg->m_statusLabel->setText("正在下发并应用网络配置...");
  dlg->m_statusLabel->setStyleSheet("color:#63b3ed; font-weight:600;");

  dlg->appendLog(QString(">>> 正在下发网络配置至 Ubuntu [%1]: IP=%2, 掩码=%3, 网关=%4 ...")
                    .arg(newIface, newIp, newMask, newGw), "#63b3ed");

  QString pyApplyScript = QString(kPyApplyTemplate).arg(newIface, newIp, newMask, newGw);
  QString b64 = pyApplyScript.toUtf8().toBase64();
  QString safePwd = pwd;
  safePwd.replace("'", "'\"'\"'");
  QString remoteCmd = QString("echo '%1' | sudo -S python3 -c \"import base64; "
                              "exec(base64.b64decode('%2').decode('utf-8'))\"").arg(safePwd, b64);

  qApp->processEvents();

  QString stdOut, stdErr;
  bool ok = NetworkConfigSshRunner::runSshCommand(host, port, user, pwd, remoteCmd, stdOut, stdErr, 12000);

  bool isSuccess = stdOut.contains("CONFIG_APPLY_SUCCESS");
  if (!ok && (stdErr.contains("Connection reset") ||
              stdErr.contains("Connection closed") ||
              stdErr.contains("Broken pipe"))) {
    isSuccess = true;
  }

  if (!isSuccess && !stdErr.isEmpty() &&
      (stdErr.contains("Permission denied") ||
       stdErr.contains("Authentication failed") || stdErr.contains("sudo: "))) {
    dlg->m_isBusy = false;
    dlg->m_readBtn->setEnabled(true);
    dlg->m_applyBtn->setEnabled(true);
    dlg->appendLog("网络配置下发失败: " + (stdErr.isEmpty() ? stdOut : stdErr), "#f56565");
    dlg->m_statusLabel->setText("应用失败");
    dlg->m_statusLabel->setStyleSheet("color:#f56565; font-weight:600;");
    QMessageBox::critical(
        dlg, "应用配置失败",
        QString("下发配置至 AGV 时发生错误。\n\n回显信息: %1\n%2\n\n请检查密码 (vsis1234) 是否具备 sudo 权限。")
            .arg(stdOut, stdErr));
    return;
  }

  dlg->appendLog(QString(">>> 网络配置已下发，正在检测目标 IP [%1] 的网络连通性...").arg(newIp), "#63b3ed");
  dlg->m_statusLabel->setText(QString("正在测试目标 IP [%1] 连通性...").arg(newIp));
  dlg->m_statusLabel->setStyleSheet("color:#63b3ed; font-weight:600;");

  bool targetConnected = testConnectivityWithRetries(dlg, newIp, port);

  dlg->m_isBusy = false;
  dlg->m_readBtn->setEnabled(true);
  dlg->m_applyBtn->setEnabled(true);

  if (targetConnected) {
    dlg->appendLog(QString("🎉 恭喜！目标 IP [%1] 连通性测试通过，网络配置已成功生效！").arg(newIp), "#48bb78");
    dlg->m_statusLabel->setText(QString("修改成功且目标 IP 已连通: %1").arg(newIp));
    dlg->m_statusLabel->setStyleSheet("color:#48bb78; font-weight:600;");

    dlg->m_appliedIp = newIp;
    dlg->m_ipChanged = (dlg->m_originalIp != newIp);
    dlg->m_appliedUser = user;
    dlg->m_appliedPwd = pwd;
    dlg->m_appliedPort = port;
    emit dlg->sshCredentialsUpdated(user, pwd, port);

    QMessageBox::information(
        dlg, "网络配置成功",
        QString("AGV 网络配置已成功下发并应用，且目标 IP [%1] 连通性测试通过！\n\n新网络参数：\n• IP 地址: %1\n• 子网掩码: %2\n• 默认网关: %3\n\n设备列表将自动更新。")
            .arg(newIp, newMask, newGw));

    emit dlg->networkUpdated(dlg->m_originalIp, newIp);
    dlg->accept();
  } else {
    dlg->appendLog(QString("❌ 目标 IP [%1] 连通性测试失败！无法与目标 IP 建立网络连接。").arg(newIp), "#f56565");
    dlg->m_statusLabel->setText(QString("目标 IP [%1] 连通失败").arg(newIp));
    dlg->m_statusLabel->setStyleSheet("color:#f56565; font-weight:600;");
    QMessageBox::warning(
        dlg, "连通性测试失败",
        QString("网络配置已下发，但目标 IP [%1] 连通性测试未通过！\n\n可能原因：\n1. 本机与目标 IP 不在同一网段或没有可达路由；\n2. 目标 Ubuntu 系统网卡重载较慢或尚未恢复就绪；\n3. 输入的 IP/掩码/网关存在冲突或格式错误。\n\n请检查本机网络配置，或在命令行使用 ping %1 进行排查。")
            .arg(newIp));
  }
}
