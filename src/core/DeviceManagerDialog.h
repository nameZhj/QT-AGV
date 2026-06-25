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

signals:
    void connectClicked(int index);
    void removeClicked(int index);

private:
    void checkOnlineStatus(const QString &ip, const QString &carId);
    void setOnlineUI(bool online);
    void finishProbe();

    int m_index = 0;
    QPushButton *m_connectBtn = nullptr;
    QLabel      *m_onlineDot   = nullptr;
    QLabel      *m_onlineLabel = nullptr;

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
    QLabel         *m_addStatus       = nullptr;
    QPushButton    *m_confirmBtn      = nullptr;
    QPushButton    *m_cancelBtn       = nullptr;

    QList<DeviceCard *> m_cards;
};

#endif // DEVICEMANAGERDIALOG_H
