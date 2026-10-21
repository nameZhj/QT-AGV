/**
 * @file    NetworkConfigDialog.cpp
 * @layer   GUILayer
 * @brief   Implementation of network connection setup dialog.
 * @deps    NetworkConfigDialog.h, NetworkConfigStyle.h, NetworkConfigReader.h, NetworkConfigApplier.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "NetworkConfigDialog.h"
#include "NetworkConfigStyle.h"
#include "NetworkConfigReader.h"
#include "NetworkConfigApplier.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QDateTime>

NetworkConfigDialog::NetworkConfigDialog(const QString &defaultIp,
                                         const QString &carName,
                                         const QString &sshUser,
                                         const QString &sshPwd, int sshPort,
                                         QWidget *parent)
    : QDialog(parent), m_originalIp(defaultIp), m_carName(carName),
      m_appliedIp(defaultIp), m_sshUser(sshUser.isEmpty() ? "tuju" : sshUser),
      m_sshPwd(sshPwd.isEmpty() ? "vsis1234" : sshPwd),
      m_sshPort(sshPort > 0 ? sshPort : 22), m_appliedUser(m_sshUser),
      m_appliedPwd(m_sshPwd), m_appliedPort(m_sshPort) {
  setupUI();
}

NetworkConfigDialog::~NetworkConfigDialog() {}

void NetworkConfigDialog::setupUI() {
  setStyleSheet(NetworkConfigStyle::kDialogStyle);
  QString title = "AGV 机器人网络配置 (SSH)";
  if (!m_carName.isEmpty())
    title += " - " + m_carName;
  else if (!m_originalIp.isEmpty())
    title += " - " + m_originalIp;
  setWindowTitle(title);
  resize(660, 600);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(16, 16, 16, 16);
  mainLayout->setSpacing(12);

  auto *topNotice = new QLabel("通过 SSH 远程直连 AGV 的 Ubuntu 系统，实时检测网卡状态并配置静态 IPv4 (IP、掩码与网关)。");
  topNotice->setStyleSheet("color:#718096; font-size:12px; margin-bottom: 2px;");
  mainLayout->addWidget(topNotice);

  // 1. SSH 连接参数面板
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

  m_pwdEdit = new QLineEdit(m_sshPwd, sshBox);
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
  m_readBtn->setStyleSheet(
      "QPushButton{background:#1e4b7a; color:#b0d7ff; border:1px solid "
      "#2d6aa8; font-weight:700; padding:6px 12px;} "
      "QPushButton:hover{background:#265f9e; color:#ffffff;}");
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

  // 2. IPv4 网络配置目标面板
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

  // 3. 实时输出与终端回显窗口
  auto *logBox = new QGroupBox("3. 执行日志与回显", this);
  auto *logLayout = new QVBoxLayout(logBox);
  logLayout->setContentsMargins(10, 12, 10, 10);

  m_logEdit = new QTextEdit(logBox);
  m_logEdit->setReadOnly(true);
  m_logEdit->setFixedHeight(120);
  logLayout->addWidget(m_logEdit);
  mainLayout->addWidget(logBox);

  // 4. 底部状态与操作按钮栏
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

void NetworkConfigDialog::appendLog(const QString &text, const QString &color) {
  QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
  m_logEdit->append(QString("<span style='color:#718096;'>[%1]</span> <span style='color:%2;'>%3</span>")
                        .arg(timeStr, color, text.toHtmlEscaped()));
  m_logEdit->ensureCursorVisible();
}

void NetworkConfigDialog::onTogglePasswordVisible() {
  if (m_pwdEdit->echoMode() == QLineEdit::Password) {
    m_pwdEdit->setEchoMode(QLineEdit::Normal);
    m_pwdEyeBtn->setText("🔒");
  } else {
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEyeBtn->setText("👁");
  }
}

void NetworkConfigDialog::onReadConfig() {
  NetworkConfigReader::readConfig(this);
}

void NetworkConfigDialog::onApplyConfig() {
  NetworkConfigApplier::applyConfig(this);
}
