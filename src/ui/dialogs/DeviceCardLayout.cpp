/**
 * @file    DeviceCardLayout.cpp
 * @layer   GUILayer
 * @brief   Implementation of DeviceCard child widget assembly.
 * @deps    DeviceCardLayout.h, DeviceCard.h, DeviceCardWidgets.h, AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "DeviceCardLayout.h"
#include "DeviceCard.h"
#include "DeviceCardWidgets.h"
#include "AppConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QIcon>

void DeviceCardLayout::buildHeader(DeviceCard *card, QVBoxLayout *root)
{
    card->m_hdrWidget = new QWidget;
    card->m_hdrWidget->setFixedHeight(32);
    auto *hl = new QHBoxLayout(card->m_hdrWidget);
    hl->setContentsMargins(10, 0, 8, 0);
    hl->setSpacing(6);

    auto *chkBox = new QLabel;
    chkBox->setFixedSize(12, 12);
    chkBox->setStyleSheet("border:1.5px solid #ffffff; border-radius:2px; background:transparent;");
    hl->addWidget(chkBox);

    card->m_onlineLabel = new QLabel("未连接");
    card->m_onlineLabel->setStyleSheet("color:#94a3b8; font-size:12px; font-weight:bold; background:transparent;");
    hl->addWidget(card->m_onlineLabel);

    card->m_onlineDot = new QLabel;
    card->m_onlineDot->setFixedSize(10, 10);
    card->m_onlineDot->setStyleSheet("background:#718096; border-radius:5px;");
    hl->addWidget(card->m_onlineDot);

    card->m_robLabel = new QLabel("机器人");
    card->m_robLabel->setStyleSheet("color:#ffffff; font-size:12px; font-weight:500; background:transparent;");
    hl->addWidget(card->m_robLabel);

    hl->addStretch();

    card->m_configBtn = new QPushButton;
    card->m_configBtn->setFlat(true);
    card->m_configBtn->setFixedSize(22, 22);
    card->m_configBtn->setIcon(QIcon(DeviceCardWidgets::makeSlidersIcon(18, QColor("#ffffff"))));
    card->m_configBtn->setIconSize(QSize(16, 16));
    card->m_configBtn->setCursor(Qt::PointingHandCursor);
    card->m_configBtn->setToolTip("网络参数配置 (SSH)");
    card->m_configBtn->setStyleSheet("QPushButton{background:transparent;border:none;border-radius:3px;}"
                                     "QPushButton:hover{background:rgba(255,255,255,0.25);}");
    QObject::connect(card->m_configBtn, &QPushButton::clicked, card, [card]{ emit card->networkConfigClicked(card->m_index); });
    hl->addWidget(card->m_configBtn);

    card->m_batteryBadge = new BatteryBadge;
    hl->addWidget(card->m_batteryBadge);

    auto *delBtn = new QPushButton("✕");
    delBtn->setFixedSize(18, 18);
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setToolTip("从列表中移除该设备");
    delBtn->setStyleSheet(
        "QPushButton{background:transparent;color:rgba(255,255,255,0.7);border:none;font-size:11px;font-weight:bold;}"
        "QPushButton:hover{background:rgba(239,68,68,0.85);color:#fff;border-radius:3px;}");
    QObject::connect(delBtn, &QPushButton::clicked, card, [card]{ emit card->removeClicked(card->m_index); });
    hl->addWidget(delBtn);

    root->addWidget(card->m_hdrWidget);
}

void DeviceCardLayout::buildBody(DeviceCard *card, QVBoxLayout *root)
{
    auto *body = new QWidget;
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *bl = new QVBoxLayout(body);
    bl->setContentsMargins(14, 10, 14, 6);
    bl->setSpacing(6);

    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(6);

    card->m_robotIcon = new QLabel;
    card->m_robotIcon->setFixedSize(26, 26);
    card->m_robotIcon->setPixmap(DeviceCardWidgets::makeRobotHeadIcon(26, QColor("#00e5ff")));
    card->m_robotIcon->setStyleSheet("background:transparent;");
    topRow->addWidget(card->m_robotIcon);

    card->m_ipLabel = new QLabel(card->m_profile.ip);
    card->m_ipLabel->setStyleSheet("color:#ffffff; font-size:13px; font-weight:bold; font-family:Consolas,monospace; background:transparent;");
    topRow->addWidget(card->m_ipLabel);

    topRow->addStretch();

    card->m_typeTag = new QLabel("潜伏底盘");
    card->m_typeTag->setStyleSheet(
        "color:#00e5ff; font-size:11px; font-weight:bold; background:transparent;"
        "border:1.2px solid #00e5ff; border-radius:2px; padding:1px 5px;");
    topRow->addWidget(card->m_typeTag);

    bl->addLayout(topRow);

    auto *metricsRow = new QHBoxLayout;
    metricsRow->setSpacing(8);

    card->m_confBadge = new QLabel("--");
    card->m_confBadge->setAlignment(Qt::AlignCenter);
    card->m_confBadge->setFixedHeight(22);
    card->m_confBadge->setStyleSheet(
        "background:#ff6600; color:#ffffff; font-size:12px; font-weight:bold; "
        "border-radius:3px; padding:0 8px; min-width:42px;");
    metricsRow->addWidget(card->m_confBadge);

    card->m_latencyLabel = new QLabel("--");
    card->m_latencyLabel->setStyleSheet("color:#64748b; font-size:12px; font-weight:bold; font-family:Consolas,monospace; background:transparent;");
    metricsRow->addWidget(card->m_latencyLabel);

    metricsRow->addStretch();
    bl->addLayout(metricsRow);

    auto *infoLay = new QVBoxLayout;
    infoLay->setSpacing(4);
    infoLay->setContentsMargins(0, 2, 0, 0);

    auto mkRow = [](const QString &keyTxt, QLabel *&valLbl) {
        auto *row = new QHBoxLayout;
        row->setSpacing(4);
        auto *kl = new QLabel(keyTxt);
        kl->setStyleSheet("color:#cbd5e1; font-size:12px; background:transparent;");
        row->addWidget(kl);

        valLbl = new QLabel("--");
        valLbl->setStyleSheet("color:#f1f5f9; font-size:12px; font-weight:500; background:transparent;");
        row->addWidget(valLbl, 1);
        return row;
    };

    infoLay->addLayout(mkRow("地图:", card->m_mapLabel));
    infoLay->addLayout(mkRow("名称:", card->m_nameLabel));
    infoLay->addLayout(mkRow("备注:", card->m_remarkLabel));
    infoLay->addLayout(mkRow("版本:", card->m_verLabel));
    infoLay->addLayout(mkRow("固件版本:", card->m_fwVerLabel));

    bl->addLayout(infoLay);

    card->m_baseIcon = new QLabel;
    card->m_baseIcon->setFixedSize(26, 20);
    card->m_baseIcon->setPixmap(DeviceCardWidgets::makeBaseStationIcon(26, 20, QColor("#00e5ff")));
    card->m_baseIcon->setStyleSheet("background:transparent;");
    bl->addWidget(card->m_baseIcon, 0, Qt::AlignLeft);

    bl->addStretch();
    root->addWidget(body, 1);
}

void DeviceCardLayout::buildFooter(DeviceCard *card, QVBoxLayout *root)
{
    auto *ftr = new QWidget;
    ftr->setFixedHeight(42);
    ftr->setStyleSheet("background:transparent; border:none;");
    auto *fl = new QHBoxLayout(ftr);
    fl->setContentsMargins(10, 0, 10, 6);
    fl->setSpacing(10);

    card->m_syncBtn = new QPushButton("✓ 已同步");
    card->m_syncBtn->setCursor(Qt::PointingHandCursor);
    card->m_syncBtn->setStyleSheet(
        "QPushButton{"
        "  background:#333842; color:#38bdf8; border:1px solid #4a5160;"
        "  border-radius:3px; padding:4px 10px; font-size:11px; font-weight:bold;"
        "}"
        "QPushButton:hover{background:#3f4756;}");
    fl->addWidget(card->m_syncBtn);

    fl->addStretch();

    card->m_connectBtn = new QPushButton("连接", ftr);
    card->m_connectBtn->setObjectName("deviceCardConnectBtn");
    card->m_connectBtn->setProperty("noClickEffect", true);
    card->m_connectBtn->setGraphicsEffect(nullptr);
    card->m_connectBtn->setCursor(Qt::PointingHandCursor);
    card->m_connectBtn->setFixedHeight(26);
    card->m_connectBtn->setMinimumWidth(72);
    card->m_connectBtn->setVisible(true);
    card->m_connectBtn->setEnabled(true);
    card->m_connectBtn->setStyleSheet(
        "QPushButton#deviceCardConnectBtn{"
        "  background:#0284c7; color:#ffffff; border:1px solid #0369a1;"
        "  border-radius:3px; padding:4px 14px; font-size:11px; font-weight:bold;"
        "}"
        "QPushButton#deviceCardConnectBtn:hover{background:#0ea5e9;}"
        "QPushButton#deviceCardConnectBtn:pressed{background:#0369a1;}");
    QObject::connect(card->m_connectBtn, &QPushButton::clicked, card, [card]{
        bool isConnected = (AppConfig::instance().currentDeviceIndex() == card->m_index);
        if (isConnected) {
            emit card->disconnectClicked(card->m_index);
        } else {
            emit card->connectClicked(card->m_index);
        }
    });
    fl->addWidget(card->m_connectBtn);

    root->addWidget(ftr);
}
