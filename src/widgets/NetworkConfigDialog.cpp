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
#include <QTcpSocket>
#include <QThread>
#include <QElapsedTimer>
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

NetworkConfigDialog::NetworkConfigDialog(const QString &defaultIp,
                                         const QString &carName,
                                         const QString &sshUser,
                                         const QString &sshPwd,
                                         int sshPort,
                                         QWidget *parent)
    : QDialog(parent)
    , m_originalIp(defaultIp)
    , m_carName(carName)
    , m_appliedIp(defaultIp)
    , m_sshUser(sshUser.isEmpty() ? "tuju" : sshUser)
    , m_sshPwd(sshPwd.isEmpty() ? "vsis1234" : sshPwd)
    , m_sshPort(sshPort > 0 ? sshPort : 22)
    , m_appliedUser(m_sshUser)
    , m_appliedPwd(m_sshPwd)
    , m_appliedPort(m_sshPort)
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

    m_portEdit = new QLineEdit(QString::number(m_sshPort), sshBox);
    m_portEdit->setFixedWidth(65);
    m_portEdit->setAlignment(Qt::AlignCenter);

    m_userEdit = new QLineEdit(m_sshUser, sshBox);
    m_userEdit->setPlaceholderText("SSH 用户名");

    m_pwdEdit  = new QLineEdit(m_sshPwd, sshBox);
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

bool NetworkConfigDialog::checkIpConnectivity(const QString &ip, int port, int timeoutMs)
{
    // 1. TCP 端口探测 (优先探测指定的 SSH 端口或系统服务端口)
    QTcpSocket socket;
    socket.connectToHost(ip, static_cast<quint16>(port));
    if (socket.waitForConnected(timeoutMs)) {
        socket.disconnectFromHost();
        return true;
    }
    // 若收到 ConnectionRefusedError，说明目标主机已成功上线并返回了 RST 报文响应
    if (socket.error() == QAbstractSocket::ConnectionRefusedError) {
        return true;
    }

    // 2. 备用方式: 系统 Ping 探测
    QProcess pingProc;
    QStringList pingArgs;
#ifdef Q_OS_WIN
    pingArgs << "-n" << "1" << "-w" << QString::number(timeoutMs) << ip;
    pingProc.start("ping.exe", pingArgs);
#else
    pingArgs << "-c" << "1" << "-W" << QString::number(qMax(1, timeoutMs / 1000)) << ip;
    pingProc.start("ping", pingArgs);
#endif
    if (pingProc.waitForFinished(timeoutMs + 1000)) {
        if (pingProc.exitCode() == 0) {
            return true;
        }
    }

    return false;
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

        m_appliedUser = user;
        m_appliedPwd  = pwd;
        m_appliedPort = port;
        emit sshCredentialsUpdated(user, pwd, port);
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

    // 构造 Python 脚本并使用 NetworkManager (nmcli) 下发并持久化网络配置，同时自愈修复旧版残留的 systemd-networkd 冲突
    QString pyApplyScript = QString(R"py(
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
        # 还原被重命名为 .bak 的原有配置
        for bak_f in glob.glob(os.path.join(netplan_dir, "*.bak")):
            orig_f = bak_f[:-4]
            if not os.path.exists(orig_f):
                try: os.rename(bak_f, orig_f)
                except: pass
            else:
                try: os.remove(bak_f)
                except: pass

        # 清除之前残留的冲突配置 01-agv-network.yaml
        agv_yaml = os.path.join(netplan_dir, "01-agv-network.yaml")
        if os.path.exists(agv_yaml):
            try: os.remove(agv_yaml)
            except: pass

        # 检查是否还有其他 yaml 错误地指定了 renderer: networkd，如有则修正为 NetworkManager
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

        # 若没有任何 yaml 文件，则生成默认的 NetworkManager 配置文件
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
    # 查找该 iface 绑定的活动连接
    res = subprocess.run(["nmcli", "-t", "-f", "NAME,DEVICE", "connection", "show"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if res.returncode == 0:
        for line in res.stdout.decode().splitlines():
            parts = line.split(":")
            if len(parts) >= 2 and parts[1].strip() == iface:
                con_name = parts[0].strip()
                break

    # 若未找到活动连接，从所有连接中查找
    if not con_name:
        res_all = subprocess.run(["nmcli", "-t", "-f", "NAME,DEVICE", "con", "show"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if res_all.returncode == 0:
            for line in res_all.stdout.decode().splitlines():
                parts = line.split(":")
                if len(parts) >= 1 and (parts[0].strip() == iface or (len(parts) >= 2 and parts[1].strip() == iface)):
                    con_name = parts[0].strip()
                    break

    # 若尚无连接配置，为该网卡新建一个 NetworkManager 以太网连接
    if not con_name:
        con_name = f"Wired-{iface}"
        subprocess.run(["nmcli", "con", "add", "type", "ethernet", "ifname", iface, "con-name", con_name], timeout=8)

    # 通过 nmcli 配置静态 IP、掩码、网关和 DNS
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

    if (!isSuccess && !stdErr.isEmpty() && (stdErr.contains("Permission denied") || stdErr.contains("Authentication failed") || stdErr.contains("sudo: "))) {
        m_isBusy = false;
        m_readBtn->setEnabled(true);
        m_applyBtn->setEnabled(true);
        appendLog("网络配置下发失败: " + (stdErr.isEmpty() ? stdOut : stdErr), "#f56565");
        m_statusLabel->setText("应用失败");
        m_statusLabel->setStyleSheet("color:#f56565; font-weight:600;");
        QMessageBox::critical(this, "应用配置失败",
                              QString("下发配置至 AGV 时发生错误。\n\n回显信息: %1\n%2\n\n请检查密码 (vsis1234) 是否具备 sudo 权限。")
                                  .arg(stdOut, stdErr));
        return;
    }

    // ── 修改 IP 后对目标 IP 进行连通性测试以判断是否修改成功 ─────────────────
    appendLog(QString(">>> 网络配置已下发，正在检测目标 IP [%1] 的网络连通性...").arg(newIp), "#63b3ed");
    m_statusLabel->setText(QString("正在测试目标 IP [%1] 连通性...").arg(newIp));
    m_statusLabel->setStyleSheet("color:#63b3ed; font-weight:600;");

    bool targetConnected = false;
    const int maxRetries = 6;
    for (int attempt = 1; attempt <= maxRetries; ++attempt) {
        appendLog(QString("正在测试目标 IP [%1] 连通性 (第 %2/%3 次尝试)...").arg(newIp).arg(attempt).arg(maxRetries), "#cbd5e1");
        qApp->processEvents();

        if (checkIpConnectivity(newIp, port, 1500)) {
            targetConnected = true;
            break;
        }

        if (attempt < maxRetries) {
            // 等待 1 秒同时保持界面事件响应
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 1000) {
                qApp->processEvents(QEventLoop::AllEvents, 50);
                QThread::msleep(50);
            }
        }
    }

    m_isBusy = false;
    m_readBtn->setEnabled(true);
    m_applyBtn->setEnabled(true);

    if (targetConnected) {
        appendLog(QString("🎉 恭喜！目标 IP [%1] 连通性测试通过，网络配置已成功生效！").arg(newIp), "#48bb78");
        m_statusLabel->setText(QString("修改成功且目标 IP 已连通: %1").arg(newIp));
        m_statusLabel->setStyleSheet("color:#48bb78; font-weight:600;");

        m_appliedIp = newIp;
        m_ipChanged = (m_originalIp != newIp);

        m_appliedUser = user;
        m_appliedPwd  = pwd;
        m_appliedPort = port;
        emit sshCredentialsUpdated(user, pwd, port);

        QMessageBox::information(this, "网络配置成功",
                                 QString("AGV 网络配置已成功下发并应用，且目标 IP [%1] 连通性测试通过！\n\n新网络参数：\n• IP 地址: %1\n• 子网掩码: %2\n• 默认网关: %3\n\n设备列表将自动更新。")
                                     .arg(newIp, newMask, newGw));

        emit networkUpdated(m_originalIp, newIp);
        accept();
    } else {
        appendLog(QString("❌ 目标 IP [%1] 连通性测试失败！无法与目标 IP 建立网络连接。").arg(newIp), "#f56565");
        m_statusLabel->setText(QString("目标 IP [%1] 连通失败").arg(newIp));
        m_statusLabel->setStyleSheet("color:#f56565; font-weight:600;");
        QMessageBox::warning(this, "连通性测试失败",
                             QString("网络配置已下发，但目标 IP [%1] 连通性测试未通过！\n\n可能原因：\n1. 本机与目标 IP 不在同一网段或没有可达路由；\n2. 目标 Ubuntu 系统网卡重载较慢或尚未恢复就绪；\n3. 输入的 IP/掩码/网关存在冲突或格式错误。\n\n请检查本机网络配置，或在命令行使用 ping %1 进行排查。")
                                 .arg(newIp));
    }
}
