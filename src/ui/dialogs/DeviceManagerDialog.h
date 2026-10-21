/**
 * @file    DeviceManagerDialog.h
 * @layer   GUILayer
 * @brief   Multi-device manager dialog displaying device cards grid, status, and network configuration.
 * @deps    QDialog, QStackedWidget, QGridLayout, DeviceCard.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QDialog>
#include <QStackedWidget>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QList>
#include "DeviceCard.h"

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
    void onNetworkConfig(int index);
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
    QLineEdit      *m_nameEdit        = nullptr;
    QWidget        *m_nameRow         = nullptr;
    QLabel         *m_addStatus       = nullptr;
    QPushButton    *m_confirmBtn      = nullptr;
    QPushButton    *m_cancelBtn       = nullptr;

    QList<DeviceCard *> m_cards;

    friend class DeviceManagerAddPanel;
    friend class DeviceManagerConnectionHandler;
};
