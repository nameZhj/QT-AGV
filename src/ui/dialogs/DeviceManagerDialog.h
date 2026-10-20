#pragma once

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
#include <QGraphicsDropShadowEffect>
#include "AppConfig.h"

class QTimer;

class BatteryBadge : public QWidget
{
public:
    explicit BatteryBadge(QWidget *parent = nullptr);
    void setBattery(int level);
    int  battery() const { return m_level; }
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    int m_level = -1;
};

// ── Single device card widget ────────────────────────────────────────────────
class DeviceCard : public QFrame
{
    Q_OBJECT
public:
    explicit DeviceCard(const DeviceProfile &profile, int index, QWidget *parent = nullptr);
    ~DeviceCard() override;
    void setIndex(int idx) { m_index = idx; }
    void setBusy(bool busy);
    bool isBusy() const { return m_isBusy; }
    // 异步更新链接状态 UI（true=已链接，false=未链接）
    void setLinkStatus(bool linked);
    // 主动断开设备连接与 MQTT
    void disconnectDevice();
    void updateCardDisplay();
    void probeDeviceStatus(const QString &ip);

signals:
    void doubleClicked(int index);
    void connectClicked(int index);
    void disconnectClicked(int index);
    void removeClicked(int index);
    void networkConfigClicked(int index); // 远程网络配置 (SSH)

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setOnlineUI(bool online);
    void testLatency(const QString &ip);

    int m_index = 0;
    DeviceProfile m_profile;

    QWidget      *m_hdrWidget    = nullptr;
    QLabel       *m_onlineDot   = nullptr;
    QLabel       *m_onlineLabel = nullptr;
    QLabel       *m_robLabel    = nullptr;
    BatteryBadge *m_batteryBadge = nullptr; // 顶部电池电量 (例如 47)
    QPushButton  *m_configBtn   = nullptr;   // 顶部配置滑块图标 🎛️
    QLabel       *m_robotIcon   = nullptr;   // 机器人图标
    QLabel       *m_ipLabel     = nullptr;   // IP 标签
    QLabel       *m_typeTag     = nullptr;   // 机器人类型 (固定写死：潜伏底盘)
    QLabel       *m_confBadge   = nullptr;   // 位置置信度 (橙色底色块，如 0.35)
    QLabel       *m_latencyLabel= nullptr;   // 通讯延迟 (亮绿色，如 9ms)
    QLabel       *m_mapLabel    = nullptr;   // 地图: qlyy
    QLabel       *m_nameLabel   = nullptr;   // 名称: AMB-01
    QLabel       *m_remarkLabel = nullptr;   // 备注:
    QLabel       *m_verLabel    = nullptr;   // 版本: v2.1.1
    QLabel       *m_fwVerLabel  = nullptr;   // 固件版本: v2.1.1
    QLabel       *m_baseIcon    = nullptr;   // 底部基站/WiFi 图标
    QPushButton  *m_syncBtn       = nullptr; // 底部左侧 已同步 按钮
    QPushButton  *m_connectBtn    = nullptr; // 底部右侧 操作按钮 (未连接显示"连接"，已连接显示"断开连接"，互斥常驻显示)
    QGraphicsDropShadowEffect *m_shadow = nullptr; // 整卡立体投影效果

    double       m_confidence   = -1.0;
    int          m_battery      = -1;
    int          m_latency      = -1;
    bool         m_isOnline     = false;
    QString      m_fwVersion;
    QString      m_remark;
    QString      m_statusText;
    QTimer      *m_latencyTimer = nullptr;
    bool         m_isProbingLatency = false;
    qint64       m_lastLatencyProbeMs = 0;
    bool         m_isBusy = false;
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
    void onCardDoubleClicked(int index);
    void onConnectCard(int index);
    void onDisconnectCard(int index);
    void onRemoveCard(int index);
    void onNetworkConfig(int index); // 远程网络配置 (SSH)
    void onRefreshAll();

private:
    void setupUI();
    void setupTopBar(QWidget *topBar);
    void setupCardArea(QWidget *area);
    void setupAddPanel();
    void refreshCards();
    void connectDevice(int index);
    void setStatus(const QString &msg, const QString &color = "#f0a060");

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



    QList<DeviceCard *> m_cards;
};

