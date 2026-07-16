#include "TaskWidget.h"
#include "MapViewWidget.h"
#include "core/ApiService.h"
#include "core/AppConfig.h"
#include "core/MqttService.h"
#include "core/WebSocketService.h"
#include "core/LoadingDialog.h"
#include "core/Toast.h"
#include "core/MapPgmUtil.h"
#include <QFile>
#include <QTextStream>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>
#include <QJsonDocument>
#include <QGraphicsPixmapItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsTextItem>
#include <QPainterPath>
#include <QInputDialog>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QFrame>
#include <QSvgRenderer>
#include <QToolButton>
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <cmath>

// ── 地图上方导航栏图标 ────────────────────────────────────────────────────────
namespace {

QPixmap drawNavToolIcon(const QString &name, const QColor &col, int sz = 20)
{
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    double c = sz * 0.5;

    if (name == "load") {
        p.setPen(QPen(col, 1.4)); p.setBrush(Qt::NoBrush);
        QPolygonF folder;
        folder << QPointF(sz*0.10,sz*0.30) << QPointF(sz*0.38,sz*0.30) << QPointF(sz*0.46,sz*0.40)
               << QPointF(sz*0.90,sz*0.40) << QPointF(sz*0.90,sz*0.78) << QPointF(sz*0.10,sz*0.78);
        p.drawPolygon(folder);
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.40,sz*0.50) << QPointF(sz*0.64,sz*0.50) << QPointF(c,sz*0.70);
        p.drawPolygon(arrow);
    } else if (name == "play") {
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF tri; tri << QPointF(sz*0.28,sz*0.18) << QPointF(sz*0.28,sz*0.82) << QPointF(sz*0.84,c);
        p.drawPolygon(tri);
    } else if (name == "pause") {
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawRect(QRectF(sz*0.26, sz*0.18, sz*0.18, sz*0.64));
        p.drawRect(QRectF(sz*0.56, sz*0.18, sz*0.18, sz*0.64));
    } else if (name == "resume") {
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush);
        QPainterPath arc;
        arc.arcMoveTo(QRectF(sz*0.12, sz*0.12, sz*0.76, sz*0.76), 30);
        arc.arcTo(QRectF(sz*0.12, sz*0.12, sz*0.76, sz*0.76), 30, 280);
        p.drawPath(arc);
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.78,sz*0.06) << QPointF(sz*0.94,sz*0.20) << QPointF(sz*0.74,sz*0.28);
        p.drawPolygon(arrow);
    } else if (name == "stop") {
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawRect(QRectF(sz*0.24, sz*0.24, sz*0.52, sz*0.52));
    } else if (name == "remove") {
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawEllipse(QRectF(sz*0.10, sz*0.10, sz*0.80, sz*0.80));
        p.drawLine(QPointF(sz*0.33, c), QPointF(sz*0.67, c));
    } else if (name == "grid") {
        p.setPen(QPen(col, 1.4));
        double m1 = sz*0.18, m2 = sz*0.82, t1 = sz*0.40, t2 = sz*0.60;
        p.drawRect(QRectF(m1, m1, m2-m1, m2-m1));
        p.drawLine(QPointF(t1,m1), QPointF(t1,m2)); p.drawLine(QPointF(t2,m1), QPointF(t2,m2));
        p.drawLine(QPointF(m1,t1), QPointF(m2,t1)); p.drawLine(QPointF(m1,t2), QPointF(m2,t2));
    } else if (name == "reset") {
        // 重置视图: 圆弧 + 箭头
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush);
        QRectF r(sz*0.22, sz*0.22, sz*0.56, sz*0.56);
        p.drawArc(r, 30 * 16, 260 * 16);
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow;
        arrow << QPointF(sz*0.78, sz*0.30)
              << QPointF(sz*0.86, sz*0.46)
              << QPointF(sz*0.66, sz*0.42);
        p.drawPolygon(arrow);
    }

    p.end();
    return pm;
}

QIcon makeNavToolIcon(const QString &name, const QColor &col)
{
    return QIcon(drawNavToolIcon(name, col));
}

} // namespace

// 临时诊断日志：console 已禁用(WIN32 子系统)，直接写文件方便排查定位问题
static void debugLog(const QString &msg)
{
    QFile f("C:/Temp/agv_map_debug.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << "  [Task]  " << msg << "\n";
    }
}

TaskWidget::TaskWidget(QWidget *parent)
    : QWidget(parent)
{
    // Load SVG robot icon at 256×256, matching MapViewWidget's rendering
    QSvgRenderer renderer(QString(":/ic-robot1.svg"));
    if (renderer.isValid()) {
        m_robotPixmap = QPixmap(256, 256);
        m_robotPixmap.fill(Qt::transparent);
        QPainter rp(&m_robotPixmap);
        rp.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        renderer.render(&rp);
    }

    setupUI();
    getList();

    // 防抖定时器: 收到多次 navigation 消息时合并为一次刷新
    m_refreshDebounce = new QTimer(this);
    m_refreshDebounce->setSingleShot(true);
    m_refreshDebounce->setInterval(800);   // 800ms 内的多次刷新请求合并为一次
    connect(m_refreshDebounce, &QTimer::timeout, this, [this]() {
        refreshList();
        // 不调用 loadCurrentTask: MQTT navigation 消息已直接更新了 m_currentTask，
        // loadCurrentTask 可能因 m_currentCarId 为空而覆盖掉有效状态
    });

    // 任务状态轮询定时器: 任务执行中以1Hz轮询API，直到暂停或结束
    m_taskPollTimer = new QTimer(this);
    m_taskPollTimer->setInterval(1000);   // 1Hz
    connect(m_taskPollTimer, &QTimer::timeout, this, &TaskWidget::onTaskPollTimeout);

    connect(&MqttService::instance(), &MqttService::connected, this, [this]() {
        initMqtt();
    });
    connect(&MqttService::instance(), &MqttService::disconnected, this, [this]() {
        m_onlineCarIds.clear();
    });

    // 如果 MQTT 已连接，立即初始化
    if (MqttService::instance().isConnected()) {
        initMqtt();
    }

    // 监听 WebSocket 通知，实时更新任务执行状态(对应 Vue show.vue 的 navigation watch)
    connect(&WebSocketService::instance(), &WebSocketService::notificationReceived,
            this, &TaskWidget::onWebSocketNotification);
}

void TaskWidget::setupUI()
{
    // ── style constants (与 MapViewWidget 一致的暗色主题) ───────────────────────
    static const char *S_MAIN   = "background:#1a1d23;";
    static const char *S_LEFT   = "background:#1a1d23;";
    static const char *S_MAPVIEW= "background:#1a1d23;border:none;";
    static const char *S_NAVBAR =
        "background:#12151a;"
        "border-bottom:1px solid #2d3340;";
    static const char *S_BTN =
        "QPushButton{"
        "  background:#252a35; color:#c8d0d8; border:1px solid #3a3f4a;"
        "  border-radius:4px; padding:5px 14px; font-size:12px; font-weight:600;"
        "}"
        "QPushButton:hover{background:#2d3340; border-color:#4a9eff;}"
        "QPushButton:pressed{background:#1e4080;}"
        "QPushButton:disabled{color:#445566; border-color:#2d3340;}";
    static const char *S_BTN_PRIMARY =
        "QPushButton{"
        "  background:#1e6fd4; color:#fff; border:none;"
        "  border-radius:4px; padding:5px 14px; font-size:12px; font-weight:600;"
        "}"
        "QPushButton:hover{background:#2980e8;}"
        "QPushButton:pressed{background:#155db8;}"
        "QPushButton:disabled{color:#7a98c0; background:#16335a;}";
    static const char *S_BTN_START =
        "QPushButton{background:#252a35;color:#4caf7a;border:1px solid #1a5c34;"
        "border-radius:4px;padding:5px 14px;font-size:12px;font-weight:600;}"
        "QPushButton:hover{background:#163322;border-color:#27ae60;}"
        "QPushButton:disabled{color:#445566;border-color:#2d3340;}";
    static const char *S_BTN_PAUSE =
        "QPushButton{background:#252a35;color:#c8a430;border:1px solid #5c4a10;"
        "border-radius:4px;padding:5px 14px;font-size:12px;font-weight:600;}"
        "QPushButton:hover{background:#342a0c;border-color:#d4a010;}"
        "QPushButton:disabled{color:#445566;border-color:#2d3340;}";
    static const char *S_BTN_CONT =
        "QPushButton{background:#252a35;color:#3ab8cc;border:1px solid #0e4a5a;"
        "border-radius:4px;padding:5px 14px;font-size:12px;font-weight:600;}"
        "QPushButton:hover{background:#0e2c38;border-color:#1198b0;}"
        "QPushButton:disabled{color:#445566;border-color:#2d3340;}";
    static const char *S_BTN_STOP =
        "QPushButton{background:#252a35;color:#c85050;border:1px solid #5a1a1a;"
        "border-radius:4px;padding:5px 14px;font-size:12px;font-weight:600;}"
        "QPushButton:hover{background:#3a1414;border-color:#a02828;}"
        "QPushButton:disabled{color:#445566;border-color:#2d3340;}";
    // 地图上方导航栏：图标在上、文字在下 (Qt::ToolButtonTextUnderIcon)
    static const char *S_TOOLBTN =
        "QToolButton{background:#252a35;color:#c8d0d8;border:1px solid #3a3f4a;"
        "border-radius:4px;padding:3px 8px;font-size:11px;font-weight:600;}"
        "QToolButton:hover{background:#2d3340;border-color:#4a9eff;}"
        "QToolButton:pressed{background:#1e4080;}"
        "QToolButton:disabled{color:#445566;border-color:#2d3340;}"
        "QToolButton:checked{background:#1e4080;border-color:#4a9eff;}";
    static const char *S_TOOLBTN_PRIMARY =
        "QToolButton{background:#1e6fd4;color:#fff;border:none;"
        "border-radius:4px;padding:3px 8px;font-size:11px;font-weight:600;}"
        "QToolButton:hover{background:#2980e8;}"
        "QToolButton:pressed{background:#155db8;}"
        "QToolButton:disabled{color:#7a98c0;background:#16335a;}";
    static const char *S_TOOLBTN_START =
        "QToolButton{background:#252a35;color:#4caf7a;border:1px solid #1a5c34;"
        "border-radius:4px;padding:3px 8px;font-size:11px;font-weight:600;}"
        "QToolButton:hover{background:#163322;border-color:#27ae60;}"
        "QToolButton:disabled{color:#445566;border-color:#2d3340;}";
    static const char *S_TOOLBTN_PAUSE =
        "QToolButton{background:#252a35;color:#c8a430;border:1px solid #5c4a10;"
        "border-radius:4px;padding:3px 8px;font-size:11px;font-weight:600;}"
        "QToolButton:hover{background:#342a0c;border-color:#d4a010;}"
        "QToolButton:disabled{color:#445566;border-color:#2d3340;}";
    static const char *S_TOOLBTN_CONT =
        "QToolButton{background:#252a35;color:#3ab8cc;border:1px solid #0e4a5a;"
        "border-radius:4px;padding:3px 8px;font-size:11px;font-weight:600;}"
        "QToolButton:hover{background:#0e2c38;border-color:#1198b0;}"
        "QToolButton:disabled{color:#445566;border-color:#2d3340;}";
    static const char *S_TOOLBTN_STOP =
        "QToolButton{background:#252a35;color:#c85050;border:1px solid #5a1a1a;"
        "border-radius:4px;padding:3px 8px;font-size:11px;font-weight:600;}"
        "QToolButton:hover{background:#3a1414;border-color:#a02828;}"
        "QToolButton:disabled{color:#445566;border-color:#2d3340;}";
    static const char *S_RIGHT =
        "background:#1a1d23;border-left:1px solid #2d3340;";
    static const char *S_TOOLBAR =
        "background:#12151a;border-bottom:1px solid #2d3340;padding:4px 8px;";
    static const char *S_TABLE =
        "QTableWidget{background:#1e2228;alternate-background-color:#22262e;"
        "  gridline-color:#2d3340;border:1px solid #2d3340;border-radius:4px;"
        "  color:#c8d0d8;outline:none;font-size:12px;}"
        "QHeaderView::section{background:#22262e;color:#8898a8;border:none;"
        "  border-bottom:1px solid #2d3340;border-right:1px solid #2d3340;"
        "  padding:6px 10px;font-weight:600;font-size:11px;}"
        "QTableWidget::item{padding:5px 8px;border:none;}"
        "QTableWidget::item:selected{background:#1e4080;color:white;}"
        "QTableWidget::item:hover{background:#252a35;}"
        "QScrollBar:vertical{background:#1a1d23;width:8px;border:none;}"
        "QScrollBar::handle:vertical{background:#3a3f4a;border-radius:4px;min-height:20px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}";

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);
    mainLayout->setSpacing(0);
    setStyleSheet(S_MAIN);

    // ========== 左侧：地图 + 导航功能按钮 ==========
    QWidget *leftPanel = new QWidget;
    leftPanel->setStyleSheet(S_LEFT);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0,0,0,0);
    leftLayout->setSpacing(0);

    // 地图上方横向导航功能按钮
    QWidget *navWidget = new QWidget; navWidget->setStyleSheet(S_NAVBAR);
    QHBoxLayout *navBar = new QHBoxLayout(navWidget);
    navBar->setContentsMargins(8,6,8,6); navBar->setSpacing(6);

    auto mkNavBtn = [&](const QString &iconName, const QString &text, const QColor &iconCol) {
        auto *b = new QToolButton;
        b->setText(text);
        b->setIcon(makeNavToolIcon(iconName, iconCol));
        b->setIconSize(QSize(18, 18));
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setCursor(Qt::PointingHandCursor);
        return b;
    };

    m_loadTaskBtn     = mkNavBtn("load",   "加载任务", QColor("#bcd4f0"));
    m_startTaskBtn    = mkNavBtn("play",   "执行",     QColor("#4caf7a"));
    m_pauseTaskBtn    = mkNavBtn("pause",  "暂停",     QColor("#c8a430"));
    m_continueTaskBtn = mkNavBtn("resume", "继续",     QColor("#3ab8cc"));
    m_stopTaskBtn     = mkNavBtn("stop",   "结束",     QColor("#c85050"));
    m_removeTaskBtn   = mkNavBtn("remove", "移除",     QColor("#c8d0d8"));

    m_loadTaskBtn->setStyleSheet(S_TOOLBTN_PRIMARY);
    m_startTaskBtn->setStyleSheet(S_TOOLBTN_START);
    m_pauseTaskBtn->setStyleSheet(S_TOOLBTN_PAUSE);
    m_continueTaskBtn->setStyleSheet(S_TOOLBTN_CONT);
    m_stopTaskBtn->setStyleSheet(S_TOOLBTN_STOP);
    m_removeTaskBtn->setStyleSheet(S_TOOLBTN);

    m_loadTaskBtn->setEnabled(false);
    m_startTaskBtn->setEnabled(false);
    m_pauseTaskBtn->setEnabled(false);
    m_continueTaskBtn->setEnabled(false);
    m_stopTaskBtn->setEnabled(false);
    m_removeTaskBtn->setEnabled(false);

    connect(m_loadTaskBtn,     &QToolButton::clicked, this, &TaskWidget::onLoadTask);
    connect(m_startTaskBtn,    &QToolButton::clicked, this, &TaskWidget::onStartTask);
    connect(m_pauseTaskBtn,    &QToolButton::clicked, this, &TaskWidget::onPauseTask);
    connect(m_continueTaskBtn, &QToolButton::clicked, this, &TaskWidget::onContinueTask);
    connect(m_stopTaskBtn,     &QToolButton::clicked, this, &TaskWidget::onStopTask);
    connect(m_removeTaskBtn,   &QToolButton::clicked, this, &TaskWidget::onRemoveTask);

    navBar->addWidget(m_loadTaskBtn);
    navBar->addWidget(m_startTaskBtn);
    navBar->addWidget(m_pauseTaskBtn);
    navBar->addWidget(m_continueTaskBtn);
    navBar->addWidget(m_stopTaskBtn);
    navBar->addWidget(m_removeTaskBtn);

    auto *sepGrid = new QFrame; sepGrid->setFrameShape(QFrame::VLine);
    sepGrid->setStyleSheet("color:#2d3340;"); navBar->addWidget(sepGrid);

    m_gridBtn = mkNavBtn("grid", "网格", QColor("#c8d0d8"));
    m_gridBtn->setCheckable(true);
    m_gridBtn->setStyleSheet(S_TOOLBTN);
    connect(m_gridBtn, &QToolButton::toggled, this, [this](bool on) {
        m_showGrid = on;
        renderAxesAndGrid();
    });
    navBar->addWidget(m_gridBtn);

    // 反光柱开关
    auto *sepReflector = new QFrame; sepReflector->setFrameShape(QFrame::VLine);
    sepReflector->setStyleSheet("color:#2d3340;"); navBar->addWidget(sepReflector);
    auto *reflectorLabel = new QLabel("反光柱");
    reflectorLabel->setStyleSheet("color:#9ab8d8;font-size:11px;padding:0 2px;");
    navBar->addWidget(reflectorLabel);
    m_reflectorBtn = new ToggleSwitch;
    m_reflectorBtn->setToolTip("切换反光柱显示");
    connect(m_reflectorBtn, &ToggleSwitch::toggled, this, [this](bool on) {
        m_showReflectors = on;
        renderMap();
    });
    navBar->addWidget(m_reflectorBtn);

    m_resetViewBtn = mkNavBtn("reset", "重置视图", QColor("#c8d0d8"));
    m_resetViewBtn->setStyleSheet(S_TOOLBTN);
    connect(m_resetViewBtn, &QToolButton::clicked, this, [this]() {
        if (m_mapView) {
            m_mapView->resetTransform();
            m_mapView->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatioByExpanding);
        }
    });
    navBar->addWidget(m_resetViewBtn);

    navBar->addStretch();

    m_currentTaskLabel = new QLabel("无任务");
    m_currentTaskLabel->setStyleSheet("color:#8898a8;font-size:11px;");
    navBar->addWidget(m_currentTaskLabel);

    leftLayout->addWidget(navWidget);

    // 地图信息标签 (与 MapViewWidget 的状态栏一致)
    m_mapInfoLabel = new QLabel("请选择任务以加载地图");
    m_mapInfoLabel->setFixedHeight(24);
    m_mapInfoLabel->setStyleSheet(
        "background:#0d1117; color:#8898a8; font-size:11px; padding:0 10px;"
        "border-bottom:1px solid #2d3340;");
    leftLayout->addWidget(m_mapInfoLabel);

    // 地图视图：corner + 顶部标尺 + [左侧标尺 + 画布]，与 MapViewWidget 一致
    QWidget *mapArea = new QWidget;
    QGridLayout *mapAreaLayout = new QGridLayout(mapArea);
    mapAreaLayout->setContentsMargins(0,0,0,0);
    mapAreaLayout->setSpacing(0);

    QLabel *corner = new QLabel;
    corner->setFixedSize(44, 22);
    corner->setStyleSheet("background:#1a1d23;");
    mapAreaLayout->addWidget(corner, 0, 0);

    m_topRuler = new RulerWidget(RulerWidget::Horizontal);
    mapAreaLayout->addWidget(m_topRuler, 0, 1);

    m_leftRuler = new RulerWidget(RulerWidget::Vertical);
    mapAreaLayout->addWidget(m_leftRuler, 1, 0);

    m_scene = new QGraphicsScene(this);
    m_mapView = new MapCanvas(m_scene, this);
    connect(m_mapView, &MapCanvas::transformChanged, this, [this]() { updateRulers(); });
    m_mapView->setStyleSheet(S_MAPVIEW);
    m_mapView->setDragMode(QGraphicsView::ScrollHandDrag);
    m_mapView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_mapView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_mapView->setMinimumWidth(400);
    m_mapView->setFrameShape(QFrame::NoFrame);
    m_mapView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_mapView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_mapView->viewport()->setMouseTracking(true);
    m_mapView->viewport()->installEventFilter(this);
    mapAreaLayout->addWidget(m_mapView, 1, 1);
    mapAreaLayout->setRowStretch(1, 1);
    mapAreaLayout->setColumnStretch(1, 1);

    leftLayout->addWidget(mapArea, 1);

    m_coordLabel = new QLabel("X: --  Y: --");
    m_coordLabel->setStyleSheet(
        "background:rgba(18,21,26,200); color:#7ab0f0; font-size:11px;"
        "padding:3px 8px; border-radius:3px; font-family:Consolas,monospace;");
    m_coordLabel->setMinimumWidth(240);
    m_coordLabel->setParent(m_mapView);
    m_coordLabel->move(8, 8);
    m_coordLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_coordLabel->adjustSize();
    m_coordLabel->raise();

    mainLayout->addWidget(leftPanel, 7);

    // ========== 右侧：任务列表 ==========
    // 完全重构：紧凑布局，确保所有内容在任何窗口尺寸下完整显示
    QWidget *rightPanel = new QWidget;
    rightPanel->setStyleSheet(S_RIGHT);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0,0,0,0);
    rightLayout->setSpacing(0);

    // 右侧标题栏
    QWidget *hdrWidget = new QWidget;
    hdrWidget->setStyleSheet("background:#1e2d3d;border-bottom:1px solid rgba(255,255,255,0.08);");
    QHBoxLayout *hdrLay = new QHBoxLayout(hdrWidget);
    hdrLay->setContentsMargins(10,6,6,6);
    QLabel *hdrLbl = new QLabel("任务列表");
    hdrLbl->setStyleSheet("color:#6a9cbf;font-size:12px;font-weight:600;letter-spacing:1px;");
    hdrLay->addWidget(hdrLbl);
    hdrLay->addStretch();
    rightLayout->addWidget(hdrWidget);

    // 任务列表工具栏：紧凑按钮，等宽分配
    QWidget *tbWidget = new QWidget; tbWidget->setStyleSheet(S_TOOLBAR);
    QHBoxLayout *toolbar = new QHBoxLayout(tbWidget);
    toolbar->setContentsMargins(6,4,6,4); toolbar->setSpacing(4);

    // 紧凑按钮样式
    static const char *S_BTN_COMPACT =
        "QPushButton{background:#252a35; color:#c8d0d8; border:1px solid #3a3f4a;"
        "border-radius:3px; padding:4px 8px; font-size:12px;}"
        "QPushButton:hover{background:#2d3340; border-color:#4a9eff;}"
        "QPushButton:disabled{color:#445566; border-color:#2d3340;}";
    static const char *S_BTN_PRIMARY_COMPACT =
        "QPushButton{background:#1e6fd4; color:#fff; border:none;"
        "border-radius:3px; padding:4px 8px; font-size:12px;}"
        "QPushButton:hover{background:#2980e8;}"
        "QPushButton:disabled{color:#7a98c0; background:#16335a;}";
    static const char *S_BTN_STOP_COMPACT =
        "QPushButton{background:#252a35; color:#c85050; border:1px solid #5a1a1a;"
        "border-radius:3px; padding:4px 8px; font-size:12px;}"
        "QPushButton:hover{background:#3a1414; border-color:#a02828;}"
        "QPushButton:disabled{color:#445566; border-color:#2d3340;}";

    m_addBtn     = new QPushButton("新增");
    m_editBtn    = new QPushButton("修改");
    m_deleteBtn  = new QPushButton("删除");
    m_refreshBtn = new QPushButton("刷新");

    m_editBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);

    m_addBtn->setStyleSheet(S_BTN_PRIMARY_COMPACT);
    m_editBtn->setStyleSheet(S_BTN_COMPACT);
    m_deleteBtn->setStyleSheet(S_BTN_STOP_COMPACT);
    m_refreshBtn->setStyleSheet(S_BTN_COMPACT);

    // 设置最小宽度防止文字被截断
    for (auto *btn : {m_addBtn, m_editBtn, m_deleteBtn, m_refreshBtn}) {
        btn->setMinimumWidth(48);
        btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    connect(m_addBtn,     &QPushButton::clicked, this, &TaskWidget::onAdd);
    connect(m_editBtn,    &QPushButton::clicked, this, &TaskWidget::onEdit);
    connect(m_deleteBtn,  &QPushButton::clicked, this, &TaskWidget::onDelete);
    connect(m_refreshBtn, &QPushButton::clicked, this, &TaskWidget::onRefresh);

    toolbar->addWidget(m_addBtn, 1);
    toolbar->addWidget(m_editBtn, 1);
    toolbar->addWidget(m_deleteBtn, 1);
    toolbar->addWidget(m_refreshBtn, 1);
    rightLayout->addWidget(tbWidget);

    // 任务表格：自适应列宽
    m_table = new QTableWidget;
    m_table->setStyleSheet(S_TABLE);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({"任务编号","执行设备","任务名称","状态","描述","创建时间"});
    // 按列设置不同的缩放模式：编号/设备/状态用 ResizeToContents，名称/描述/时间用 Stretch
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setMinimumSectionSize(40);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(false);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &TaskWidget::onTableSelectionChanged);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &TaskWidget::onTaskSelectionChanged);
    rightLayout->addWidget(m_table);

    mainLayout->addWidget(rightPanel, 3);
}

void TaskWidget::refreshList()
{
    getList();
    // 重新初始化 MQTT 订阅，确保设备切换后能收到新设备的位置更新
    initMqtt();
    // 不调用 loadCurrentTask: MQTT navigation 已直接更新 m_currentTask，
    // loadCurrentTask 会覆盖用户手动选中的行
}

void TaskWidget::getList()
{
    QJsonObject query;
    query["pageNum"] = m_pageNum;
    query["pageSize"] = m_pageSize;
    query["carId"] = AppConfig::instance().agvId();

    ApiService::instance().listTask(query, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        m_taskList = data.value("rows").toArray();
        m_total = data.value("total").toInt();

        // 重建表格时阻塞信号，避免中间状态触发 onTaskSelectionChanged
        bool wasBlocking = m_table->blockSignals(true);

        m_table->setRowCount(m_taskList.size());
        for (int i = 0; i < m_taskList.size(); ++i) {
            QJsonObject task = m_taskList[i].toObject();

            m_table->setItem(i, 0, new QTableWidgetItem(jsonIdStr(task.value("taskNumber"))));
            m_table->setItem(i, 1, new QTableWidgetItem(task.value("carName").toString()));
            m_table->setItem(i, 2, new QTableWidgetItem(task.value("taskName").toString()));

            // 这里的 status 是任务实体本身的有效性状态(字典 sys_task_status：
            // 0正常 1路网失效 2地图失效)，跟 m_currentTask/loadCurrentTask() 里
            // 设备实时执行状态(0执行中/1已完成/2失败/3暂停/4未开始)是完全不同的
            // 两个字段，之前误用了执行状态的文案，与任务列表页要展示的内容不符。
            QString status = jsonIdStr(task.value("status"));
            QString statusText;
            QColor  statusColor;
            if      (status == "0") { statusText = "正常";   statusColor = QColor("#4caf7a"); }
            else if (status == "1") { statusText = "路网失效"; statusColor = QColor("#c0392b"); }
            else if (status == "2") { statusText = "地图失效"; statusColor = QColor("#c0392b"); }
            else                    { statusText = status;   statusColor = QColor("#a8c0d8"); }
            auto *statusItem = new QTableWidgetItem(statusText);
            statusItem->setForeground(statusColor);
            m_table->setItem(i, 3, statusItem);

            m_table->setItem(i, 4, new QTableWidgetItem(task.value("remark").toString()));
            m_table->setItem(i, 5, new QTableWidgetItem(task.value("createTime").toString()));
        }

        applyTaskValidityStyle();

        // 恢复之前选中的任务(若仍存在)，否则选中第一个
        int restoreRow = -1;
        if (!m_lastSelectedTaskId.isEmpty()) {
            for (int i = 0; i < m_taskList.size(); ++i) {
                QJsonObject t = m_taskList[i].toObject();
                if (jsonIdStr(t.value("taskId")) == m_lastSelectedTaskId) {
                    restoreRow = i;
                    break;
                }
            }
        }
        if (restoreRow < 0 && m_taskList.size() > 0) restoreRow = 0;
        if (restoreRow >= 0) {
            m_table->blockSignals(false);   // 恢复信号，让 selectRow 触发 selectionChanged
            m_suppressTaskLoad = true;     // 恢复选中时不要触发 loadCurrentTask
            m_table->selectRow(restoreRow);
            onTaskSelectionChanged();       // 手动触发一次，确保地图/任务点更新
            m_suppressTaskLoad = false;
        } else {
            m_table->blockSignals(false);   // 恢复信号
            // 没有任务时也应该看到设备当前所在地图与实时位置，而不是空白页面
            loadCurrentCarMap();
        }
    });

    loadTaskValidity();
}

// 没有任务被选中时，直接按配置的设备 ID 加载它当前所在的地图，
// 这样设备图标仍然可以正常显示，而不必依赖于"已选中一个任务"。
void TaskWidget::loadCurrentCarMap()
{
    QString agvId = AppConfig::instance().agvId();
    if (agvId.isEmpty()) return;
    m_currentCarId = agvId;
    ApiService::instance().getCar(agvId, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonObject carData = data.value("data").toObject(data);
        QString carMapId = jsonIdStr(carData.value("mapId"));
        if (!carMapId.isEmpty()) loadMapForTask(carMapId);
    });
}

void TaskWidget::loadTaskValidity()
{
    QString agvId = AppConfig::instance().agvId();
    if (agvId.isEmpty()) return;
    // 与 Vue show.vue 的 onloadTask() 一致: 服务器把任务分为 rows_normal/rows_invalid，
    // 已失效的任务无法加载，需要在 UI 上标注出来，避免用户点击"加载任务"必然失败。
    ApiService::instance().listTaskAllStatus(agvId, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        m_invalidTaskIds.clear();
        for (const auto &v : data.value("rows_invalid").toArray())
            m_invalidTaskIds.insert(jsonIdStr(v.toObject().value("taskId")));
        applyTaskValidityStyle();
        onTaskSelectionChanged();
        // 失效任务列表更新后，检查当前设备加载的任务是否已失效
        checkAndRemoveInvalidLoaded();
    });
}

void TaskWidget::applyTaskValidityStyle()
{
    for (int i = 0; i < m_taskList.size(); ++i) {
        QJsonObject task = m_taskList[i].toObject();
        QString taskId = jsonIdStr(task.value("taskId"));
        QString taskName = task.value("taskName").toString();
        bool invalid = m_invalidTaskIds.contains(taskId);

        auto *item = m_table->item(i, 2);
        if (!item) continue;
        item->setText(invalid ? taskName + " (待切换地图)" : taskName);
        item->setForeground(invalid ? QColor("#c8a430") : QColor("#c8d0d8"));
        item->setToolTip(invalid ? "该任务地图与设备当前地图不一致，加载时将自动切换地图" : QString());
    }
}

void TaskWidget::selectFirstTask()
{
    if (m_taskList.size() > 0) {
        m_table->selectRow(0);
    }
}

void TaskWidget::onTaskSelectionChanged()
{
    int row = m_table->currentRow();
    if (row < 0 || row >= m_taskList.size()) {
        m_currentTaskId.clear();
        m_currentCarId.clear();
        m_loadTaskBtn->setEnabled(false);
        return;
    }

    QJsonObject task = m_taskList[row].toObject();
    m_currentTaskId = jsonIdStr(task.value("taskId"));
    m_currentCarId = jsonIdStr(task.value("carId"));
    m_lastSelectedTaskId = m_currentTaskId;   // 记录当前选中，刷新后恢复

    // 服务器标记为"已失效"的任务通常是因为设备当前地图与任务地图不一致，
    // "加载任务"会先自动切换地图再加载，因此这里不禁用按钮，仅提示原因。
    bool invalid = m_invalidTaskIds.contains(m_currentTaskId);
    m_loadTaskBtn->setEnabled(true);
    m_loadTaskBtn->setToolTip(invalid ? "该任务地图与设备当前地图不一致，加载时将自动切换地图" : QString());

    // 加载任务点 (taskInfo.features)
    loadTaskTargets(m_currentTaskId);

    // 加载任务关联的地图
    QString mapId = jsonIdStr(task.value("mapId"));
    if (mapId.isEmpty()) {
        // 如果任务没有 mapId，尝试从设备信息获取
        if (!m_currentCarId.isEmpty()) {
            ApiService::instance().getCar(m_currentCarId, [this](int code, const QJsonObject &data) {
                if (code != 200) return;
                QJsonObject carData = data.value("data").toObject(data);
                QString carMapId = jsonIdStr(carData.value("mapId"));
                if (!carMapId.isEmpty()) {
                    loadMapForTask(carMapId);
                }
            });
        }
    } else {
        loadMapForTask(mapId);
    }

    // 加载当前任务状态
    // 仅在用户手动选择时调用，刷新列表恢复选中时不调用(避免覆盖 WebSocket 设置的状态)
    if (!m_suppressTaskLoad) {
        loadCurrentTask();
    }
}

void TaskWidget::loadMapForTask(const QString &mapId)
{
    if (mapId.isEmpty() || mapId == m_currentMapId) return;

    m_currentMapId = mapId;
    m_mapImage = QImage();
    m_points = QJsonArray();
    m_lines = QJsonArray();
    m_curves = QJsonArray();
    m_reflectorPoles = QJsonArray();
    m_hasInitialFit = false;

    ApiService::instance().getMap(mapId, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonObject mapInfo = data.value("data").toObject(data);
        m_currentMapName = mapInfo.value("mapName").toString();
        m_mapInfoLabel->setText(QString("地图: %1 [%2]").arg(m_currentMapName, m_currentMapId));

        // 提取坐标转换参数（仅作为 pgm/yaml 拉取失败时的兜底）
        auto tryDbl = [&](const QStringList &keys, double def) -> double {
            for (const QString &k : keys) {
                QJsonValue v = mapInfo.value(k);
                if (!v.isUndefined() && !v.isNull()) return v.toDouble(def);
            }
            return def;
        };
        double apiPhysW = tryDbl({"mapWidth","map_width","width"}, 0.0);
        double apiPhysH = tryDbl({"mapHeight","map_height","height"}, 0.0);

        // 与 Vue edit.vue 完全一致：永远用 -offsetX/-offsetY 计算零点，不优先采用
        // originX 字段（Vue 的 getMap 流程从不读取该字段）。
        auto getOrigin = [&](const QStringList &negate) -> double {
            for (const QString &k : negate) {
                QJsonValue v = mapInfo.value(k);
                if (!v.isUndefined() && !v.isNull()) return -v.toDouble();
            }
            return 0.0;
        };
        double apiOriginX = getOrigin({"offsetX","mapOffsetX","offset_x"});
        double apiOriginY = getOrigin({"offsetY","mapOffsetY","offset_y"});

        QString mapHost     = AppConfig::instance().mapHost();
        QString tilemapPath = mapInfo.value("tilemapPath").toString();
        QString thumbUrl;
        if (!tilemapPath.isEmpty())
            thumbUrl = QString("%1%2_%3/%4/thumbnail.png").arg(mapHost, m_currentMapName, m_currentMapId, tilemapPath);

        // 兜底方案：thumbnail.png + getMap() 返回的 mapWidth/mapHeight/offsetX/offsetY。
        // 这些字段是后端为生成瓦片金字塔对图片 padding 后派生的，并不是地图的真实物理
        // 尺寸与零点，只在 pgm/yaml 拉取失败时使用。
        auto useThumbnailFallback = [this, thumbUrl, apiOriginX, apiOriginY, apiPhysW, apiPhysH]() {
            debugLog(QString("[loadMapForTask] FALLBACK to thumbnail.png url=%1").arg(thumbUrl));
            if (thumbUrl.isEmpty()) return;
            m_worldL = apiOriginX;
            m_worldB = apiOriginY;
            m_worldR = apiOriginX + apiPhysW;
            m_worldT = apiOriginY + apiPhysH;
            ApiService::instance().downloadFile(thumbUrl, [this](bool ok, const QByteArray &imgData) {
                if (!ok) return;
                m_mapImage.loadFromData(imgData);
                m_imgW = m_mapImage.width();
                m_imgH = m_mapImage.height();
                renderMap();
                QTimer::singleShot(0, this, [this]{ fitView(); });
            });
        };

        // 主路径：直接拉取并解析原始 carto_map.pgm + carto_map.yaml（与 MapEditWidget/
        // MapViewWidget 一致）。yaml 的 origin 本身就是 pgm 左下角像素对应的世界坐标，
        // 不需要取负，也不依赖 padding 后的 mapWidth/mapHeight/offsetX/offsetY。
        QString resBase = AppConfig::instance().agvMapResourceHost() + m_currentMapName + "_" + m_currentMapId + "/";
        QString yamlUrl = resBase + "carto_map.yaml";
        QString pgmUrl  = resBase + "carto_map.pgm";

        ApiService::instance().downloadFile(yamlUrl,
            [this, pgmUrl, useThumbnailFallback](bool yamlOk, const QByteArray &yamlRaw) {
                if (!yamlOk) { useThumbnailFallback(); return; }
                double resolution = 0, originX = 0, originY = 0; bool negate = false;
                if (!MapPgmUtil::parseMapYaml(QString::fromUtf8(yamlRaw), resolution, originX, originY, negate)
                    || resolution <= 0) {
                    useThumbnailFallback(); return;
                }
                ApiService::instance().downloadFile(pgmUrl,
                    [this, resolution, originX, originY, negate, useThumbnailFallback]
                    (bool pgmOk, const QByteArray &pgmRaw) {
                        if (!pgmOk) { useThumbnailFallback(); return; }
                        QImage img = MapPgmUtil::pgmToImage(pgmRaw, negate);
                        if (img.isNull()) { useThumbnailFallback(); return; }

                        m_mapImage = img;
                        m_imgW = img.width();
                        m_imgH = img.height();
                        m_worldL = originX;
                        m_worldB = originY;
                        m_worldR = m_worldL + img.width()  * resolution;
                        m_worldT = m_worldB + img.height() * resolution;
                        debugLog(QString("[loadMapForTask] FINAL(pgm) imgW=%1 imgH=%2 worldL=%3 worldB=%4 worldR=%5 worldT=%6")
                            .arg(m_imgW).arg(m_imgH).arg(m_worldL).arg(m_worldB).arg(m_worldR).arg(m_worldT));
                        renderMap();
                        QTimer::singleShot(0, this, [this]{ fitView(); });
                    });
            });

        // 加载路网数据
        loadRouteData(m_currentMapId);
        loadReflectorData(m_currentMapId);
    });
}

void TaskWidget::loadRouteData(const QString &mapId)
{
    QJsonObject query;
    query["mapId"] = mapId;
    ApiService::instance().loadEditedMap(query, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        m_points = QJsonArray();
        m_lines  = QJsonArray();
        m_curves = QJsonArray();
        m_charges = QJsonArray();
        m_highPrecisionPoints = QJsonArray();

        QString jsonStr = data.value("json").toString();
        if (jsonStr.isEmpty()) {
            renderMap();
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (!doc.isObject()) return;
        QJsonObject root = doc.object();

        // 兼容旧版（修复前）保存的扁平格式: {points:[...], lines:[...], curves:[...]}
        if (!root.contains("features") && (root.contains("points") || root.contains("lines") || root.contains("curves"))) {
            m_points = root.value("points").toArray();
            m_lines  = root.value("lines").toArray();
            m_curves = root.value("curves").toArray();
            renderMap();
            return;
        }

        QJsonArray features = root.value("features").toArray();

        for (const auto &fv : features) {
            QJsonObject f      = fv.toObject();
            QJsonObject props  = f.value("properties").toObject();
            QJsonObject geom   = f.value("geometry").toObject();
            QString     gtype  = geom.value("type").toString();
            QJsonArray  coords = geom.value("coordinates").toArray();
            QString     id     = props.value("id").toString();

            if (gtype == "Point") {
                QJsonObject pt;
                pt["id"]   = id;
                pt["x"]    = coords[0].toDouble();
                pt["y"]    = coords[1].toDouble();
                pt["name"] = props.value("name").toString();
                if (id.startsWith("c_"))
                    m_charges.append(pt);
                else if (id.startsWith("hp_")) {
                    pt["visible"] = props.value("visible").toBool(true);
                    m_highPrecisionPoints.append(pt);
                } else
                    m_points.append(pt);
            } else if (gtype == "LineString") {
                if (coords.size() < 2) continue;
                QJsonObject ln;
                ln["id"]   = id;
                ln["x1"]   = coords.first().toArray()[0].toDouble();
                ln["y1"]   = coords.first().toArray()[1].toDouble();
                ln["x2"]   = coords.last().toArray()[0].toDouble();
                ln["y2"]   = coords.last().toArray()[1].toDouble();
                ln["name"] = props.value("name").toString();
                m_lines.append(ln);
            } else if (gtype == "MultiPoint") {
                if (coords.size() < 2) continue;
                QJsonObject cv;
                cv["id"]   = id;
                cv["name"] = props.value("name").toString();
                cv["x1"] = coords.first().toArray()[0].toDouble();
                cv["y1"] = coords.first().toArray()[1].toDouble();
                cv["x2"] = coords.last().toArray()[0].toDouble();
                cv["y2"] = coords.last().toArray()[1].toDouble();
                QJsonArray ctrlArr;
                for (const auto &c : coords) {
                    QJsonArray ca = c.toArray();
                    if (ca.size() < 2) continue;
                    QJsonObject pt;
                    pt["x"] = ca[0].toDouble();
                    pt["y"] = ca[1].toDouble();
                    ctrlArr.append(pt);
                }
                cv["controlPoints"] = ctrlArr;
                if (coords.size() == 3) {
                    cv["cx"] = coords[1].toArray()[0].toDouble();
                    cv["cy"] = coords[1].toArray()[1].toDouble();
                }
                m_curves.append(cv);
            }
        }
        renderMap();
    });
}

void TaskWidget::loadReflectorData(const QString &mapId)
{
    if (mapId.isEmpty()) return;
    ApiService::instance().getReflectorColumnInfo(mapId, [this](int code, const QJsonObject &data) {
        if (code != 200) {
            m_mapInfoLabel->setText(QString("地图: %1 [%2] (反光柱加载失败)").arg(m_currentMapName, m_currentMapId));
            return;
        }
        
        QJsonValue dataVal = data.value("data");
        QJsonArray features;
        
        if (dataVal.isArray()) {
            features = dataVal.toArray();
        } else if (dataVal.isObject()) {
            QJsonObject geo = dataVal.toObject();
            features = geo.value("features").toArray();
            if (features.isEmpty() && geo.value("type").toString() == "Feature") {
                features.append(geo);
            }
        } else if (dataVal.isString()) {
            QJsonDocument doc = QJsonDocument::fromJson(dataVal.toString().toUtf8());
            if (doc.isArray()) {
                features = doc.array();
            } else if (doc.isObject()) {
                QJsonObject geo = doc.object();
                features = geo.value("features").toArray();
                if (features.isEmpty() && geo.value("type").toString() == "Feature") {
                    features.append(geo);
                }
            }
        }
        
        if (features.isEmpty()) {
            features = data.value("features").toArray();
        }
        
        m_reflectorPoles = features;
        m_mapInfoLabel->setText(QString("地图: %1 [%2] (反光柱: %3)").arg(m_currentMapName, m_currentMapId).arg(m_reflectorPoles.size()));
        if (m_showReflectors) renderMap();
    });
}

void TaskWidget::loadTaskTargets(const QString &taskId)
{
    if (taskId.isEmpty()) {
        m_targets = QJsonArray();
        renderMap();
        return;
    }
    // task_json = {"taskInfo": {type:FeatureCollection, features:[...point features...], ...}}
    ApiService::instance().getTaskInfo(taskId, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonObject info = data.value("data").toObject(data);
        QString taskJson = info.value("task_json").toString();
        m_targets = QJsonArray();
        if (!taskJson.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(taskJson.toUtf8());
            QJsonObject form = doc.object();
            QJsonObject taskInfo = form.value("taskInfo").toObject();
            for (const auto &fv : taskInfo.value("features").toArray()) {
                QJsonObject f      = fv.toObject();
                QJsonObject props  = f.value("properties").toObject();
                QJsonArray  coords = f.value("geometry").toObject().value("coordinates").toArray();
                QJsonObject t = props;
                if (coords.size() >= 2) {
                    t["x"] = coords[0].toDouble();
                    t["y"] = coords[1].toDouble();
                }
                m_targets.append(t);
            }
        }
        renderMap();
    });
}

QPointF TaskWidget::worldToScene(double wx, double wy) const
{
    double mapW = m_worldR - m_worldL, mapH = m_worldT - m_worldB;
    if (m_imgW <= 0 || m_imgH <= 0 || mapW <= 0 || mapH <= 0) return {wx, wy};
    return { (wx - m_worldL) / mapW * m_imgW,
             (m_worldT - wy) / mapH * m_imgH };
}

QPointF TaskWidget::sceneToWorld(double sx, double sy) const
{
    double mapW = m_worldR - m_worldL, mapH = m_worldT - m_worldB;
    if (m_imgW <= 0 || m_imgH <= 0 || mapW <= 0 || mapH <= 0) return {sx, sy};
    return { sx / m_imgW * mapW + m_worldL,
             m_worldT - sy / m_imgH * mapH };
}

void TaskWidget::fitView()
{
    if (m_scene->sceneRect().isEmpty()) return;
    // 最短边铺满、长边居中裁切 ("cover" 而非 "contain")，与地图管理预览一致
    m_mapView->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatioByExpanding);
    m_hasInitialFit = true;
    updateRulers();
}

void TaskWidget::updateCoordinateDisplay(const QPointF &scenePos)
{
    QPointF w = sceneToWorld(scenePos.x(), scenePos.y());
    m_coordLabel->setText(QString("X: %1  Y: %2")
        .arg(w.x(), 0, 'f', 2).arg(w.y(), 0, 'f', 2));
}

bool TaskWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != m_mapView->viewport())
        return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::Wheel) {
        return false;
    }

    if (event->type() == QEvent::MouseMove) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        updateCoordinateDisplay(m_mapView->mapToScene(me->pos()));
    }

    return QWidget::eventFilter(obj, event);
}

void TaskWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (!m_hasInitialFit)
        QTimer::singleShot(0, this, [this]{ fitView(); });
}

void TaskWidget::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    // 该页面在 QStackedWidget 中处于隐藏状态时就已经异步加载了地图/任务数据，
    // 此时视图尚未获得真实尺寸，fitInView 会按错误的视口大小计算，导致地图
    // 没有居中铺满。每次页面真正显示出来时都重新 fit 一次，确保按正确尺寸缩放。
    QTimer::singleShot(0, this, [this]{ fitView(); });

    // 路网可能在"地图编辑"页被修改后才切回本页，本页是常驻的 QStackedWidget
    // 页面、不会重新构造，因此每次显示时都要重新拉取一次路网数据，否则地图
    // 预览里的路网会停留在第一次加载时的旧版本，不会动态更新。
    if (!m_currentMapId.isEmpty()) loadRouteData(m_currentMapId);
}

void TaskWidget::renderAxesAndGrid()
{
    if (m_imgW <= 0 || m_imgH <= 0) return;

    QList<QGraphicsItem *> toRemove;
    for (auto *item : m_scene->items())
        if (item->data(0).toInt() == 1) toRemove.append(item);
    for (auto *item : toRemove) { m_scene->removeItem(item); delete item; }

    auto tag = [](QGraphicsItem *item, int z) {
        item->setData(0, 1); item->setZValue(z); return item;
    };

    double wLeft = m_worldL, wRight = m_worldR, wBottom = m_worldB, wTop = m_worldT;

    if (!m_showGrid) return;

    const double gridStep = 1.0;
    QPen gridPen (QColor(80, 110, 160,  80), 0.5);
    QPen axisGPen(QColor(120, 160, 240, 130), 0.8);

    for (double wx = std::ceil(wLeft / gridStep) * gridStep; wx <= wRight + 1e-9; wx += gridStep) {
        QPointF a = worldToScene(wx, wBottom);
        QPointF b = worldToScene(wx, wTop);
        tag(m_scene->addLine(a.x(), a.y(), b.x(), b.y(), std::abs(wx) < 1e-6 ? axisGPen : gridPen), 9);
    }
    for (double wy = std::ceil(wBottom / gridStep) * gridStep; wy <= wTop + 1e-9; wy += gridStep) {
        QPointF a = worldToScene(wLeft,  wy);
        QPointF b = worldToScene(wRight, wy);
        tag(m_scene->addLine(a.x(), a.y(), b.x(), b.y(), std::abs(wy) < 1e-6 ? axisGPen : gridPen), 9);
    }
}

void TaskWidget::updateRulers()
{
    if (m_topRuler)
        m_topRuler->syncWithView(m_mapView, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
    if (m_leftRuler)
        m_leftRuler->syncWithView(m_mapView, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
}

void TaskWidget::renderMap()
{
    m_scene->clear();

    // 渲染底图
    if (!m_mapImage.isNull()) {
        QGraphicsPixmapItem *bgItem = m_scene->addPixmap(QPixmap::fromImage(m_mapImage));
        bgItem->setZValue(-100);
    }

    renderAxesAndGrid();

    for (const auto &item : m_points) {
        QJsonObject pt = item.toObject();
        QPointF sp = worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble());
        auto *e = m_scene->addEllipse(sp.x()-5, sp.y()-5, 10, 10,
            QPen(QColor(60, 160, 255), 1), QBrush(QColor(60, 160, 255, 180)));
        e->setZValue(20);
    }

    // 渲染充电点
    for (const auto &item : m_charges) {
        QJsonObject pt = item.toObject();
        QPointF sp = worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble());
        auto *e = m_scene->addEllipse(sp.x()-5, sp.y()-5, 10, 10,
            QPen(QColor("#f39c12"), 1.5), QBrush(QColor("#f39c12")));
        e->setZValue(15); e->setOpacity(0.85);
    }

    // 渲染高精度点：黄色圆点，与普通点统一大小和样式
    for (const auto &item : m_highPrecisionPoints) {
        QJsonObject pt = item.toObject();
        QPointF sp = worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble());
        auto *e = m_scene->addEllipse(sp.x()-5, sp.y()-5, 10, 10,
            QPen(QColor("#f2c200"), 1), QBrush(QColor(242, 194, 0, 180)));
        e->setZValue(20);
    }

    for (const auto &item : m_lines) {
        QJsonObject ln = item.toObject();
        QPointF a = worldToScene(ln.value("x1").toDouble(), ln.value("y1").toDouble());
        QPointF b = worldToScene(ln.value("x2").toDouble(), ln.value("y2").toDouble());
        auto *li = m_scene->addLine(a.x(),a.y(),b.x(),b.y(), QPen(QColor(60, 160, 255, 160), 1.5));
        li->setZValue(15);
    }

    for (const auto &item : m_curves) {
        QJsonObject cv = item.toObject();
        QJsonArray ctrlArr = cv.value("controlPoints").toArray();
        if (ctrlArr.size() >= 2) {
            QList<QPointF> scenePts;
            for (const auto &pv : ctrlArr) {
                QJsonObject pt = pv.toObject();
                scenePts.append(worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble()));
            }
            QPainterPath path = MapPgmUtil::getBezierPath(scenePts);
            auto *pi = m_scene->addPath(path, QPen(QColor(60, 160, 255, 160), 1.5));
            pi->setZValue(15);
        } else {
            QPointF p1 = worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble());
            QPointF pc = worldToScene(cv.value("cx").toDouble(), cv.value("cy").toDouble());
            QPointF p2 = worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble());
            QPainterPath path; path.moveTo(p1); path.quadTo(pc, p2);
            auto *pi = m_scene->addPath(path, QPen(QColor(60, 160, 255, 160), 1.5));
            pi->setZValue(15);
        }
    }

    // 渲染任务目标点 (与 TaskEditWidget 一致: 距离近的点合并，标号逗号分隔按序显示)
    {
        // 先计算所有点的场景坐标
        QList<QPointF> scenePositions;
        for (int i = 0; i < m_targets.size(); ++i) {
            QJsonObject t = m_targets[i].toObject();
            QPointF sp = worldToScene(t.value("x").toDouble(), t.value("y").toDouble());
            scenePositions.append(sp);
        }

        // 按距离聚类分组: 场景坐标距离小于阈值(12px)的归为一组
        const double mergeDist = 12.0;
        QList<QList<int>> groups;
        QList<bool> assigned;
        assigned.fill(false, m_targets.size());
        for (int i = 0; i < m_targets.size(); ++i) {
            if (assigned[i]) continue;
            QList<int> group;
            group.append(i);
            assigned[i] = true;
            for (int j = i + 1; j < m_targets.size(); ++j) {
                if (assigned[j]) continue;
                for (int k : group) {
                    if (std::hypot(scenePositions[j].x() - scenePositions[k].x(),
                                   scenePositions[j].y() - scenePositions[k].y()) < mergeDist) {
                        group.append(j);
                        assigned[j] = true;
                        break;
                    }
                }
            }
            groups.append(group);
        }

        for (const QList<int> &indices : groups) {
            // 取组内平均位置
            QPointF center(0, 0);
            for (int idx : indices) center += scenePositions[idx];
            center /= indices.size();

            auto *e = m_scene->addEllipse(center.x()-4, center.y()-4, 8, 8,
                QPen(Qt::red, 1.5), QBrush(QColor(255, 0, 0, 128)));
            e->setZValue(25);

            // 收集标号并按 targetNumber 排序
            QList<int> nums;
            for (int idx : indices) {
                QJsonObject t = m_targets[idx].toObject();
                nums << t.value("targetNumber").toInt(idx + 1);
            }
            std::sort(nums.begin(), nums.end());
            QStringList numStrs;
            for (int n : nums) numStrs << QString::number(n);
            QString labelText = numStrs.join(",");

            auto *label = m_scene->addText(labelText);
            label->setDefaultTextColor(QColor(255, 170, 170));
            label->setPos(center.x() + 6, center.y() - 6);
            label->setZValue(26);
        }
    }

    // 清除设备标记指针（scene->clear 已删除所有 item）
    for (auto it = m_carInfos.begin(); it != m_carInfos.end(); ++it) {
        it->marker = nullptr;
        it->nameLabel = nullptr;
    }

    // 渲染设备位置
    for (auto it = m_carInfos.begin(); it != m_carInfos.end(); ++it) {
        CarInfo &info = it.value();
        if (info.x != 0 || info.y != 0) {
            updateCarPosition(info.carId, info.x, info.y, info.angle);
        }
    }

    // 反光柱：开关控制显示，标准 GeoJSON Feature 数组，geometry.coordinates=[x,y]
    if (m_showReflectors) {
        auto toDbl = [](const QJsonValue &v) {
            return v.isString() ? v.toString().toDouble() : v.toDouble();
        };
        for (const auto &v : m_reflectorPoles) {
            QJsonObject f = v.toObject();
            QJsonArray coords = f.value("geometry").toObject().value("coordinates").toArray();
            if (coords.size() < 2) continue;
            QPointF sp = worldToScene(toDbl(coords[0]), toDbl(coords[1]));
            auto *e = m_scene->addEllipse(sp.x()-5, sp.y()-5, 10, 10,
                QPen(QColor("#caa020"), 1.5), QBrush(QColor("#f0c040")));
            e->setZValue(8);
        }
    }

    m_scene->setSceneRect(0, 0, m_imgW, m_imgH);
}


void TaskWidget::updateCarPosition(const QString &carId, double x, double y, double angle)
{
    if (!m_carInfos.contains(carId)) return;
    CarInfo &info = m_carInfos[carId];

    // 移除旧标记 (含名称标签，避免每次 MQTT 更新都叠加新的文字导致出现大量重复数字)
    if (info.marker)    { m_scene->removeItem(info.marker);    delete info.marker;    info.marker = nullptr; }
    if (info.nameLabel) { m_scene->removeItem(info.nameLabel); delete info.nameLabel; info.nameLabel = nullptr; }

    // 世界坐标→场景像素坐标
    QPointF sp = worldToScene(x, y);

    // ENU: yaw=0→East, yaw=π/2→North, CCW+. Icon/triangle tip points North.
    // Qt rotate(a) CW; tip (0,-10) → (sin a, -cos a). Match (cos yaw, -sin yaw) → a = 90 - yaw.
    double yawRad = (qAbs(angle) > M_PI * 2) ? angle * M_PI / 180.0 : angle;
    double screenAngle = 90.0 - yawRad * 180.0 / M_PI;

    if (!m_robotPixmap.isNull()) {
        const double iconSz = 15.0;
        const double srcSz  = 256.0;
        const double pivot  = srcSz / 2.0;
        auto *robotItem = m_scene->addPixmap(m_robotPixmap);
        robotItem->setScale(iconSz / srcSz);
        robotItem->setTransformOriginPoint(pivot, pivot);
        robotItem->setPos(sp.x() - pivot, sp.y() - pivot);
        robotItem->setRotation(screenAngle);
        robotItem->setZValue(100);
        info.marker = robotItem;
    } else {
        QPolygonF triangle;
        triangle << QPointF(0, -10) << QPointF(-7, 7) << QPointF(7, 7);
        QTransform t;
        t.translate(sp.x(), sp.y());
        t.rotate(screenAngle);
        QPolygonF mapped = t.map(triangle);

        QGraphicsPolygonItem *robotItem = new QGraphicsPolygonItem(mapped);
        robotItem->setPen(QPen(QColor("#1a6ca0"), 1.5));
        robotItem->setBrush(QBrush(QColor(30, 140, 220, 210)));
        robotItem->setZValue(100);
        m_scene->addItem(robotItem);
        info.marker = robotItem;
    }

    QGraphicsTextItem *nameLabel = m_scene->addText(info.carName);
    nameLabel->setPos(sp.x() + 10, sp.y() - 20);
    nameLabel->setZValue(101);
    nameLabel->setDefaultTextColor(QColor("#7ec8e8"));
    info.nameLabel = nameLabel;
}

void TaskWidget::initMqtt()
{
    auto &mqtt = MqttService::instance();
    if (!mqtt.isConnected()) return;

    QString agvId = AppConfig::instance().agvId();
    if (agvId.isEmpty()) return;

    // 先断开旧订阅，避免设备切换后仍订阅旧设备的 topic
    disconnectMqtt();

    // 订阅设备在线状态
    mqtt.subscribeTopic("/rob/online", 0, [this](const QString &topic, const QByteArray &payload) {
        Q_UNUSED(topic)
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
        if (err.error != QJsonParseError::NoError) return;
        QJsonObject data = doc.object();
        QString carId = jsonIdStr(data.value("carId"));
        if (!carId.isEmpty()) {
            if (data.value("online").toBool()) {
                m_onlineCarIds.insert(carId);
            } else {
                m_onlineCarIds.remove(carId);
            }
        }
    });

    // 订阅设备信息
    QString infoTopic = QString("/rob/%1/info").arg(agvId);
    mqtt.subscribeTopic(infoTopic, 0, [this](const QString &topic, const QByteArray &payload) {
        onMqttMessage(topic, payload);
    });
}

void TaskWidget::disconnectMqtt()
{
    auto &mqtt = MqttService::instance();
    QString agvId = AppConfig::instance().agvId();
    if (!agvId.isEmpty()) {
        mqtt.unsubscribeTopic("/rob/online");
        mqtt.unsubscribeTopic(QString("/rob/%1/info").arg(agvId));
    }
}

void TaskWidget::onMqttMessage(const QString &topic, const QByteArray &payload)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError) return;
    QJsonObject data = doc.object();

    // 处理设备位置信息
    if (topic.contains("/info") || topic.contains("/status")) {
        QString carId = jsonIdStr(data.value("carId"));
        if (carId.isEmpty()) carId = AppConfig::instance().agvId();

        if (!m_carInfos.contains(carId)) {
            CarInfo info;
            info.carId = carId;
            info.carName = data.value("carName").toString(carId);
            m_carInfos[carId] = info;
        }

        CarInfo &info = m_carInfos[carId];
        auto dv = [&](const QStringList &keys, double def) -> double {
            for (const QString &k : keys) {
                QJsonValue v = data.value(k);
                if (!v.isUndefined() && !v.isNull()) return v.toDouble(def);
            }
            return def;
        };
        info.x     = dv({"car_x","carX","x"}, info.x);
        info.y     = dv({"car_y","carY","y"}, info.y);
        info.angle = dv({"car_yaw","carYaw","yaw","angle"}, info.angle);
        info.speed   = dv({"speed","linear_x","linearX"}, info.speed);
        info.battery = dv({"battery","power","bat"}, info.battery);
        info.online  = true;

        updateCarPosition(carId, info.x, info.y, info.angle);
    }
}

void TaskWidget::onWebSocketNotification(const QJsonObject &msg)
{
    // 对应 Vue show.vue 中的 websocket watch:
    //   if (newData.type === "navigation" && newData.car_id === this.carSelect)
    //   { taskInfo.taskId = newData.task_id; taskInfo.status = newData.status; ... }
    QString type = msg.value("type").toString();
    if (type != "navigation") return;

    QString carId = jsonIdStr(msg.value("car_id"));
    if (carId.isEmpty()) carId = jsonIdStr(msg.value("carId"));
    // 仅处理当前选中设备的任务通知
    QString agvId = AppConfig::instance().agvId();
    if (!carId.isEmpty() && carId != agvId) return;

    // 直接用 WebSocket 通知数据更新任务状态(与 Vue 一致)
    // API status 编码: 0=执行中 1=已完成 2=失败 3=暂停 4=未开始
    m_currentTask["taskId"]          = msg.value("task_id");
    m_currentTask["taskName"]         = msg.value("task_name");
    m_currentTask["status"]           = msg.value("status");
    m_currentTask["current_count"]    = msg.value("current_count");
    m_currentTask["execution_count"]  = msg.value("execution_count");
    m_currentTaskId = jsonIdStr(msg.value("task_id"));

    QString status = jsonIdStr(msg.value("status"));
    QString statusText;
    if      (status == "0") statusText = "执行中";
    else if (status == "1") statusText = "已完成";
    else if (status == "2") statusText = "失败";
    else if (status == "3") statusText = "暂停";
    else if (status == "4") statusText = "未开始";
    else                    statusText = status;

    QString taskName = msg.value("task_name").toString();
    m_currentTaskLabel->setText(QString("当前任务: %1 [%2]").arg(taskName, statusText));
    updateTaskButtons();

    // 与轮询定时器同步: WebSocket 报告执行中(0)时确保轮询运行，
    // 报告终态/暂停(1/2/3)时停止轮询
    if (status == "0") {
        if (!m_taskPollTimer->isActive()) startTaskPolling();
    } else if (status == "1" || status == "2" || status == "3") {
        stopTaskPolling();
    }

    // 防抖刷新任务列表
    m_refreshDebounce->start();
}

void TaskWidget::loadCurrentTask()
{
    if (m_currentCarId.isEmpty()) {
        return;
    }

    QJsonObject query;
    query["carId"] = m_currentCarId;
    if (!m_currentMapId.isEmpty()) query["mapId"] = m_currentMapId;

    ApiService::instance().getCurrentTask(query, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        // 响应结构: {data:{taskId,taskName,...}, taskInfo_data:{status,current_count,execution_count}}
        QJsonValue dataVal = data.value("data");
        if (dataVal.isUndefined() || dataVal.isNull() || !dataVal.isObject()) {
            m_currentTask = QJsonObject();
            m_currentTaskLabel->setText("无任务");
            stopTaskPolling();
        } else {
            QJsonObject taskData   = dataVal.toObject();
            QJsonObject statusData = data.value("taskInfo_data").toObject();
            m_currentTask = taskData;
            for (auto it = statusData.begin(); it != statusData.end(); ++it)
                m_currentTask[it.key()] = it.value();

            QString taskName = taskData.value("taskName").toString();
            QString status = jsonIdStr(m_currentTask.value("status"));
            // API status 编码: 0=执行中 1=已完成 2=失败 3=暂停 4=未开始
            QString statusText;
            if      (status == "0") statusText = "执行中";
            else if (status == "1") statusText = "已完成";
            else if (status == "2") statusText = "失败";
            else if (status == "3") statusText = "暂停";
            else if (status == "4") statusText = "未开始";
            else statusText = status;
            m_currentTaskLabel->setText(QString("当前任务: %1 [%2]").arg(taskName, statusText));
        }
        updateTaskButtons();
    });
}

void TaskWidget::checkAndRemoveInvalidLoaded()
{
    QString agvId = AppConfig::instance().agvId();
    if (agvId.isEmpty() || m_invalidTaskIds.isEmpty()) return;

    // 查询设备当前加载的任务，如果该任务已失效则调用移除接口
    QJsonObject query;
    query["carId"] = agvId;
    ApiService::instance().getCurrentTask(query, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonValue dataVal = data.value("data");
        if (dataVal.isUndefined() || dataVal.isNull() || !dataVal.isObject()) return;

        QJsonObject taskData = dataVal.toObject();
        QString taskId = jsonIdStr(taskData.value("taskId"));
        if (taskId.isEmpty() || !m_invalidTaskIds.contains(taskId)) return;

        // 任务已失效且仍处于加载状态，调用移除接口
        QJsonObject statusData = data.value("taskInfo_data").toObject();
        QString execStatus = jsonIdStr(statusData.value("status"));
        // 仅移除非执行中的任务(1=已完成 2=失败 4=未开始)，执行中(0)/暂停(3)不自动移除
        if (execStatus != "1" && execStatus != "2" && execStatus != "4") return;

        ApiService::instance().removeTask(QJsonObject{{"taskId", taskId}},
            [this, taskId](int /*code*/, const QJsonObject &/*resp*/) {
                // 移除后刷新当前任务状态和列表
                m_currentTask = QJsonObject();
                m_currentTaskLabel->setText("当前任务: 无");
                updateTaskButtons();
                stopTaskPolling();
                loadCurrentTask();
            });
    });
}

void TaskWidget::updateTaskButtons()
{
    QString status = jsonIdStr(m_currentTask.value("status"));
    bool hasTask = !m_currentTask.isEmpty();
    // API status 编码(与 Vue show.vue 一致):
    //   0=执行中 1=已完成 2=失败 3=暂停 4=未开始
    // Vue 按钮逻辑:
    //   执行: disabled when ['1','2','4'].indexOf(status) < 0  → 仅 1/2/4 启用
    //   暂停: disabled when ['0'].indexOf(status) < 0          → 仅 0 启用
    //   继续: disabled when ['3'].indexOf(status) < 0          → 仅 3 启用
    //   结束: disabled when ['0','3'].indexOf(status) < 0      → 仅 0/3 启用
    //   移除: disabled when ['1','2','4'].indexOf(status) < 0  → 仅 1/2/4 启用
    m_startTaskBtn->setEnabled(hasTask && (status == "1" || status == "2" || status == "4"));
    m_pauseTaskBtn->setEnabled(hasTask && status == "0");
    m_continueTaskBtn->setEnabled(hasTask && status == "3");
    m_stopTaskBtn->setEnabled(hasTask && (status == "0" || status == "3"));
    m_removeTaskBtn->setEnabled(hasTask && (status == "1" || status == "2" || status == "4"));
}

// ── 任务状态轮询 ──────────────────────────────────────────────────────────────

void TaskWidget::startTaskPolling()
{
    if (!m_taskPollTimer) return;
    m_taskPollTimer->start(1000);   // 1Hz
}

void TaskWidget::stopTaskPolling()
{
    if (!m_taskPollTimer) return;
    m_taskPollTimer->stop();
    m_taskPolling = false;
}

void TaskWidget::onTaskPollTimeout()
{
    // 防止重入: 上一次轮询请求尚未返回时不发起新请求
    if (m_taskPolling) return;
    if (m_currentCarId.isEmpty()) { stopTaskPolling(); return; }

    m_taskPolling = true;
    QJsonObject query;
    query["carId"] = m_currentCarId;
    if (!m_currentMapId.isEmpty()) query["mapId"] = m_currentMapId;

    ApiService::instance().getCurrentTask(query, [this](int code, const QJsonObject &data) {
        m_taskPolling = false;

        if (code != 200) return;
        QJsonValue dataVal = data.value("data");
        if (dataVal.isUndefined() || dataVal.isNull() || !dataVal.isObject()) {
            // API 返回无当前任务 → 任务已结束/被移除，停止轮询
            m_currentTask = QJsonObject();
            m_currentTaskLabel->setText("无任务");
            updateTaskButtons();
            stopTaskPolling();
            return;
        }

        QJsonObject taskData   = dataVal.toObject();
        QJsonObject statusData = data.value("taskInfo_data").toObject();
        m_currentTask = taskData;
        for (auto it = statusData.begin(); it != statusData.end(); ++it)
            m_currentTask[it.key()] = it.value();

        QString taskName = taskData.value("taskName").toString();
        QString status = jsonIdStr(m_currentTask.value("status"));
        // API status 编码: 0=执行中 1=已完成 2=失败 3=暂停 4=未开始
        QString statusText;
        if      (status == "0") statusText = "执行中";
        else if (status == "1") statusText = "已完成";
        else if (status == "2") statusText = "失败";
        else if (status == "3") statusText = "暂停";
        else if (status == "4") statusText = "未开始";
        else statusText = status;
        m_currentTaskLabel->setText(QString("当前任务: %1 [%2]").arg(taskName, statusText));
        updateTaskButtons();

        // 状态为暂停(3)或终态(1已完成/2失败)时停止轮询
        // 执行中(0)继续轮询；未开始(4)也给一点时间让设备启动后继续轮询
        if (status == "1" || status == "2" || status == "3") {
            stopTaskPolling();
        }
    });
}

void TaskWidget::onLoadTask()
{
    if (m_currentTaskId.isEmpty() || m_currentCarId.isEmpty()) return;

    auto *loading = new LoadingDialog("正在检查设备地图...", this);
    loading->show();

    // 任务"已失效"的根本原因是设备当前地图与任务绑定地图不一致。
    // 必须先确保设备已切换到任务所在地图、等待切换成功，才能加载任务。
    ApiService::instance().getCar(m_currentCarId, [this, loading](int code, const QJsonObject &data) {
        if (code != 200) {
            loading->close();
            loading->deleteLater();
            QMessageBox::warning(this, "错误", "加载任务失败：无法获取设备信息");
            return;
        }
        QJsonObject car = data.value("data").toObject(data);
        QString carMapId = jsonIdStr(car.value("mapId"));

        if (carMapId == m_currentMapId) {
            // 设备已在任务所在地图，无需切换
            doLoadTaskRequest(loading);
            return;
        }

        loading->setMessage("正在切换地图...");
        car["mapId"] = m_currentMapId;
        // 必须刷新 updateTime 为当前时间，否则设备端据此计算消息时间差会报 "crud 消息超时"
        car["updateTime"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        ApiService::instance().updateCar(car, [this, loading](int code2, const QJsonObject &) {
            if (code2 != 200) {
                loading->close();
                loading->deleteLater();
                QMessageBox::warning(this, "错误", "加载任务失败：切换地图未成功");
                return;
            }
            // 设备切换地图是异步处理的，后台确认切换请求和设备真正完成切换之间有延迟，
            // 留出缓冲时间再加载任务，避免设备还没切换完成就被判定为"任务已失效"。
            loading->setMessage("正在等待设备完成地图切换...");
            QTimer::singleShot(1500, this, [this, loading]() {
                doLoadTaskRequest(loading, true);
            });
        });
    });
}

void TaskWidget::doLoadTaskRequest(LoadingDialog *loading, bool allowRetry)
{
    loading->setMessage("正在加载任务...");
    // 与 Vue show.vue 的 submitForm() 一致: carId + mapId + taskId 三者缺一不可
    QJsonObject payload{
        {"carId", m_currentCarId},
        {"mapId", m_currentMapId},
        {"taskId", m_currentTaskId}
    };
    ApiService::instance().loadTask(payload, [this, loading, allowRetry](int code, const QJsonObject &resp) {
        QString msg = resp.value("msg").toString();
        bool staleMapError = msg.contains("失效") || msg.contains("无法载入");

        // 地图刚切换完成、设备状态还没完全同步时可能仍短暂报"已失效"，
        // 这种情况下不打扰用户，安静地再重试一次。
        if (code != 200 && staleMapError && allowRetry) {
            QTimer::singleShot(1500, this, [this, loading]() {
                doLoadTaskRequest(loading, false);
            });
            return;
        }

        loading->close();
        loading->deleteLater();
        if (code == 200) {
            Toast::success(this, "任务加载成功");
            // 加载任务后立即拉取一次状态并启动轮询
            loadCurrentTask();
            startTaskPolling();
        } else {
            // 不再把"任务已失效/无法载入"原文展示给用户，统一提示重试
            QMessageBox::warning(this, "错误",
                (staleMapError || msg.isEmpty()) ? "任务加载失败，请稍后重试" : msg);
        }
    });
}

void TaskWidget::onStartTask()
{
    if (m_currentTaskId.isEmpty() || m_currentCarId.isEmpty()) return;
    auto *loading = new LoadingDialog("正在执行任务...", this);
    loading->show();
    // 与 Vue show.vue 的 onstartTask() 一致: carId + mapId + taskId
    ApiService::instance().startTask(QJsonObject{
        {"carId", m_currentCarId},
        {"mapId", m_currentMapId},
        {"taskId", m_currentTaskId}
    }, [this, loading](int code, const QJsonObject &resp) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            Toast::success(this, "任务已开始执行");
            // 执行成功后启动1Hz轮询，持续读取任务状态直到暂停或结束
            startTaskPolling();
        } else {
            QString msg = resp.value("msg").toString();
            QMessageBox::warning(this, "错误", msg.isEmpty() ? "执行任务失败" : msg);
        }
    });
}

void TaskWidget::sendNavCommand(const QString &command, const QString &loadingText,
                                 const QString &successText, const QString &failText)
{
    if (m_currentTaskId.isEmpty() || m_currentCarId.isEmpty()) return;
    auto *loading = new LoadingDialog(loadingText, this);
    loading->show();
    QJsonObject params;
    params["command"] = command;
    // 与 Vue show.vue 的 onCommandTask() 一致: carId + mapId + taskId + 嵌套的 params.command
    ApiService::instance().commandTask(QJsonObject{
        {"carId", m_currentCarId},
        {"mapId", m_currentMapId},
        {"taskId", m_currentTaskId},
        {"params", params}
    }, [this, loading, successText, failText, command](int code, const QJsonObject &resp) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            Toast::success(this, successText);
            // 继续(continue navigation)命令: 重新启动1Hz轮询
            // 暂停/结束命令: 轮询循环会在下次检测到状态变为3/1时自动停止
            if (command == "continue navigation") {
                startTaskPolling();
            }
        } else {
            QString msg = resp.value("msg").toString();
            QMessageBox::warning(this, "错误", msg.isEmpty() ? failText : msg);
        }
    });
}

void TaskWidget::onPauseTask()
{
    sendNavCommand("pause navigation", "正在暂停任务...", "任务已暂停", "暂停任务失败");
}

void TaskWidget::onContinueTask()
{
    sendNavCommand("continue navigation", "正在继续任务...", "任务已继续", "继续任务失败");
}

void TaskWidget::onStopTask()
{
    sendNavCommand("stop navigation", "正在结束任务...", "任务已结束", "结束任务失败");
}

void TaskWidget::onRemoveTask()
{
    if (m_currentTaskId.isEmpty()) return;
    auto *loading = new LoadingDialog("正在移除任务...", this);
    loading->show();
    // 与 Vue show.vue 的 onRemoveTask() 一致: 只需 taskId，不是 carId
    ApiService::instance().removeTask(QJsonObject{{"taskId", m_currentTaskId}}, [this, loading](int code, const QJsonObject &resp) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            Toast::success(this, "任务已移除");
            // 移除任务后停止轮询，并清除状态
            stopTaskPolling();
            m_currentTask = QJsonObject();
            m_currentTaskLabel->setText("无任务");
            updateTaskButtons();
            loadCurrentTask();
        } else {
            QString msg = resp.value("msg").toString();
            QMessageBox::warning(this, "错误", msg.isEmpty() ? "移除任务失败" : msg);
        }
    });
}

void TaskWidget::onAdd()
{
    emit navigateRequested("task_edit", "");
}

void TaskWidget::onEdit()
{
    int row = m_table->currentRow();
    if (row < 0) return;
    QJsonObject task = m_taskList[row].toObject();
    emit navigateRequested("task_edit", jsonIdStr(task.value("taskId")));
}

void TaskWidget::onDelete()
{
    int row = m_table->currentRow();
    if (row < 0) return;
    QJsonObject task = m_taskList[row].toObject();
    QString taskId = jsonIdStr(task.value("taskId"));

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认");
    msgBox.setText(QString("是否确认删除任务编号为\"%1\"的数据项？").arg(taskId));
    QPushButton *yesBtn = msgBox.addButton("确定", QMessageBox::YesRole);
    msgBox.addButton("取消", QMessageBox::NoRole);
    msgBox.exec();
    if (msgBox.clickedButton() == yesBtn) {
        // 如果该任务处于"被加载"状态(status==0执行中或status==3暂停)，
        // 先尝试"移除"操作，无论移除是否成功都继续执行删除
        bool isLoaded = (jsonIdStr(task.value("status")) == "0" ||
                         jsonIdStr(task.value("status")) == "3" ||
                         taskId == m_currentTaskId);
        if (isLoaded) {
            auto *loading = new LoadingDialog("正在删除任务...", this);
            loading->show();
            // 先尝试移除，失败也继续删除
            ApiService::instance().removeTask(QJsonObject{{"taskId", taskId}},
                [this, taskId, loading](int code, const QJsonObject &resp) {
                    // 无论移除成功或失败，都执行删除
                    Q_UNUSED(resp);
                    ApiService::instance().delTask(taskId, [this, taskId, loading](int code2, const QJsonObject &data) {
                        loading->close();
                        loading->deleteLater();
                        if (code2 == 200) {
                            Toast::success(this, "删除成功");
                            if (!m_currentTaskId.isEmpty() && m_currentTaskId == taskId) {
                                m_currentTaskId.clear();
                                m_currentTask = QJsonObject();
                                m_currentTaskLabel->setText("当前任务: 无");
                                updateTaskButtons();
                                if (m_taskPollTimer) m_taskPollTimer->stop();
                            }
                            getList();
                            loadCurrentTask();
                        } else {
                            QString msg = data.value("msg").toString();
                            QMessageBox::warning(this, "错误", msg.isEmpty() ? "删除任务失败" : msg);
                        }
                    });
                });
        } else {
            // 不在加载状态，直接删除
            ApiService::instance().delTask(taskId, [this](int code, const QJsonObject &data) {
                if (code == 200) {
                    Toast::success(this, "删除成功");
                    getList();
                } else {
                    QString msg = data.value("msg").toString();
                    QMessageBox::warning(this, "错误", msg.isEmpty() ? "删除任务失败" : msg);
                }
            });
        }
    }
}

void TaskWidget::onRefresh()
{
    getList();
}

void TaskWidget::onTableSelectionChanged()
{
    bool hasSelection = m_table->currentRow() >= 0;
    m_editBtn->setEnabled(hasSelection);
    m_deleteBtn->setEnabled(hasSelection);
}
