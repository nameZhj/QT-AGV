/**
 * @file    DeviceCard.h
 * @layer   GUILayer
 * @brief   Single device card frame widget representing robot telemetry and controls.
 * @deps    QFrame, DeviceProfile, BatteryBadge, QGraphicsDropShadowEffect
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include "AppConfig.h"
#include "DeviceCardWidgets.h"

class DeviceCard : public QFrame
{
    Q_OBJECT
public:
    explicit DeviceCard(const DeviceProfile &profile, int index, QWidget *parent = nullptr);
    ~DeviceCard() override;

    void setIndex(int idx) { m_index = idx; }
    void setBusy(bool busy);
    bool isBusy() const { return m_isBusy; }
    void setLinkStatus(bool linked);
    void disconnectDevice();
    void updateCardDisplay();
    void probeDeviceStatus(const QString &ip);

signals:
    void doubleClicked(int index);
    void connectClicked(int index);
    void disconnectClicked(int index);
    void removeClicked(int index);
    void networkConfigClicked(int index);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setOnlineUI(bool online);

    int m_index = 0;
    DeviceProfile m_profile;

    QWidget      *m_hdrWidget    = nullptr;
    QLabel       *m_onlineDot   = nullptr;
    QLabel       *m_onlineLabel = nullptr;
    QLabel       *m_robLabel    = nullptr;
    BatteryBadge *m_batteryBadge = nullptr;
    QPushButton  *m_configBtn   = nullptr;
    QLabel       *m_robotIcon   = nullptr;
    QLabel       *m_ipLabel     = nullptr;
    QLabel       *m_typeTag     = nullptr;
    QLabel       *m_confBadge   = nullptr;
    QLabel       *m_latencyLabel= nullptr;
    QLabel       *m_mapLabel    = nullptr;
    QLabel       *m_nameLabel   = nullptr;
    QLabel       *m_remarkLabel = nullptr;
    QLabel       *m_verLabel    = nullptr;
    QLabel       *m_fwVerLabel  = nullptr;
    QLabel       *m_baseIcon    = nullptr;
    QPushButton  *m_syncBtn       = nullptr;
    QPushButton  *m_connectBtn    = nullptr;
    QGraphicsDropShadowEffect *m_shadow = nullptr;

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

    friend class DeviceCardProbe;
    friend class DeviceCardLayout;
    friend class DeviceCardDisplay;
};
