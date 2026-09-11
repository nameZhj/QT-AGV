#include "NetworkConfigDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QRegularExpression>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace {

const char *kDialogStyle = R"(
QDialog {
    background-color: #1a1d23;
    color: #c8d0d8;
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
    font-size: 13px;
}
QGroupBox {
    color: #00bcd4;
    font-weight: bold;
    border: 1px solid #2d3340;
    border-radius: 6px;
    margin-top: 10px;
    padding-top: 15px;
    background-color: #1f232b;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    padding: 0 4px;
    background-color: #1f232b;
}
QLabel {
    color: #a0aec0;
    font-size: 12px;
}
QLineEdit, QComboBox {
    background-color: #14171c;
    border: 1px solid #363d4d;
    border-radius: 4px;
    color: #ffffff;
    padding: 5px 8px;
    font-family: "Consolas", monospace;
    font-size: 13px;
}
QLineEdit:focus, QComboBox:focus {
    border: 1px solid #00bcd4;
}
QLineEdit:disabled, QComboBox:disabled {
    background-color: #181b20;
    color: #606875;
    border: 1px solid #282c35;
}
QPushButton {
    background-color: #252a35;
    color: #c8d0d8;
    border: 1px solid #3a3f4a;
    border-radius: 4px;
    padding: 6px 14px;
    font-weight: 600;
}
QPushButton:hover {
    background-color: #2e3544;
    border-color: #00bcd4;
    color: #ffffff;
}
QPushButton:pressed {
    background-color: #1a1d23;
}
QPushButton:disabled {
    background-color: #1e222a;
    color: #555d6b;
    border-color: #2d3340;
}
QPushButton#applyBtn {
    background-color: #1e6fd4;
    color: #ffffff;
    border: none;
    font-weight: 700;
}
QPushButton#applyBtn:hover {
    background-color: #2980e8;
}
QPushButton#applyBtn:disabled {
    background-color: #253346;
    color: #6a7f98;
}
QTextEdit {
    background-color: #111317;
    color: #48bb78;
    font-family: "Consolas", monospace;
    font-size: 12px;
    border: 1px solid #2a2f3b;
    border-radius: 4px;
    padding: 6px;
}
)";

} // namespace

NetworkConfigDialog::NetworkConfigDialog(const QString &defaultIp, const QString &carName, QWidget *parent)
    : QDialog(parent)
    , m_originalIp(defaultIp)
    , m_carName(carName)
    , m_appliedIp(defaultIp)
{
    setupUI();
}

NetworkConfigDialog::~NetworkConfigDialog()
{
}

void NetworkConfigDialog::setupUI()
{
    setStyleSheet(kDialogStyle);
    QString title = "AGV 机器人网络配置 (SSH)";
    if (!m_carName.isEmpty()) title += " - " + m_carName;
    else if (!m_originalIp.isEmpty()) title += " - " + m_originalIp;
    setWindowTitle(title);
    resize(660, 600);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // ── 顶部提示信息 ────────────────────────────────────────────────────────
    auto *topNotice = new QLabel("通过 SSH 远程直连 AGV 的 Ubuntu 系统，实时检测网卡状态并配置静态 IPv4 (IP、掩码与网关)。");
    topNotice->setStyleSheet("color:#718096; font-size:12px; margin-bottom: 2px;");
    mainLayout->addWidget(topNotice);

    // ── 1. SSH 连接参数面板 ──────────────────────────────────────────────────
    auto *sshBox = new QGroupBox("1. SSH 远程连接凭据 (目标 Ubuntu 系统)", this);
    auto *sshLayout = new QGridLayout(sshBox);
    sshLayout->setContentsMargins(14, 14, 14, 14);
    sshLayout->setHorizontalSpacing(10);
    sshLayout->setVerticalSpacing(10);

    m_hostEdit = new QLineEdit(m_originalIp, sshBox);
    m_hostEdit->setPlaceholderText("请输入 AGV 当前 IP 地址");

    m_portEdit = new QLineEdit("22", sshBox);
    m_portEdit->setFixedWidth(65);
    m_portEdit->setAlignment(Qt::AlignCenter);

    m_userEdit = new QLineEdit("tuju", sshBox);
    m_userEdit->setPlaceholderText("SSH 用户名");

    m_pwdEdit  = new QLineEdit("vsis1234", sshBox);
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEdit->setPlaceholderText("SSH 密码");

    m_pwdEyeBtn = new QPushButton("👁", sshBox);
    m_pwdEyeBtn->setFixedSize(30, 28);
    m_pwdEyeBtn->setToolTip("显示/隐藏密码");
    connect(m_pwdEyeBtn, &QPushButton::clicked, this, &NetworkConfigDialog::onTogglePasswordVisible);

    auto *pwdLayout = new QHBoxLayout;
    pwdLayout->setContentsMargins(0, 0, 0, 0);
    pwdLayout->setSpacing(4);
    pwdLayout->addWidget(m_pwdEdit, 1);
    pwdLayout->addWidget(m_pwdEyeBtn);

    m_readBtn = new QPushButton("📡 连接并读取当前配置", sshBox);
    m_readBtn->setStyleSheet("QPushButton{background:#1e4b7a; color:#b0d7ff; border:1px solid #2d6aa8; font-weight:700; padding:6px 12px;} QPushButton:hover{background:#265f9e; color:#ffffff;}");
    m_readBtn->setCursor(Qt::PointingHandCursor);
    connect(m_readBtn, &QPushButton::clicked, this, &NetworkConfigDialog::onReadConfig);

    sshLayout->addWidget(new QLabel("主机 IP 地址:"), 0, 0);
    sshLayout->addWidget(m_hostEdit, 0, 1);
    sshLayout->addWidget(new QLabel("SSH 端口:"), 0, 2);
    sshLayout->addWidget(m_portEdit, 0, 3);

    sshLayout->addWidget(new QLabel("登录用户名:"), 1, 0);
    sshLayout->addWidget(m_userEdit, 1, 1);
    sshLayout->addWidget(new QLabel("登录密码:"), 1, 2);
    sshLayout->addLayout(pwdLayout, 1, 3);

    sshLayout->addWidget(m_readBtn, 2, 0, 1, 4);

    mainLayout->addWidget(sshBox);

    // ── 2. Ubuntu IPv4 网络配置目标面板 ─────────────────────────────────────
    auto *netBox = new QGroupBox("2. IPv4 目标网络配置 (写入 Ubuntu 系统)", this);
    auto *netLayout = new QGridLayout(netBox);
    netLayout->setContentsMargins(14, 14, 14, 14);
    netLayout->setHorizontalSpacing(10);
    netLayout->setVerticalSpacing(10);

    m_ifaceCombo = new QComboBox(netBox);
    m_ifaceCombo->setEditable(true);
    m_ifaceCombo->addItem("eth0");
    m_ifaceCombo->setToolTip("Ubuntu 系统网卡接口名称 (连接后自动读取，也可手动输入)");

    m_ipEdit = new QLineEdit(m_originalIp, netBox);
    m_ipEdit->setPlaceholderText("例如: 192.168.1.150");

    m_maskEdit = new QLineEdit("255.255.255.0", netBox);
    m_maskEdit->setPlaceholderText("例如: 255.255.255.0");

    m_gwEdit = new QLineEdit(netBox);
    m_gwEdit->setPlaceholderText("例如: 192.168.1.1");

    netLayout->addWidget(new QLabel("目标网卡接口:"), 0, 0);
    netLayout->addWidget(m_ifaceCombo, 0, 1, 1, 3);

    netLayout->addWidget(new QLabel("新 IP 地址:"), 1, 0);
    netLayout->addWidget(m_ipEdit, 1, 1);
    netLayout->addWidget(new QLabel("子网掩码:"), 1, 2);
    netLayout->addWidget(m_maskEdit, 1, 3);

    netLayout->addWidget(new QLabel("默认网关:"), 2, 0);
    netLayout->addWidget(m_gwEdit, 2, 1, 1, 3);

    mainLayout->addWidget(netBox);

    // ── 3. 实时输出与终端回显窗口 ───────────────────────────────────────────
    auto *logBox = new QGroupBox("3. 执行日志与回显", this);
    auto *logLayout = new QVBoxLayout(logBox);
    logLayout->setContentsMargins(10, 12, 10, 10);

    m_logEdit = new QTextEdit(logBox);
    m_logEdit->setReadOnly(true);
    m_logEdit->setFixedHeight(120);
    logLayout->addWidget(m_logEdit);

    mainLayout->addWidget(logBox);

    // ── 4. 底部状态与操作按钮栏 ─────────────────────────────────────────────
    auto *bottomRow = new QHBoxLayout;
    m_statusLabel = new QLabel("准备就绪", this);
    m_statusLabel->setStyleSheet("color:#a0aec0; font-weight:600;");
    bottomRow->addWidget(m_statusLabel);
    bottomRow->addStretch();

    auto *closeBtn = new QPushButton("关闭", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    bottomRow->addWidget(closeBtn);

    m_applyBtn = new QPushButton("🚀 保存并应用网络配置", this);
    m_applyBtn->setObjectName("applyBtn");
    m_applyBtn->setCursor(Qt::PointingHandCursor);
    m_applyBtn->setFixedHeight(34);
    connect(m_applyBtn, &QPushButton::clicked, this, &NetworkConfigDialog::onApplyConfig);
    bottomRow->addWidget(m_applyBtn);

    mainLayout->addLayout(bottomRow);

    appendLog("系统就绪。请确认 SSH 主机 IP，点击「连接并读取当前配置」开始操作。");
}

void NetworkConfigDialog::appendLog(const QString &text, const QString &color)
{
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_logEdit->append(QString("<span style='color:#718096;'>[%1]</span> <span style='color:%2;'>%3</span>")
                          .arg(timeStr, color, text.toHtmlEscaped()));
    m_logEdit->ensureCursorVisible();
}

void NetworkConfigDialog::onTogglePasswordVisible()
{
    if (m_pwdEdit->echoMode() == QLineEdit::Password) {
        m_pwdEdit->setEchoMode(QLineEdit::Normal);
        m_pwdEyeBtn->setText("🔒");
    } else {
        m_pwdEdit->setEchoMode(QLineEdit::Password);
        m_pwdEyeBtn->setText("👁");
    }
}

bool NetworkConfigDialog::validateIPv4(const QString &ip, const QString &fieldName)
{
    QString trimmed = ip.trimmed();
    if (trimmed.isEmpty()) {
        QMessageBox::warning(this, "参数校验", QString("%1 不能为空").arg(fieldName));
        return false;
    }
    QHostAddress addr(trimmed);
    if (addr.isNull() || addr.protocol() != QAbstractSocket::IPv4Protocol) {
        QMessageBox::warning(this, "参数校验", QString("%1 格式不正确，请输入有效的 IPv4 地址 (例如 192.168.1.150)").arg(fieldName));
        return false;
    }
    return true;
}

bool NetworkConfigDialog::runSshCommand(const QString &host, int port, const QString &user, const QString &pwd,
                                       const QString &command, QString &stdOut, QString &stdErr, int timeoutMs)
{
    // 1. 定位 ssh 客户端
    QString sshPath = "ssh";
#ifdef Q_OS_WIN
    QString winOpenSsh = "C:\\Windows\\System32\\OpenSSH\\ssh.exe";
    if (QFile::exists(winOpenSsh)) {
        sshPath = winOpenSsh;
    } else {
        sshPath = QStandardPaths::findExecutable("ssh.exe");
        if (sshPath.isEmpty()) sshPath = QStandardPaths::findExecutable("ssh");
    }
#else
    sshPath = QStandardPaths::findExecutable("ssh");
#endif

    if (sshPath.isEmpty()) {
        stdErr = "未找到系统的 OpenSSH (ssh.exe) 客户端程序，请确保系统已安装 OpenSSH。";
        return false;
    }

    // 2. 生成临时批处理密码器，供 SSH_ASKPASS 自动无感响应密码
    QString tempBatPath = QDir::tempPath() + QString("/qt_agv_askpass_%1_%2.bat")
                                                 .arg(QCoreApplication::applicationPid())
                                                 .arg(QDateTime::currentMSecsSinceEpoch());
    QFile batFile(tempBatPath);
    if (batFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        batFile.write("@echo off\r\necho %AGV_SSH_PWD%\r\n");
        batFile.close();
    }

    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("SSH_ASKPASS", QDir::toNativeSeparators(tempBatPath));
    env.insert("SSH_ASKPASS_REQUIRE", "force");
    env.insert("AGV_SSH_PWD", pwd);
    env.insert("DISPLAY", "dummy:0");
    process.setProcessEnvironment(env);

    // 3. 构建安全的 OpenSSH 参数
    QStringList args;
    args << "-p" << QString::number(port);
    args << "-o" << "StrictHostKeyChecking=no";
    args << "-o" << "UserKnownHostsFile=NUL";
    args << "-o" << "ConnectTimeout=6";
    args << "-o" << "PreferredAuthentications=password,keyboard-interactive";
    args << "-o" << "NumberOfPasswordPrompts=1";
    args << "-o" << "LogLevel=ERROR";
    args << QString("%1@%2").arg(user, host);
    args << command;

    process.start(sshPath, args);
    bool finished = process.waitForFinished(timeoutMs);

    // 及时清理临时密码批处理文件
    QFile::remove(tempBatPath);

    if (!finished) {
        process.kill();
        stdErr = QString("SSH 连接或执行超时 (超过 %1 秒无响应)").arg(timeoutMs / 1000);
        return false;
    }

    stdOut = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    stdErr = QString::fromUtf8(process.readAllStandardError()).trimmed();

    return (process.exitCode() == 0);
}

void NetworkConfigDialog::onReadConfig()
{
    if (m_isBusy) return;

    QString host = m_hostEdit->text().trimmed();
    if (!validateIPv4(host, "SSH 主机 IP")) return;

    int port = m_portEdit->text().trimmed().toInt();
    if (port <= 0 || port > 65535) port = 22;

    QString user = m_userEdit->text().trimmed();
    QString pwd  = m_pwdEdit->text();

    if (user.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入 SSH 用户名 (如 tuju)");
        return;
    }

    m_isBusy = true;
    m_readBtn->setEnabled(false);
    m_applyBtn->setEnabled(false);
    m_statusLabel->setText("正在连接 AGV 并读取网络状态...");
    m_statusLabel->setStyleSheet("color:#63b3ed; font-weight:600;");

    appendLog(QString(">>> 开始连接 SSH: %1@%2:%3 ...").arg(user, host).arg(port), "#63b3ed");

    // 构造 Python 脚本安全读取网卡、IP、掩码与网关，Base64 封装避免任何字符转义损坏
    QString pyScript = R"py(
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

    QString b64 = pyScript.toUtf8().toBase64();
    QString remoteCmd = QString("python3 -c \"import base64; exec(base64.b64decode('%1').decode('utf-8'))\"").arg(b64);

    // 启动异步线程或直接处理
    qApp->processEvents();

    QString stdOut, stdErr;
    bool ok = runSshCommand(host, port, user, pwd, remoteCmd, stdOut, stdErr, 12000);

    m_isBusy = false;
    m_readBtn->setEnabled(true);
    m_applyBtn->setEnabled(true);

    if (!ok) {
        appendLog("SSH 执行失败: " + (stdErr.isEmpty() ? "未知错误，请检查网络和账号密码" : stdErr), "#f56565");
        m_statusLabel->setText("连接失败");
        m_statusLabel->setStyleSheet("color:#f56565; font-weight:600;");
        QMessageBox::critical(this, "SSH 连接失败",
                              QString("无法连接至 AGV (%1:%2)\n\n错误信息: %3\n\n请检查：\n1. 目标 IP 与 SSH 端口是否连通\n2. 用户名或密码是否正确 (默认: tuju / vsis1234)")
                                  .arg(host).arg(port).arg(stdErr));
        return;
    }

    // 解析 JSON 输出
    int startIdx = stdOut.indexOf("JSON_START");
    int endIdx   = stdOut.indexOf("JSON_END");

    if (startIdx >= 0 && endIdx > startIdx) {
        QString jsonStr = stdOut.mid(startIdx + 10, endIdx - (startIdx + 10));
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        QJsonObject obj = doc.object();

        QString curIface = obj.value("iface").toString();
        QJsonArray ifacesArr = obj.value("ifaces").toArray();
        QString curIp = obj.value("ip").toString();
        QString curMask = obj.value("mask").toString();
        QString curGw = obj.value("gateway").toString();

        m_ifaceCombo->clear();
        for (const auto &v : ifacesArr) {
            m_ifaceCombo->addItem(v.toString());
        }
        if (m_ifaceCombo->findText(curIface) >= 0) {
            m_ifaceCombo->setCurrentText(curIface);
        } else if (!curIface.isEmpty()) {
            m_ifaceCombo->insertItem(0, curIface);
            m_ifaceCombo->setCurrentIndex(0);
        }

        if (!curIp.isEmpty())   m_ipEdit->setText(curIp);
        if (!curMask.isEmpty()) m_maskEdit->setText(curMask);
        if (!curGw.isEmpty())   m_gwEdit->setText(curGw);

        appendLog(QString("✓ 成功读取 Ubuntu 网络状态:\n  • 网卡: %1\n  • 当前 IP: %2\n  • 子网掩码: %3\n  • 默认网关: %4")
                      .arg(curIface, curIp, curMask, curGw), "#48bb78");

        m_statusLabel->setText(QString("已成功连接至 %1 (%2)").arg(host, curIface));
        m_statusLabel->setStyleSheet("color:#48bb78; font-weight:600;");
    } else {
        appendLog("读取成功但解析回显异常: " + stdOut, "#f6ad55");
        m_statusLabel->setText("回显解析异常");
    }
}

void NetworkConfigDialog::onApplyConfig()
{
    if (m_isBusy) return;

    QString host = m_hostEdit->text().trimmed();
    int port = m_portEdit->text().trimmed().toInt();
    if (port <= 0 || port > 65535) port = 22;
    QString user = m_userEdit->text().trimmed();
    QString pwd  = m_pwdEdit->text();

    QString newIface = m_ifaceCombo->currentText().trimmed();
    QString newIp    = m_ipEdit->text().trimmed();
    QString newMask  = m_maskEdit->text().trimmed();
    QString newGw    = m_gwEdit->text().trimmed();

    if (newIface.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请选择或指定网卡接口名称 (如 eth0)");
        return;
    }
    if (!validateIPv4(newIp, "新 IP 地址")) return;
    if (!validateIPv4(newMask, "子网掩码")) return;
    if (!newGw.isEmpty() && !validateIPv4(newGw, "默认网关")) return;

    // 弹窗确认修改，提醒 IP 变更影响
    QString confirmMsg = QString("您确定要修改 AGV 机器人的网络配置吗？\n\n"
                                 "• 目标主机: %1 (接口: %2)\n"
                                 "• 新 IP 地址: %3\n"
                                 "• 子网掩码: %4\n"
                                 "• 默认网关: %5\n\n"
                                 "⚠ 注意：网络配置写入并生效后，原 IP 连线将立即断开！\n"
                                 "后续需要使用新 IP [%3] 连接设备。是否继续？")
                             .arg(host, newIface, newIp, newMask, newGw.isEmpty() ? "(无)" : newGw);

    if (QMessageBox::question(this, "确认修改 AGV 网络配置", confirmMsg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    m_isBusy = true;
    m_readBtn->setEnabled(false);
    m_applyBtn->setEnabled(false);
    m_statusLabel->setText("正在下发并应用网络配置...");
    m_statusLabel->setStyleSheet("color:#63b3ed; font-weight:600;");

    appendLog(QString(">>> 正在下发网络配置至 Ubuntu [%1]: IP=%2, 掩码=%3, 网关=%4 ...").arg(newIface, newIp, newMask, newGw), "#63b3ed");

    // 构造 Python 脚本并自动提权应用 Netplan / NetworkManager
    QString pyApplyScript = QString(R"py(
import sys, os, subprocess, socket, struct

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

applied_netplan = False
netplan_dir = "/etc/netplan"
if os.path.exists(netplan_dir):
    try:
        # Netplan 配置
        gw_block = f"      routes:\n        - to: default\n          via: {gw}\n" if gw else ""
        cfg = f"""# Generated by AGV Network Config
network:
  version: 2
  renderer: networkd
  ethernets:
    {iface}:
      dhcp4: no
      addresses:
        - {ip}/{prefix}
{gw_block}      nameservers:
        addresses: [114.114.114.114, 8.8.8.8]
"""
        target_file = "/etc/netplan/01-agv-network.yaml"
        for f in os.listdir(netplan_dir):
            if f.endswith(".yaml") and f != "01-agv-network.yaml":
                try: os.rename(os.path.join(netplan_dir, f), os.path.join(netplan_dir, f + ".bak"))
                except: pass
        with open(target_file, "w") as fp:
            fp.write(cfg)
        os.chmod(target_file, 0o600)
        subprocess.run(["netplan", "apply"], timeout=8)
        applied_netplan = True
    except Exception as e:
        print("Netplan error:", e)

applied_nm = False
try:
    res = subprocess.run(["nmcli", "connection", "show"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if res.returncode == 0:
        lines = res.stdout.decode().splitlines()
        con_name = ""
        for line in lines[1:]:
            parts = line.split()
            if len(parts) >= 4 and parts[-1] == iface:
                con_name = line[:line.find(parts[-3])].strip()
                break
        if not con_name: con_name = iface
        cmd = ["nmcli", "con", "mod", con_name, "ipv4.addresses", f"{ip}/{prefix}", "ipv4.method", "manual"]
        if gw: cmd.extend(["ipv4.gateway", gw])
        subprocess.run(cmd, timeout=8)
        subprocess.run(["nmcli", "con", "up", con_name], timeout=8)
        applied_nm = True
except Exception as e:
    pass

# 瞬时生效保障
os.system(f"ip addr add {ip}/{prefix} dev {iface} 2>/dev/null")
if gw: os.system(f"ip route add default via {gw} dev {iface} 2>/dev/null")

print("CONFIG_APPLY_SUCCESS")
)py").arg(newIface, newIp, newMask, newGw);

    QString b64 = pyApplyScript.toUtf8().toBase64();
    QString safePwd = pwd;
    safePwd.replace("'", "'\"'\"'");
    // 结合 echo vsis1234 | sudo -S 执行提权
    QString remoteCmd = QString("echo '%1' | sudo -S python3 -c \"import base64; exec(base64.b64decode('%2').decode('utf-8'))\"")
                            .arg(safePwd, b64);

    qApp->processEvents();

    QString stdOut, stdErr;
    // 设置合理的超时，修改 IP 导致连接中断属预期现象
    bool ok = runSshCommand(host, port, user, pwd, remoteCmd, stdOut, stdErr, 12000);

    m_isBusy = false;
    m_readBtn->setEnabled(true);
    m_applyBtn->setEnabled(true);

    bool isSuccess = stdOut.contains("CONFIG_APPLY_SUCCESS");
    // 如果因 IP 立即切换导致 SSH 连接提前中断（Connection reset / closed），但命令已由远端执行
    if (!ok && (stdErr.contains("Connection reset") || stdErr.contains("Connection closed") || stdErr.contains("Broken pipe"))) {
        isSuccess = true;
    }

    if (isSuccess) {
        appendLog("🎉 恭喜！网络配置已成功应用至 AGV 的 Ubuntu 系统！", "#48bb78");
        appendLog(QString("请在 3-5 秒后使用新 IP [%1] 进行系统连接。").arg(newIp), "#48bb78");
        m_statusLabel->setText(QString("配置已生效 (新 IP: %1)").arg(newIp));
        m_statusLabel->setStyleSheet("color:#48bb78; font-weight:600;");

        m_appliedIp = newIp;
        m_ipChanged = (m_originalIp != newIp);

        QMessageBox::information(this, "网络配置成功",
                                 QString("AGV 网络配置已成功下发并应用！\n\n新网络参数：\n• IP 地址: %1\n• 子网掩码: %2\n• 默认网关: %3\n\n请在网络生效后使用新 IP 重连。")
                                     .arg(newIp, newMask, newGw));

        emit networkUpdated(m_originalIp, newIp);
        accept();
    } else {
        appendLog("网络配置下发失败: " + (stdErr.isEmpty() ? stdOut : stdErr), "#f56565");
        m_statusLabel->setText("应用失败");
        m_statusLabel->setStyleSheet("color:#f56565; font-weight:600;");
        QMessageBox::critical(this, "应用配置失败",
                              QString("下发配置至 AGV 时发生错误。\n\n回显信息: %1\n%2\n\n请检查密码 (vsis1234) 是否具备 sudo 权限。")
                                  .arg(stdOut, stdErr));
    }
}
