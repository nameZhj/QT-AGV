#ifndef DEVICEMANAGERDIALOG_H
#define DEVICEMANAGERDIALOG_H

#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QStackedWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QMqttClient>
#include <QProgressDialog>
#include "AppConfig.h"

class QTimer;

// ── Single device card widget ────────────────────────────────────────────────
class DeviceCard : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceCard(const DeviceProfile &profile, int index, QWidget *parent = nullptr);
    void setIndex(int idx) { m_index = idx; }
    void setBusy(bool busy);
    // 异步更新链接状态 UI（true=已链接，false=未链接）
    void setLinkStatus(bool linked);

signals:
    void connectClicked(int index);
    void removeClicked(int index);
    void linkClicked(int index);     // 链接设备 (未链接状态下显示)

private:
    void checkOnlineStatus(const QString &ip, const QString &carId);
    void setOnlineUI(bool online);
    void finishProbe();

    int m_index = 0;
    QPushButton *m_connectBtn  = nullptr;
    QPushButton *m_linkBtn     = nullptr;   // 链接设备按钮 (未链接时显示)
    QLabel      *m_warnLabel   = nullptr;   // "⚠ 未链接" 警告标签
    QLabel      *m_onlineDot   = nullptr;
    QLabel      *m_onlineLabel = nullptr;
    QHBoxLayout *m_footerLay   = nullptr;   // footer 布局，供 setLinkStatus 动态调整

    // 临时探测连接：通过实际订阅设备的 MQTT 信息主题判断是否真正在线，
    // 与应用主连接(MqttService)互不干扰，探测完成后即销毁。
    QMqttClient *m_probeClient  = nullptr;
    QTimer      *m_probeTimer   = nullptr;
    bool         m_probeResolved = false;
};

// ── Main device-manager dialog ────────────────────────────────────────────────
class DeviceManagerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DeviceManagerDialog(QWidget *parent = nullptr);

private slots:
    void onAddDevice();
    void onConfirmAdd();
    void onCancelAdd();
    void onConnectCard(int index);
    void onRemoveCard(int index);
    void onLinkCard(int index);     // 重新链接未链接的设备
    void onRefreshAll();

private:
    void setupUI();
    void setupTopBar(QWidget *topBar);
    void setupCardArea(QWidget *area);
    void setupAddPanel();
    void refreshCards();
    void connectDevice(int index);
    void setStatus(const QString &msg, const QString &color = "#f0a060");
    // 轮询 getCar 检查 linkStatus 是否为 '1' (已链接), 最多尝试 maxAttempts 次
    void checkLinkStatus(const QString &carId, const QString &ip, const QString &carName, int attempt);

    QStackedWidget *m_stack           = nullptr;
    QWidget        *m_mainPanel       = nullptr;
    QWidget        *m_addPanel        = nullptr;

    QWidget        *m_cardsContainer  = nullptr;
    QGridLayout    *m_cardsGrid       = nullptr;
    QScrollArea    *m_scrollArea      = nullptr;
    QLabel         *m_statusLabel     = nullptr;

    QLineEdit      *m_ipEdit          = nullptr;
    QLineEdit      *m_nameEdit        = nullptr; // 设备名称输入（无 carId 时显示）
    QWidget        *m_nameRow         = nullptr; // 设备名称行容器
    QLabel         *m_addStatus       = nullptr;
    QPushButton    *m_confirmBtn      = nullptr;
    QPushButton    *m_cancelBtn       = nullptr;

    QProgressDialog *m_linkProgress   = nullptr;  // 链接状态轮询进度弹窗
    bool             m_linkCancelled   = false;    // 用户是否取消了链接

    QList<DeviceCard *> m_cards;
};

#endif // DEVICEMANAGERDIALOG_H
