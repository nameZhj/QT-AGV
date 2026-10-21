/**
 * @file    NetworkConfigDialog.h
 * @layer   GUILayer
 * @brief   Network connection parameter setup dialog for REST and MQTT endpoints.
 * @deps    QDialog
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QDialog>

class QLineEdit;
class QComboBox;
class QPushButton;
class QTextEdit;
class QLabel;

class NetworkConfigReader;
class NetworkConfigApplier;

/**
 * @brief AGV 机器人远程网络配置对话框
 * 通过 SSH 登录到 AGV Ubuntu 系统 (默认用户 tuju, 密码 vsis1234)
 * 允许读取当前网络参数并配置新的 IPv4 IP地址、子网掩码与网关。
 */
class NetworkConfigDialog : public QDialog
{
    Q_OBJECT
    friend class NetworkConfigReader;
    friend class NetworkConfigApplier;

public:
    explicit NetworkConfigDialog(const QString &defaultIp,
                                 const QString &carName = "",
                                 const QString &sshUser = "tuju",
                                 const QString &sshPwd = "vsis1234",
                                 int sshPort = 22,
                                 QWidget *parent = nullptr);
    ~NetworkConfigDialog() override;

    QString newIp() const { return m_appliedIp; }
    bool isIpChanged() const { return m_ipChanged; }
    QString sshUser() const { return m_appliedUser.isEmpty() ? m_sshUser : m_appliedUser; }
    QString sshPwd() const { return m_appliedPwd.isEmpty() ? m_sshPwd : m_appliedPwd; }
    int sshPort() const { return m_appliedPort > 0 ? m_appliedPort : m_sshPort; }

    void appendLog(const QString &text, const QString &color = "#cbd5e1");

signals:
    void networkUpdated(const QString &oldIp, const QString &newIp);
    void sshCredentialsUpdated(const QString &user, const QString &pwd, int port);

private slots:
    void onReadConfig();
    void onApplyConfig();
    void onTogglePasswordVisible();

private:
    void setupUI();

    QString m_originalIp;
    QString m_carName;
    QString m_appliedIp;
    bool m_ipChanged = false;

    QString m_sshUser;
    QString m_sshPwd;
    int     m_sshPort = 22;
    QString m_appliedUser;
    QString m_appliedPwd;
    int     m_appliedPort = 22;

    // SSH 连接配置控件
    QLineEdit   *m_hostEdit   = nullptr;
    QLineEdit   *m_portEdit   = nullptr;
    QLineEdit   *m_userEdit   = nullptr;
    QLineEdit   *m_pwdEdit    = nullptr;
    QPushButton *m_pwdEyeBtn  = nullptr;
    QPushButton *m_readBtn    = nullptr;

    // 网络参数配置控件
    QComboBox   *m_ifaceCombo = nullptr;
    QLineEdit   *m_ipEdit     = nullptr;
    QLineEdit   *m_maskEdit   = nullptr;
    QLineEdit   *m_gwEdit     = nullptr;
    QPushButton *m_applyBtn   = nullptr;

    // 状态与日志输出控件
    QLabel      *m_statusLabel= nullptr;
    QTextEdit   *m_logEdit    = nullptr;

    bool m_isBusy = false;
};
