/**
 * @file    MotionDialogTabs.cpp
 * @layer   GUILayer
 * @brief   Builder for translation and rotation tabs in MotionDialog.
 * @deps    MotionDialogTabs.h, MotionDialog.h, MotionDialogStyle.h, MotionConfigManager.h
 * @note    Refactored for AI readability; visual layout & widget hierarchy invariant.
 */

#include "MotionDialogTabs.h"
#include "MotionDialog.h"
#include "MotionDialogStyle.h"
#include "core/MotionConfigManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QButtonGroup>

void MotionDialogTabs::updateParamToolTips(MotionDialog *dlg) {
  const auto &mc = MotionConfigManager::instance();
  const double limitMaxLinear = mc.paramMax(QStringLiteral("max_speed"), 1.0);
  const double limitMaxAngular = mc.paramMax(QStringLiteral("max_angular_speed"), 60.0);

  const QString distTip = QStringLiteral("平动距离\n范围: 0.000 ~ 100.000 m (数值不能为负数，距离应大于 0m)");
  if (dlg->m_transDistSpin) dlg->m_transDistSpin->setToolTip(distTip);
  if (dlg->m_transDistLbl) dlg->m_transDistLbl->setToolTip(distTip);

  const QString vxTip = QStringLiteral("前后线速度 vx\n范围: -%1 ~ %1 m/s (可以为负值，正数前进，负数后退)").arg(limitMaxLinear, 0, 'f', 2);
  if (dlg->m_transVxSpin) dlg->m_transVxSpin->setToolTip(vxTip);
  if (dlg->m_transVxLbl) dlg->m_transVxLbl->setToolTip(vxTip);

  const QString vyTip = QStringLiteral("左右横移速度 vy\n范围: -%1 ~ %1 m/s (可以为负值，全向轮机型支持，正数向左，负数向右)").arg(limitMaxLinear, 0, 'f', 2);
  if (dlg->m_transVySpin) dlg->m_transVySpin->setToolTip(vyTip);
  if (dlg->m_transVyLbl) dlg->m_transVyLbl->setToolTip(vyTip);

  const QString angleTip = QStringLiteral("旋转角度\n范围: 0.000° ~ 360.000° (数值不能为负数，角度应大于 0°)");
  if (dlg->m_rotAngleSpin) dlg->m_rotAngleSpin->setToolTip(angleTip);
  if (dlg->m_rotAngleLbl) dlg->m_rotAngleLbl->setToolTip(angleTip);

  const QString speedTip = QStringLiteral("旋转角速度\n范围: -%1°/s ~ %1°/s (可以为负值，正数逆时针，负数顺时针)").arg(limitMaxAngular, 0, 'f', 1);
  if (dlg->m_rotSpeedSpin) dlg->m_rotSpeedSpin->setToolTip(speedTip);
  if (dlg->m_rotSpeedLbl) dlg->m_rotSpeedLbl->setToolTip(speedTip);

  if (dlg->m_odomModeRadio) dlg->m_odomModeRadio->setToolTip(QStringLiteral("里程模式：根据轮式编码器累积计算移动距离"));
  if (dlg->m_locModeRadio) dlg->m_locModeRadio->setToolTip(QStringLiteral("定位模式：根据激光SLAM/全局定位坐标计算移动距离"));
}

QWidget *MotionDialogTabs::createTranslationTab(MotionDialog *dlg) {
  auto *tab = new QWidget(dlg);
  auto *layout = new QVBoxLayout(tab);
  layout->setContentsMargins(0, 4, 0, 0);
  layout->setSpacing(6);

  auto *panel = new QFrame(dlg);
  panel->setStyleSheet(MotionDialogStyle::kPanelStyle);
  auto *panelLayout = new QVBoxLayout(panel);
  panelLayout->setContentsMargins(14, 10, 14, 12);
  panelLayout->setSpacing(8);

  auto *descLabel = new QLabel("机器人平动，以固定速度直线运动固定距离", panel);
  descLabel->setStyleSheet("border: none; color: #a6b0be; font-size: 12px;");
  panelLayout->addWidget(descLabel);

  auto *formLayout = new QGridLayout;
  formLayout->setHorizontalSpacing(10);
  formLayout->setVerticalSpacing(8);

  // 1. 设置距离
  dlg->m_transDistLbl = new QLabel("设置距离：", panel);
  dlg->m_transDistLbl->setStyleSheet("border: none; color: #c5cbd5;");
  dlg->m_transDistSpin = new QDoubleSpinBox(panel);
  dlg->m_transDistSpin->setRange(0.0, 100.0);
  dlg->m_transDistSpin->setDecimals(3);
  dlg->m_transDistSpin->setSingleStep(0.1);
  dlg->m_transDistSpin->setValue(1.000);
  auto *distUnitLbl = new QLabel("m", panel);
  distUnitLbl->setStyleSheet("border: none; color: #a6b0be; font-size: 12px;");
  formLayout->addWidget(dlg->m_transDistLbl, 0, 0);
  formLayout->addWidget(dlg->m_transDistSpin, 0, 1);
  formLayout->addWidget(distUnitLbl, 0, 2);

  // 2. 设置 vx 速度
  dlg->m_transVxLbl = new QLabel("设置 vx 速度：", panel);
  dlg->m_transVxLbl->setStyleSheet("border: none; color: #c5cbd5;");
  const auto &mc = MotionConfigManager::instance();
  const double limitMaxLinear = mc.paramMax(QStringLiteral("max_speed"), 1.0);
  dlg->m_transVxSpin = new QDoubleSpinBox(panel);
  dlg->m_transVxSpin->setRange(-limitMaxLinear, limitMaxLinear);
  dlg->m_transVxSpin->setDecimals(3);
  dlg->m_transVxSpin->setSingleStep(0.05);
  dlg->m_transVxSpin->setValue(mc.maxSpeed());
  auto *vxUnitLbl = new QLabel("m/s", panel);
  vxUnitLbl->setStyleSheet("border: none; color: #a6b0be; font-size: 12px;");
  formLayout->addWidget(dlg->m_transVxLbl, 1, 0);
  formLayout->addWidget(dlg->m_transVxSpin, 1, 1);
  formLayout->addWidget(vxUnitLbl, 1, 2);

  // 3. 设置 vy 速度
  dlg->m_transVyLbl = new QLabel("设置 vy 速度：", panel);
  dlg->m_transVyLbl->setStyleSheet("border: none; color: #c5cbd5;");
  dlg->m_transVySpin = new QDoubleSpinBox(panel);
  dlg->m_transVySpin->setRange(-limitMaxLinear, limitMaxLinear);
  dlg->m_transVySpin->setDecimals(3);
  dlg->m_transVySpin->setSingleStep(0.05);
  dlg->m_transVySpin->setValue(0.000);
  auto *vyUnitLbl = new QLabel("m/s", panel);
  vyUnitLbl->setStyleSheet("border: none; color: #a6b0be; font-size: 12px;");
  formLayout->addWidget(dlg->m_transVyLbl, 2, 0);
  formLayout->addWidget(dlg->m_transVySpin, 2, 1);
  formLayout->addWidget(vyUnitLbl, 2, 2);

  // 4. 模式
  auto *modeLbl = new QLabel("模式：", panel);
  modeLbl->setStyleSheet("border: none; color: #c5cbd5;");
  auto *radioWidget = new QWidget(panel);
  radioWidget->setStyleSheet("border: none; background: transparent;");
  auto *radioLayout = new QHBoxLayout(radioWidget);
  radioLayout->setContentsMargins(0, 0, 0, 0);
  radioLayout->setSpacing(24);

  dlg->m_odomModeRadio = new QRadioButton("里程模式", radioWidget);
  dlg->m_locModeRadio  = new QRadioButton("定位模式", radioWidget);
  dlg->m_odomModeRadio->setChecked(true);

  auto *modeGroup = new QButtonGroup(dlg);
  modeGroup->addButton(dlg->m_odomModeRadio);
  modeGroup->addButton(dlg->m_locModeRadio);

  radioLayout->addWidget(dlg->m_odomModeRadio);
  radioLayout->addWidget(dlg->m_locModeRadio);
  radioLayout->addStretch();

  formLayout->addWidget(modeLbl, 3, 0);
  formLayout->addWidget(radioWidget, 3, 1, 1, 2);

  formLayout->setColumnStretch(0, 0);
  formLayout->setColumnStretch(1, 1);
  formLayout->setColumnStretch(2, 0);
  panelLayout->addLayout(formLayout);
  layout->addWidget(panel);

  updateParamToolTips(dlg);
  return tab;
}

QWidget *MotionDialogTabs::createRotationTab(MotionDialog *dlg) {
  auto *tab = new QWidget(dlg);
  auto *layout = new QVBoxLayout(tab);
  layout->setContentsMargins(0, 4, 0, 0);
  layout->setSpacing(6);

  auto *panel = new QFrame(dlg);
  panel->setStyleSheet(MotionDialogStyle::kPanelStyle);
  auto *panelLayout = new QVBoxLayout(panel);
  panelLayout->setContentsMargins(14, 10, 14, 12);
  panelLayout->setSpacing(8);

  auto *descLabel = new QLabel("机器人转动，以固定角速度旋转固定角度", panel);
  descLabel->setStyleSheet("border: none; color: #a6b0be; font-size: 12px;");
  panelLayout->addWidget(descLabel);

  auto *formLayout = new QGridLayout;
  formLayout->setHorizontalSpacing(10);
  formLayout->setVerticalSpacing(10);

  // 1. 设置旋转角度
  dlg->m_rotAngleLbl = new QLabel("设置旋转角度：", panel);
  dlg->m_rotAngleLbl->setStyleSheet("border: none; color: #c5cbd5;");
  dlg->m_rotAngleSpin = new QDoubleSpinBox(panel);
  dlg->m_rotAngleSpin->setRange(0.0, 360.0);
  dlg->m_rotAngleSpin->setDecimals(3);
  dlg->m_rotAngleSpin->setSingleStep(5.0);
  dlg->m_rotAngleSpin->setValue(90.000);
  auto *angleUnitLbl = new QLabel("°", panel);
  angleUnitLbl->setStyleSheet("border: none; color: #a6b0be; font-size: 12px;");
  formLayout->addWidget(dlg->m_rotAngleLbl, 0, 0);
  formLayout->addWidget(dlg->m_rotAngleSpin, 0, 1);
  formLayout->addWidget(angleUnitLbl, 0, 2);

  // 2. 设置角速度
  dlg->m_rotSpeedLbl = new QLabel("设置角速度：", panel);
  dlg->m_rotSpeedLbl->setStyleSheet("border: none; color: #c5cbd5;");
  const auto &mcRot = MotionConfigManager::instance();
  const double limitMaxAngular = mcRot.paramMax(QStringLiteral("max_angular_speed"), 60.0);
  dlg->m_rotSpeedSpin = new QDoubleSpinBox(panel);
  dlg->m_rotSpeedSpin->setRange(-limitMaxAngular, limitMaxAngular);
  dlg->m_rotSpeedSpin->setDecimals(3);
  dlg->m_rotSpeedSpin->setSingleStep(2.0);
  dlg->m_rotSpeedSpin->setValue(mcRot.maxAngularSpeedDeg());
  auto *speedUnitLbl = new QLabel("°/s", panel);
  speedUnitLbl->setStyleSheet("border: none; color: #a6b0be; font-size: 12px;");
  formLayout->addWidget(dlg->m_rotSpeedLbl, 1, 0);
  formLayout->addWidget(dlg->m_rotSpeedSpin, 1, 1);
  formLayout->addWidget(speedUnitLbl, 1, 2);

  formLayout->setColumnStretch(0, 0);
  formLayout->setColumnStretch(1, 1);
  formLayout->setColumnStretch(2, 0);
  panelLayout->addLayout(formLayout);
  layout->addWidget(panel);

  updateParamToolTips(dlg);
  return tab;
}
