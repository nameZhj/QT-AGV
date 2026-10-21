/**
 * @file    ManualControlConfigDialog.cpp
 * @module  GUILayer
 * @brief   Implementation of manual control shortcut and sensitivity config dialog.
 * @deps    ManualControlConfigDialog.h, TeleopController.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "ManualControlConfigDialog.h"
#include "core/TeleopController.h"
#include "network/ApiService.h"
#include "core/MotionConfigManager.h"
#include <QDebug>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QFrame>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ManualControlConfigDialog::ManualControlConfigDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("设置手动控制参数");
    setFixedSize(460, 310);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    setupUI();

    // 读取当前速度配置并严格限制在全局最大速度之内
    double curLinear = TeleopController::instance().baseLinearSpeed();
    double curAngularRad = TeleopController::instance().baseAngularSpeed();
    double curAngularDeg = curAngularRad * 180.0 / M_PI;

    const double globalMaxLinear = MotionConfigManager::instance().maxSpeed();
    const double globalMaxAngular = MotionConfigManager::instance().maxAngularSpeedDeg();

    if (m_linearSpin) {
        m_linearSpin->setValue(qBound(0.01, curLinear, globalMaxLinear));
    }
    if (m_angularSpin) {
        m_angularSpin->setValue(qBound(1.00, curAngularDeg, globalMaxAngular));
    }
}

void ManualControlConfigDialog::setupUI()
{
    // 整体深色对话框背景
    setStyleSheet(R"(
        QDialog {
            background-color: #24272e;
            color: #d8dde3;
            font-family: "Microsoft YaHei UI", sans-serif;
            font-size: 13px;
        }
        QLabel {
            color: #c8d0d8;
            font-size: 13px;
        }
        QDoubleSpinBox {
            background-color: #1a1d22;
            color: #40a9ff;
            border: 1px solid #007acc;
            border-radius: 3px;
            padding: 3px 6px;
            font-size: 13px;
            font-family: "Consolas", monospace;
        }
        QDoubleSpinBox:focus {
            border: 1px solid #1890ff;
        }
        QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
            width: 18px;
            background-color: #2c313a;
            border: none;
        }
        QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover {
            background-color: #3b424e;
        }
    )");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 14, 16, 14);
    mainLayout->setSpacing(12);

    // ── 顶部单选项卡头部：仅保留"设置速度" ──
    auto *tabHeaderLayout = new QHBoxLayout;
    tabHeaderLayout->setSpacing(0);
    tabHeaderLayout->setContentsMargins(0, 0, 0, 0);

    auto *speedTabLabel = new QLabel("设置速度");
    speedTabLabel->setStyleSheet(
        "QLabel { background: #2f3542; color: #ffffff; padding: 6px 16px; "
        "border-top-left-radius: 4px; border-top-right-radius: 4px; font-weight: bold; }");
    tabHeaderLayout->addWidget(speedTabLabel);
    tabHeaderLayout->addStretch();
    mainLayout->addLayout(tabHeaderLayout);

    // ── 参数配置主体卡片 ──
    auto *cardFrame = new QFrame(this);
    cardFrame->setStyleSheet(
        "QFrame#cardFrame { background: #2b303b; border: 1px solid #3d4554; border-radius: 4px; }");
    cardFrame->setObjectName("cardFrame");

    auto *cardLayout = new QVBoxLayout(cardFrame);
    cardLayout->setContentsMargins(18, 16, 18, 16);
    cardLayout->setSpacing(14);

    // 1. 全局最大速度与手动最大速度
    auto *linearGrid = new QGridLayout;
    linearGrid->setHorizontalSpacing(10);
    linearGrid->setVerticalSpacing(10);

    const double curGlobalLinear = MotionConfigManager::instance().maxSpeed();
    const double curGlobalAngular = MotionConfigManager::instance().maxAngularSpeedDeg();

    auto *globalLinearTitle = new QLabel("全局最大速度:");
    auto *globalLinearVal   = new QLabel(QString::asprintf("%.3f", curGlobalLinear));
    globalLinearVal->setStyleSheet("color: #ffffff; font-weight: bold; font-family: Consolas;");
    auto *globalLinearUnit  = new QLabel("m/s");
    globalLinearUnit->setStyleSheet("color: #8898a8;");

    linearGrid->addWidget(globalLinearTitle, 0, 0);
    linearGrid->addWidget(globalLinearVal,   0, 1);
    linearGrid->addWidget(globalLinearUnit,  0, 2);

    auto *manualLinearTitle = new QLabel("最大速度:");
    m_linearSpin = new QDoubleSpinBox(this);
    m_linearSpin->setDecimals(2);
    m_linearSpin->setRange(0.01, curGlobalLinear);
    m_linearSpin->setSingleStep(0.05);
    m_linearSpin->setValue(qMin(0.50, curGlobalLinear));

    auto *manualLinearUnit  = new QLabel("m/s");
    manualLinearUnit->setStyleSheet("color: #8898a8;");

    linearGrid->addWidget(manualLinearTitle, 1, 0);
    linearGrid->addWidget(m_linearSpin,       1, 1);
    linearGrid->addWidget(manualLinearUnit,  1, 2);
    linearGrid->setColumnStretch(1, 1);

    cardLayout->addLayout(linearGrid);

    // 分隔横线
    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #3d4554; background-color: #3d4554; height: 1px; border: none;");
    cardLayout->addWidget(sep);

    // 2. 全局最大转速度与手动最大转速度
    auto *angularGrid = new QGridLayout;
    angularGrid->setHorizontalSpacing(10);
    angularGrid->setVerticalSpacing(10);

    auto *globalAngularTitle = new QLabel("全局最大转速度:");
    auto *globalAngularVal   = new QLabel(QString::asprintf("%.3f", curGlobalAngular));
    globalAngularVal->setStyleSheet("color: #ffffff; font-weight: bold; font-family: Consolas;");
    auto *globalAngularUnit  = new QLabel("°/s");
    globalAngularUnit->setStyleSheet("color: #8898a8;");

    angularGrid->addWidget(globalAngularTitle, 0, 0);
    angularGrid->addWidget(globalAngularVal,   0, 1);
    angularGrid->addWidget(globalAngularUnit,  0, 2);

    auto *manualAngularTitle = new QLabel("最大转速度:");
    m_angularSpin = new QDoubleSpinBox(this);
    m_angularSpin->setDecimals(2);
    m_angularSpin->setRange(1.00, curGlobalAngular);
    m_angularSpin->setSingleStep(5.00);
    m_angularSpin->setValue(qMin(45.00, curGlobalAngular));

    auto *manualAngularUnit  = new QLabel("°/s");
    manualAngularUnit->setStyleSheet("color: #8898a8;");

    angularGrid->addWidget(manualAngularTitle, 1, 0);
    angularGrid->addWidget(m_angularSpin,       1, 1);
    angularGrid->addWidget(manualAngularUnit,  1, 2);
    angularGrid->setColumnStretch(1, 1);

    cardLayout->addLayout(angularGrid);

    mainLayout->addWidget(cardFrame);

    // ── 确定按钮 ──
    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    m_confirmBtn = new QPushButton("确 定", this);
    m_confirmBtn->setFixedSize(110, 32);
    m_confirmBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #0088ff;
            color: #ffffff;
            font-size: 13px;
            font-weight: bold;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #1a94ff;
        }
        QPushButton:pressed {
            background-color: #0070d6;
        }
    )");
    connect(m_confirmBtn, &QPushButton::clicked, this, &ManualControlConfigDialog::onConfirm);
    btnLayout->addWidget(m_confirmBtn);
    btnLayout->addStretch();

    mainLayout->addLayout(btnLayout);

    // ── 底部安全提示文字 (粉红色) ──
    auto *noteLabel = new QLabel("注: 安全起见，实际速度受全局最大速度限制，不可超速。", this);
    noteLabel->setStyleSheet("color: #d63384; font-size: 11px; padding-top: 2px;");
    mainLayout->addWidget(noteLabel);
}

void ManualControlConfigDialog::onConfirm()
{
    const double rawLinear = m_linearSpin ? m_linearSpin->value() : 0.50;
    const double rawAngularDeg = m_angularSpin ? m_angularSpin->value() : 45.00;

    // 严格限制不可超速
    const double linear = MotionConfigManager::instance().clampLinearSpeed(rawLinear);
    const double angularDeg = MotionConfigManager::instance().clampSignedAngularSpeedDeg(rawAngularDeg);
    const double angularRad = angularDeg * M_PI / 180.0;

    // 1. 立即保存并对运动生效 (MQTT与本地遥控)
    TeleopController::instance().setBaseSpeeds(linear, angularRad);

    // 2. 调用控制器 8061 端口的速度设置接口
    ApiService::instance().setRobotSpeed(linear, angularRad, [linear, angularRad](int code, const QJsonObject &resp) {
        qDebug() << "[ManualControlConfigDialog] 8061 setRobotSpeed result:" << code << "linear:" << linear << "angular:" << angularRad << resp;
    });

    accept();
}
