#include "RobotStatusDialog.h"
#include "core/AppConfig.h"
#include "core/MqttService.h"
#include "core/RobotInfoManager.h"

#include <QScrollArea>
#include <QFont>
#include <QNetworkInterface>
#include <QTcpSocket>
#include <QProcess>
#include <QRegularExpression>
#include <QJsonDocument>
#include <cmath>

RobotStatusDialog::RobotStatusDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    m_startTimeMs = QDateTime::currentMSecsSinceEpoch();

    // 定时刷新连接时长与网络状态
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &RobotStatusDialog::onPeriodicRefresh);
    m_timer->start(2000);

    initMqttSubscriptions();
    refreshNetworkAndWifiInfo();

    connect(&RobotInfoManager::instance(), &RobotInfoManager::currentRobotInfoUpdated,
            this, &RobotStatusDialog::updateFromRobotInfo);
    updateFromRobotInfo(RobotInfoManager::instance().currentInfo());
}

RobotStatusDialog::~RobotStatusDialog()
{
    if (m_timer) {
        m_timer->stop();
    }
}

void RobotStatusDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    updateFromRobotInfo(RobotInfoManager::instance().currentInfo());
    refreshNetworkAndWifiInfo();
}

void RobotStatusDialog::setupUi()
{
    setWindowTitle("运行状态");
    setMinimumSize(980, 680);
    resize(1060, 740);

    // 启用最小化、最大化和关闭按钮
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    setStyleSheet(
        "QDialog {"
        "  background: #181b22;"
        "  color: #e2e8f0;"
        "}"
        "QTabWidget::pane {"
        "  border: none;"
        "  background: #181b22;"
        "}"
        "QTabBar::tab {"
        "  background: transparent;"
        "  color: #94a3b8;"
        "  padding: 8px 18px;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "  border-bottom: 2px solid transparent;"
        "}"
        "QTabBar::tab:hover {"
        "  color: #f1f5f9;"
        "}"
        "QTabBar::tab:selected {"
        "  color: #ffffff;"
        "  border-bottom: 2px solid #3b82f6;"
        "  font-weight: bold;"
        "}"
        "QScrollArea {"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QScrollBar:vertical {"
        "  background: #181b22;"
        "  width: 8px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #334155;"
        "  min-height: 20px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: #475569;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
    );

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 10, 12, 12);
    rootLayout->setSpacing(8);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createBasicInfoTab(), "基本信息");
    m_tabWidget->addTab(createPlaceholderTab("运行曲线"), "运行曲线");
    m_tabWidget->addTab(createPlaceholderTab("PGV"), "PGV");
    m_tabWidget->addTab(createPlaceholderTab("定位信息"), "定位信息");
    m_tabWidget->addTab(createPlaceholderTab("所有网卡信息"), "所有网卡信息");
    m_tabWidget->addTab(createPlaceholderTab("Driver info"), "Driver info");
    m_tabWidget->addTab(createPlaceholderTab("所有数据"), "所有数据");

    rootLayout->addWidget(m_tabWidget);
}

QLabel *RobotStatusDialog::createBadge(const QString &text, BadgeType type)
{
    auto *b = new QLabel(text);
    b->setAlignment(Qt::AlignCenter);
    updateBadge(b, text, type);
    b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return b;
}

void RobotStatusDialog::updateBadge(QLabel *lbl, const QString &text, BadgeType type)
{
    if (!lbl) return;
    lbl->setText(text);

    QString bg = "#65a30d"; // 绿底正常
    if (type == BadgeType::Orange) {
        bg = "#ea580c";     // 橙底警戒
    } else if (type == BadgeType::Red) {
        bg = "#dc2626";     // 红底报警
    } else if (type == BadgeType::Gray) {
        bg = "rgba(255, 255, 255, 0.12)"; // 灰色中性/未生效
    }

    lbl->setStyleSheet(QString(
        "background: %1; color: #ffffff; border-radius: 3px; padding: 2px 7px; font-size: 11px; font-weight: bold;"
    ).arg(bg));
}

void RobotStatusDialog::addRow(QGridLayout *grid, int row, const QString &label, const QString &value,
                               QLabel **outValueLbl, const QString &valColor)
{
    auto *l = new QLabel(label);
    l->setStyleSheet("color: #94a3b8; font-size: 12px;");
    l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(l, row, 0);

    auto *v = new QLabel(value);
    v->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 500;").arg(valColor));
    v->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid->addWidget(v, row, 1);

    if (outValueLbl) {
        *outValueLbl = v;
    }
}

void RobotStatusDialog::addBadgeRow(QGridLayout *grid, int row, const QString &label, const QString &badgeText,
                                    BadgeType type, QLabel **outBadgeLbl)
{
    auto *l = new QLabel(label);
    l->setStyleSheet("color: #94a3b8; font-size: 12px;");
    l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(l, row, 0);

    auto *b = createBadge(badgeText, type);
    grid->addWidget(b, row, 1, Qt::AlignLeft | Qt::AlignVCenter);

    if (outBadgeLbl) {
        *outBadgeLbl = b;
    }
}

void RobotStatusDialog::addMultiBadgeRow(QGridLayout *grid, int row, const QString &label,
                                         const QList<QPair<QString, BadgeType>> &badges,
                                         QList<QLabel*> *outBadgeList)
{
    auto *l = new QLabel(label);
    l->setStyleSheet("color: #94a3b8; font-size: 12px;");
    l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(l, row, 0);

    auto *hl = new QHBoxLayout;
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);
    for (const auto &pair : badges) {
        auto *b = createBadge(pair.first, pair.second);
        hl->addWidget(b);
        if (outBadgeList) {
            outBadgeList->append(b);
        }
    }
    hl->addStretch();
    grid->addLayout(hl, row, 1, Qt::AlignLeft | Qt::AlignVCenter);
}

QWidget *RobotStatusDialog::createBasicInfoTab()
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);

    auto *contentWidget = new QWidget;
    auto *vlyt = new QVBoxLayout(contentWidget);
    vlyt->setContentsMargins(4, 8, 4, 8);
    vlyt->setSpacing(10);

    // ──────────────── 上部区域：4列状态参数容器 ────────────────
    auto *topFrame = new QFrame;
    topFrame->setObjectName("topFrame");
    topFrame->setStyleSheet("QFrame#topFrame { background: #16191f; border: 1px solid #2d3340; border-radius: 4px; padding: 12px; }");
    auto *topLyt = new QHBoxLayout(topFrame);
    topLyt->setContentsMargins(12, 10, 12, 10);
    topLyt->setSpacing(14);

    auto makeColumnHeader = [](const QString &title) {
        auto *lbl = new QLabel(title);
        lbl->setStyleSheet("color: #cbd5e1; font-weight: bold; font-size: 13px; padding-bottom: 6px;");
        return lbl;
    };

    auto makeVLine = []() {
        auto *line = new QFrame;
        line->setFrameShape(QFrame::VLine);
        line->setStyleSheet("color: #2d3340;");
        return line;
    };

    // ── 第 1 列：机器人运行状态 ──
    auto *col1Widget = new QWidget;
    auto *col1Lyt = new QVBoxLayout(col1Widget);
    col1Lyt->setContentsMargins(0, 0, 0, 0);
    col1Lyt->setSpacing(6);
    col1Lyt->addWidget(makeColumnHeader("机器人运行状态"));

    auto *grid1 = new QGridLayout;
    grid1->setContentsMargins(0, 0, 0, 0);
    grid1->setHorizontalSpacing(10);
    grid1->setVerticalSpacing(5);
    int r1 = 0;
    addRow(grid1, r1++, "运行模式:", "--", &m_valRunMode);
    addRow(grid1, r1++, "累计里程:", "--", &m_valTotalDist);
    addRow(grid1, r1++, "今日累计里程:", "--", &m_valTodayDist);
    addRow(grid1, r1++, "累计运行时间:", "--", &m_valTotalTime);
    addRow(grid1, r1++, "本次运行时间:", "--", &m_valThisTime);
    addRow(grid1, r1++, "控制器电压:", "23.66 V", &m_valCtrlVolt);
    addRow(grid1, r1++, "控制器温度:", "46.26 ℃", &m_valCtrlTemp);
    addRow(grid1, r1++, "控制器湿度:", "23.15 %", &m_valCtrlHumi);
    addRow(grid1, r1++, "电池电量:", "--", &m_valBatteryPct);
    addRow(grid1, r1++, "电池温度:", "23.00 ℃", &m_valBatteryTemp);
    addRow(grid1, r1++, "电池电压:", "49.70 V", &m_valBatteryVolt);
    addRow(grid1, r1++, "电池电流:", "-0.80 A", &m_valBatteryCurr);
    addRow(grid1, r1++, "电池循环次数:", "0", &m_valCycleCount);
    addRow(grid1, r1++, "电池数据(自定义):", "--", &m_valBatteryCustom);
    addRow(grid1, r1++, "手动充电:", "未充电", &m_valChargeStatus);
    addRow(grid1, r1++, "最大充电电压:", "未知", &m_valMaxChargeVolt);
    addRow(grid1, r1++, "最大充电电流:", "未知", &m_valMaxChargeCurr);
    grid1->setRowStretch(r1, 1);
    col1Lyt->addLayout(grid1);
    topLyt->addWidget(col1Widget, 1);

    topLyt->addWidget(makeVLine());

    // ── 第 2 列：机器人运动状态 ──
    auto *col2Widget = new QWidget;
    auto *col2Lyt = new QVBoxLayout(col2Widget);
    col2Lyt->setContentsMargins(0, 0, 0, 0);
    col2Lyt->setSpacing(6);
    col2Lyt->addWidget(makeColumnHeader("机器人运动状态"));

    auto *grid2 = new QGridLayout;
    grid2->setContentsMargins(0, 0, 0, 0);
    grid2->setHorizontalSpacing(10);
    grid2->setVerticalSpacing(5);
    int r2 = 0;
    addRow(grid2, r2++, "机器人位置:", "--", &m_valPosition);
    addRow(grid2, r2++, "实际速度:", "--", &m_valActualSpeed);
    addRow(grid2, r2++, "接收速度:", "--", &m_valTargetSpeed);
    addBadgeRow(grid2, r2++, "置信度:", "0.24", BadgeType::Orange, &m_badgeConfidence);
    addBadgeRow(grid2, r2++, "阻挡:", "--", BadgeType::Gray, &m_badgeBlocked);
    addRow(grid2, r2++, "最近障碍物:", "--", &m_valNearestObs);
    addRow(grid2, r2++, "最近动态障碍物:", "--", &m_valDynObs);
    addRow(grid2, r2++, "障碍物坐标系:", "base_link", &m_valObsFrameId);
    addRow(grid2, r2++, "避障扫描时间:", "--", &m_valObsStamp);
    addRow(grid2, r2++, "阻挡传感器 ID:", "--", &m_valBlockSensorId);
    addBadgeRow(grid2, r2++, "是否抱闸:", "未抱闸", BadgeType::Green, &m_badgeBrake);

    QList<QLabel*> estopList;
    addMultiBadgeRow(grid2, r2++, "电机是否使能:", {{"未急停", BadgeType::Green}, {"驱动器未急停", BadgeType::Green}}, &estopList);
    if (estopList.size() >= 2) {
        m_badgeEstop1 = estopList[0];
        m_badgeEstop2 = estopList[1];
    }

    addRow(grid2, r2++, "陀螺仪:", "0.000°,1.000°,1.000°", &m_valGyro);
    addRow(grid2, r2++, "控制模式:", "SRC 控制", &m_valControlMode);
    addBadgeRow(grid2, r2++, "底盘是否静止:", "是", BadgeType::Green, &m_badgeStopped);
    grid2->setRowStretch(r2, 1);
    col2Lyt->addLayout(grid2);
    topLyt->addWidget(col2Widget, 1);

    topLyt->addWidget(makeVLine());

    // ── 第 3 列：机器人导航状态 ──
    auto *col3Widget = new QWidget;
    auto *col3Lyt = new QVBoxLayout(col3Widget);
    col3Lyt->setContentsMargins(0, 0, 0, 0);
    col3Lyt->setSpacing(6);
    col3Lyt->addWidget(makeColumnHeader("机器人导航状态"));

    auto *grid3 = new QGridLayout;
    grid3->setContentsMargins(0, 0, 0, 0);
    grid3->setHorizontalSpacing(10);
    grid3->setVerticalSpacing(5);
    int r3 = 0;
    addRow(grid3, r3++, "导航类型:", "--", &m_valNavType);
    addRow(grid3, r3++, "导航状态:", "--", &m_valNavStatus);
    addRow(grid3, r3++, "当前站点:", "--", &m_valCurStation);
    addRow(grid3, r3++, "当前目标站点:", "--", &m_valTargetStation);
    addRow(grid3, r3++, "当前目标点坐标:", "0.000 m, 0.000 m, 0.000 °", &m_valTargetCoord);
    addRow(grid3, r3++, "距离目标点距离:", "0 m", &m_valDistToTarget);
    addRow(grid3, r3++, "已经过站点:", "--", &m_valPassedStations);
    addRow(grid3, r3++, "未经过去站点:", "--", &m_valUnpassedStations);
    addRow(grid3, r3++, "定位状态:", "定位完成(需确认)", &m_valLocStatus, "#ef4444");
    addRow(grid3, r3++, "地图载入状态:", "加载地图成功", &m_valMapLoadStatus);
    addRow(grid3, r3++, "SLAM 状态:", "暂无", &m_valSlamStatus);
    addRow(grid3, r3++, "机器人所在区域:", "--", &m_valArea);
    addRow(grid3, r3++, "当前加载的地图:", "20260819201834963", &m_valCurMap);
    grid3->setRowStretch(r3, 1);
    col3Lyt->addLayout(grid3);
    topLyt->addWidget(col3Widget, 1);

    topLyt->addWidget(makeVLine());

    // ── 第 4 列：机器人基本信息 (无版本信息严格显示 "--") ──
    auto *col4Widget = new QWidget;
    auto *col4Lyt = new QVBoxLayout(col4Widget);
    col4Lyt->setContentsMargins(0, 0, 0, 0);
    col4Lyt->setSpacing(6);
    col4Lyt->addWidget(makeColumnHeader("机器人基本信息"));

    auto *grid4 = new QGridLayout;
    grid4->setContentsMargins(0, 0, 0, 0);
    grid4->setHorizontalSpacing(10);
    grid4->setVerticalSpacing(5);
    int r4 = 0;
    addRow(grid4, r4++, "机器人 ID:", "1", &m_valRobotId);
    addRow(grid4, r4++, "机器人名称:", "AMB-01", &m_valRobotName);
    addRow(grid4, r4++, "机器人备注:", "--", &m_valRobotRemark);
    addRow(grid4, r4++, "机器人型号:", "AMB-300", &m_valRobotModel);
    addRow(grid4, r4++, "Robokit 版本:", "--", &m_valRobokitVer);
    addRow(grid4, r4++, "底层固件版本:", "--", &m_valFirmwareVer);
    addRow(grid4, r4++, "陀螺仪版本:", "--", &m_valGyroVer);
    addRow(grid4, r4++, "TCP API 版本:", "--", &m_valTcpApiVer);
    addRow(grid4, r4++, "Modbus 版本:", "--", &m_valModbusVer);
    addRow(grid4, r4++, "地图版本:", "--", &m_valMapVer);
    addRow(grid4, r4++, "模型版本:", "--", &m_valModelVer);
    addRow(grid4, r4++, "MAC:", "--", &m_valMac);
    addRow(grid4, r4++, "有线网卡型号:", "--", &m_valWiredNicModel);
    addRow(grid4, r4++, "无线网 MAC:", "--", &m_valWirelessMac);
    addRow(grid4, r4++, "无线网卡型号:", "--", &m_valWirelessNicModel);
    addRow(grid4, r4++, "避障坐标系 (frame_id):", "base_link", &m_valBaseFrameId);
    addRow(grid4, r4++, "避障时间戳 (stamp):", "--", &m_valBaseStamp);
    grid4->setRowStretch(r4, 1);
    col4Lyt->addLayout(grid4);
    topLyt->addWidget(col4Widget, 1);

    vlyt->addWidget(topFrame);

    // ──────────────── 下部区域：客户端连接与网络信息 ────────────────
    auto *botFrame = new QFrame;
    botFrame->setObjectName("botFrame");
    botFrame->setStyleSheet("QFrame#botFrame { background: #16191f; border: 1px solid #2d3340; border-radius: 4px; padding: 10px; }");
    auto *botLyt = new QVBoxLayout(botFrame);
    botLyt->setContentsMargins(12, 10, 12, 10);
    botLyt->setSpacing(8);

    m_lblClientTitle = new QLabel("当前已连接机器人的客户端 IP (正在检测网络通信适配器...)");
    m_lblClientTitle->setStyleSheet("color: #94a3b8; font-size: 12px; font-weight: 600;");
    botLyt->addWidget(m_lblClientTitle);

    auto *clientContentHl = new QHBoxLayout;
    clientContentHl->setContentsMargins(0, 0, 0, 0);
    clientContentHl->setSpacing(20);

    // 左侧端口连接清单
    m_lblConnList = new QLabel;
    m_lblConnList->setStyleSheet("color: #cbd5e1; font-family: Consolas, monospace; font-size: 12px; line-height: 1.4;");
    m_lblConnList->setTextInteractionFlags(Qt::TextSelectableByMouse);
    clientContentHl->addWidget(m_lblConnList, 1);

    // 右侧字段明细
    m_lblDetail = new QLabel;
    m_lblDetail->setStyleSheet("color: #94a3b8; font-family: Consolas, monospace; font-size: 12px; line-height: 1.4;");
    m_lblDetail->setTextInteractionFlags(Qt::TextSelectableByMouse);
    clientContentHl->addWidget(m_lblDetail, 1);

    botLyt->addLayout(clientContentHl);
    vlyt->addWidget(botFrame);

    // ── 底部 AP / 信号状态行 ──
    m_lblFooter = new QLabel("正在检测无线信号状态...");
    m_lblFooter->setStyleSheet("color: #94a3b8; font-size: 12px; padding: 2px 4px;");
    m_lblFooter->setTextInteractionFlags(Qt::TextSelectableByMouse);
    vlyt->addWidget(m_lblFooter);

    scroll->setWidget(contentWidget);
    return scroll;
}

QWidget *RobotStatusDialog::createPlaceholderTab(const QString &tabName)
{
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    l->setAlignment(Qt::AlignCenter);

    auto *lbl = new QLabel(QString("暂无「%1」相关数据").arg(tabName));
    lbl->setStyleSheet("color: #64748b; font-size: 14px; font-weight: 500;");
    l->addWidget(lbl);

    return w;
}

void RobotStatusDialog::initMqttSubscriptions()
{
    auto cb = [this](const QString &topic, const QByteArray &payload) {
        onMqttMessage(topic, payload);
    };

    MqttService::instance().subscribeTopic("/info", 0, cb);
    MqttService::instance().subscribeTopic("info", 0, cb);
    MqttService::instance().subscribeTopic("/rob/info", 0, cb);
    MqttService::instance().subscribeTopic("/status", 0, cb);
    MqttService::instance().subscribeTopic("status", 0, cb);
    MqttService::instance().subscribeTopic("/rob/status", 0, cb);

    QString carId = AppConfig::instance().agvId();
    if (carId.isEmpty()) carId = "1";
    MqttService::instance().subscribeTopic(QString("/rob/%1/info").arg(carId), 0, cb);
    MqttService::instance().subscribeTopic(QString("/rob/%1/status").arg(carId), 0, cb);

    // ── 订阅最近障碍物话题 (支持空格、下划线及前缀格式) ──
    MqttService::instance().subscribeTopic(QString("/rob/%1/nearest obstacle").arg(carId), 0, cb);
    MqttService::instance().subscribeTopic(QString("/rob/%1/nearest_obstacle").arg(carId), 0, cb);
    MqttService::instance().subscribeTopic(QString(":/rob/%1/nearest obstacle").arg(carId), 0, cb);
    MqttService::instance().subscribeTopic(QString(":/rob/%1/nearest_obstacle").arg(carId), 0, cb);
    MqttService::instance().subscribeTopic("/rob/1/nearest obstacle", 0, cb);
    MqttService::instance().subscribeTopic("/rob/1/nearest_obstacle", 0, cb);
    MqttService::instance().subscribeTopic(":/rob/1/nearest obstacle", 0, cb);
    MqttService::instance().subscribeTopic(":/rob/1/nearest_obstacle", 0, cb);
    MqttService::instance().subscribeTopic("/nearest obstacle", 0, cb);
    MqttService::instance().subscribeTopic("/nearest_obstacle", 0, cb);
}

void RobotStatusDialog::setMapInfo(const QString &mapId, const QString &mapName)
{
    m_mapId = mapId;
    m_mapName = mapName;
    if (m_valCurMap) {
        QString disp = m_mapName.isEmpty() ? (m_mapId.isEmpty() ? "--" : m_mapId) : m_mapName;
        m_valCurMap->setText(disp);
    }
    if (m_valMapLoadStatus) {
        m_valMapLoadStatus->setText(mapId.isEmpty() ? "未加载地图" : "加载地图成功");
    }
}

void RobotStatusDialog::updateFromRobotInfo(const RobotInfo &info)
{
    m_hasMqttData = true;

    // ── 1. 机器人运行状态 ──
    if (m_valRunMode) {
        if (!info.status.isEmpty()) {
            m_valRunMode->setText(info.statusText());
            m_valRunMode->setStyleSheet(QString("color:%1;").arg(info.statusColor()));
        } else {
            m_valRunMode->setText("--");
            m_valRunMode->setStyleSheet("color:#94a3b8;");
        }
    }
    if (m_valTotalDist) {
        m_valTotalDist->setText(info.mileage.valid ? QString("%1 m").arg(info.mileage.total_distance_m, 0, 'f', 2) : "--");
    }
    if (m_valTodayDist) {
        m_valTodayDist->setText(info.mileage.valid ? QString("%1 m").arg(info.mileage.today_distance_m, 0, 'f', 2) : "--");
    }
    if (m_valTotalTime) {
        m_valTotalTime->setText(info.runtime.valid ? RobotInfoManager::formatDuration(info.runtime.total_runtime_sec) : "--");
    }
    if (m_valThisTime) {
        m_valThisTime->setText(info.runtime.valid ? RobotInfoManager::formatDuration(info.runtime.current_runtime_sec) : "--");
    }
    if (info.battery >= 0.0 && m_valBatteryPct) {
        m_valBatteryPct->setText(QString("%1 %").arg(info.batteryPercent()));
    }

    // ── 2. 机器人运动状态 ──
    double yawDeg = info.car_yaw * 180.0 / M_PI;
    if (m_valPosition) {
        if (info.hasPose) {
            m_valPosition->setText(QString("%1 m, %2 m, %3 °")
                .arg(info.car_x, 0, 'f', 3)
                .arg(info.car_y, 0, 'f', 3)
                .arg(yawDeg, 0, 'f', 3));
        } else {
            m_valPosition->setText("--");
        }
    }

    // 实际速度: vx (m/s), vy (m/s), wz (rad/s 及 °/s)
    if (m_valActualSpeed) {
        if (info.actual_vel.valid) {
            double wDeg = info.actual_vel.wz * 180.0 / M_PI;
            m_valActualSpeed->setText(QString("%1 m/s, %2 m/s, %3 rad/s (%4 °/s)")
                .arg(info.actual_vel.vx, 0, 'f', 3)
                .arg(info.actual_vel.vy, 0, 'f', 3)
                .arg(info.actual_vel.wz, 0, 'f', 3)
                .arg(wDeg, 0, 'f', 3));

            if (m_badgeStopped) {
                bool stopped = (std::abs(info.actual_vel.vx) < 0.005 &&
                                std::abs(info.actual_vel.vy) < 0.005 &&
                                std::abs(info.actual_vel.wz) < 0.005);
                updateBadge(m_badgeStopped, stopped ? "是" : "否", stopped ? BadgeType::Green : BadgeType::Orange);
            }
        } else {
            m_valActualSpeed->setText("--");
        }
    }

    // 接收速度 (最终指令速度 /cmd_vel_safe): vx (m/s), vy (m/s), wz (rad/s 及 °/s)
    if (m_valTargetSpeed) {
        if (info.cmd_vel.valid) {
            double cmdWDeg = info.cmd_vel.wz * 180.0 / M_PI;
            m_valTargetSpeed->setText(QString("%1 m/s, %2 m/s, %3 rad/s (%4 °/s)")
                .arg(info.cmd_vel.vx, 0, 'f', 3)
                .arg(info.cmd_vel.vy, 0, 'f', 3)
                .arg(info.cmd_vel.wz, 0, 'f', 3)
                .arg(cmdWDeg, 0, 'f', 3));
        } else {
            m_valTargetSpeed->setText("--");
        }
    }

    if (info.obstacle.valid) {
        if (m_badgeBlocked) {
            updateBadge(m_badgeBlocked, info.obstacle.blocked ? "遮挡" : "未遮挡",
                        info.obstacle.blocked ? BadgeType::Orange : BadgeType::Green);
        }
        if (info.obstacle.hasPoint) {
            if (m_valNearestObs) {
                m_valNearestObs->setText(QString("%1 m, %2 m")
                    .arg(info.obstacle.point.x, 0, 'f', 3)
                    .arg(info.obstacle.point.y, 0, 'f', 3));
            }
            if (m_valDynObs) {
                m_valDynObs->setText(QString("x:%1m y:%2m")
                    .arg(info.obstacle.point.x, 0, 'f', 4)
                    .arg(info.obstacle.point.y, 0, 'f', 4));
            }
        }
    }

    if (info.covScore >= 0.0 && m_badgeConfidence) {
        double cs = (info.covScore > 1.0) ? info.covScore / 100.0 : info.covScore;
        BadgeType bt = cs >= 0.7 ? BadgeType::Green : (cs >= 0.3 ? BadgeType::Orange : BadgeType::Red);
        updateBadge(m_badgeConfidence, QString::number(cs, 'f', 2), bt);
        if (m_valLocStatus) {
            m_valLocStatus->setText(cs < 0.5 ? "<font color='#ef4444'>定位完成(需确认)</font>" : "<font color='#22c55e'>定位完成</font>");
        }
    }

    // ── 3. 机器人导航状态 ──
    if (info.map_id != 0 && m_valCurMap) {
        m_valCurMap->setText(QString::number(info.map_id));
    }
    if (m_valNavStatus) {
        m_valNavStatus->setText(info.navStatusText());
    }

    // ── 4. 机器人基本信息 ──
    if (info.car_id != 0 && m_valRobotId) {
        m_valRobotId->setText(QString::number(info.car_id));
    }
    if (!info.carName.isEmpty() && m_valRobotName) {
        m_valRobotName->setText(info.carName);
    }
    if (!info.remark.isEmpty() && m_valRobotRemark) {
        m_valRobotRemark->setText(info.remark);
    }
    if (!info.fwVersion.isEmpty() && m_valFirmwareVer) {
        m_valFirmwareVer->setText(info.fwVersion);
    }
}

void RobotStatusDialog::updateFromRobotState(const QString &carId, double x, double y, double yawRad,
                                            double speed, double battery, double covScore, bool posValid)
{
    double yawDeg = yawRad * 180.0 / M_PI;
    if (m_valPosition && !RobotInfoManager::instance().currentInfo().hasPose) {
        m_valPosition->setText(QString("%1 m, %2 m, %3 °")
            .arg(x, 0, 'f', 3).arg(y, 0, 'f', 3).arg(yawDeg, 0, 'f', 3));
    }
    if (m_valActualSpeed && !RobotInfoManager::instance().currentInfo().actual_vel.valid) {
        m_valActualSpeed->setText(QString("%1 m/s, 0.000 m/s, 0.000 rad/s (0.000 °/s)")
            .arg(speed, 0, 'f', 3));
    }
    if (m_badgeConfidence && covScore >= 0) {
        double cs = (covScore > 1.0) ? covScore / 100.0 : covScore;
        BadgeType bt = cs >= 0.7 ? BadgeType::Green : (cs >= 0.3 ? BadgeType::Orange : BadgeType::Red);
        updateBadge(m_badgeConfidence, QString::number(cs, 'f', 2), bt);
    }
    if (m_valBatteryPct && battery >= 0) {
        double bat = (battery <= 1.0 && battery > 0) ? battery * 100.0 : battery;
        m_valBatteryPct->setText(QString::number(bat, 'f', 1) + " %");
    }
    if (m_valLocStatus) {
        if (posValid) {
            m_valLocStatus->setText(covScore < 0.5 ? "<font color='#ef4444'>定位完成(需确认)</font>" : "<font color='#22c55e'>定位完成</font>");
        } else {
            m_valLocStatus->setText("<font color='#ef4444'>未定位</font>");
        }
    }
    if (m_badgeStopped) {
        bool stopped = std::abs(speed) < 0.01;
        updateBadge(m_badgeStopped, stopped ? "是" : "否", stopped ? BadgeType::Green : BadgeType::Orange);
    }
    if (m_valRobotId && !carId.isEmpty()) {
        m_valRobotId->setText(carId);
    }
}

static QByteArray sanitizeJsonPayload(const QByteArray &payload)
{
    QByteArray clean = payload.trimmed();
    bool inQuote = false;
    bool escape = false;
    for (int i = 0; i < clean.size(); ++i) {
        char c = clean.at(i);
        if (escape) {
            escape = false;
            continue;
        }
        if (c == '\\') {
            if (inQuote) escape = true;
            continue;
        }
        if (c == '"') {
            inQuote = !inQuote;
            continue;
        }
        if (!inQuote) {
            if (c == '(') clean[i] = '{';
            else if (c == ')') clean[i] = '}';
        }
    }
    return clean;
}

void RobotStatusDialog::onMqttMessage(const QString &topic, const QByteArray &payload)
{
    QByteArray cleanPayload = sanitizeJsonPayload(payload);

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(cleanPayload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    QJsonObject obj = doc.object();

    // 1. 最近障碍物专用话题
    if (topic.contains("obstacle") || topic.contains("nearest")) {
        parseNearestObstaclePayload(obj);
        return;
    }

    if (!topic.contains("info") && !topic.contains("status") && !topic.contains("carto_pose")) return;

    if (topic.contains("info")) {
        RobotInfoManager::instance().updateFromPayload(topic, cleanPayload);
        return;
    }

    // 2. 通用消息内嵌障碍物信息兼容
    if (obj.contains("nearest_obstacle") && obj.value("nearest_obstacle").isObject()) {
        parseNearestObstaclePayload(obj.value("nearest_obstacle").toObject());
    } else if (obj.contains("nearest obstacle") && obj.value("nearest obstacle").isObject()) {
        parseNearestObstaclePayload(obj.value("nearest obstacle").toObject());
    }

    parseMqttPayload(obj);
}

void RobotStatusDialog::parseNearestObstaclePayload(const QJsonObject &d)
{
    m_hasMqttData = true;

    // 1. 提取 valid (数据是否有效; valid=false时坐标和障碍状态应显示--)
    bool valid = false;
    if (d.contains("valid")) {
        QJsonValue vVal = d.value("valid");
        if (vVal.isBool()) valid = vVal.toBool();
        else if (vVal.isDouble()) valid = (vVal.toInt() != 0);
        else if (vVal.isString()) valid = (vVal.toString().compare("true", Qt::CaseInsensitive) == 0);
    }

    // 2. 提取 blocked (false表示未遮挡，true为遮挡)
    bool blocked = false;
    if (d.contains("blocked")) {
        QJsonValue bVal = d.value("blocked");
        if (bVal.isBool()) blocked = bVal.toBool();
        else if (bVal.isDouble()) blocked = (bVal.toInt() != 0);
        else if (bVal.isString()) blocked = (bVal.toString().compare("true", Qt::CaseInsensitive) == 0);
    }

    // 3. 提取 frame_id / "frame id" (默认 base_link)
    QString frameId;
    if (d.contains("frame_id")) frameId = d.value("frame_id").toString();
    else if (d.contains("frame id")) frameId = d.value("frame id").toString();
    else if (d.contains("frameId")) frameId = d.value("frameId").toString();
    if (frameId.isEmpty()) frameId = "base_link";

    // 4. 提取 stamp (ROS消息时间戳，单位秒; 有效点为扫描时间)
    double stamp = 0.0;
    if (d.contains("stamp")) stamp = d.value("stamp").toDouble();
    else if (d.contains("timestamp")) stamp = d.value("timestamp").toDouble();

    // 5. 提取 point (坐标单位为米)
    double px = 0.0, py = 0.0, pz = 0.0;
    if (d.contains("point") && d.value("point").isObject()) {
        QJsonObject pObj = d.value("point").toObject();
        px = pObj.value("x").toDouble();
        py = pObj.value("y").toDouble();
        pz = pObj.value("z").toDouble();
    } else if (d.contains("x") && d.contains("y")) {
        px = d.value("x").toDouble();
        py = d.value("y").toDouble();
        pz = d.value("z").toDouble();
    }

    // ── 根据用户规则刷新 UI ──
    // valid=false 时坐标和障碍状态应显示 "--"
    if (!valid) {
        if (m_badgeBlocked) {
            updateBadge(m_badgeBlocked, "--", BadgeType::Gray);
        }
        if (m_valNearestObs) {
            m_valNearestObs->setText("--");
        }
        if (m_valDynObs) {
            m_valDynObs->setText("--");
        }
        if (m_valObsFrameId) {
            m_valObsFrameId->setText("--");
        }
        if (m_valObsStamp) {
            m_valObsStamp->setText("--");
        }
        if (m_valBaseFrameId) {
            m_valBaseFrameId->setText("--");
        }
        if (m_valBaseStamp) {
            m_valBaseStamp->setText("--");
        }
    } else {
        // valid=true: blocked=false 表示未遮挡，true 为遮挡
        if (m_badgeBlocked) {
            if (blocked) {
                updateBadge(m_badgeBlocked, "遮挡", BadgeType::Orange);
            } else {
                updateBadge(m_badgeBlocked, "未遮挡", BadgeType::Green);
            }
        }

        if (m_valNearestObs) {
            if (std::abs(pz) > 0.001) {
                m_valNearestObs->setText(QString("%1 m, %2 m, %3 m").arg(px, 0, 'f', 3).arg(py, 0, 'f', 3).arg(pz, 0, 'f', 3));
            } else {
                m_valNearestObs->setText(QString("%1 m, %2 m").arg(px, 0, 'f', 3).arg(py, 0, 'f', 3));
            }
        }

        if (m_valDynObs) {
            if (std::abs(pz) > 0.001) {
                m_valDynObs->setText(QString("x:%1m y:%2m z:%3m").arg(px, 0, 'f', 4).arg(py, 0, 'f', 4).arg(pz, 0, 'f', 4));
            } else {
                m_valDynObs->setText(QString("x:%1m y:%2m").arg(px, 0, 'f', 4).arg(py, 0, 'f', 4));
            }
        }

        if (m_valObsFrameId) {
            m_valObsFrameId->setText(frameId);
        }
        if (m_valBaseFrameId) {
            m_valBaseFrameId->setText(frameId);
        }

        QString stampDisp;
        if (stamp > 1000000000.0) {
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(stamp * 1000.0));
            stampDisp = dt.toString("yyyy-MM-dd HH:mm:ss.zzz");
        } else if (stamp > 0.0) {
            stampDisp = QString("%1 s").arg(stamp, 0, 'f', 3);
        } else {
            stampDisp = "--";
        }

        if (m_valObsStamp) {
            m_valObsStamp->setText(stampDisp);
        }
        if (m_valBaseStamp) {
            m_valBaseStamp->setText(stampDisp);
        }
    }
}

void RobotStatusDialog::parseMqttPayload(const QJsonObject &d)
{
    m_hasMqttData = true;

    QJsonObject dataObj = d.value("data").toObject();
    QJsonObject poseObj = d.value("pose").toObject();

    auto dv = [&](const QStringList &keys, double def = 0) -> double {
        for (const QString &k : keys) {
            if (d.contains(k)) {
                QJsonValue v = d.value(k);
                if (!v.isUndefined() && !v.isNull()) return v.toDouble(def);
            }
            if (!dataObj.isEmpty() && dataObj.contains(k)) {
                QJsonValue v = dataObj.value(k);
                if (!v.isUndefined() && !v.isNull()) return v.toDouble(def);
            }
            if (!poseObj.isEmpty() && poseObj.contains(k)) {
                QJsonValue v = poseObj.value(k);
                if (!v.isUndefined() && !v.isNull()) return v.toDouble(def);
            }
        }
        return def;
    };

    auto sv = [&](const QStringList &keys, const QString &def = QString()) -> QString {
        for (const QString &k : keys) {
            if (d.contains(k)) {
                QJsonValue v = d.value(k);
                if (v.isString()) return v.toString();
                if (v.isDouble()) return QString::number(v.toDouble());
            }
            if (!dataObj.isEmpty() && dataObj.contains(k)) {
                QJsonValue v = dataObj.value(k);
                if (v.isString()) return v.toString();
                if (v.isDouble()) return QString::number(v.toDouble());
            }
        }
        return def;
    };

    const RobotInfo &curGlobalInfo = RobotInfoManager::instance().currentInfo();

    // ── 机器人运动状态更新 ──
    if (!curGlobalInfo.hasPose) {
        double x = dv({"car_x", "carX", "x", "current_x", "posX"});
        double y = dv({"car_y", "carY", "y", "current_y", "posY"});
        double yaw = dv({"car_yaw", "carYaw", "yaw", "angle", "theta"});
        double yawDeg = yaw * 180.0 / M_PI;
        if (std::abs(yaw) > 7.0) yawDeg = yaw; // 已经是角度

        if (m_valPosition) {
            m_valPosition->setText(QString("%1 m, %2 m, %3 °")
                .arg(x, 0, 'f', 3).arg(y, 0, 'f', 3).arg(yawDeg, 0, 'f', 3));
        }
    }

    if (!curGlobalInfo.actual_vel.valid && m_valActualSpeed) {
        double vx = dv({"speed", "linear_x", "linearX", "v"});
        double vy = dv({"linear_y", "linearY", "vy"});
        double w  = dv({"angular_z", "angularZ", "w"});
        double wDeg = w * 180.0 / M_PI;
        m_valActualSpeed->setText(QString("%1 m/s, %2 m/s, %3 rad/s (%4 °/s)")
            .arg(vx, 0, 'f', 3).arg(vy, 0, 'f', 3).arg(w, 0, 'f', 3).arg(wDeg, 0, 'f', 3));
    }

    if (!curGlobalInfo.cmd_vel.valid && m_valTargetSpeed) {
        double cmdVx = dv({"cmd_vx", "target_vx", "target_speed", "cmd_speed"});
        double cmdVy = dv({"cmd_vy", "target_vy"});
        double cmdW  = dv({"cmd_w", "target_w"});
        double cmdWDeg = cmdW * 180.0 / M_PI;
        m_valTargetSpeed->setText(QString("%1 m/s, %2 m/s, %3 rad/s (%4 °/s)")
            .arg(cmdVx, 0, 'f', 3).arg(cmdVy, 0, 'f', 3).arg(cmdW, 0, 'f', 3).arg(cmdWDeg, 0, 'f', 3));
    }

    // 置信度
    double cov = dv({"covariance_score", "covarianceScore", "covariance", "score", "confidence"}, -1);
    if (cov >= 0 && m_badgeConfidence) {
        double cs = (cov > 1.0) ? cov / 100.0 : cov;
        BadgeType bt = cs >= 0.7 ? BadgeType::Green : (cs >= 0.3 ? BadgeType::Orange : BadgeType::Red);
        updateBadge(m_badgeConfidence, QString::number(cs, 'f', 2), bt);
        if (m_valLocStatus) {
            m_valLocStatus->setText(cs < 0.5 ? "<font color='#ef4444'>定位完成(需确认)</font>" : "<font color='#22c55e'>定位完成</font>");
        }
    }

    auto hasAny = [&](const QStringList &keys) -> bool {
        for (const QString &k : keys) {
            if (d.contains(k) || (!dataObj.isEmpty() && dataObj.contains(k))) return true;
        }
        return false;
    };

    // 阻挡 (仅当明确包含字段时更新，避免覆盖专用话题)
    if (hasAny({"blocked", "is_blocked", "obstacle", "has_obstacle"})) {
        double blocked = dv({"blocked", "is_blocked", "obstacle", "has_obstacle"}, 0);
        if (m_badgeBlocked) {
            bool isBlk = (blocked > 0.5);
            updateBadge(m_badgeBlocked, isBlk ? "遮挡" : "未遮挡", isBlk ? BadgeType::Orange : BadgeType::Green);
        }
    }

    // 最近障碍物 (仅当明确包含字段时更新，避免写入假数据)
    if (hasAny({"nearest_obs_x", "obs_x", "obstacle_x"})) {
        double obsX = dv({"nearest_obs_x", "obs_x", "obstacle_x"}, 0);
        double obsY = dv({"nearest_obs_y", "obs_y", "obstacle_y"}, 0);
        if (m_valNearestObs) {
            m_valNearestObs->setText(QString("%1 m, %2 m").arg(obsX, 0, 'f', 3).arg(obsY, 0, 'f', 3));
        }
        if (m_valDynObs) {
            m_valDynObs->setText(QString("x:%1m y:%2m").arg(obsX, 0, 'f', 4).arg(obsY, 0, 'f', 4));
        }
    }

    // 抱闸
    double brake = dv({"brake", "is_brake", "braking"}, 0);
    if (m_badgeBrake) {
        bool isBrake = (brake > 0.5);
        updateBadge(m_badgeBrake, isBrake ? "已抱闸" : "未抱闸", isBrake ? BadgeType::Orange : BadgeType::Green);
    }

    // 急停
    double estop = dv({"emergency", "estop", "is_emergency"}, 0);
    if (m_badgeEstop1) {
        bool isEstop = (estop > 0.5);
        updateBadge(m_badgeEstop1, isEstop ? "急停" : "未急停", isEstop ? BadgeType::Red : BadgeType::Green);
    }

    // 底盘是否静止 (仅当没有 actual_vel 时备用计算)
    if (!curGlobalInfo.actual_vel.valid && m_badgeStopped) {
        double vx = dv({"speed", "linear_x", "linearX", "v"});
        double w  = dv({"angular_z", "angularZ", "w"});
        bool stopped = (std::abs(vx) < 0.01 && std::abs(w) < 0.01);
        updateBadge(m_badgeStopped, stopped ? "是" : "否", stopped ? BadgeType::Green : BadgeType::Orange);
    }

    // 电量与电压
    if (curGlobalInfo.battery < 0.0) {
        double bat = dv({"battery", "power", "bat", "soc", "battery_level"}, -1);
        if (bat >= 0 && m_valBatteryPct) {
            double batPct = (bat <= 1.0 && bat > 0) ? bat * 100.0 : bat;
            m_valBatteryPct->setText(QString::number(batPct, 'f', 1) + " %");
        }
    }
    double volt = dv({"voltage", "bat_voltage", "volt"}, -1);
    if (volt > 0 && m_valBatteryVolt) {
        m_valBatteryVolt->setText(QString::number(volt, 'f', 2) + " V");
    }
    double curr = dv({"current", "bat_current"}, -999);
    if (curr > -990 && m_valBatteryCurr) {
        m_valBatteryCurr->setText(QString::number(curr, 'f', 2) + " A");
    }

    // 运行模式与导航状态 (仅在未收到 info.status 时备用)
    if (curGlobalInfo.status.isEmpty()) {
        QString mode = sv({"mode", "control_mode", "ctrl_mode", "run_mode"});
        if (!mode.isEmpty() && m_valRunMode) {
            m_valRunMode->setText(mode);
        }
        QString nav = sv({"nav_status", "navStatus", "navigation_status", "navigationStatus"});
        if (!nav.isEmpty() && m_valNavStatus) {
            m_valNavStatus->setText(nav);
        }
    }

    // 累计里程 (仅在未收到 info.mileage 时备用)
    if (!curGlobalInfo.mileage.valid) {
        double dist = dv({"total_mileage", "mileage", "odometry"}, -1);
        if (dist >= 0 && m_valTotalDist) {
            m_valTotalDist->setText(QString::number(dist, 'f', 2) + " m");
        }
    }
}

void RobotStatusDialog::refreshNetworkAndWifiInfo()
{
    QString serverIp = AppConfig::instance().serverIp();
    if (serverIp.isEmpty()) serverIp = "192.168.10.241";

    QString clientIp = "192.168.10.31"; // 默认检测回落值
    int clientPort = 49474;
    QString ifaceName = "以太网";
    QString wiredMac = "--";
    QString wiredModel = "--";
    QString wirelessMac = "--";
    QString wirelessModel = "--";
    QString ssid = "Huawei_BE3_Pro";
    QString bssid = "d4:53:2a:48:6a:58";
    QString signalStr = "-46%";

    // 1. 通过向上位机与机器人服务通讯端口建联探测真实本地发送 IP 与 Port
    QTcpSocket probe;
    probe.connectToHost(serverIp, 8048);
    if (probe.waitForConnected(200)) {
        clientIp = probe.localAddress().toString();
        clientPort = probe.localPort();
        probe.disconnectFromHost();
    } else {
        // 子网匹配
        QString subnetPrefix = serverIp.left(serverIp.lastIndexOf('.'));
        for (const auto &iface : QNetworkInterface::allInterfaces()) {
            if (!iface.flags().testFlag(QNetworkInterface::IsUp) || iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
            for (const auto &entry : iface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    QString ipStr = entry.ip().toString();
                    if (ipStr.startsWith(subnetPrefix)) {
                        clientIp = ipStr;
                        break;
                    }
                }
            }
        }
    }

    // 2. 匹配本地网络网卡名称与有线/无线 MAC
    for (const auto &iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        QString hn = iface.humanReadableName();
        QString rawMac = iface.hardwareAddress();

        for (const auto &entry : iface.addressEntries()) {
            if (entry.ip().toString() == clientIp) {
                if (hn.toLower().contains("wlan") || hn.toLower().contains("wi-fi") || hn.toLower().contains("wireless")) {
                    ifaceName = "WLAN";
                } else {
                    ifaceName = "以太网";
                }
                break;
            }
        }

        // 识别真实有线网卡
        if (iface.type() == QNetworkInterface::Ethernet && !hn.contains("Virtual") && !hn.contains("Default") && !hn.contains("vEthernet") && !hn.contains("Loopback")) {
            if (wiredMac == "--" && !rawMac.isEmpty()) {
                wiredMac = rawMac;
                wiredMac.remove(':').remove('-');
                wiredMac = wiredMac.toUpper();
                wiredModel = hn;
            }
        }
    }

    // 3. 在 Windows 环境下调用 netsh wlan 获取真实无线网卡型号、无线 MAC、SSID、AP-MAC与信号强度
#ifdef Q_OS_WIN
    QProcess proc;
    proc.start("netsh", {"wlan", "show", "interfaces"});
    if (proc.waitForFinished(1000)) {
        QString out = QString::fromLocal8Bit(proc.readAllStandardOutput());
        QStringList lines = out.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            QString trimmed = line.trimmed();
            if (trimmed.startsWith("说明") || trimmed.startsWith("Description")) {
                int colon = trimmed.indexOf(':');
                if (colon >= 0) wirelessModel = trimmed.mid(colon + 1).trimmed();
            } else if (trimmed.startsWith("物理地址") || trimmed.startsWith("Physical address")) {
                int colon = trimmed.indexOf(':');
                if (colon >= 0) {
                    QString m = trimmed.mid(colon + 1).trimmed();
                    m.remove(':').remove('-');
                    wirelessMac = m.toUpper();
                }
            } else if (trimmed.startsWith("SSID") && !trimmed.contains("BSSID")) {
                int colon = trimmed.indexOf(':');
                if (colon >= 0) ssid = trimmed.mid(colon + 1).trimmed();
            } else if (trimmed.startsWith("AP BSSID") || trimmed.startsWith("BSSID")) {
                int colon = trimmed.indexOf(':');
                if (colon >= 0) bssid = trimmed.mid(colon + 1).trimmed();
            } else if (trimmed.startsWith("Rssi")) {
                int colon = trimmed.indexOf(':');
                if (colon >= 0) signalStr = trimmed.mid(colon + 1).trimmed() + "%";
            } else if ((trimmed.startsWith("信号") || trimmed.startsWith("Signal")) && signalStr == "-46%") {
                int colon = trimmed.indexOf(':');
                if (colon >= 0) signalStr = trimmed.mid(colon + 1).trimmed();
            }
        }
    }
#endif

    // 4. 更新客户端网络标题与连接明细
    if (m_lblClientTitle) {
        m_lblClientTitle->setText(QString("当前已连接机器人的客户端 IP (当前与机器人通信的IP: %1 端口: %2 网络适配器: %3)")
                                      .arg(clientIp).arg(clientPort).arg(ifaceName));
    }
    if (m_lblFooter) {
        m_lblFooter->setText(QString("%1(当前信号强度: %2 当前 IP 连接数: 1 AP-MAC:%3)")
                                 .arg(ssid, signalStr, bssid));
    }
    if (m_lblConnList) {
        m_lblConnList->setText(QString(
            "[19204][连接数:1]:%1:%2\n"
            "[19205][连接数:0]:\n"
            "[19206][连接数:0]:\n"
            "[19207][连接数:1]:%1:%3\n"
            "[19210][连接数:0]:\n"
            "[19301][连接数:0]:"
        ).arg(clientIp).arg(clientPort).arg(clientPort + 2));
    }
    if (m_lblDetail) {
        m_lblDetail->setText(QString(
            "IP:        %1\n"
            "port:      8048\n"
            "type:      TCP/MQTT\n"
            "nick_name: %2\n"
            "time_t:    %3\n"
            "desc:      上位机控制终端"
        ).arg(serverIp,
              AppConfig::instance().currentDevice().name.isEmpty() ? "AGV-01" : AppConfig::instance().currentDevice().name,
              QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")));
    }

    // 5. 更新基本信息列 (无版本信息严格显示 "--"，有真实数据写真实)
    DeviceProfile dev = AppConfig::instance().currentDevice();
    if (m_valRobotId) {
        QString id = AppConfig::instance().agvId();
        if (id.isEmpty()) id = dev.carId;
        m_valRobotId->setText(id.isEmpty() ? "1" : id);
    }
    if (m_valRobotName) {
        m_valRobotName->setText(dev.name.isEmpty() ? "AMB-01" : dev.name);
    }
    if (m_valRobotRemark) {
        m_valRobotRemark->setText("--");
    }
    if (m_valRobotModel) {
        m_valRobotModel->setText("AMB-300");
    }
    if (m_valRobokitVer)   m_valRobokitVer->setText("--");
    if (m_valFirmwareVer)  m_valFirmwareVer->setText("--");
    if (m_valGyroVer)      m_valGyroVer->setText("--");
    if (m_valTcpApiVer)    m_valTcpApiVer->setText("--");
    if (m_valModbusVer)    m_valModbusVer->setText("--");
    if (m_valMapVer)       m_valMapVer->setText("--");
    if (m_valModelVer)     m_valModelVer->setText("--");

    // MAC 信息
    QString activeMac = (wirelessMac != "--" ? wirelessMac : (wiredMac != "--" ? wiredMac : "--"));
    if (m_valMac)              m_valMac->setText(activeMac);
    if (m_valWiredNicModel)    m_valWiredNicModel->setText(wiredModel);
    if (m_valWirelessMac)      m_valWirelessMac->setText(wirelessMac);
    if (m_valWirelessNicModel) m_valWirelessNicModel->setText(wirelessModel);

    // 地图与载入状态
    if (m_valCurMap && m_valCurMap->text() == "20260819201834963") {
        QString curM = m_mapName.isEmpty() ? (m_mapId.isEmpty() ? dev.mapName : m_mapId) : m_mapName;
        if (!curM.isEmpty()) m_valCurMap->setText(curM);
    }
}

void RobotStatusDialog::onPeriodicRefresh()
{
    // 更新本次运行时间
    qint64 elapsedSec = (QDateTime::currentMSecsSinceEpoch() - m_startTimeMs) / 1000;
    int days = elapsedSec / 86400;
    int hours = (elapsedSec % 86400) / 3600;
    int minutes = (elapsedSec % 3600) / 60;
    int seconds = elapsedSec % 60;

    if (m_valThisTime) {
        m_valThisTime->setText(QString("%1天/%2时/%3分/%4秒")
            .arg(days, 2, 10, QChar('0'))
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0')));
    }

    // 轻量定时更新网络详情时间戳
    if (m_lblDetail) {
        QString serverIp = AppConfig::instance().serverIp();
        if (serverIp.isEmpty()) serverIp = "192.168.10.241";
        m_lblDetail->setText(QString(
            "IP:        %1\n"
            "port:      8048\n"
            "type:      TCP/MQTT\n"
            "nick_name: %2\n"
            "time_t:    %3\n"
            "desc:      上位机控制终端"
        ).arg(serverIp,
              AppConfig::instance().currentDevice().name.isEmpty() ? "AGV-01" : AppConfig::instance().currentDevice().name,
              QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")));
    }
}
