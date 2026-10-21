/**
 * @file    DeviceManagerDialog.cpp
 * @layer   GUILayer
 * @brief   Multi-device manager dialog shell, card layout, and telemetry synchronization.
 * @deps    DeviceManagerDialog.h, DeviceCard.h, DeviceManagerAddPanel.h, DeviceManagerConnectionHandler.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "DeviceManagerDialog.h"
#include "DeviceCard.h"
#include "DeviceCardWidgets.h"
#include "DeviceManagerAddPanel.h"
#include "DeviceManagerConnectionHandler.h"
#include "AppConfig.h"
#include "AppConstants.h"
#include "MqttService.h"
#include "RobotInfoManager.h"
#include "Toast.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QPointer>
#include "DeviceManagerStyle.h"

DeviceManagerDialog::DeviceManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(AppConstants::AppTitle);
    setModal(true);
    setStyleSheet(DeviceManagerStyle::kDlgStyle);

    if (parent) {
        resize(parent->size());
        move(parent->pos());
    } else {
        QRect sg = QApplication::primaryScreen()->availableGeometry();
        resize(sg.width() * 88 / 100, sg.height() * 88 / 100);
        move((sg.width() - width()) / 2, (sg.height() - height()) / 2);
    }

    setupUI();
    refreshCards();

    connect(&MqttService::instance(), &MqttService::connected, this, [this]() {
        int curIdx = AppConfig::instance().currentDeviceIndex();
        if (curIdx >= 0 && curIdx < m_cards.size() && m_cards[curIdx]) {
            m_cards[curIdx]->setBusy(false);
            m_cards[curIdx]->probeDeviceStatus(AppConfig::instance().serverIp());
            setStatus(QString("设备 [%1] 已成功连接").arg(AppConfig::instance().serverIp()), "#22c55e");
        }
    });

    connect(&MqttService::instance(), &MqttService::disconnected, this, [this]() {
        for (auto *c : m_cards) {
            if (c) c->probeDeviceStatus(AppConfig::instance().serverIp());
        }
    });

    connect(&RobotInfoManager::instance(), &RobotInfoManager::currentRobotInfoUpdated, this, [this]() {
        int curIdx = AppConfig::instance().currentDeviceIndex();
        if (curIdx >= 0 && curIdx < m_cards.size() && m_cards[curIdx]) {
            m_cards[curIdx]->probeDeviceStatus(AppConfig::instance().serverIp());
        }
    });
}

void DeviceManagerDialog::setupUI()
{
    m_stack = new QStackedWidget(this);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(m_stack);

    m_mainPanel = new QWidget;
    auto *mainLay = new QHBoxLayout(m_mainPanel);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->setSpacing(0);

    auto *content = new QWidget;
    content->setStyleSheet("background:#1a1d23;");
    auto *cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);

    auto *topBar = new QWidget;
    topBar->setFixedHeight(52);
    topBar->setStyleSheet("background:#12151a; border-bottom:1px solid #2d3340;");
    setupTopBar(topBar);
    cl->addWidget(topBar);

    auto *cardsWidget = new QWidget;
    cardsWidget->setStyleSheet("background:#1a1d23;");
    setupCardArea(cardsWidget);
    cl->addWidget(cardsWidget, 1);

    m_statusLabel = new QLabel;
    m_statusLabel->setFixedHeight(26);
    m_statusLabel->setStyleSheet(
        "background:#0d1117; color:#8898a8; font-size:11px; padding:0 16px;"
        "border-top:1px solid #2d3340;");
    cl->addWidget(m_statusLabel);

    mainLay->addWidget(content, 1);
    m_stack->addWidget(m_mainPanel);

    setupAddPanel();
    m_stack->setCurrentWidget(m_mainPanel);
}

void DeviceManagerDialog::setupTopBar(QWidget *topBar)
{
    auto *lay = new QHBoxLayout(topBar);
    lay->setContentsMargins(12, 0, 12, 0);
    lay->setSpacing(8);
    lay->addStretch();

    auto mkToolBtn = [&](const QString &text, bool primary = false) {
        auto *b = new QPushButton(text);
        b->setFixedHeight(32);
        b->setCursor(Qt::PointingHandCursor);
        if (primary) {
            b->setStyleSheet(DeviceManagerStyle::kBtnPrimary);
        } else {
            b->setStyleSheet(DeviceManagerStyle::kBtnOutline);
        }
        lay->addWidget(b);
        return b;
    };

    auto *refreshAllBtn = mkToolBtn("⟳  刷新全部");
    auto *addDevBtn     = mkToolBtn("＋  添加设备", true);
    auto *enterHomeBtn  = mkToolBtn("进入主页 →", false);
    enterHomeBtn->setStyleSheet(DeviceManagerStyle::kBtnEnterHome);

    connect(refreshAllBtn, &QPushButton::clicked, this, &DeviceManagerDialog::onRefreshAll);
    connect(addDevBtn,     &QPushButton::clicked, this, &DeviceManagerDialog::onAddDevice);
    connect(enterHomeBtn,  &QPushButton::clicked, this, [this]() {
        if (AppConfig::instance().currentDeviceIndex() < 0) {
            Toast::warning(this, "请先选择并点击「连接」设备");
            return;
        }
        accept();
    });
}

void DeviceManagerDialog::setupCardArea(QWidget *area)
{
    auto *lay = new QVBoxLayout(area);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet("QScrollArea{background:#13161c; border:none;}");

    m_cardsContainer = new QWidget;
    m_cardsContainer->setStyleSheet("background:#13161c;");
    m_cardsGrid = new QGridLayout(m_cardsContainer);
    m_cardsGrid->setContentsMargins(20, 20, 20, 20);
    m_cardsGrid->setSpacing(20);
    m_cardsGrid->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_scrollArea->setWidget(m_cardsContainer);
    lay->addWidget(m_scrollArea, 1);
}

void DeviceManagerDialog::setupAddPanel()
{
    m_addPanel = DeviceManagerAddPanel::buildAddPanel(this);
    m_stack->addWidget(m_addPanel);
}

void DeviceManagerDialog::refreshCards()
{
    for (DeviceCard *c : m_cards) {
        m_cardsGrid->removeWidget(c);
        c->deleteLater();
    }
    m_cards.clear();

    while (m_cardsGrid->count() > 0) {
        QLayoutItem *item = m_cardsGrid->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    const QList<DeviceProfile> list = AppConfig::instance().deviceList();
    if (list.isEmpty()) {
        auto *emptyLbl = new QLabel("暂无设备，点击「＋ 添加设备」添加");
        emptyLbl->setAlignment(Qt::AlignCenter);
        emptyLbl->setStyleSheet("color:#556677; font-size:14px;");
        m_cardsGrid->addWidget(emptyLbl, 0, 0, 1, 3);
        return;
    }

    const int cols = 3;
    for (int i = 0; i < list.size(); ++i) {
        auto *card = new DeviceCard(list[i], i, m_cardsContainer);
        connect(card, &DeviceCard::doubleClicked,        this, &DeviceManagerDialog::onCardDoubleClicked);
        connect(card, &DeviceCard::connectClicked,       this, &DeviceManagerDialog::onConnectCard);
        connect(card, &DeviceCard::disconnectClicked,    this, &DeviceManagerDialog::onDisconnectCard);
        connect(card, &DeviceCard::removeClicked,        this, &DeviceManagerDialog::onRemoveCard);
        connect(card, &DeviceCard::networkConfigClicked, this, &DeviceManagerDialog::onNetworkConfig);
        m_cardsGrid->addWidget(card, i / cols, i % cols);
        m_cards.append(card);
    }

    QPointer<DeviceManagerDialog> self(this);
    DeviceCardWidgets::fetchGlobalMapList([self]() {
        if (!self) return;
        for (auto *c : self->m_cards) {
            if (c) c->updateCardDisplay();
        }
    });
}

void DeviceManagerDialog::onRefreshAll()
{
    refreshCards();
    setStatus("已刷新设备列表", "#5cb85c");
}

void DeviceManagerDialog::onAddDevice() { DeviceManagerAddPanel::onAddDevice(this); }
void DeviceManagerDialog::onCancelAdd() { DeviceManagerAddPanel::onCancelAdd(this); }
void DeviceManagerDialog::onConfirmAdd() { DeviceManagerAddPanel::onConfirmAdd(this); }

void DeviceManagerDialog::onCardDoubleClicked(int index) { connectDevice(index); }
void DeviceManagerDialog::onConnectCard(int index) { DeviceManagerConnectionHandler::onConnectCard(this, index); }
void DeviceManagerDialog::onDisconnectCard(int index) { DeviceManagerConnectionHandler::onDisconnectCard(this, index); }
void DeviceManagerDialog::onRemoveCard(int index) { DeviceManagerConnectionHandler::onRemoveCard(this, index); }
void DeviceManagerDialog::connectDevice(int index) { DeviceManagerConnectionHandler::connectDevice(this, index); }
void DeviceManagerDialog::onNetworkConfig(int index) { DeviceManagerConnectionHandler::onNetworkConfig(this, index); }

void DeviceManagerDialog::setStatus(const QString &msg, const QString &color)
{
    if (!m_statusLabel) return;
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(
        QString("background:#0d1117; color:%1; font-size:11px;"
                "padding:0 16px; border-top:1px solid #2d3340;").arg(color));
}
