/**
 * @file    CreateMapTeleopPanel.cpp
 * @layer   GUI
 * @brief   Teleoperation control pad and HUD metrics panel implementation.
 * @deps    CreateMapTeleopPanel.h, TeleopController.h, MqttService.h, AppConfig.h, Toast.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "CreateMapTeleopPanel.h"
#include "core/TeleopController.h"
#include "network/MqttService.h"
#include "app/AppConfig.h"
#include "ui/common/Toast.h"
#include "CreateMapStyle.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>

CreateMapTeleopPanel::CreateMapTeleopPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void CreateMapTeleopPanel::setupUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto *hudGroup = new QGroupBox("设备状态", this);
    auto *hudLayout = new QFormLayout(hudGroup);
    m_linearLabel  = new QLabel("-- m/s", this);
    m_angularLabel = new QLabel("-- rad/s", this);
    m_teleopLabel  = new QLabel("已停用", this);
    m_brakeLabel   = new QLabel("松开", this);
    hudLayout->addRow("线速度:", m_linearLabel);
    hudLayout->addRow("角速度:", m_angularLabel);
    hudLayout->addRow("遥控:",   m_teleopLabel);
    hudLayout->addRow("刹车:",   m_brakeLabel);
    root->addWidget(hudGroup);

    auto *teleopGroup = new QGroupBox("遥控控制", this);
    auto *teleopLayout = new QVBoxLayout(teleopGroup);

    m_teleopEnableBtn = new QPushButton("启用遥控", this);
    m_teleopEnableBtn->setCheckable(true);
    m_teleopEnableBtn->setFocusPolicy(Qt::NoFocus);
    m_teleopEnableBtn->setChecked(TeleopController::instance().isActive());
    m_teleopEnableBtn->setText(TeleopController::instance().isActive() ? "停用遥控" : "启用遥控");
    m_teleopLabel->setText(TeleopController::instance().isActive() ? "已启用" : "已停用");

    connect(m_teleopEnableBtn, &QPushButton::clicked, this, [this]() {
        bool target = !TeleopController::instance().isActive();
        if (!MqttService::instance().isConnected()) {
            Toast::error(this, "MQTT 未连接，无法启用遥控");
            QMetaObject::invokeMethod(this, [this]() { m_teleopEnableBtn->setChecked(TeleopController::instance().isActive()); }, Qt::QueuedConnection);
            return;
        }
        if (AppConfig::instance().agvId().isEmpty()) {
            Toast::error(this, "未选择设备，无法启用遥控");
            QMetaObject::invokeMethod(this, [this]() { m_teleopEnableBtn->setChecked(TeleopController::instance().isActive()); }, Qt::QueuedConnection);
            return;
        }
        TeleopController::instance().setActive(target);
    });
    teleopLayout->addWidget(m_teleopEnableBtn);

    auto *dirLayout = new QGridLayout;
    auto *forwardBtn  = new QPushButton("前进", this);
    auto *backwardBtn = new QPushButton("后退", this);
    auto *leftBtn     = new QPushButton("左转", this);
    auto *rightBtn    = new QPushButton("右转", this);
    auto *brakeBtn    = new QPushButton("刹车", this);
    brakeBtn->setStyleSheet(CreateMapStyle::kBrakeBtn);

    for (auto btn : {forwardBtn, backwardBtn, leftBtn, rightBtn, brakeBtn}) btn->setFocusPolicy(Qt::NoFocus);

    dirLayout->addWidget(forwardBtn,  0, 1);
    dirLayout->addWidget(leftBtn,     1, 0);
    dirLayout->addWidget(brakeBtn,    1, 1);
    dirLayout->addWidget(rightBtn,    1, 2);
    dirLayout->addWidget(backwardBtn, 2, 1);

    auto setupDirBtn = [](QPushButton *btn, int key) {
        QObject::connect(btn, &QPushButton::pressed,  [key]() { TeleopController::instance().setVirtualKey(key, true); });
        QObject::connect(btn, &QPushButton::released, [key]() { TeleopController::instance().setVirtualKey(key, false); });
    };
    setupDirBtn(forwardBtn,  Qt::Key_W);
    setupDirBtn(backwardBtn, Qt::Key_S);
    setupDirBtn(leftBtn,     Qt::Key_A);
    setupDirBtn(rightBtn,    Qt::Key_D);
    setupDirBtn(brakeBtn,    Qt::Key_Space);
    teleopLayout->addLayout(dirLayout);

    auto *kbHint = new QLabel("<small style='color:#7a8a99;'>键盘: W A S D 移动 &nbsp;|&nbsp; Space 刹车<br>Ctrl 加速 &nbsp;|&nbsp; P 升降上 &nbsp;|&nbsp; L 升降下</small>", this);
    kbHint->setTextFormat(Qt::RichText);
    kbHint->setWordWrap(true);
    kbHint->setAlignment(Qt::AlignCenter);
    teleopLayout->addWidget(kbHint);

    root->addWidget(teleopGroup);
    root->addStretch();
}

void CreateMapTeleopPanel::setLinearVelocity(double lv)
{
    if (m_linearLabel) m_linearLabel->setText(QString::number(lv, 'f', 2) + " m/s");
}

void CreateMapTeleopPanel::setAngularVelocity(double av)
{
    if (m_angularLabel) m_angularLabel->setText(QString::number(av, 'f', 2) + " rad/s");
}

void CreateMapTeleopPanel::setTeleopActive(bool active)
{
    if (!m_teleopEnableBtn) return;
    QSignalBlocker blocker(m_teleopEnableBtn);
    m_teleopEnableBtn->setChecked(active);
    m_teleopEnableBtn->setText(active ? "停用遥控" : "启用遥控");
    if (m_teleopLabel) m_teleopLabel->setText(active ? "已启用" : "已停用");
}

void CreateMapTeleopPanel::setBrakeActive(bool brake)
{
    if (m_brakeLabel) m_brakeLabel->setText(brake ? "按下(键盘)" : "松开");
}
