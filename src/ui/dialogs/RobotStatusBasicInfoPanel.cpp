/**
 * @file    RobotStatusBasicInfoPanel.cpp
 * @layer   GUILayer
 * @brief   Builder for the 4-column basic info scroll panel in RobotStatusDialog.
 * @deps    RobotStatusBasicInfoPanel.h, RobotStatusDialog.h, RobotStatusStyle.h, RobotStatusUiFactory.h
 * @note    Refactored for AI readability; visual layout & widget hierarchy invariant.
 */

#include "RobotStatusBasicInfoPanel.h"
#include "RobotStatusDialog.h"
#include "RobotStatusStyle.h"
#include "RobotStatusUiFactory.h"

#include <QScrollArea>
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>

namespace {

QLabel *makeColumnHeader(const QString &title) {
  auto *lbl = new QLabel(title);
  lbl->setStyleSheet("color: #cbd5e1; font-weight: bold; font-size: 13px; padding-bottom: 6px;");
  return lbl;
}

QFrame *makeVLine() {
  auto *line = new QFrame;
  line->setFrameShape(QFrame::VLine);
  line->setStyleSheet("color: #2d3340;");
  return line;
}

} // namespace

QWidget *RobotStatusBasicInfoPanel::buildCol1(RobotStatusDialog *dlg) {
  auto *w = new QWidget;
  auto *lyt = new QVBoxLayout(w);
  lyt->setContentsMargins(0, 0, 0, 0);
  lyt->setSpacing(6);
  lyt->addWidget(makeColumnHeader("机器人运行状态"));

  auto *g = new QGridLayout;
  g->setContentsMargins(0, 0, 0, 0);
  g->setHorizontalSpacing(10);
  g->setVerticalSpacing(5);
  int r = 0;
  RobotStatusUiFactory::addRow(g, r++, "运行模式:", "--", &dlg->m_valRunMode);
  RobotStatusUiFactory::addRow(g, r++, "累计里程:", "--", &dlg->m_valTotalDist);
  RobotStatusUiFactory::addRow(g, r++, "今日累计里程:", "--", &dlg->m_valTodayDist);
  RobotStatusUiFactory::addRow(g, r++, "累计运行时间:", "--", &dlg->m_valTotalTime);
  RobotStatusUiFactory::addRow(g, r++, "本次运行时间:", "--", &dlg->m_valThisTime);
  RobotStatusUiFactory::addRow(g, r++, "控制器电压:", "23.66 V", &dlg->m_valCtrlVolt);
  RobotStatusUiFactory::addRow(g, r++, "控制器温度:", "46.26 ℃", &dlg->m_valCtrlTemp);
  RobotStatusUiFactory::addRow(g, r++, "控制器湿度:", "23.15 %", &dlg->m_valCtrlHumi);
  RobotStatusUiFactory::addRow(g, r++, "电池电量:", "--", &dlg->m_valBatteryPct);
  RobotStatusUiFactory::addRow(g, r++, "电池温度:", "23.00 ℃", &dlg->m_valBatteryTemp);
  RobotStatusUiFactory::addRow(g, r++, "电池电压:", "49.70 V", &dlg->m_valBatteryVolt);
  RobotStatusUiFactory::addRow(g, r++, "电池电流:", "-0.80 A", &dlg->m_valBatteryCurr);
  RobotStatusUiFactory::addRow(g, r++, "电池循环次数:", "0", &dlg->m_valCycleCount);
  RobotStatusUiFactory::addRow(g, r++, "电池数据(自定义):", "--", &dlg->m_valBatteryCustom);
  RobotStatusUiFactory::addRow(g, r++, "手动充电:", "未充电", &dlg->m_valChargeStatus);
  RobotStatusUiFactory::addRow(g, r++, "最大充电电压:", "未知", &dlg->m_valMaxChargeVolt);
  RobotStatusUiFactory::addRow(g, r++, "最大充电电流:", "未知", &dlg->m_valMaxChargeCurr);
  g->setRowStretch(r, 1);
  lyt->addLayout(g);
  return w;
}

QWidget *RobotStatusBasicInfoPanel::buildCol2(RobotStatusDialog *dlg) {
  auto *w = new QWidget;
  auto *lyt = new QVBoxLayout(w);
  lyt->setContentsMargins(0, 0, 0, 0);
  lyt->setSpacing(6);
  lyt->addWidget(makeColumnHeader("机器人运动状态"));

  auto *g = new QGridLayout;
  g->setContentsMargins(0, 0, 0, 0);
  g->setHorizontalSpacing(10);
  g->setVerticalSpacing(5);
  int r = 0;
  RobotStatusUiFactory::addRow(g, r++, "机器人位置:", "--", &dlg->m_valPosition);
  RobotStatusUiFactory::addRow(g, r++, "实际速度:", "--", &dlg->m_valActualSpeed);
  RobotStatusUiFactory::addRow(g, r++, "接收速度:", "--", &dlg->m_valTargetSpeed);
  RobotStatusUiFactory::addBadgeRow(g, r++, "置信度:", "0.24", RobotStatusBadgeType::Orange, &dlg->m_badgeConfidence);
  RobotStatusUiFactory::addBadgeRow(g, r++, "阻挡:", "--", RobotStatusBadgeType::Gray, &dlg->m_badgeBlocked);
  RobotStatusUiFactory::addRow(g, r++, "最近障碍物:", "--", &dlg->m_valNearestObs);
  RobotStatusUiFactory::addRow(g, r++, "最近动态障碍物:", "--", &dlg->m_valDynObs);
  RobotStatusUiFactory::addRow(g, r++, "避障扫描时间:", "--", &dlg->m_valObsStamp);
  RobotStatusUiFactory::addRow(g, r++, "阻挡传感器 ID:", "--", &dlg->m_valBlockSensorId);
  RobotStatusUiFactory::addBadgeRow(g, r++, "是否抱闸:", "未抱闸", RobotStatusBadgeType::Green, &dlg->m_badgeBrake);

  QList<QLabel*> estopList;
  RobotStatusUiFactory::addMultiBadgeRow(g, r++, "电机是否使能:",
                                         {{"未急停", RobotStatusBadgeType::Green}, {"驱动器未急停", RobotStatusBadgeType::Green}},
                                         &estopList);
  if (estopList.size() >= 2) {
    dlg->m_badgeEstop1 = estopList[0];
    dlg->m_badgeEstop2 = estopList[1];
  }

  RobotStatusUiFactory::addRow(g, r++, "陀螺仪:", "0.000°,1.000°,1.000°", &dlg->m_valGyro);
  RobotStatusUiFactory::addRow(g, r++, "控制模式:", "RT 控制", &dlg->m_valControlMode);
  RobotStatusUiFactory::addBadgeRow(g, r++, "底盘是否静止:", "是", RobotStatusBadgeType::Green, &dlg->m_badgeStopped);
  g->setRowStretch(r, 1);
  lyt->addLayout(g);
  return w;
}

QWidget *RobotStatusBasicInfoPanel::buildCol3(RobotStatusDialog *dlg) {
  auto *w = new QWidget;
  auto *lyt = new QVBoxLayout(w);
  lyt->setContentsMargins(0, 0, 0, 0);
  lyt->setSpacing(6);
  lyt->addWidget(makeColumnHeader("机器人导航状态"));

  auto *g = new QGridLayout;
  g->setContentsMargins(0, 0, 0, 0);
  g->setHorizontalSpacing(10);
  g->setVerticalSpacing(5);
  int r = 0;
  RobotStatusUiFactory::addRow(g, r++, "导航类型:", "--", &dlg->m_valNavType);
  RobotStatusUiFactory::addRow(g, r++, "导航状态:", "--", &dlg->m_valNavStatus);
  RobotStatusUiFactory::addRow(g, r++, "当前站点:", "--", &dlg->m_valCurStation);
  RobotStatusUiFactory::addRow(g, r++, "当前目标站点:", "--", &dlg->m_valTargetStation);
  RobotStatusUiFactory::addRow(g, r++, "当前目标点坐标:", "0.000 m, 0.000 m, 0.000 °", &dlg->m_valTargetCoord);
  RobotStatusUiFactory::addRow(g, r++, "距离目标点距离:", "0 m", &dlg->m_valDistToTarget);
  RobotStatusUiFactory::addRow(g, r++, "已经过站点:", "--", &dlg->m_valPassedStations);
  RobotStatusUiFactory::addRow(g, r++, "未经过去站点:", "--", &dlg->m_valUnpassedStations);
  RobotStatusUiFactory::addRow(g, r++, "定位状态:", "定位完成(需确认)", &dlg->m_valLocStatus, "#ef4444");
  RobotStatusUiFactory::addRow(g, r++, "地图载入状态:", "加载地图成功", &dlg->m_valMapLoadStatus);
  RobotStatusUiFactory::addRow(g, r++, "SLAM 状态:", "暂无", &dlg->m_valSlamStatus);
  RobotStatusUiFactory::addRow(g, r++, "机器人所在区域:", "--", &dlg->m_valArea);
  RobotStatusUiFactory::addRow(g, r++, "当前加载的地图:", "20260819201834963", &dlg->m_valCurMap);
  g->setRowStretch(r, 1);
  lyt->addLayout(g);
  return w;
}

QWidget *RobotStatusBasicInfoPanel::buildCol4(RobotStatusDialog *dlg) {
  auto *w = new QWidget;
  auto *lyt = new QVBoxLayout(w);
  lyt->setContentsMargins(0, 0, 0, 0);
  lyt->setSpacing(6);
  lyt->addWidget(makeColumnHeader("机器人基本信息"));

  auto *g = new QGridLayout;
  g->setContentsMargins(0, 0, 0, 0);
  g->setHorizontalSpacing(10);
  g->setVerticalSpacing(5);
  int r = 0;
  RobotStatusUiFactory::addRow(g, r++, "机器人 ID:", "1", &dlg->m_valRobotId);
  RobotStatusUiFactory::addRow(g, r++, "机器人名称:", "AMB-01", &dlg->m_valRobotName);
  RobotStatusUiFactory::addRow(g, r++, "机器人备注:", "--", &dlg->m_valRobotRemark);
  RobotStatusUiFactory::addRow(g, r++, "机器人型号:", "AMB-300", &dlg->m_valRobotModel);
  RobotStatusUiFactory::addRow(g, r++, "Robokit 版本:", "--", &dlg->m_valRobokitVer);
  RobotStatusUiFactory::addRow(g, r++, "底层固件版本:", "--", &dlg->m_valFirmwareVer);
  RobotStatusUiFactory::addRow(g, r++, "陀螺仪版本:", "--", &dlg->m_valGyroVer);
  RobotStatusUiFactory::addRow(g, r++, "TCP API 版本:", "--", &dlg->m_valTcpApiVer);
  RobotStatusUiFactory::addRow(g, r++, "Modbus 版本:", "--", &dlg->m_valModbusVer);
  RobotStatusUiFactory::addRow(g, r++, "地图版本:", "--", &dlg->m_valMapVer);
  RobotStatusUiFactory::addRow(g, r++, "模型版本:", "--", &dlg->m_valModelVer);
  RobotStatusUiFactory::addRow(g, r++, "MAC:", "--", &dlg->m_valMac);
  RobotStatusUiFactory::addRow(g, r++, "有线网卡型号:", "--", &dlg->m_valWiredNicModel);
  RobotStatusUiFactory::addRow(g, r++, "无线网 MAC:", "--", &dlg->m_valWirelessMac);
  RobotStatusUiFactory::addRow(g, r++, "无线网卡型号:", "--", &dlg->m_valWirelessNicModel);
  g->setRowStretch(r, 1);
  lyt->addLayout(g);
  return w;
}

QWidget *RobotStatusBasicInfoPanel::createBasicInfoTab(RobotStatusDialog *dlg) {
  auto *scroll = new QScrollArea;
  scroll->setWidgetResizable(true);

  auto *contentWidget = new QWidget;
  auto *vlyt = new QVBoxLayout(contentWidget);
  vlyt->setContentsMargins(4, 8, 4, 8);
  vlyt->setSpacing(10);

  auto *topFrame = new QFrame;
  topFrame->setObjectName("topFrame");
  topFrame->setStyleSheet(RobotStatusStyle::kTopFrameStyle);
  auto *topLyt = new QHBoxLayout(topFrame);
  topLyt->setContentsMargins(12, 10, 12, 10);
  topLyt->setSpacing(14);

  topLyt->addWidget(buildCol1(dlg), 1);
  topLyt->addWidget(makeVLine());
  topLyt->addWidget(buildCol2(dlg), 1);
  topLyt->addWidget(makeVLine());
  topLyt->addWidget(buildCol3(dlg), 1);
  topLyt->addWidget(makeVLine());
  topLyt->addWidget(buildCol4(dlg), 1);
  vlyt->addWidget(topFrame);

  auto *botFrame = new QFrame;
  botFrame->setObjectName("botFrame");
  botFrame->setStyleSheet(RobotStatusStyle::kBotFrameStyle);
  auto *botLyt = new QVBoxLayout(botFrame);
  botLyt->setContentsMargins(12, 10, 12, 10);
  botLyt->setSpacing(8);

  dlg->m_lblClientTitle = new QLabel("当前已连接机器人的客户端 IP (正在检测网络通信适配器...)");
  dlg->m_lblClientTitle->setStyleSheet("color: #94a3b8; font-size: 12px; font-weight: 600;");
  botLyt->addWidget(dlg->m_lblClientTitle);

  auto *clientContentHl = new QHBoxLayout;
  clientContentHl->setContentsMargins(0, 0, 0, 0);
  clientContentHl->setSpacing(20);

  dlg->m_lblConnList = new QLabel;
  dlg->m_lblConnList->setStyleSheet("color: #cbd5e1; font-family: Consolas, monospace; font-size: 12px; line-height: 1.4;");
  dlg->m_lblConnList->setTextInteractionFlags(Qt::TextSelectableByMouse);
  clientContentHl->addWidget(dlg->m_lblConnList, 1);

  dlg->m_lblDetail = new QLabel;
  dlg->m_lblDetail->setStyleSheet("color: #94a3b8; font-family: Consolas, monospace; font-size: 12px; line-height: 1.4;");
  dlg->m_lblDetail->setTextInteractionFlags(Qt::TextSelectableByMouse);
  clientContentHl->addWidget(dlg->m_lblDetail, 1);

  botLyt->addLayout(clientContentHl);
  vlyt->addWidget(botFrame);

  dlg->m_lblFooter = new QLabel("正在检测无线信号状态...");
  dlg->m_lblFooter->setStyleSheet("color: #94a3b8; font-size: 12px; padding: 2px 4px;");
  dlg->m_lblFooter->setTextInteractionFlags(Qt::TextSelectableByMouse);
  vlyt->addWidget(dlg->m_lblFooter);

  scroll->setWidget(contentWidget);
  return scroll;
}
