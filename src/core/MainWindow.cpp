#include "MainWindow.h"
#include "widgets/AgvInfoWidget.h"
#include "widgets/CreateMapWidget.h"
#include "widgets/EditConfigWidget.h"
#include "widgets/MapViewWidget.h"
#include "widgets/MapEditWidget.h"
#include "widgets/MapImageEditWidget.h"
#include "widgets/TaskWidget.h"
#include "widgets/TaskEditWidget.h"
#include "widgets/TaskLogWidget.h"
#include "core/AppConfig.h"
#include "core/MqttService.h"
#include "core/WebSocketService.h"
#include "core/DeviceManagerDialog.h"
#include "core/Toast.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QActionGroup>
#include <QStatusBar>
#include <QLabel>
#include <QTimer>
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

// ─────────────────────────────────────────────────────────────────────────────
//  Global dark stylesheet  (matches reference images)
// ─────────────────────────────────────────────────────────────────────────────
static const char *kGlobalStyle = R"(
/* ── Base ── */
QMainWindow, QWidget {
    background-color: #1a1d23;
    color: #c8d0d8;
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
    font-size: 13px;
}

/* ── Header toolbar ── */
QToolBar#headerToolBar {
    background-color: #12151a;
    border-bottom: 2px solid #2d3340;
    spacing: 0px;
    padding: 0px 8px;
}
QToolBar#headerToolBar::separator { width: 0px; }

/* ── Section toolbar (top navigation tabs) ── */
QToolBar#sectionBar {
    background-color: #12151a;
    border-bottom: 1px solid #2d3340;
    spacing: 0px;
    padding: 0px;
}
QToolBar#sectionBar QToolButton {
    background-color: transparent;
    color: #8898a8;
    border: none;
    border-bottom: 3px solid transparent;
    padding: 9px 22px;
    font-size: 13px;
    font-weight: 600;
    min-width: 80px;
    margin: 0px;
}
QToolBar#sectionBar QToolButton:hover {
    color: #c8d0d8;
    background-color: #1e2228;
}
QToolBar#sectionBar QToolButton:checked {
    color: #4a9eff;
    border-bottom: 3px solid #4a9eff;
    background-color: #1e2228;
}

/* ── Splitter ── */
QSplitter::handle:horizontal { width: 1px; background-color: #2d3340; }
QSplitter::handle:vertical   { height: 1px; background-color: #2d3340; }

/* ── Table ── */
QTableWidget {
    background-color: #1e2228;
    alternate-background-color: #22262e;
    gridline-color: #2d3340;
    border: 1px solid #2d3340;
    border-radius: 4px;
    selection-background-color: #1e4080;
    selection-color: #ffffff;
}
QTableWidget QHeaderView::section {
    background-color: #22262e;
    color: #8898a8;
    border: none;
    border-bottom: 1px solid #2d3340;
    border-right: 1px solid #2d3340;
    padding: 6px 10px;
    font-weight: 600;
}
QTableWidget::item { padding: 5px 10px; border: none; }
QTableWidget::item:hover { background-color: #252a35; }
QTableWidget::item:selected { background-color: #1e4080; color: white; }

/* ── TreeWidget ── */
QTreeWidget {
    background-color: #1e2228;
    border: 1px solid #2d3340;
    border-radius: 4px;
    selection-background-color: #1e4080;
    selection-color: #ffffff;
    outline: none;
}
QTreeWidget::item { padding: 4px 6px; }
QTreeWidget::item:hover { background-color: #252a35; }
QTreeWidget::item:selected { background-color: #1e4080; color: white; }
QTreeWidget QHeaderView::section {
    background-color: #22262e;
    color: #8898a8;
    border: none;
    border-bottom: 1px solid #2d3340;
    padding: 5px 8px;
}

/* ── Buttons ── */
QPushButton {
    background-color: #252a35;
    color: #c8d0d8;
    border: 1px solid #3a3f4a;
    border-radius: 4px;
    padding: 6px 14px;
    font-size: 12px;
}
QPushButton:hover { background-color: #2d3340; border-color: #4a5568; }
QPushButton:pressed { background-color: #1e4080; }
QPushButton:disabled { color: #4a5568; border-color: #2d3340; background-color: #1e2228; }

/* ── LineEdit / ComboBox ── */
QLineEdit {
    background-color: #22262e;
    border: 1px solid #3a3f4a;
    border-radius: 4px;
    color: #c8d0d8;
    padding: 5px 8px;
}
QLineEdit:focus { border-color: #4a9eff; }

QComboBox {
    background-color: #22262e;
    border: 1px solid #3a3f4a;
    border-radius: 4px;
    color: #c8d0d8;
    padding: 4px 8px;
    min-height: 22px;
}
QComboBox::drop-down { border: none; width: 20px; }
QComboBox::down-arrow { width: 10px; height: 10px; }
QComboBox QAbstractItemView {
    background-color: #22262e;
    border: 1px solid #3a3f4a;
    selection-background-color: #1e4080;
    color: #c8d0d8;
    outline: none;
}

/* ── GroupBox ── */
QGroupBox {
    border: 1px solid #2d3340;
    border-radius: 5px;
    margin-top: 12px;
    font-weight: 600;
    color: #8898a8;
    padding-top: 6px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 10px;
    top: -2px;
    padding: 0 4px;
}

/* ── ScrollBar ── */
QScrollBar:vertical {
    background: #1a1d23;
    width: 8px;
    border-radius: 4px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: #3a3f4a;
    border-radius: 4px;
    min-height: 20px;
}
QScrollBar::handle:vertical:hover { background: #4a9eff; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
QScrollBar:horizontal {
    background: #1a1d23;
    height: 8px;
    border-radius: 4px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: #3a3f4a;
    border-radius: 4px;
    min-width: 20px;
}
QScrollBar::handle:horizontal:hover { background: #4a9eff; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }

/* ── TextEdit / PlainTextEdit ── */
QTextEdit, QPlainTextEdit {
    background-color: #0d1117;
    border: 1px solid #2d3340;
    border-radius: 4px;
    color: #c8d0d8;
    selection-background-color: #1e4080;
    font-family: "Consolas", "Courier New", monospace;
    font-size: 12px;
}

/* ── SpinBox ── */
QSpinBox, QDoubleSpinBox {
    background-color: #22262e;
    border: 1px solid #3a3f4a;
    border-radius: 4px;
    color: #c8d0d8;
    padding: 4px;
    padding-right: 16px;
}
/* 步进器一旦自定义了 border/padding，必须显式给出 up-button/down-button 的
   subcontrol 矩形，否则 Qt 实际命中区域和绘制位置会错位，导致靠近输入框
   一侧的按钮区域点击无响应。 */
QSpinBox::up-button, QDoubleSpinBox::up-button {
    subcontrol-origin: border;
    subcontrol-position: top right;
    width: 16px;
    border-left: 1px solid #3a3f4a;
    border-bottom: 1px solid #3a3f4a;
    border-top-right-radius: 4px;
    background-color: #2d3340;
}
QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover { background-color: #3a4255; }
QSpinBox::up-button:pressed, QDoubleSpinBox::up-button:pressed { background-color: #1e4080; }
QSpinBox::down-button, QDoubleSpinBox::down-button {
    subcontrol-origin: border;
    subcontrol-position: bottom right;
    width: 16px;
    border-left: 1px solid #3a3f4a;
    border-top: 1px solid #3a3f4a;
    border-bottom-right-radius: 4px;
    background-color: #2d3340;
}
QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover { background-color: #3a4255; }
QSpinBox::down-button:pressed, QDoubleSpinBox::down-button:pressed { background-color: #1e4080; }
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
    width: 0; height: 0;
    border-left: 3px solid transparent;
    border-right: 3px solid transparent;
    border-bottom: 4px solid #9ab0c8;
}
QSpinBox::up-arrow:hover, QDoubleSpinBox::up-arrow:hover { border-bottom-color: #e0e8f0; }
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
    width: 0; height: 0;
    border-left: 3px solid transparent;
    border-right: 3px solid transparent;
    border-top: 4px solid #9ab0c8;
}
QSpinBox::down-arrow:hover, QDoubleSpinBox::down-arrow:hover { border-top-color: #e0e8f0; }

/* ── CheckBox / RadioButton ── */
QCheckBox, QRadioButton { color: #c8d0d8; spacing: 6px; }
QCheckBox::indicator {
    width: 14px; height: 14px;
    border: 1px solid #3a3f4a;
    border-radius: 3px;
    background-color: #22262e;
}
QCheckBox::indicator:checked {
    background-color: #1e6fd4;
    border-color: #1e6fd4;
}
QRadioButton::indicator {
    width: 14px; height: 14px;
    border: 1px solid #3a3f4a;
    border-radius: 7px;
    background-color: #22262e;
}
QRadioButton::indicator:checked {
    background-color: #1e6fd4;
    border-color: #1e6fd4;
}

/* ── Tab (embedded tab widgets inside pages) ── */
QTabWidget::pane {
    border: 1px solid #2d3340;
    background-color: #1e2228;
}
QTabBar::tab {
    background-color: #22262e;
    color: #8898a8;
    border: 1px solid #2d3340;
    border-bottom: none;
    padding: 6px 14px;
    border-radius: 4px 4px 0 0;
}
QTabBar::tab:selected { background-color: #1e2228; color: #c8d0d8; }
QTabBar::tab:hover:!selected { background-color: #252a35; }

/* ── Menu ── */
QMenuBar {
    background-color: #12151a;
    color: #8898a8;
    border-bottom: 1px solid #2d3340;
    padding: 1px 0;
}
QMenuBar::item { padding: 5px 12px; border-radius: 3px; }
QMenuBar::item:selected { background-color: #1e4080; color: white; }
QMenu {
    background-color: #22262e;
    border: 1px solid #2d3340;
    color: #c8d0d8;
    padding: 4px 0;
}
QMenu::item { padding: 6px 24px 6px 16px; }
QMenu::item:selected { background-color: #1e4080; color: white; }
QMenu::separator { height: 1px; background-color: #2d3340; margin: 4px 0; }

/* ── StatusBar ── */
QStatusBar {
    background-color: #0d1117;
    color: #8898a8;
    border-top: 1px solid #2d3340;
    font-size: 12px;
}
QStatusBar::item { border: none; }
QStatusBar QLabel {
    color: #8898a8;
    padding: 2px 10px;
    border-right: 1px solid #2d3340;
    font-size: 12px;
    background: transparent;
}

/* ── Dialog ── */
QDialog {
    background-color: #1a1d23;
}
QDialogButtonBox QPushButton { min-width: 80px; }

/* ── Label ── */
QLabel { background-color: transparent; color: #c8d0d8; }
)";

// ─────────────────────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupGlobalStyle();
    setupHeaderBar();
    setupContentArea();
    initPages();
    setupStatusBar();
    connectServices();
}

void MainWindow::setupGlobalStyle()
{
    qApp->setStyleSheet(kGlobalStyle);
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
    // entry inside "地图管理" (see MapViewWidget's toolbar / onCreateMap()).
    struct SectionDef { QString text; int pageIndex; };
    const QList<SectionDef> sections = {
        {"  地图管理  ", 1},
        {"  任务管理  ", 3},
        {"  参数配置  ", 5},
        {"  设备信息  ", 0},
        {"  任务日志  ", 4},
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
    highlightTabForPageIndex(1);   // 与 initPages() 的 m_stack->setCurrentIndex(1) 保持一致

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
    m_agvInfoPage    = new AgvInfoWidget;
    m_createMapPage  = new CreateMapWidget;
    m_editConfigPage = new EditConfigWidget;
    m_mapPage        = new MapViewWidget;
    m_taskPage       = new TaskWidget;
    m_taskLogPage    = new TaskLogWidget;

    m_stack->addWidget(m_agvInfoPage);     // 0 设备信息
    m_stack->addWidget(m_mapPage);         // 1 地图列表
    m_stack->addWidget(m_createMapPage);   // 2 创建地图
    m_stack->addWidget(m_taskPage);        // 3 任务管理
    m_stack->addWidget(m_taskLogPage);     // 4 任务日志
    m_stack->addWidget(m_editConfigPage);  // 5 参数配置

    connect(m_taskPage,    &TaskWidget::navigateRequested,
            this, &MainWindow::onNavigateRequested);
    connect(m_taskLogPage, &TaskLogWidget::navigateRequested,
            this, &MainWindow::onNavigateRequested);
    connect(m_mapPage,     &MapViewWidget::navigateRequested,
            this, &MainWindow::onNavigateRequested);
    connect(m_createMapPage, &CreateMapWidget::goBack, this, [this]() {
        m_stack->setCurrentWidget(m_mapPage);
        highlightTabForPageIndex(1);
    });

    m_stack->setCurrentIndex(1);
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

    sb->addWidget(m_sbDevice);
    sb->addWidget(m_sbCarId);
    sb->addWidget(m_sbConn);

    QString ipText = dev.ip.isEmpty() ? "--" : dev.ip;
    sb->addPermanentWidget(new QLabel(QString("IP: %1").arg(ipText)));

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_statusTimer->start(3000);
    updateStatusBar();
}

void MainWindow::connectServices()
{
    QString ip = AppConfig::instance().serverIp();
    if (ip.isEmpty()) return;

    MqttService::instance().connectToHost(ip, 1883);

    QString wsUrl = QString("ws://%1:8048/websocket/message").arg(ip);
    WebSocketService::instance().connectToServer(wsUrl);
}

// ── Slots ──────────────────────────────────────────────────────────────────

void MainWindow::onSectionChanged(int index)
{
    m_stack->setCurrentIndex(index);
}

void MainWindow::switchToPage(const QString &pageKey)
{
    static const QMap<QString, int> pageMap = {
        {"agv_info",    0},
        {"map",         1},
        {"create_map",  2},
        {"task",        3},
        {"tasklog",     4},
        {"edit_config", 5},
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
        highlightTabForPageIndex(1);

    } else if (target == "create_map") {
        // Secondary page under 地图管理 — no tab to highlight, leave it as-is.
        m_stack->setCurrentWidget(m_createMapPage);

    } else if (target == "map_edit") {
        QString key = QString("map_edit_%1").arg(id);
        if (!m_dynamicPages.contains(key)) {
            MapEditWidget *w = new MapEditWidget(id);
            connect(w, &MapEditWidget::goBack, this, [this, key]() {
                m_stack->setCurrentWidget(m_mapPage);
                QWidget *p = m_dynamicPages.take(key);
                if (p) { m_stack->removeWidget(p); p->deleteLater(); }
            });
            m_dynamicPages[key] = w;
            m_stack->addWidget(w);
        }
        m_stack->setCurrentWidget(m_dynamicPages[key]);

    } else if (target == "map_image_edit") {
        QString key = QString("map_image_edit_%1").arg(id);
        if (!m_dynamicPages.contains(key)) {
            MapImageEditWidget *w = new MapImageEditWidget(id);
            connect(w, &MapImageEditWidget::goBack, this, [this, key]() {
                m_stack->setCurrentWidget(m_mapPage);
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
                m_taskPage->refreshList();
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

        // Refresh first page
        m_stack->setCurrentIndex(0);
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
    bool mqttOk = MqttService::instance().isConnected();
    if (mqttOk) {
        m_sbConn->setText("● MQTT: 已连接");
        m_sbConn->setStyleSheet("color:#5cb85c; background:transparent; border-right:1px solid #2d3340;");
    } else {
        m_sbConn->setText("○ MQTT: 断开");
        m_sbConn->setStyleSheet("color:#e05555; background:transparent; border-right:1px solid #2d3340;");
    }
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
    if (MqttService::instance().isConnected()) {
        Toast::info(this, "MQTT 已连接");
        return;
    }

    QString ip = AppConfig::instance().serverIp();
    if (ip.isEmpty()) {
        Toast::error(this, "未配置服务器地址，无法重连");
        return;
    }

    // 不再手动调用 disconnectFromHost() 再 connectToHost()。
    // 在 Connecting 状态下 disconnectFromHost 会触发 aboutToClose ->
    // transportConnectionClosed -> TransportInvalid 错误。
    // MqttService::connectToHost 内部会处理非 Disconnected 状态。
    connectServices();
    updateStatusBar();
    Toast::info(this, "正在重新连接 MQTT...");
}
