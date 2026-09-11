#ifndef NETWORKCONFIGDIALOG_H
#define NETWORKCONFIGDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QProcess>

/**
 * @brief AGV 机器人远程网络配置对话框
 * 通过 SSH 登录到 AGV Ubuntu 系统 (默认用户 tuju, 密码 vsis1234)
 * 允许读取当前网络参数并配置新的 IPv4 IP地址、子网掩码与网关。
 */
class NetworkConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NetworkConfigDialog(const QString &defaultIp, const QString &carName = "", QWidget *parent = nullptr);
    ~NetworkConfigDialog() override;

    QString newIp() const { return m_appliedIp; }
    bool isIpChanged() const { return m_ipChanged; }

signals:
    void networkUpdated(const QString &oldIp, const QString &newIp);

private slots:
    void onReadConfig();
    void onApplyConfig();
    void onTogglePasswordVisible();

private:
    void setupUI();
    void appendLog(const QString &text, const QString &color = "#cbd5e1");
    bool validateIPv4(const QString &ip, const QString &fieldName);
    bool runSshCommand(const QString &host, int port, const QString &user, const QString &pwd,
                       const QString &command, QString &stdOut, QString &stdErr, int timeoutMs = 15000);

    QString m_originalIp;
    QString m_carName;
    QString m_appliedIp;
    bool m_ipChanged = false;

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

#endif // NETWORKCONFIGDIALOG_H
