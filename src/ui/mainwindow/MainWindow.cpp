#include "MainWindow.h"
#include "ui/map/CreateMapWidget.h"
#include "ui/control/EditConfigWidget.h"
#include "ui/map/MapViewWidget.h"
#include "ui/map/MapEditWidget.h"
#include "ui/map/MapImageEditWidget.h"
#include "ui/task/TaskWidget.h"
#include "ui/task/TaskEditWidget.h"
#include "ui/task/TaskLogWidget.h"
#include "ui/task/TaskChainWidget.h"
#include "app/AppConfig.h"
#include "network/MqttService.h"
#include "ui/dialogs/DeviceManagerDialog.h"
#include "ui/common/Toast.h"
#include "core/TeleopController.h"
#include "network/ApiService.h"
#include "core/RobotInfoManager.h"
#include "core/TaskChainManager.h"
#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QActionGroup>
#include <QStatusBar>
#include <QLabel>
#include <QTimer>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QFrame>
#include <QMessageBox>
#include <QSplitter>
#include <QApplication>
#include <QPushButton>
#include <QMenuBar>
#include <QMenu>
#include <QIcon>
#include <QPalette>
#include <QEvent>
#include <QCursor>
#include "ui/common/StyleLoader.h"

// ─────────────────────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupGlobalStyle();
    setupHeaderBar();
    setupContentArea();
    connectServices();
    initPages();
    setupStatusBar();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
}

void MainWindow::setupGlobalStyle()
{
    StyleLoader::applyApplicationStyle();
    setWindowTitle("QT-AGV  移动机器人管理平台");
    resize(1440, 900);

    // 设置应用图标
    setWindowIcon(QIcon(":/app-icon.svg"));

    // 设置深色Palette，让Windows原生标题栏匹配主题色
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(18, 21, 26));          // #12151a
    darkPalette.setColor(QPalette::WindowText, QColor(200, 208, 216));   // #c8d0d8
    darkPalette.setColor(QPalette::Base, QColor(30, 34, 40));            // #1e2228
    darkPalette.setColor(QPalette::AlternateBase, QColor(34, 38, 46));   // #22262e
    darkPalette.setColor(QPalette::Text, QColor(200, 208, 216));
    darkPalette.setColor(QPalette::Button, QColor(37, 42, 53));          // #252a35
    darkPalette.setColor(QPalette::ButtonText, QColor(200, 208, 216));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Highlight, QColor(30, 64, 128));      // #1e4080
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    darkPalette.setColor(QPalette::ToolTipBase, QColor(18, 21, 26));
    darkPalette.setColor(QPalette::ToolTipText, QColor(200, 208, 216));
    darkPalette.setColor(QPalette::PlaceholderText, QColor(122, 138, 153));
    // 标题栏相关
    darkPalette.setColor(QPalette::Active, QPalette::Window, QColor(18, 21, 26));
    darkPalette.setColor(QPalette::Inactive, QPalette::Window, QColor(18, 21, 26));
    qApp->setPalette(darkPalette);
}

void MainWindow::setupHeaderBar()
{
    // ── Section navigation tabs ──
    m_sectionBar = new QToolBar(this);
    m_sectionBar->setObjectName("sectionBar");
    m_sectionBar->setMovable(false);
    m_sectionBar->setFloatable(false);
    m_sectionBar->setFixedHeight(42);
    m_sectionBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    m_secGroup = new QActionGroup(this);
    m_secGroup->setExclusive(true);

    // text + the QStackedWidget page index each tab activates.
    // "创建地图" is no longer a top-level tab — it now lives as a secondary
    struct SectionDef { QString text; int pageIndex; };
    const QList<SectionDef> sections = {
        {"  地图与控制", 0},
        // {"  任务管理  ", 2},  // 暂不需要任务管理功能，已隐藏
        {"  任务链  ", 5},
        // {"  参数配置  ", 4},  // 已迁移至“地图与控制”工具栏中的“参数”弹窗
        {"  任务日志  ", 3},
    };

    for (int i = 0; i < sections.size(); ++i) {
        QAction *act = m_sectionBar->addAction(sections[i].text);
        act->setCheckable(true);
        act->setData(sections[i].pageIndex);
        m_secGroup->addAction(act);
        connect(act, &QAction::triggered, this, [this, act]() {
            onSectionChanged(act->data().toInt());
        });
    }
    highlightTabForPageIndex(0);   // 与 initPages() 的 m_stack->setCurrentIndex(0) 保持一致

    addToolBar(Qt::TopToolBarArea, m_sectionBar);

    // ── Menu bar (system/help) ──
    QMenu *systemMenu = menuBar()->addMenu("系统");
    systemMenu->addAction("切换设备", this, &MainWindow::onChangeDevice);
    systemMenu->addSeparator();
    systemMenu->addAction("退出", qApp, &QApplication::quit);

    QMenu *helpMenu = menuBar()->addMenu("帮助");
    helpMenu->addAction("关于", this, &MainWindow::onAbout);
}

void MainWindow::setupContentArea()
{
    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);
}

void MainWindow::initPages()
{
    // 全局启动任务链 MQTT 状态与进度监听及缓存
    TaskChainManager::instance().init();

    m_createMapPage  = new CreateMapWidget;
    m_editConfigPage = new EditConfigWidget;
    m_mapPage        = new MapViewWidget;
    m_taskPage       = new TaskWidget;
    m_taskLogPage    = new TaskLogWidget;
    m_taskChainPage  = new TaskChainWidget;

    m_stack->addWidget(m_mapPage);         // 0 地图列表
    m_stack->addWidget(m_createMapPage);   // 1 创建地图
    m_stack->addWidget(m_taskPage);        // 2 任务管理
    m_stack->addWidget(m_taskLogPage);     // 3 任务日志
    m_stack->addWidget(m_editConfigPage);  // 4 参数配置
    m_stack->addWidget(m_taskChainPage);   // 5 任务链

    connect(m_taskPage,    &TaskWidget::navigateRequested,
            this, &MainWindow::onNavigateRequested);
    connect(m_taskLogPage, &TaskLogWidget::navigateRequested,
            this, &MainWindow::onNavigateRequested);
    connect(m_mapPage,     &MapViewWidget::navigateRequested,
            this, &MainWindow::onNavigateRequested);
    connect(m_mapPage,     &MapViewWidget::mapStatusChanged,
            this, &MainWindow::updateStatusBar);
    connect(m_createMapPage, &CreateMapWidget::goBack, this, [this]() {
        m_stack->setCurrentWidget(m_mapPage);
        m_mapPage->forceRefreshAfterMapping();  // 强制清缓存+重新下载最新地图
        highlightTabForPageIndex(0);
    });
    connect(m_editConfigPage, &EditConfigWidget::routeNetworkUpdated,
            m_mapPage, &MapViewWidget::refreshData);

    connect(m_stack, &QStackedWidget::currentChanged, this, &MainWindow::refreshCurrentPage);

    m_stack->setCurrentIndex(0);
}

void MainWindow::setupStatusBar()
{
    QStatusBar *sb = statusBar();

    DeviceProfile dev = AppConfig::instance().currentDevice();

    m_sbDevice = new QLabel(QString("设备: %1").arg(dev.name.isEmpty() ? "未连接" : dev.name));
    m_sbCarId  = new QLabel(QString("CarID: %1").arg(dev.carId.isEmpty() ? "--" : dev.carId));
    m_sbConn   = new QLabel("● MQTT: 连接中...");
    m_sbConn->setCursor(Qt::PointingHandCursor);
    m_sbConn->setToolTip("点击重新连接 MQTT");
    m_sbConn->installEventFilter(this);

    m_sbCov       = new QLabel("置信度: --");
    m_sbLatency   = new QLabel("网络延迟: --");
    m_sbTeleop    = new QLabel("无人抢占控制权");
    m_sbLocStatus = new QLabel("定位状态: 定位失败");
    m_sbMapStatus = new QLabel("地图加载: 未加载");

    sb->addWidget(m_sbDevice);
    sb->addWidget(m_sbCarId);
    sb->addWidget(m_sbConn);
    sb->addWidget(m_sbCov);
    sb->addWidget(m_sbLatency);
    sb->addWidget(m_sbTeleop);
    sb->addWidget(m_sbLocStatus);
    sb->addWidget(m_sbMapStatus);

    // 抢占控制权按钮已移至顶部地图视图工具栏，底栏不再放置

    QString ipText = dev.ip.isEmpty() ? "--" : dev.ip;
    sb->addPermanentWidget(new QLabel(QString("IP: %1").arg(ipText)));

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_statusTimer->start(3000);

    // 实时同步 MQTT 状态与车辆数据
    connect(&MqttService::instance(), &MqttService::connected,    this, &MainWindow::updateStatusBar);
    connect(&MqttService::instance(), &MqttService::disconnected, this, &MainWindow::updateStatusBar);
    connect(&MqttService::instance(), &MqttService::stateChanged, this, &MainWindow::updateStatusBar);
    connect(&RobotInfoManager::instance(), &RobotInfoManager::currentRobotInfoUpdated,
            this, &MainWindow::updateStatusBar);

    updateStatusBar();

    // 同步 TeleopController 状态到按钮（防止外部 setActive 触发后按钮不同步）
    connect(&TeleopController::instance(), &TeleopController::activeChanged,
            this, &MainWindow::onTeleopActiveChanged);
}

void MainWindow::connectServices()
{
    // 单车独占：连接新车前主动断开旧的所有连接，清空旧订阅与旧车数据缓存
    MqttService::instance().clearAllSubscriptions();
    MqttService::instance().disconnectFromHost();

    QString curCarId = AppConfig::instance().agvId().trimmed();
    if (curCarId.isEmpty()) curCarId = AppConfig::instance().currentDevice().carId.trimmed();
    RobotInfoManager::instance().resetForCar(curCarId);
    RobotInfoManager::instance().init();

    QString ip = AppConfig::instance().serverIp();
    if (ip.isEmpty()) return;

    MqttService::instance().connectToHost(ip, 1883);
}

// ── Slots ──────────────────────────────────────────────────────────────────

void MainWindow::onSectionChanged(int index)
{
    // 检查当前页面是否有未保存的修改(如路网编辑/建图)
    QWidget *cur = m_stack->currentWidget();
    if (cur) {
        bool canLeave = true;
        auto *mapEdit = qobject_cast<MapEditWidget*>(cur);
        if (mapEdit) canLeave = mapEdit->checkUnsavedChanges();

        auto *createMap = qobject_cast<CreateMapWidget*>(cur);
        if (createMap) canLeave = createMap->tryExitMapping();

        if (!canLeave) {
            // 用户取消或 API 调用中，恢复之前选中的菜单项
            // 创建地图页面(index=2)没有对应的菜单项，其父级是"地图管理"(index=1)
            int prevIdx = m_stack->currentIndex();
            if (prevIdx == 1) prevIdx = 0;   // 创建地图 → 地图管理
            for (QAction *act : m_secGroup->actions()) {
                if (act->data().toInt() == prevIdx) {
                    m_secGroup->blockSignals(true);
                    act->setChecked(true);
                    m_secGroup->blockSignals(false);
                    break;
                }
            }
            return;
        }
    }
    if (m_stack->currentIndex() == index) {
        refreshCurrentPage(index);
    } else {
        m_stack->setCurrentIndex(index);
    }
}

// 每次切换主菜单时刷新当前页面数据，保证数据是最新的
void MainWindow::refreshCurrentPage(int index)
{
    switch (index) {
    case 0:  // 地图管理
        if (m_mapPage) m_mapPage->refreshData();
        break;
    case 1:  // 创建地图 - 无需刷新列表
        break;
    case 2:  // 任务管理
        if (m_taskPage) m_taskPage->refreshList();
        break;
    case 3:  // 任务日志
        if (m_taskLogPage) m_taskLogPage->refreshData();
        break;
    case 4:  // 参数配置
        if (m_editConfigPage) m_editConfigPage->refreshData();
        break;
    default: break;
    }
}

void MainWindow::switchToPage(const QString &pageKey)
{
    if (pageKey == "edit_config") {
        m_stack->setCurrentWidget(m_mapPage);
        highlightTabForPageIndex(0);
        if (m_mapPage) {
            m_mapPage->onShowConfigDialog();
        }
        return;
    }

    static const QMap<QString, int> pageMap = {
        {"map",         0},
        {"create_map",  1},
        {"task",        2},
        {"tasklog",     3},
        {"taskchain",   5},
    };
    auto it = pageMap.find(pageKey);
    if (it != pageMap.end()) {
        m_stack->setCurrentIndex(it.value());
        highlightTabForPageIndex(it.value());
    }
}

// "创建地图" has no top-level tab of its own (it's reached via the secondary
// menu inside 地图管理). If pageIndex has no matching tab, leave the
// currently-checked tab untouched rather than checking nothing/wrong tab.
void MainWindow::highlightTabForPageIndex(int pageIndex)
{
    const QList<QAction *> actions = m_secGroup->actions();
    for (QAction *act : actions) {
        if (act->data().toInt() == pageIndex) {
            act->setChecked(true);
            return;
        }
    }
}

void MainWindow::onNavigateRequested(const QString &target, const QString &id)
{
    if (target == "map_show") {
        // MapViewWidget handles map switching internally; navigate to map page
        m_stack->setCurrentWidget(m_mapPage);
        highlightTabForPageIndex(0);

    } else if (target == "create_map") {
        // Secondary page under 地图管理 — no tab to highlight, leave it as-is.
        m_stack->setCurrentWidget(m_createMapPage);

    } else if (target == "map_edit" || target == "map_image_edit") {
        QString key = QString("map_edit_%1").arg(id);
        if (!m_dynamicPages.contains(key)) {
            MapEditWidget *w = new MapEditWidget(id);
            connect(w, &MapEditWidget::goBack, this, [this, key]() {
                m_stack->setCurrentWidget(m_mapPage);
                if (m_mapPage) m_mapPage->refreshData();
                QWidget *p = m_dynamicPages.take(key);
                if (p) { m_stack->removeWidget(p); p->deleteLater(); }
            });
            m_dynamicPages[key] = w;
            m_stack->addWidget(w);
        }
        m_stack->setCurrentWidget(m_dynamicPages[key]);

    } else if (target == "task_edit") {
        QString key = QString("task_edit_%1").arg(id.isEmpty() ? "new" : id);
        if (!m_dynamicPages.contains(key)) {
            TaskEditWidget *w = new TaskEditWidget(id);
            connect(w, &TaskEditWidget::goBack, this, [this, key]() {
                m_stack->setCurrentWidget(m_taskPage);
                m_taskPage->refreshData();
                QWidget *p = m_dynamicPages.take(key);
                if (p) { m_stack->removeWidget(p); p->deleteLater(); }
            });
            m_dynamicPages[key] = w;
            m_stack->addWidget(w);
        }
        m_stack->setCurrentWidget(m_dynamicPages[key]);

    } else if (target == "task_edit_map") {
        if (!id.isEmpty())
            onNavigateRequested("map_show", id);
    }
}

void MainWindow::onChangeDevice()
{
    DeviceManagerDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        connectServices();

        DeviceProfile dev = AppConfig::instance().currentDevice();
        m_sbDevice->setText(QString("设备: %1").arg(dev.name));
        m_sbCarId->setText(QString("CarID: %1").arg(dev.carId));

        // Clear dynamic pages so they reload for new device
        for (QWidget *w : m_dynamicPages.values()) {
            m_stack->removeWidget(w);
            w->deleteLater();
        }
        m_dynamicPages.clear();

        // 切换设备后返回默认主页：地图与控制 (pageIndex 0) 并同步高亮导航栏与刷新数据
        highlightTabForPageIndex(0);
        if (m_stack->currentIndex() == 0) {
            if (m_mapPage) m_mapPage->refreshData();
        } else {
            m_stack->setCurrentIndex(0);
        }
    }
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, "关于",
        "<b>QT-AGV 移动机器人管理平台</b><br>"
        "版本: v1.0<br><br>"
        "基于 Qt6 开发的 AGV 设备管理系统");
}

void MainWindow::updateStatusBar()
{
    QMqttClient::ClientState st = MqttService::instance().state();
    if (st == QMqttClient::Connected) {
        m_sbConn->setText("● MQTT: 已连接");
        m_sbConn->setStyleSheet("color:#5cb85c; background:transparent; border-right:1px solid #2d3340;");
    } else if (st == QMqttClient::Connecting) {
        m_sbConn->setText("● MQTT: 连接中...");
        m_sbConn->setStyleSheet("color:#ecc94b; background:transparent; border-right:1px solid #2d3340;");
    } else {
        m_sbConn->setText("○ MQTT: 未连接");
        m_sbConn->setStyleSheet("color:#e05555; background:transparent; border-right:1px solid #2d3340;");
    }

    const RobotInfo &info = RobotInfoManager::instance().currentInfo();

    // 1. 置信度
    if (m_sbCov) {
        if (info.covScore > 0.0) {
            double cs = (info.covScore > 1.0) ? info.covScore / 100.0 : info.covScore;
            int pct = qRound(cs * 100.0);
            if (pct <= 0) pct = 1;
            // 置信度大于0显示绿色正常
            m_sbCov->setText(QString("置信度: <span style='color:#22c55e;'>%1%</span>").arg(pct));
        } else if (info.covScore == 0.0) {
            m_sbCov->setText("置信度: <span style='color:#ef4444;'>0%</span>");
        } else {
            m_sbCov->setText("置信度: <span style='color:#8898a8;'>--</span>");
        }
    }

    // 2. 网络延迟
    if (m_sbLatency) {
        int lat = (info.latency >= 0) ? info.latency : m_networkLatency;
        if (lat >= 0) {
            QString col = (lat < 60) ? "#22c55e" : ((lat < 150) ? "#eab308" : "#ef4444");
            m_sbLatency->setText(QString("网络延迟: <span style='color:%1;'>%2 ms</span>").arg(col).arg(lat));
        } else {
            m_sbLatency->setText("网络延迟: <span style='color:#8898a8;'>--</span>");
        }
    }

    // 3. 是否抢占控制权 (抢占控制权 / 无人抢占控制权)
    if (m_sbTeleop) {
        bool isTeleop = TeleopController::instance().isActive();
        if (isTeleop) {
            m_sbTeleop->setText("<span style='color:#f87171;'>抢占控制权</span>");
        } else {
            m_sbTeleop->setText("<span style='color:#38bdf8;'>无人抢占控制权</span>");
        }
    }

    // 4. 定位状态 (只有置信度大于0才显示定位成功，不然显示定位失败)
    if (m_sbLocStatus) {
        bool locSuccess = (info.covScore > 0.0);
        if (locSuccess) {
            m_sbLocStatus->setText("定位状态: <span style='color:#22c55e;'>定位成功</span>");
        } else {
            m_sbLocStatus->setText("定位状态: <span style='color:#ef4444;'>定位失败</span>");
        }
    }

    // 5. 地图加载状态 (成功/失败)
    if (m_sbMapStatus) {
        if (m_mapPage) {
            m_sbMapStatus->setText(QString("地图加载: <span style='color:%1;'>%2</span>")
                                      .arg(m_mapPage->mapStatusColor(), m_mapPage->mapStatusText()));
        } else {
            m_sbMapStatus->setText("地图加载: <span style='color:#8898a8;'>未加载</span>");
        }
    }

    testNetworkLatency();
}

void MainWindow::testNetworkLatency()
{
    const RobotInfo &info = RobotInfoManager::instance().currentInfo();
    if (info.latency >= 0) {
        m_networkLatency = info.latency;
        return;
    }

    QString ip = AppConfig::instance().serverIp();
    if (ip.isEmpty()) return;

    auto *socket = new QTcpSocket(this);
    auto *timer = new QElapsedTimer;
    timer->start();

    connect(socket, &QTcpSocket::connected, this, [this, socket, timer]() {
        int elapsed = static_cast<int>(timer->elapsed());
        m_networkLatency = qMax(1, elapsed);
        if (m_sbLatency) {
            QString col = (m_networkLatency < 60) ? "#22c55e" : ((m_networkLatency < 150) ? "#eab308" : "#ef4444");
            m_sbLatency->setText(QString("网络延迟: <span style='color:%1;'>%2 ms</span>").arg(col).arg(m_networkLatency));
        }
        socket->disconnectFromHost();
        socket->deleteLater();
        delete timer;
    });

    connect(socket, &QTcpSocket::errorOccurred, this, [socket, timer](QAbstractSocket::SocketError) {
        socket->deleteLater();
        delete timer;
    });

    socket->connectToHost(ip, 8048);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_sbConn && event->type() == QEvent::MouseButtonRelease) {
        onMqttStatusClicked();
        return true;
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::onMqttStatusClicked()
{
    updateStatusBar();
    if (MqttService::instance().isConnected()) {
        Toast::info(this, "MQTT 已连接");
        return;
    }

    QString ip = AppConfig::instance().serverIp();
    if (ip.isEmpty()) {
        Toast::error(this, "未配置服务器地址，无法重连");
        return;
    }

    connectServices();
    updateStatusBar();
    Toast::info(this, "正在重新连接 MQTT...");
}

void MainWindow::onTeleopToggled()
{
    if (!MqttService::instance().isConnected()) {
        Toast::error(this, "MQTT 未连接，无法启用遥控");
        return;
    }
    if (AppConfig::instance().agvId().isEmpty()) {
        Toast::error(this, "未选择设备，无法启用遥控");
        return;
    }
    TeleopController::instance().setActive(!TeleopController::instance().isActive());
}

void MainWindow::onTeleopActiveChanged(bool active)
{
    if (m_teleopBtn) {
        m_teleopBtn->setObjectName(active ? "teleopBtnActive" : "");
        m_teleopBtn->style()->unpolish(m_teleopBtn);
        m_teleopBtn->style()->polish(m_teleopBtn);
    }
}
