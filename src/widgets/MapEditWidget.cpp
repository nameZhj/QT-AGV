#include "MapEditWidget.h"
#include "MapViewWidget.h"
#include "core/ApiService.h"
#include "core/AppConfig.h"
#include "core/MqttService.h"
#include "core/LoadingDialog.h"
#include "core/Toast.h"
#include "core/MapPgmUtil.h"
#include "core/ToggleSwitch.h"
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QHeaderView>
#include <QMessageBox>
#include <QGraphicsPixmapItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QJsonDocument>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFrame>
#include <QApplication>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <cmath>
#include <QPainter>
#include <QToolTip>
#include <QDebug>

// ── Style constants ───────────────────────────────────────────────────────────

static const char *S_TOOLBAR =
    "background:#1a2230;";

static const char *S_TOOLBTN =
    "QPushButton{"
    "  background:transparent;"
    "  border:none; border-radius:7px; padding:0;"
    "}"
    "QPushButton:hover{"
    "  background:rgba(100,160,220,0.14);"
    "}"
    "QPushButton:checked{"
    "  background:rgba(41,128,185,210);"
    "}";

static const char *S_TOPBAR =
    "background:#1e2d3d; border-bottom:1px solid rgba(255,255,255,0.06);";

static const char *S_RIGHTPANEL =
    "background:#16202e;";

static const char *S_CARD =
    "QGroupBox{"
    "  font-weight:bold; font-size:11px; color:#4a7098;"
    "  border:1px solid rgba(255,255,255,0.07); border-radius:7px;"
    "  margin-top:12px; padding-top:4px; background:#1a2638;"
    "}"
    "QGroupBox::title{"
    "  subcontrol-origin:margin; left:10px;"
    "  padding:0 5px; background:#16202e; color:#5888a8;"
    "}"
    "QLabel{background:transparent; color:#90b0cc; font-size:12px;}"
    "QLineEdit,QComboBox{"
    "  border:1px solid rgba(255,255,255,0.10); border-radius:4px;"
    "  padding:4px 7px; background:#1e2d40; color:#b0c8e0; font-size:12px;"
    "}"
    "QComboBox QAbstractItemView{background:#1e2d40;color:#b0c8e0;selection-background-color:#2a4060;}"
    "QPushButton{"
    "  border:1px solid rgba(255,255,255,0.10); border-radius:5px;"
    "  padding:5px 10px; font-size:12px; background:#1e2d40; color:#90b0cc;"
    "}"
    "QPushButton:hover{background:#263850;}";

static const char *S_TABLE =
    "QTableWidget{"
    "  background:#1a2638; border:none; gridline-color:transparent;"
    "  color:#a8c0d8; font-size:12px; outline:0;"
    "}"
    "QHeaderView::section{"
    "  background:#131d2a; color:#4a7098;"
    "  border:none; border-bottom:1px solid rgba(255,255,255,0.07);"
    "  padding:5px 8px; font-size:11px; font-weight:bold;"
    "}"
    "QTableWidget::item{"
    "  padding:5px 8px;"
    "  border-bottom:1px solid rgba(255,255,255,0.04);"
    "}"
    "QTableWidget::item:selected{"
    "  background:rgba(41,128,185,0.38); color:white;"
    "}"
    "QTableWidget::item:hover:!selected{"
    "  background:rgba(100,160,220,0.10);"
    "}"
    "QScrollBar:vertical{background:#131d2a;width:6px;border:none;}"
    "QScrollBar::handle:vertical{background:#2a4060;border-radius:3px;min-height:20px;}"
    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}";

// 临时诊断日志：console 已禁用(WIN32 子系统)，直接写文件方便排查定位问题
static void debugLog(const QString &msg)
{
    QFile f("C:/Temp/agv_map_debug.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << "  " << msg << "\n";
    }
}

static QString ttHtml(const QString &title, const QString &body)
{
    QString b = body; b.replace('\n', QLatin1String("<br>"));
    return QString("<div style='min-width:150px;'>"
                   "<div style='font-size:13px;font-weight:600;color:#ddeeff;"
                   "margin-bottom:3px;'>%1</div>"
                   "<div style='color:#90adc8;font-size:11px;line-height:1.6;'>%2</div>"
                   "</div>").arg(title, b);
}

// ── Icon helpers ──────────────────────────────────────────────────────────────

namespace {

QPixmap drawToolIcon(const QString &name, const QColor &col, int sz = 22)
{
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    double c = sz * 0.5;

    if (name == "pan") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        double arm = sz * 0.34, head = sz * 0.17;
        auto arw = [&](double dx, double dy) {
            QPointF tip(c + dx*arm, c + dy*arm), base(c + dx*2.5, c + dy*2.5);
            p.drawLine(base, tip);
            QPointF n(-dy, dx);
            p.drawLine(tip, tip - QPointF(dx,dy)*head + n*(head*0.5));
            p.drawLine(tip, tip - QPointF(dx,dy)*head - n*(head*0.5));
        };
        arw(0,-1); arw(0,1); arw(-1,0); arw(1,0);
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(QPointF(c,c), sz*0.09, sz*0.09);
    }
    else if (name == "point") {
        p.setPen(QPen(col, 1.6)); p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(c,c), sz*0.35, sz*0.35);
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(QPointF(c,c), sz*0.12, sz*0.12);
    }
    else if (name == "line") {
        QPointF a(sz*0.15, sz*0.85), b2(sz*0.85, sz*0.15);
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap)); p.drawLine(a, b2);
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(a, sz*0.13, sz*0.13); p.drawEllipse(b2, sz*0.13, sz*0.13);
    }
    else if (name == "curve") {
        QPointF a(sz*0.08, sz*0.88), ctrl(c, sz*0.10), b2(sz*0.92, sz*0.88);
        QPainterPath path; path.moveTo(a); path.quadTo(ctrl, b2);
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(a, sz*0.12, sz*0.12); p.drawEllipse(b2, sz*0.12, sz*0.12);
        p.setPen(QPen(col.lighter(170), 0.9, Qt::DotLine)); p.setBrush(Qt::NoBrush);
        p.drawLine(a, ctrl); p.drawLine(ctrl, b2);
        p.drawEllipse(ctrl, sz*0.10, sz*0.10);
    }
    else if (name == "charge") {
        QPolygonF bolt;
        bolt << QPointF(c+sz*0.10, sz*0.06)  << QPointF(c-sz*0.20, c+sz*0.06)
             << QPointF(c+sz*0.04, c+sz*0.06) << QPointF(c-sz*0.10, sz*0.94)
             << QPointF(c+sz*0.20, c-sz*0.06) << QPointF(c-sz*0.04, c-sz*0.06);
        p.setPen(QPen(col.darker(115), 1.0)); p.setBrush(col);
        p.drawPolygon(bolt);
    }
    else if (name == "highprecision") {
        double r = sz*0.35, gap = sz*0.14;
        p.setPen(QPen(col, 1.5)); p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(c,c), r, r);
        p.drawLine(QPointF(c,sz*0.04), QPointF(c,c-gap));
        p.drawLine(QPointF(c,c+gap),   QPointF(c,sz*0.96));
        p.drawLine(QPointF(sz*0.04,c), QPointF(c-gap,c));
        p.drawLine(QPointF(c+gap,c),   QPointF(sz*0.96,c));
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(QPointF(c,c), sz*0.09, sz*0.09);
    }
    else if (name == "list") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap));
        double x1=sz*0.18, x2=sz*0.82;
        p.drawLine(QPointF(x1,c-sz*0.24), QPointF(x2,c-sz*0.24));
        p.drawLine(QPointF(x1,c),          QPointF(x2,c));
        p.drawLine(QPointF(x1,c+sz*0.24), QPointF(x2,c+sz*0.24));
    }
    else if (name == "save") {
        p.setPen(QPen(col, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath cp;
        cp.moveTo(sz*0.14, sz*0.50); cp.lineTo(sz*0.42, sz*0.80); cp.lineTo(sz*0.86, sz*0.20);
        p.drawPath(cp);
    }
    else if (name == "grid") {
        p.setPen(QPen(col, 1.4));
        double m1 = sz*0.18, m2 = sz*0.82, t1 = sz*0.40, t2 = sz*0.60;
        p.drawRect(QRectF(m1, m1, m2-m1, m2-m1));
        p.drawLine(QPointF(t1,m1), QPointF(t1,m2)); p.drawLine(QPointF(t2,m1), QPointF(t2,m2));
        p.drawLine(QPointF(m1,t1), QPointF(m2,t1)); p.drawLine(QPointF(m1,t2), QPointF(m2,t2));
    }
    else if (name == "reset") {
        // 重置视图: 圆弧 + 箭头(顺时针箭头表示"恢复初始")
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
    else if (name == "robot") {
        QPolygonF tri;
        tri << QPointF(sz*0.85, c) << QPointF(sz*0.20, sz*0.82) << QPointF(sz*0.20, sz*0.18);
        p.setPen(QPen(col.darker(115), 1.0));
        p.setBrush(col);
        p.drawPolygon(tri);
    }
    return pm;
}

QIcon makeToolIcon(const QString &name)
{
    QIcon icon;
    icon.addPixmap(drawToolIcon(name, QColor("#7a95b0")), QIcon::Normal,  QIcon::Off);
    icon.addPixmap(drawToolIcon(name, QColor("#c8ddef")), QIcon::Active,  QIcon::Off);
    icon.addPixmap(drawToolIcon(name, QColor("white")),   QIcon::Normal,  QIcon::On);
    icon.addPixmap(drawToolIcon(name, QColor("white")),   QIcon::Active,  QIcon::On);
    return icon;
}

struct InstantTip : QObject {
    using QObject::QObject;
    bool eventFilter(QObject *o, QEvent *e) override {
        if (e->type() == QEvent::Enter) {
            auto *w = qobject_cast<QWidget*>(o);
            if (w && !w->toolTip().isEmpty())
                QToolTip::showText(QCursor::pos(), w->toolTip(), w);
        } else if (e->type() == QEvent::Leave) {
            QToolTip::hideText();
        }
        return false;
    }
};

} // namespace

// ── Construction ──────────────────────────────────────────────────────────────

MapEditWidget::MapEditWidget(const QString &mapId, QWidget *parent)
    : QWidget(parent), m_mapId(mapId)
{
    setupUI();
    loadMapData();
    loadRouteData();
    loadReflectorData();
    loadCarList();
}

MapEditWidget::~MapEditWidget()
{
    disconnectMqtt();
}

// ── UI Setup ──────────────────────────────────────────────────────────────────

void MapEditWidget::setupUI()
{
    setContentsMargins(0,0,0,0);

    static bool s_tipStyle = false;
    if (!s_tipStyle) {
        s_tipStyle = true;
        qApp->setStyleSheet(qApp->styleSheet() +
            "QToolTip{background:rgba(13,20,36,242);color:#c8ddef;"
            "border:1px solid rgba(72,130,180,0.55);"
            "border-radius:7px;padding:7px 12px;font-size:12px;}");
    }

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    // ── Top bar ───────────────────────────────────────────────────────────────
    QWidget *topBar = new QWidget;
    topBar->setFixedHeight(44);
    topBar->setStyleSheet(S_TOPBAR);
    QHBoxLayout *tl = new QHBoxLayout(topBar);
    tl->setContentsMargins(8,4,8,4);
    tl->setSpacing(8);

    auto topBtn = [&](const QString &txt, const QString &bg = QString()) -> QPushButton* {
        auto *b = new QPushButton(txt);
        QString base = bg.isEmpty() ? "transparent" : bg;
        b->setStyleSheet(QString(
            "QPushButton{color:white;border:none;border-radius:4px;"
            "padding:4px 12px;font-size:12px;background:%1;}"
            "QPushButton:hover{background:%2;}")
            .arg(base, bg.isEmpty() ? "rgba(255,255,255,18)" : bg));
        return b;
    };

    QPushButton *backBtn = topBtn("← 返回");
    connect(backBtn, &QPushButton::clicked, this, &MapEditWidget::goBack);
    tl->addWidget(backBtn);

    QFrame *div = new QFrame; div->setFrameShape(QFrame::VLine);
    div->setStyleSheet("color:rgba(255,255,255,0.08);"); tl->addWidget(div);

    m_mapInfoLabel = new QLabel("路网编辑");
    m_mapInfoLabel->setStyleSheet("color:#9ab8d8;font-weight:bold;font-size:13px;");
    tl->addWidget(m_mapInfoLabel);
    tl->addStretch();

    m_resetViewBtn = new QPushButton;
    m_resetViewBtn->setIcon(makeToolIcon("reset"));
    m_resetViewBtn->setIconSize(QSize(18, 18));
    m_resetViewBtn->setFixedSize(30, 30);
    m_resetViewBtn->setToolTip(ttHtml("重置视图", "恢复地图到初始缩放与角度"));
    m_resetViewBtn->setCursor(Qt::PointingHandCursor);
    m_resetViewBtn->installEventFilter(new InstantTip(m_resetViewBtn));
    m_resetViewBtn->setStyleSheet(
        "QPushButton{background:transparent;border:none;border-radius:5px;padding:0;}"
        "QPushButton:hover{background:rgba(255,255,255,12);}");
    connect(m_resetViewBtn, &QPushButton::clicked, this, [this]() {
        if (!m_mapView || !m_scene) return;
        m_mapView->resetTransform();
        m_mapView->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatioByExpanding);
        updateRulers();
    });
    tl->addWidget(m_resetViewBtn);

    m_gridBtn = new QPushButton;
    m_gridBtn->setIcon(makeToolIcon("grid"));
    m_gridBtn->setIconSize(QSize(18, 18));
    m_gridBtn->setCheckable(true);
    m_gridBtn->setFixedSize(30, 30);
    m_gridBtn->setToolTip(ttHtml("网格", "切换网格显示"));
    m_gridBtn->setCursor(Qt::PointingHandCursor);
    m_gridBtn->installEventFilter(new InstantTip(m_gridBtn));
    m_gridBtn->setStyleSheet(
        "QPushButton{background:transparent;border:none;border-radius:5px;padding:0;}"
        "QPushButton:hover{background:rgba(255,255,255,12);}"
        "QPushButton:checked{background:rgba(41,128,185,160);}");
    connect(m_gridBtn, &QPushButton::toggled, this, [this](bool on) {
        m_showGrid = on; renderMap();
    });
    tl->addWidget(m_gridBtn);

    QLabel *robotSwitchLabel = new QLabel("设备位置");
    robotSwitchLabel->setStyleSheet("color:#9ab8d8;font-size:12px;");
    robotSwitchLabel->setToolTip(ttHtml("设备位置", "切换实时设备位置显示"));
    tl->addWidget(robotSwitchLabel);

    m_robotBtn = new ToggleSwitch;
    m_robotBtn->setToolTip(ttHtml("设备位置", "切换实时设备位置显示"));
    m_robotBtn->installEventFilter(new InstantTip(m_robotBtn));
    connect(m_robotBtn, &ToggleSwitch::toggled, this, [this](bool on) {
        m_showRobotPosition = on;
        updateRobotMarker();
    });
    tl->addWidget(m_robotBtn);

    QLabel *reflectorSwitchLabel = new QLabel("反光柱");
    reflectorSwitchLabel->setStyleSheet("color:#9ab8d8;font-size:12px;");
    reflectorSwitchLabel->setToolTip(ttHtml("反光柱", "切换反光柱显示"));
    tl->addWidget(reflectorSwitchLabel);

    m_reflectorBtn = new ToggleSwitch;
    m_reflectorBtn->setToolTip(ttHtml("反光柱", "切换反光柱显示"));
    m_reflectorBtn->installEventFilter(new InstantTip(m_reflectorBtn));
    connect(m_reflectorBtn, &ToggleSwitch::toggled, this, [this](bool on) {
        m_showReflectors = on;
        renderMap();
    });
    tl->addWidget(m_reflectorBtn);

    m_editMapBtn = topBtn("地图属性");
    connect(m_editMapBtn, &QPushButton::clicked, this, &MapEditWidget::onEditMap);
    tl->addWidget(m_editMapBtn);

    m_saveBtn = topBtn("保存路网", "#2471a3");
    connect(m_saveBtn, &QPushButton::clicked, this, &MapEditWidget::onSaveRoute);
    tl->addWidget(m_saveBtn);

    root->addWidget(topBar);

    // ── Content row ───────────────────────────────────────────────────────────
    QWidget *content = new QWidget;
    QHBoxLayout *cl  = new QHBoxLayout(content);
    cl->setContentsMargins(0,0,0,0);
    cl->setSpacing(0);

    // ── Left toolbar ──────────────────────────────────────────────────────────
    QWidget *toolbar = new QWidget;
    toolbar->setFixedWidth(50);
    toolbar->setStyleSheet(S_TOOLBAR);
    QVBoxLayout *tbLayout = new QVBoxLayout(toolbar);
    tbLayout->setContentsMargins(5,10,5,10);
    tbLayout->setSpacing(2);

    struct Tool { int mode; QString icon; QString tip; QString detail; };
    const QList<Tool> tools = {
        {Drag,          "pan",          "拖拽",   "平移地图 / 选择要素\n拖动曲线控制手柄调整形状"},
        {-1,            "",             "",        ""},
        {Point,         "point",        "节点",   "点击放置普通路网节点"},
        {Line,          "line",         "线段",   "点击起点 → 再点终点"},
        {Curve,         "curve",        "曲线",   "点击两点自动生成曲线\n拖动橙色控制手柄微调"},
        {Charge,        "charge",       "充电桩", "点击放置充电节点"},
        {HighPrecision, "highprecision","高精度", "点击放置高精度节点"},
    };

    m_toolBtns.clear();
    for (const auto &t : tools) {
        if (t.mode == -1) {
            auto *sep = new QFrame; sep->setFrameShape(QFrame::HLine);
            sep->setFixedHeight(1);
            sep->setStyleSheet("background:rgba(255,255,255,0.07); border:none; margin:4px 4px;");
            tbLayout->addWidget(sep);
            continue;
        }
        auto *b = new QPushButton;
        b->setIcon(makeToolIcon(t.icon));
        b->setIconSize(QSize(22, 22));
        b->setCheckable(true);
        b->setFixedSize(40, 40);
        b->setStyleSheet(S_TOOLBTN);
        b->setToolTip(ttHtml(t.tip, t.detail));
        b->setCursor(Qt::PointingHandCursor);
        b->installEventFilter(new InstantTip(b));
        int mode = t.mode;
        connect(b, &QPushButton::clicked, this, [this, mode]() { setDrawMode(mode); });
        tbLayout->addWidget(b, 0, Qt::AlignHCenter);
        m_toolBtns[mode] = b;
    }

    tbLayout->addStretch();

    auto *sep2 = new QFrame; sep2->setFrameShape(QFrame::HLine);
    sep2->setFixedHeight(1);
    sep2->setStyleSheet("background:rgba(255,255,255,0.07); border:none; margin:4px 4px;");
    tbLayout->addWidget(sep2);

    m_tableModeBtn = new QPushButton;
    m_tableModeBtn->setIcon(makeToolIcon("list"));
    m_tableModeBtn->setIconSize(QSize(22, 22));
    m_tableModeBtn->setCheckable(true);
    m_tableModeBtn->setFixedSize(40, 40);
    m_tableModeBtn->setStyleSheet(S_TOOLBTN);
    m_tableModeBtn->setToolTip(ttHtml("要素列表", "显示/隐藏路网要素列表"));
    m_tableModeBtn->setCursor(Qt::PointingHandCursor);
    m_tableModeBtn->installEventFilter(new InstantTip(m_tableModeBtn));
    connect(m_tableModeBtn, &QPushButton::clicked, this, &MapEditWidget::onTableToggle);
    tbLayout->addWidget(m_tableModeBtn, 0, Qt::AlignHCenter);

    m_toolBtns[Drag]->setChecked(true);
    cl->addWidget(toolbar);

    // ── Map view with ruler strips ────────────────────────────────────────────
    m_scene   = new QGraphicsScene(this);
    m_mapView = new MapCanvas(m_scene, this);
    m_mapView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_mapView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_mapView->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_mapView->setFrameStyle(QFrame::NoFrame);
    m_mapView->setBackgroundBrush(QColor("#0f1520"));
    m_mapView->setDragMode(QGraphicsView::ScrollHandDrag);
    m_mapView->viewport()->installEventFilter(this);
    m_mapView->viewport()->setMouseTracking(true);

    m_coordLabel = new QLabel("X: --  Y: --");
    m_coordLabel->setStyleSheet(
        "background:rgba(10,16,26,195); color:#00c8a0;"
        "padding:3px 10px; border-radius:3px;"
        "font-size:11px; font-family:Consolas,monospace;");
    m_coordLabel->setParent(m_mapView);
    m_coordLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_coordLabel->adjustSize();
    m_coordLabel->raise();

    m_topRuler  = new RulerWidget(RulerWidget::Horizontal);
    m_leftRuler = new RulerWidget(RulerWidget::Vertical);

    // Wrap: [leftRuler | mapView]  (top ruler sits above both)
    QWidget *mapArea = new QWidget;
    mapArea->setContentsMargins(0,0,0,0);
    QVBoxLayout *maVBox = new QVBoxLayout(mapArea);
    maVBox->setContentsMargins(0,0,0,0); maVBox->setSpacing(0);
    maVBox->addWidget(m_topRuler);
    QWidget *mapInner = new QWidget;
    QHBoxLayout *miHBox = new QHBoxLayout(mapInner);
    miHBox->setContentsMargins(0,0,0,0); miHBox->setSpacing(0);
    miHBox->addWidget(m_leftRuler);
    miHBox->addWidget(m_mapView, 1);
    maVBox->addWidget(mapInner, 1);

    cl->addWidget(mapArea, 1);

    // ── Right panel ───────────────────────────────────────────────────────────
    QWidget *rightOuter = new QWidget;
    rightOuter->setFixedWidth(230);
    rightOuter->setStyleSheet(S_RIGHTPANEL);

    QScrollArea *sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameStyle(QFrame::NoFrame);
    sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sa->setStyleSheet(
        "background:transparent; border:none;"
        "QScrollBar:vertical{background:#131d2a;width:5px;border:none;}"
        "QScrollBar::handle:vertical{background:#2a4060;border-radius:2px;}");

    QWidget *rc = new QWidget;
    rc->setStyleSheet("background:#16202e;");
    QVBoxLayout *rl = new QVBoxLayout(rc);
    rl->setContentsMargins(8,8,8,8);
    rl->setSpacing(8);
    sa->setWidget(rc);

    QVBoxLayout *rol = new QVBoxLayout(rightOuter);
    rol->setContentsMargins(0,0,0,0);
    rol->addWidget(sa);

    // Map info
    {
        auto *g = new QGroupBox("地图信息");
        g->setStyleSheet(S_CARD);
        auto *gl = new QVBoxLayout(g);
        gl->setSpacing(4);
        auto *idLbl = new QLabel("ID: " + m_mapId);
        idLbl->setStyleSheet("color:#4a7098;font-size:11px;background:transparent;");
        idLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        idLbl->setWordWrap(true);
        gl->addWidget(idLbl);
        rl->addWidget(g);
    }

    // Device
    {
        auto *g = new QGroupBox("MQTT 设备");
        g->setStyleSheet(S_CARD);
        auto *gl = new QVBoxLayout(g);
        gl->setSpacing(5);
        m_carCombo = new QComboBox;
        gl->addWidget(m_carCombo);
        auto *conBtn = new QPushButton("发送继续导航");
        conBtn->setStyleSheet(
            "QPushButton{background:rgba(39,174,96,160);color:white;border:none;"
            "border-radius:5px;padding:5px 10px;font-size:11px;}"
            "QPushButton:hover{background:rgba(46,204,113,190);}");
        connect(conBtn, &QPushButton::clicked, this, [this]() {
            if (!m_currentCarId.isEmpty())
                MqttService::instance().publish(
                    QString("/rob/%1/navigationContinue").arg(m_currentCarId), "1");
        });
        gl->addWidget(conBtn);
        rl->addWidget(g);
    }

    // Properties (hidden until selection)
    {
        m_propsPanel = new QGroupBox("选中要素");
        m_propsPanel->setStyleSheet(S_CARD);
        auto *pl = new QVBoxLayout(m_propsPanel);
        pl->setSpacing(5);

        m_propTypeLabel = new QLabel("类型: -");
        m_propTypeLabel->setStyleSheet("color:#4a7098;font-size:11px;background:transparent;");
        pl->addWidget(m_propTypeLabel);

        QFormLayout *form = new QFormLayout;
        form->setSpacing(4);
        form->setLabelAlignment(Qt::AlignRight);
        m_propIdEdit = new QLineEdit; m_propIdEdit->setReadOnly(true);
        m_propIdEdit->setStyleSheet("background:#f8f9fc;color:#6a7888;");
        m_propNameEdit = new QLineEdit;
        form->addRow("编号:", m_propIdEdit);
        form->addRow("名称:", m_propNameEdit);
        pl->addLayout(form);

        m_propDirectRow = new QWidget;
        auto *dl = new QHBoxLayout(m_propDirectRow);
        dl->setContentsMargins(0,0,0,0);
        auto *dLbl = new QLabel("方向:");
        dLbl->setStyleSheet("color:#4a6080;font-size:12px;background:transparent;");
        m_propDirectCombo = new QComboBox;
        m_propDirectCombo->addItem("单向 →", 100);
        m_propDirectCombo->addItem("双向 ↔", 10);
        dl->addWidget(dLbl); dl->addWidget(m_propDirectCombo);
        pl->addWidget(m_propDirectRow);

        QHBoxLayout *pb = new QHBoxLayout;
        auto *applyBtn = new QPushButton("应用");
        auto *delBtn   = new QPushButton("删除");
        applyBtn->setStyleSheet(
            "QPushButton{background:rgba(36,113,163,180);color:white;border:none;border-radius:5px;padding:5px 10px;}"
            "QPushButton:hover{background:rgba(52,152,219,200);}");
        delBtn->setStyleSheet(
            "QPushButton{background:rgba(192,57,43,160);color:white;border:none;border-radius:5px;padding:5px 10px;}"
            "QPushButton:hover{background:rgba(231,76,60,190);}");
        connect(applyBtn, &QPushButton::clicked, this, &MapEditWidget::applyPropsEdit);
        connect(delBtn,   &QPushButton::clicked, this, &MapEditWidget::deleteSelected);
        pb->addWidget(applyBtn); pb->addWidget(delBtn);
        pl->addLayout(pb);

        m_propsPanel->setVisible(false);
        rl->addWidget(m_propsPanel);
    }

    // Feature list (collapsible)
    {
        m_tablePanel = new QGroupBox("要素列表");
        m_tablePanel->setStyleSheet(S_CARD);
        auto *tl2 = new QVBoxLayout(m_tablePanel);
        tl2->setSpacing(4);

        m_tableTypeCombo = new QComboBox;
        m_tableTypeCombo->addItem("节点",   "point");
        m_tableTypeCombo->addItem("线段",   "line");
        m_tableTypeCombo->addItem("曲线",   "curve");
        m_tableTypeCombo->addItem("充电桩", "charge");
        m_tableTypeCombo->addItem("高精度", "highPrecision");
        connect(m_tableTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this]() {
            m_currentTable = m_tableTypeCombo->currentData().toString();
            refreshTable();
        });
        tl2->addWidget(m_tableTypeCombo);

        m_featureTable = new QTableWidget;
        m_featureTable->setColumnCount(2);
        m_featureTable->setHorizontalHeaderLabels({"编号", "名称"});
        m_featureTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_featureTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_featureTable->setSelectionMode(QAbstractItemView::SingleSelection);
        m_featureTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_featureTable->setMaximumHeight(210);
        m_featureTable->setStyleSheet(S_TABLE);
        connect(m_featureTable, &QTableWidget::itemSelectionChanged, this, [this]() {
            int row = m_featureTable->currentRow();
            if (row < 0) return;
            auto *it = m_featureTable->item(row, 0);
            if (it) showFeatureProperties(m_currentTable, it->text());
        });
        tl2->addWidget(m_featureTable);

        auto *delBtn = new QPushButton("删除选中");
        delBtn->setStyleSheet(
            "QPushButton{background:rgba(192,57,43,160);color:white;border:none;"
            "border-radius:5px;padding:5px 10px;font-size:11px;}"
            "QPushButton:hover{background:rgba(231,76,60,190);}");
        connect(delBtn, &QPushButton::clicked, this, &MapEditWidget::onDeleteFeature);
        tl2->addWidget(delBtn);

        m_tablePanel->setVisible(false);
        rl->addWidget(m_tablePanel);
    }

    rl->addStretch();
    cl->addWidget(rightOuter);
    root->addWidget(content, 1);
}

// ── Snap helpers ──────────────────────────────────────────────────────────────

double MapEditWidget::hitTolScene(double screenPx) const
{
    // sp 坐标已经是场景坐标，但 view 会随滚轮缩放(scale())，场景单位与屏幕像素的
    // 比例并非 1:1。固定的场景容差在缩小地图后对应的屏幕像素会越来越少，导致难以
    // 点中。这里反向除以当前缩放比例，使命中范围始终保持约 screenPx 个屏幕像素。
    double s = m_mapView->transform().m11();
    return s > 1e-6 ? screenPx / s : screenPx;
}

MapEditWidget::SnapResult MapEditWidget::findSnap(const QPointF &sp, double pixelTol) const
{
    SnapResult best;
    double bestDist = hitTolScene(pixelTol);
    auto check = [&](const QJsonArray &arr, const QString &type) {
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            double wx = o.value("x").toDouble(), wy = o.value("y").toDouble();
            QPointF snap = worldToScene(wx, wy);
            double dist = std::hypot(sp.x()-snap.x(), sp.y()-snap.y());
            if (dist < bestDist) {
                bestDist = dist;
                best.valid    = true;
                best.type     = type;
                best.id       = o.value("id").toString();
                best.scenePos = snap;
                best.worldPos = {wx, wy};
            }
        }
    };
    check(m_points, "point");
    check(m_charges, "charge");
    check(m_highPrecisionPoints, "highPrecision");
    return best;
}

bool MapEditWidget::hasOverlap(const QPointF &sp) const
{
    const double kTol = 0.3;
    QPointF wc = sceneToWorld(sp.x(), sp.y());
    auto chk = [&](const QJsonArray &arr) {
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            if (std::hypot(o.value("x").toDouble()-wc.x(),
                           o.value("y").toDouble()-wc.y()) < kTol) return true;
        }
        return false;
    };
    return chk(m_points) || chk(m_charges) || chk(m_highPrecisionPoints);
}

void MapEditWidget::updateSnapIndicator(const SnapResult &r)
{
    if (m_snapIndicator) {
        m_scene->removeItem(m_snapIndicator);
        delete m_snapIndicator;
        m_snapIndicator = nullptr;
    }
    if (!r.valid) return;
    const double rad = 10.0;
    m_snapIndicator = m_scene->addEllipse(-rad,-rad,rad*2,rad*2,
                                          QPen(QColor("#00e676"),2.0),
                                          QBrush(QColor(0,230,118,28)));
    m_snapIndicator->setPos(r.scenePos);
    m_snapIndicator->setZValue(200);
}

// ── Grid & scale bar ──────────────────────────────────────────────────────────

void MapEditWidget::updateGrid()
{
    if (m_imgW <= 0 || m_imgH <= 0 || m_worldR <= m_worldL) return;

    const double gridStep = 1.0;  // 1 metre grid
    double xMin = m_worldL, xMax = m_worldR;
    double yMin = m_worldB, yMax = m_worldT;
    QPen gridPen (QColor(80, 110, 160,  80), 0.5);
    QPen axisGPen(QColor(120, 160, 240, 130), 0.8);

    for (double wx = std::ceil(xMin/gridStep)*gridStep; wx <= xMax+1e-9; wx += gridStep) {
        QPointF a = worldToScene(wx, yMin), b = worldToScene(wx, yMax);
        m_scene->addLine(a.x(),a.y(),b.x(),b.y(),
            std::abs(wx) < 1e-6 ? axisGPen : gridPen)->setZValue(-50);
    }
    for (double wy = std::ceil(yMin/gridStep)*gridStep; wy <= yMax+1e-9; wy += gridStep) {
        QPointF a = worldToScene(xMin, wy), b = worldToScene(xMax, wy);
        m_scene->addLine(a.x(),a.y(),b.x(),b.y(),
            std::abs(wy) < 1e-6 ? axisGPen : gridPen)->setZValue(-50);
    }

    // Origin axes indicator (matches MapViewWidget style)
    QPointF O = worldToScene(0.0, 0.0);
    const double arm = 52.0, aL = 11.0, aW = 4.5, lw = 1.8;
    const QColor xCol(210,45,45,230), yCol(30,185,60,230);

    auto mkArrow = [&](QPointF tip, double dx, double dy, QColor col) {
        double px=-dy, py=dx;
        QPolygonF tri;
        tri << tip
            << QPointF(tip.x()-dx*aL+px*aW, tip.y()-dy*aL+py*aW)
            << QPointF(tip.x()-dx*aL-px*aW, tip.y()-dy*aL-py*aW);
        m_scene->addPolygon(tri, QPen(Qt::NoPen), QBrush(col))->setZValue(12);
    };

    QPointF xTip(O.x()+arm, O.y());
    m_scene->addLine(O.x(),O.y(),xTip.x()-aL,O.y(), QPen(xCol,lw))->setZValue(10);
    mkArrow(xTip, 1.0, 0.0, xCol);

    QPointF yTip(O.x(), O.y()-arm);
    m_scene->addLine(O.x(),O.y(),O.x(),yTip.y()+aL, QPen(yCol,lw))->setZValue(10);
    mkArrow(yTip, 0.0, -1.0, yCol);
}

void MapEditWidget::updateRulers()
{
    if (m_topRuler)
        m_topRuler->syncWithView(m_mapView, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
    if (m_leftRuler)
        m_leftRuler->syncWithView(m_mapView, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
}

// ── Draw mode ─────────────────────────────────────────────────────────────────

void MapEditWidget::setDrawMode(int mode)
{
    cancelDraw();
    m_drawStatus = static_cast<DrawStatus>(mode);

    for (auto it = m_toolBtns.begin(); it != m_toolBtns.end(); ++it)
        if (it.value()) it.value()->setChecked(it.key() == mode);

    bool handles = (mode == Drag);
    for (auto &lst : m_curveHandles.values())
        for (auto *h : lst) h->setVisible(handles);

    if (mode == Drag) {
        m_mapView->setDragMode(QGraphicsView::ScrollHandDrag);
        m_mapView->viewport()->setCursor(Qt::OpenHandCursor);
    } else {
        m_mapView->setDragMode(QGraphicsView::NoDrag);
        m_mapView->viewport()->setCursor(Qt::CrossCursor);
    }
}

void MapEditWidget::cancelDraw()
{
    m_drawHasP1 = false;
    m_drawP1    = {};
    m_drawP1Id.clear();
    m_curveCtrlPts.clear();
    m_curveCtrlIds.clear();
    clearPreview();
    updateSnapIndicator({});
}

// ── Data loading ──────────────────────────────────────────────────────────────

void MapEditWidget::loadMapData()
{
    ApiService::instance().getMap(m_mapId, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonObject info = data.value("data").toObject(data);
        m_mapName = info.value("mapName").toString();
        m_mapInfoLabel->setText(QString("路网编辑  %1").arg(m_mapName));

        auto tryDbl = [&](const QStringList &keys, double def) {
            for (const QString &k : keys) {
                QJsonValue v = info.value(k);
                if (!v.isUndefined() && !v.isNull()) return v.toDouble(def);
            }
            return def;
        };
        double apiPhysW = tryDbl({"mapWidth","map_width","width"}, 0.0);
        double apiPhysH = tryDbl({"mapHeight","map_height","height"}, 0.0);

        // 与 Vue edit.vue 完全一致：永远用 -offsetX/-offsetY 计算零点，不优先采用
        // originX 字段（Vue 的 getMap 流程从不读取该字段）。仅用于 pgm/yaml 拉取失败时
        // 的兜底（thumbnail.png 方案）。
        auto getOrigin = [&](const QStringList &negate) -> double {
            for (const QString &k : negate) {
                QJsonValue v = info.value(k);
                if (!v.isUndefined() && !v.isNull()) return -v.toDouble();
            }
            return 0.0;
        };
        double apiOriginX = getOrigin({"offsetX","mapOffsetX","offset_x"});
        double apiOriginY = getOrigin({"offsetY","mapOffsetY","offset_y"});

        debugLog(QString("[loadMapData] mapId=%1 mapName=%2 api.physW=%3 api.physH=%4 api.originX=%5 api.originY=%6")
            .arg(m_mapId, m_mapName).arg(apiPhysW).arg(apiPhysH).arg(apiOriginX).arg(apiOriginY));
        debugLog(QString("[loadMapData] raw getMap info keys/values: offsetX=%1 offsetY=%2 mapWidth=%3 mapHeight=%4 originX(if any)=%5 originY(if any)=%6")
            .arg(info.value("offsetX").toVariant().toString(), info.value("offsetY").toVariant().toString(),
                 info.value("mapWidth").toVariant().toString(), info.value("mapHeight").toVariant().toString(),
                 info.value("originX").toVariant().toString(), info.value("originY").toVariant().toString()));

        QString mapHost     = AppConfig::instance().mapHost();
        QString tilemapPath = info.value("tilemapPath").toString();
        QString thumbUrl;
        if (!tilemapPath.isEmpty())
            thumbUrl = QString("%1%2_%3/%4/thumbnail.png").arg(mapHost, m_mapName, m_mapId, tilemapPath);

        // 兜底方案：thumbnail.png + getMap() 返回的 mapWidth/mapHeight/offsetX/offsetY。
        // 注意：后端生成瓦片金字塔时会把图片 padding 成 512*2^n 的正方形，这些字段是基于
        // padding 后画布派生的，并不是地图的真实物理尺寸与零点，因此只在 pgm/yaml 拉取失败
        // 时才使用（见下方主路径的说明）。
        auto useThumbnailFallback = [this, thumbUrl, apiOriginX, apiOriginY, apiPhysW, apiPhysH]() {
            debugLog(QString("[loadMapData] FALLBACK to thumbnail.png url=%1").arg(thumbUrl));
            if (thumbUrl.isEmpty()) return;
            m_worldL = apiOriginX;
            m_worldB = apiOriginY;
            m_worldR = apiOriginX + apiPhysW;
            m_worldT = apiOriginY + apiPhysH;
            ApiService::instance().downloadFile(thumbUrl, [this](bool ok, const QByteArray &raw) {
                debugLog(QString("[loadMapData] thumbnail download ok=%1 bytes=%2").arg(ok).arg(raw.size()));
                if (!ok) return;
                m_mapImage.loadFromData(raw);
                m_imgW = m_mapImage.width();
                m_imgH = m_mapImage.height();
                debugLog(QString("[loadMapData] FINAL(thumbnail) imgW=%1 imgH=%2 worldL=%3 worldB=%4 worldR=%5 worldT=%6")
                    .arg(m_imgW).arg(m_imgH).arg(m_worldL).arg(m_worldB).arg(m_worldR).arg(m_worldT));
                renderMap();
                QTimer::singleShot(0, this, [this]{ fitView(); });
            });
        };

        // 主路径：直接拉取并解析原始 carto_map.pgm + carto_map.yaml。
        // yaml 的 resolution/origin 是 ROS map_server 的标准、无歧义定义（origin = pgm
        // 左下角像素对应的世界坐标），pgm 本身也是未经 padding/缩放的原始尺寸——
        // 不依赖 getMap() 派生出的 mapWidth/mapHeight/offsetX/offsetY，从根本上避免了
        // padding 画布带来的零点与比例偏差。
        QString resBase = AppConfig::instance().agvMapResourceHost() + m_mapName + "_" + m_mapId + "/";
        QString yamlUrl = resBase + "carto_map.yaml";
        QString pgmUrl  = resBase + "carto_map.pgm";

        debugLog(QString("[loadMapData] trying pgm path: yamlUrl=%1 pgmUrl=%2").arg(yamlUrl, pgmUrl));
        ApiService::instance().downloadFile(yamlUrl,
            [this, pgmUrl, useThumbnailFallback](bool yamlOk, const QByteArray &yamlRaw) {
                debugLog(QString("[loadMapData] yaml download ok=%1 bytes=%2 content=[%3]")
                    .arg(yamlOk).arg(yamlRaw.size()).arg(QString::fromUtf8(yamlRaw).left(500)));
                if (!yamlOk) { useThumbnailFallback(); return; }
                double resolution = 0, originX = 0, originY = 0; bool negate = false;
                bool parsed = MapPgmUtil::parseMapYaml(QString::fromUtf8(yamlRaw), resolution, originX, originY, negate);
                debugLog(QString("[loadMapData] yaml parsed=%1 resolution=%2 originX=%3 originY=%4 negate=%5")
                    .arg(parsed).arg(resolution).arg(originX).arg(originY).arg(negate));
                if (!parsed || resolution <= 0) {
                    useThumbnailFallback(); return;
                }
                ApiService::instance().downloadFile(pgmUrl,
                    [this, resolution, originX, originY, negate, useThumbnailFallback]
                    (bool pgmOk, const QByteArray &pgmRaw) {
                        debugLog(QString("[loadMapData] pgm download ok=%1 bytes=%2").arg(pgmOk).arg(pgmRaw.size()));
                        if (!pgmOk) { useThumbnailFallback(); return; }
                        QImage img = MapPgmUtil::pgmToImage(pgmRaw, negate);
                        debugLog(QString("[loadMapData] pgm parsed image isNull=%1 w=%2 h=%3")
                            .arg(img.isNull()).arg(img.width()).arg(img.height()));
                        if (img.isNull()) { useThumbnailFallback(); return; }

                        m_mapImage = img;
                        m_imgW = img.width();
                        m_imgH = img.height();
                        // ROS map_server 标准：yaml 的 origin 本身就是 pgm 左下角像素对应的
                        // 世界坐标，不需要再取负——那是 API 的 offsetX/Y 字段才需要的换算。
                        m_worldL = originX;
                        m_worldB = originY;
                        m_worldR = m_worldL + img.width()  * resolution;
                        m_worldT = m_worldB + img.height() * resolution;
                        debugLog(QString("[loadMapData] FINAL(pgm) imgW=%1 imgH=%2 worldL=%3 worldB=%4 worldR=%5 worldT=%6")
                            .arg(m_imgW).arg(m_imgH).arg(m_worldL).arg(m_worldB).arg(m_worldR).arg(m_worldT));
                        renderMap();
                        QTimer::singleShot(0, this, [this]{ fitView(); });
                    });
            });
    });
}

void MapEditWidget::loadRouteData()
{
    QJsonObject query;
    query["mapId"] = m_mapId;
    ApiService::instance().loadEditedMap(query, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QString jsonStr = data.value("json").toString();
        if (jsonStr.isEmpty()) {
            renderMap();
            QTimer::singleShot(0, this, [this]{ fitView(); });
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (!doc.isObject()) return;
        QJsonObject root = doc.object();

        // 兼容旧版（修复前）保存的扁平格式: {points:[...], lines:[...], curves:[...], ...}
        // 而不是标准 GeoJSON FeatureCollection
        if (!root.contains("features") && (root.contains("points") || root.contains("lines") || root.contains("curves"))) {
            m_points              = root.value("points").toArray();
            m_lines               = root.value("lines").toArray();
            m_curves              = root.value("curves").toArray();
            m_charges             = root.value("charges").toArray();
            m_highPrecisionPoints = root.value("highPrecisionPoints").toArray();
            renderMap();
            QTimer::singleShot(0, this, [this]{ fitView(); });
            if (m_tableVisible) refreshTable();
            return;
        }

        QJsonArray features = root.value("features").toArray();

        m_points              = QJsonArray();
        m_lines               = QJsonArray();
        m_curves              = QJsonArray();
        m_charges             = QJsonArray();
        m_highPrecisionPoints = QJsonArray();

        for (const auto &fv : features) {
            QJsonObject f      = fv.toObject();
            QJsonObject props  = f.value("properties").toObject();
            QJsonObject geom   = f.value("geometry").toObject();
            QString     gtype  = geom.value("type").toString();
            QString     id     = props.value("id").toString();
            QJsonArray  coords = geom.value("coordinates").toArray();

            if (gtype == "Point") {
                double x = coords[0].toDouble(), y = coords[1].toDouble();
                QJsonObject obj = props;
                obj["x"] = x; obj["y"] = y;
                if (id.startsWith("c_"))
                    m_charges.append(obj);
                else if (id.startsWith("hp_"))
                    m_highPrecisionPoints.append(obj);
                else
                    m_points.append(obj);
            } else if (gtype == "LineString") {
                QJsonObject obj = props;
                if (coords.size() >= 2) {
                    QJsonArray c0 = coords.first().toArray();
                    QJsonArray c1 = coords.last().toArray();
                    obj["x1"] = c0[0].toDouble(); obj["y1"] = c0[1].toDouble();
                    obj["x2"] = c1[0].toDouble(); obj["y2"] = c1[1].toDouble();
                }
                m_lines.append(obj);
            } else if (gtype == "MultiPoint") {
                QJsonObject obj = props;
                if (coords.size() >= 3) {
                    QJsonArray c0 = coords[0].toArray();
                    QJsonArray c1 = coords[1].toArray();
                    QJsonArray c2 = coords[2].toArray();
                    obj["x1"] = c0[0].toDouble(); obj["y1"] = c0[1].toDouble();
                    obj["cx"] = c1[0].toDouble(); obj["cy"] = c1[1].toDouble();
                    obj["x2"] = c2[0].toDouble(); obj["y2"] = c2[1].toDouble();
                }
                m_curves.append(obj);
            }
        }
        renderMap();
        QTimer::singleShot(0, this, [this]{ fitView(); });
        if (m_tableVisible) refreshTable();
    });
}

// 反光柱：标准 GeoJSON FeatureCollection（与 Vue edit.vue 的
// loadReflectivePillarEditedMap/loadReflectivePillarFeaturesFromJson 同源数据）
void MapEditWidget::loadReflectorData()
{
    ApiService::instance().getReflectorColumnInfo(m_mapId, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonObject geo = data.value("data").toObject(data);
        m_reflectorPoles = geo.value("features").toArray();
        if (m_showReflectors) renderMap();
    });
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void MapEditWidget::renderMap()
{
    m_scene->clear();
    m_featureItems.clear();
    m_curveHandles.clear();
    m_previewItem   = nullptr;
    m_selItem       = nullptr;
    m_snapIndicator = nullptr;   // scene cleared
    m_robotItem     = nullptr;  // scene cleared

    if (!m_mapImage.isNull()) {
        auto *bg = m_scene->addPixmap(QPixmap::fromImage(m_mapImage));
        bg->setZValue(-100);
    }

    auto addNode = [&](const QJsonObject &pt, const QColor &col, int r, const QString &type, bool visible = true) {
        QPointF sp = worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble());
        QString id = pt.value("id").toString();
        auto *e = m_scene->addEllipse(-r, -r, r*2, r*2,
                                       QPen(col.darker(130), 1.5), QBrush(col));
        e->setPos(sp);
        e->setZValue(10);
        e->setData(0, type); e->setData(1, id);
        e->setVisible(visible);
        m_featureItems[type + "_" + id] = e;
        auto *lbl = m_scene->addText(pt.value("name").toString());
        lbl->setDefaultTextColor(QColor(210, 225, 240));
        lbl->setZValue(11);
        QFont f = lbl->font(); f.setPointSizeF(7); lbl->setFont(f);
        lbl->setPos(sp.x() + r + 2, sp.y() - 7);
        lbl->setVisible(visible);
    };

    for (const auto &v : m_points)  addNode(v.toObject(), QColor("#4a9fd4"), 5, "point");
    for (const auto &v : m_charges) addNode(v.toObject(), QColor("#e8a020"), 7, "charge");
    // 高精度点：按需求隐藏绘制，但仍保留 scene item 以维持选中/删除/表格等既有逻辑
    for (const auto &v : m_highPrecisionPoints) addNode(v.toObject(), QColor("#e05050"), 4, "highPrecision", false);

    for (const auto &v : m_lines) {
        QJsonObject ln = v.toObject();
        QPointF a = worldToScene(ln.value("x1").toDouble(), ln.value("y1").toDouble());
        QPointF b = worldToScene(ln.value("x2").toDouble(), ln.value("y2").toDouble());
        QString id     = ln.value("id").toString();
        int direct     = ln.value("direct").toInt(100);
        QPen pen(direct == 10 ? QColor("#2ecc71") : QColor("#1abc9c"), 1.8);
        auto *li = m_scene->addLine(a.x(), a.y(), b.x(), b.y(), pen);
        li->setZValue(5); li->setData(0, "line"); li->setData(1, id);
        m_featureItems["line_" + id] = li;
        // Direction arrow at midpoint
        QPointF mid = (a + b) / 2, dir = b - a;
        double len = std::hypot(dir.x(), dir.y());
        if (len > 1e-3) {
            dir /= len;
            QPointF n(-dir.y(), dir.x());
            QPolygonF arr; arr << mid+dir*7 << mid-dir*7+n*4 << mid-dir*7-n*4;
            m_scene->addPolygon(arr, QPen(Qt::NoPen), QBrush(pen.color()))->setZValue(6);
        }
    }

    for (const auto &v : m_curves) {
        QJsonObject cv = v.toObject();
        QString id     = cv.value("id").toString();
        int direct     = cv.value("direct").toInt(100);
        QPointF p1 = worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble());
        QPointF pc = worldToScene(cv.value("cx").toDouble(),  cv.value("cy").toDouble());
        QPointF p2 = worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble());
        QPainterPath path; path.moveTo(p1); path.quadTo(pc, p2);
        QPen pen(direct == 10 ? QColor("#a29bfe") : QColor("#6c5ce7"), 1.8);
        auto *pi = m_scene->addPath(path, pen);
        pi->setZValue(5); pi->setData(0, "curve"); pi->setData(1, id);
        m_featureItems["curve_" + id] = pi;
        rebuildCurveHandles(id);
    }

    // 反光柱：开关控制显示，标准 GeoJSON Feature 数组，geometry.coordinates=[x,y]
    if (m_showReflectors) {
        for (const auto &v : m_reflectorPoles) {
            QJsonObject f = v.toObject();
            QJsonArray coords = f.value("geometry").toObject().value("coordinates").toArray();
            if (coords.size() < 2) continue;
            QPointF sp = worldToScene(coords[0].toDouble(), coords[1].toDouble());
            auto *e = m_scene->addEllipse(-5, -5, 10, 10,
                QPen(QColor("#caa020"), 1.5), QBrush(QColor("#f0c040")));
            e->setPos(sp);
            e->setZValue(8);
        }
    }

    if (m_showGrid) updateGrid();
    m_scene->setSceneRect(0, 0, m_imgW, m_imgH);
    updateRulers();

    // 实时设备位置标记：三角形默认朝北(0,-10)，与 MapViewWidget 机器人图标约定一致
    QPolygonF robotPoly;
    robotPoly << QPointF(0, -10) << QPointF(-6, 6) << QPointF(6, 6);
    m_robotItem = m_scene->addPolygon(robotPoly,
        QPen(QColor(10, 90, 180), 1.2), QBrush(QColor(30, 144, 255)));
    m_robotItem->setZValue(50);
    m_robotItem->setVisible(false);
    updateRobotMarker();

    // Handle visibility matches current mode
    bool showH = (m_drawStatus == Drag);
    for (auto &lst : m_curveHandles.values())
        for (auto *h : lst) h->setVisible(showH);
}

void MapEditWidget::fitView()
{
    QRectF r = m_scene->sceneRect();
    if (!r.isEmpty()) {
        m_mapView->fitInView(r, Qt::KeepAspectRatio);
        m_hasInitialFit = true;
        updateRulers();
    }
}

void MapEditWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (!m_hasInitialFit)
        QTimer::singleShot(0, this, [this]{ fitView(); });
}

// ── Bezier handle management ──────────────────────────────────────────────────

void MapEditWidget::rebuildCurveHandles(const QString &id)
{
    // Remove stale handles
    if (m_curveHandles.contains(id)) {
        for (auto *h : m_curveHandles[id]) { m_scene->removeItem(h); delete h; }
        m_curveHandles.remove(id);
    }
    QJsonObject cv;
    for (const auto &v : m_curves) {
        auto o = v.toObject();
        if (o.value("id").toString() == id) { cv = o; break; }
    }
    if (cv.isEmpty()) return;

    QPointF pts[3] = {
        worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble()),
        worldToScene(cv.value("cx").toDouble(),  cv.value("cy").toDouble()),
        worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble()),
    };
    // Endpoint handles: blue; control handle: orange
    QColor cols[3] = { QColor("#4a9fd4"), QColor("#f39c12"), QColor("#4a9fd4") };
    QList<QGraphicsEllipseItem*> handles;

    for (int i = 0; i < 3; ++i) {
        auto *h = m_scene->addEllipse(-5, -5, 10, 10,
                                       QPen(cols[i].darker(140), 1.5),
                                       QBrush(cols[i].lighter(130)));
        h->setPos(pts[i]);
        h->setZValue(50);
        h->setData(Qt::UserRole,   id);
        h->setData(Qt::UserRole+1, i);
        h->setVisible(m_drawStatus == Drag);
        handles.append(h);
    }
    // Control arms (dotted lines from endpoints to control point)
    auto arm = QPen(QColor(240,180,50,100), 1, Qt::DotLine);
    m_scene->addLine(pts[0].x(), pts[0].y(), pts[1].x(), pts[1].y(), arm)->setZValue(49);
    m_scene->addLine(pts[1].x(), pts[1].y(), pts[2].x(), pts[2].y(), arm)->setZValue(49);

    m_curveHandles[id] = handles;
}

void MapEditWidget::removeAllHandles()
{
    for (auto &lst : m_curveHandles.values())
        for (auto *h : lst) { m_scene->removeItem(h); delete h; }
    m_curveHandles.clear();
}

void MapEditWidget::updateCurveShape(const QString &id)
{
    auto it = m_curveHandles.find(id);
    if (it == m_curveHandles.end() || it.value().size() < 2) return;
    // 支持任意阶贝塞尔曲线: 收集所有控制点位置
    QList<QPointF> pts;
    for (auto *h : it.value()) pts.append(h->pos());
    QPainterPath path = MapPgmUtil::getBezierPath(pts);
    auto *pi = qgraphicsitem_cast<QGraphicsPathItem*>(m_featureItems.value("curve_" + id));
    if (pi) pi->setPath(path);
}

// ── Preview rubber-band ───────────────────────────────────────────────────────

void MapEditWidget::updatePreview(const QPointF &sp)
{
    clearPreview();
    if (m_drawStatus == Curve && !m_curveCtrlPts.isEmpty()) {
        // 多阶贝塞尔曲线预览: 已添加控制点 + 鼠标当前位置作为最后一个控制点
        QList<QPointF> pts = m_curveCtrlPts;
        pts.append(sp);
        if (pts.size() >= 2) {
            QPainterPath path = MapPgmUtil::getBezierPath(pts);
            QPen pen(QColor(108, 92, 231, 200), 1.6, Qt::DashLine);
            m_previewItem = m_scene->addPath(path, pen);
            m_previewItem->setZValue(100);
            // 显示控制点连线(辅助参考线)
            QPen aidPen(QColor(250, 190, 50, 120), 1.0, Qt::DotLine);
            for (int i = 0; i + 1 < pts.size(); ++i) {
                auto *ln = m_scene->addLine(pts[i].x(), pts[i].y(), pts[i+1].x(), pts[i+1].y(), aidPen);
                ln->setZValue(99);
                m_previewAids.append(ln);
            }
        }
        return;
    }
    if (!m_drawHasP1) return;
    if (m_drawStatus == Line || m_drawStatus == Curve) {
        QPen pen(QColor(250, 190, 50, 170), 1.4, Qt::DashLine);
        m_previewItem = m_scene->addLine(m_drawP1.x(), m_drawP1.y(), sp.x(), sp.y(), pen);
        m_previewItem->setZValue(100);
    }
}

void MapEditWidget::clearPreview()
{
    if (m_previewItem) { m_scene->removeItem(m_previewItem); delete m_previewItem; m_previewItem = nullptr; }
    for (auto *ln : m_previewAids) { m_scene->removeItem(ln); delete ln; }
    m_previewAids.clear();
}

// ── Feature addition ──────────────────────────────────────────────────────────

void MapEditWidget::addPoint(const QPointF &sp)
{
    QPointF w = sceneToWorld(sp.x(), sp.y());
    int n = m_points.size() + 1;
    QJsonObject pt;
    pt["id"] = QString("p_%1").arg(n); pt["name"] = QString("P%1").arg(n);
    pt["x"] = w.x(); pt["y"] = w.y();
    m_points.append(pt);
    renderMap();
    if (m_tableVisible && m_currentTable == "point") refreshTable();
}

void MapEditWidget::addCharge(const QPointF &sp)
{
    QPointF w = sceneToWorld(sp.x(), sp.y());
    int n = m_charges.size() + 1;
    QJsonObject ch;
    ch["id"] = QString("c_%1").arg(n); ch["name"] = QString("充电%1").arg(n);
    ch["x"] = w.x(); ch["y"] = w.y();
    m_charges.append(ch);
    renderMap();
    if (m_tableVisible && m_currentTable == "charge") refreshTable();
}

void MapEditWidget::addHighPrecisionPoint(const QPointF &sp)
{
    QPointF w = sceneToWorld(sp.x(), sp.y());
    int n = m_highPrecisionPoints.size() + 1;
    QJsonObject hp;
    hp["id"] = QString("hp_%1").arg(n); hp["name"] = QString("高精%1").arg(n);
    hp["x"] = w.x(); hp["y"] = w.y();
    m_highPrecisionPoints.append(hp);
    renderMap();
    if (m_tableVisible && m_currentTable == "highPrecision") refreshTable();
}

void MapEditWidget::addLine(const QPointF &a, const QPointF &b,
                            const QString &startId, const QString &endId)
{
    QPointF wa = sceneToWorld(a.x(), a.y()), wb = sceneToWorld(b.x(), b.y());
    int n = m_lines.size() + 1;
    QJsonObject ln;
    ln["id"] = QString("l_%1").arg(n); ln["name"] = QString("线%1").arg(n);
    ln["x1"] = wa.x(); ln["y1"] = wa.y(); ln["x2"] = wb.x(); ln["y2"] = wb.y();
    ln["direct"] = 100; ln["startid"] = startId; ln["endid"] = endId;
    m_lines.append(ln);
    renderMap();
    if (m_tableVisible && m_currentTable == "line") refreshTable();
}

void MapEditWidget::addCurve(const QPointF &a, const QPointF &b,
                             const QString &startId, const QString &endId)
{
    QPointF mid  = (a + b) / 2;
    QPointF wa   = sceneToWorld(a.x(), a.y());
    QPointF wc   = sceneToWorld(mid.x(), mid.y());
    QPointF wb   = sceneToWorld(b.x(), b.y());
    int n = m_curves.size() + 1;
    QJsonObject cv;
    cv["id"] = QString("b_%1").arg(n); cv["name"] = QString("曲%1").arg(n);
    cv["x1"] = wa.x(); cv["y1"] = wa.y();
    cv["cx"] = wc.x(); cv["cy"] = wc.y();
    cv["x2"] = wb.x(); cv["y2"] = wb.y();
    cv["direct"] = 100; cv["startid"] = startId; cv["endid"] = endId;
    m_curves.append(cv);
    renderMap();
    if (m_tableVisible && m_currentTable == "curve") refreshTable();
    // Stay in Curve mode for continuous drawing; right-click → Drag to adjust handles
}

void MapEditWidget::finalizeCurve()
{
    if (m_curveCtrlPts.size() < 2) {
        cancelDraw();
        return;
    }
    addBezierCurve(m_curveCtrlPts, m_curveCtrlIds);
    // 清理绘制状态，保持在 Curve 模式以便连续绘制
    m_curveCtrlPts.clear();
    m_curveCtrlIds.clear();
    m_drawHasP1 = false;
    m_drawP1 = {};
    m_drawP1Id.clear();
    clearPreview();
}

void MapEditWidget::addBezierCurve(const QList<QPointF> &scenePts,
                                   const QList<QString> &snapIds)
{
    if (scenePts.size() < 2) return;
    int n = m_curves.size() + 1;
    QJsonObject cv;
    cv["id"]   = QString("b_%1").arg(n);
    cv["name"] = QString("曲%1").arg(n);
    cv["direct"] = 100;

    // 起终点(世界坐标)
    QPointF wStart = sceneToWorld(scenePts.first().x(), scenePts.first().y());
    QPointF wEnd   = sceneToWorld(scenePts.last().x(),  scenePts.last().y());
    cv["x1"] = wStart.x(); cv["y1"] = wStart.y();
    cv["x2"] = wEnd.x();   cv["y2"] = wEnd.y();
    cv["startid"] = snapIds.isEmpty()     ? QString() : snapIds.first();
    cv["endid"]   = (snapIds.size() >= 2) ? snapIds.last() : QString();

    // 控制点数组(世界坐标)
    QJsonArray ctrlArr;
    for (const QPointF &sp : scenePts) {
        QPointF wp = sceneToWorld(sp.x(), sp.y());
        QJsonObject pt;
        pt["x"] = wp.x();
        pt["y"] = wp.y();
        ctrlArr.append(pt);
    }
    cv["controlPoints"] = ctrlArr;

    // 兼容旧字段(二次/三次贝塞尔)
    if (scenePts.size() == 3) {
        QPointF wc = sceneToWorld(scenePts[1].x(), scenePts[1].y());
        cv["cx"] = wc.x(); cv["cy"] = wc.y();
    } else if (scenePts.size() == 4) {
        QPointF c1 = sceneToWorld(scenePts[1].x(), scenePts[1].y());
        QPointF c2 = sceneToWorld(scenePts[2].x(), scenePts[2].y());
        cv["cx1"] = c1.x(); cv["cy1"] = c1.y();
        cv["cx2"] = c2.x(); cv["cy2"] = c2.y();
    }

    m_curves.append(cv);
    renderMap();
    if (m_tableVisible && m_currentTable == "curve") refreshTable();
}

// ── Feature selection ─────────────────────────────────────────────────────────

namespace {
const QColor kSelectColor("#00e676");   // 选中态：醒目的亮绿色，与既有配色都不重复

// 记录在 data(2) 里的原始颜色（用 setItemColor 改色前先取出保存，取消选中时还原）
QColor itemOrigColor(QGraphicsItem *item)
{
    if (auto *e = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) return e->brush().color();
    if (auto *l = qgraphicsitem_cast<QGraphicsLineItem*>(item))    return l->pen().color();
    if (auto *p = qgraphicsitem_cast<QGraphicsPathItem*>(item))    return p->pen().color();
    return {};
}

void setItemColor(QGraphicsItem *item, const QColor &col, bool selected)
{
    if (auto *e = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
        QPen pen = e->pen(); pen.setColor(col.darker(130)); pen.setWidthF(selected ? 2.4 : 1.5);
        e->setPen(pen);
        e->setBrush(col);
    } else if (auto *l = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
        QPen pen = l->pen(); pen.setColor(col); pen.setWidthF(selected ? 3.2 : 1.8);
        l->setPen(pen);
    } else if (auto *p = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        QPen pen = p->pen(); pen.setColor(col); pen.setWidthF(selected ? 3.2 : 1.8);
        p->setPen(pen);
    }
}

// 选中变色：先把当前(原始)颜色缓存到 data(2)，再统一改为高亮绿；取消选中时按
// data(2) 还原，恢复点/线/曲线各自原本的配色（蓝/橙/红/绿/紫等）。
void applySelectColor(QGraphicsItem *item)
{
    if (!item) return;
    if (!item->data(2).isValid()) item->setData(2, itemOrigColor(item));
    setItemColor(item, kSelectColor, true);
}

void clearSelectColor(QGraphicsItem *item)
{
    if (!item) return;
    QColor orig = item->data(2).value<QColor>();
    if (orig.isValid()) setItemColor(item, orig, false);
}
} // namespace

void MapEditWidget::selectFeatureAt(const QPointF &sp)
{
    const double tol = hitTolScene(12.0);
    for (auto *it : m_scene->items(QRectF(sp.x()-tol, sp.y()-tol, tol*2, tol*2))) {
        QString type = it->data(0).toString();
        QString id   = it->data(1).toString();
        if (!type.isEmpty()) { showFeatureProperties(type, id); return; }
    }
    clearSelection();
}

void MapEditWidget::clearSelection()
{
    clearSelectColor(m_selItem);
    m_selType.clear(); m_selId.clear(); m_selItem = nullptr;
    m_propsPanel->setVisible(false);
}

void MapEditWidget::showFeatureProperties(const QString &type, const QString &id)
{
    auto find = [&](const QJsonArray &arr) {
        for (const auto &v : arr)
            if (v.toObject().value("id").toString() == id) return v.toObject();
        return QJsonObject{};
    };
    QJsonObject obj;
    if      (type == "point")         obj = find(m_points);
    else if (type == "line")          obj = find(m_lines);
    else if (type == "curve")         obj = find(m_curves);
    else if (type == "charge")        obj = find(m_charges);
    else if (type == "highPrecision") obj = find(m_highPrecisionPoints);
    if (obj.isEmpty()) return;

    m_selType = type; m_selId = id;
    QGraphicsItem *newSel = m_featureItems.value(type + "_" + id);
    if (m_selItem && m_selItem != newSel) clearSelectColor(m_selItem);
    m_selItem = newSel;
    applySelectColor(m_selItem);

    static const QMap<QString,QString> names = {
        {"point","普通节点"},{"line","线段"},{"curve","贝塞尔曲线"},
        {"charge","充电桩"},{"highPrecision","高精度节点"}
    };
    m_propTypeLabel->setText("类型: " + names.value(type, type));
    m_propIdEdit->setText(id);
    m_propNameEdit->setText(obj.value("name").toString());
    bool hasDir = (type == "line" || type == "curve");
    m_propDirectRow->setVisible(hasDir);
    if (hasDir) m_propDirectCombo->setCurrentIndex(obj.value("direct").toInt(100) == 10 ? 1 : 0);
    m_propsPanel->setVisible(true);

    if (m_tableVisible) {
        int idx = m_tableTypeCombo->findData(type);
        if (idx >= 0 && m_tableTypeCombo->currentIndex() != idx)
            m_tableTypeCombo->setCurrentIndex(idx);
        for (int r = 0; r < m_featureTable->rowCount(); ++r) {
            auto *it = m_featureTable->item(r, 0);
            if (it && it->text() == id) { m_featureTable->selectRow(r); break; }
        }
    }
}

void MapEditWidget::applyPropsEdit()
{
    if (m_selType.isEmpty()) return;
    QString newName = m_propNameEdit->text().trimmed();
    int newDirect   = m_propDirectCombo->currentData().toInt();
    auto upd = [&](QJsonArray &arr) {
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject o = arr[i].toObject();
            if (o.value("id").toString() != m_selId) continue;
            o["name"] = newName;
            if (m_selType == "line" || m_selType == "curve") o["direct"] = newDirect;
            arr[i] = o; break;
        }
    };
    if      (m_selType == "point")         upd(m_points);
    else if (m_selType == "line")          upd(m_lines);
    else if (m_selType == "curve")         upd(m_curves);
    else if (m_selType == "charge")        upd(m_charges);
    else if (m_selType == "highPrecision") upd(m_highPrecisionPoints);
    renderMap();
    showFeatureProperties(m_selType, m_selId);
    if (m_tableVisible) refreshTable();
}

void MapEditWidget::deleteSelected()
{
    if (m_selType.isEmpty()) return;
    auto del = [&](QJsonArray &arr) {
        for (int i = 0; i < arr.size(); ++i)
            if (arr[i].toObject().value("id").toString() == m_selId) { arr.removeAt(i); return; }
    };
    if      (m_selType == "point")         del(m_points);
    else if (m_selType == "line")          del(m_lines);
    else if (m_selType == "curve")         del(m_curves);
    else if (m_selType == "charge")        del(m_charges);
    else if (m_selType == "highPrecision") del(m_highPrecisionPoints);
    clearSelection();
    renderMap();
    if (m_tableVisible) refreshTable();
}

// ── Table ─────────────────────────────────────────────────────────────────────

void MapEditWidget::onTableToggle()
{
    m_tableVisible = m_tableModeBtn->isChecked();
    m_tablePanel->setVisible(m_tableVisible);
    if (m_tableVisible) refreshTable();
}

void MapEditWidget::onDeleteFeature()
{
    int row = m_featureTable->currentRow();
    if (row < 0) return;
    auto *it = m_featureTable->item(row, 0);
    if (!it) return;
    QString id = it->text();
    auto del = [&](QJsonArray &arr) {
        for (int i = 0; i < arr.size(); ++i)
            if (arr[i].toObject().value("id").toString() == id) { arr.removeAt(i); return; }
    };
    if      (m_currentTable == "point")         del(m_points);
    else if (m_currentTable == "line")          del(m_lines);
    else if (m_currentTable == "curve")         del(m_curves);
    else if (m_currentTable == "charge")        del(m_charges);
    else if (m_currentTable == "highPrecision") del(m_highPrecisionPoints);
    renderMap();
    refreshTable();
}

void MapEditWidget::refreshTable()
{
    const QJsonArray *arr = nullptr;
    if      (m_currentTable == "point")         arr = &m_points;
    else if (m_currentTable == "line")          arr = &m_lines;
    else if (m_currentTable == "curve")         arr = &m_curves;
    else if (m_currentTable == "charge")        arr = &m_charges;
    else if (m_currentTable == "highPrecision") arr = &m_highPrecisionPoints;
    if (!arr) return;
    m_featureTable->setRowCount(arr->size());
    for (int i = 0; i < arr->size(); ++i) {
        QJsonObject o = (*arr)[i].toObject();
        m_featureTable->setItem(i, 0, new QTableWidgetItem(o.value("id").toString()));
        m_featureTable->setItem(i, 1, new QTableWidgetItem(o.value("name").toString()));
    }
}

// ── Save / Edit map ───────────────────────────────────────────────────────────

void MapEditWidget::onSaveRoute()
{
    // Flush any dragged handle positions to JSON
    for (auto it = m_curveHandles.begin(); it != m_curveHandles.end(); ++it) {
        auto &lst = it.value();
        if (lst.size() < 3) continue;
        const QString &id = it.key();
        for (int i = 0; i < m_curves.size(); ++i) {
            QJsonObject cv = m_curves[i].toObject();
            if (cv.value("id").toString() != id) continue;
            QPointF w1 = sceneToWorld(lst[0]->pos().x(), lst[0]->pos().y());
            QPointF wc = sceneToWorld(lst[1]->pos().x(), lst[1]->pos().y());
            QPointF w2 = sceneToWorld(lst[2]->pos().x(), lst[2]->pos().y());
            cv["x1"]=w1.x(); cv["y1"]=w1.y(); cv["cx"]=wc.x();
            cv["cy"]=wc.y(); cv["x2"]=w2.x(); cv["y2"]=w2.y();
            m_curves[i] = cv; break;
        }
    }

    // Build GeoJSON FeatureCollection (same format as Vue exportFeaturesObj)
    QJsonArray features;

    auto makePointFeat = [](const QJsonObject &obj) -> QJsonObject {
        QJsonArray coords;
        coords.append(obj.value("x").toDouble());
        coords.append(obj.value("y").toDouble());
        coords.append(0.0);
        QJsonObject geom; geom["type"] = "Point"; geom["coordinates"] = coords;
        QJsonObject props = obj; props.remove("x"); props.remove("y");
        QJsonObject f; f["type"] = "Feature"; f["geometry"] = geom; f["properties"] = props;
        return f;
    };

    for (const auto &v : m_points)              features.append(makePointFeat(v.toObject()));
    for (const auto &v : m_charges)             features.append(makePointFeat(v.toObject()));
    for (const auto &v : m_highPrecisionPoints) features.append(makePointFeat(v.toObject()));

    for (const auto &v : m_lines) {
        QJsonObject ln = v.toObject();
        QJsonArray ca, cb;
        ca.append(ln.value("x1").toDouble()); ca.append(ln.value("y1").toDouble());
        cb.append(ln.value("x2").toDouble()); cb.append(ln.value("y2").toDouble());
        QJsonArray coords; coords.append(ca); coords.append(cb);
        QJsonObject geom; geom["type"] = "LineString"; geom["coordinates"] = coords;
        QJsonObject props = ln;
        props.remove("x1"); props.remove("y1"); props.remove("x2"); props.remove("y2");
        QJsonObject f; f["type"] = "Feature"; f["geometry"] = geom; f["properties"] = props;
        features.append(f);
    }

    for (const auto &v : m_curves) {
        QJsonObject cv = v.toObject();
        QJsonArray c0, c1, c2;
        c0.append(cv.value("x1").toDouble()); c0.append(cv.value("y1").toDouble());
        c1.append(cv.value("cx").toDouble()); c1.append(cv.value("cy").toDouble());
        c2.append(cv.value("x2").toDouble()); c2.append(cv.value("y2").toDouble());
        QJsonArray coords; coords.append(c0); coords.append(c1); coords.append(c2);
        QJsonObject geom; geom["type"] = "MultiPoint"; geom["coordinates"] = coords;
        QJsonObject props = cv;
        props.remove("x1"); props.remove("y1"); props.remove("cx"); props.remove("cy");
        props.remove("x2"); props.remove("y2");
        QJsonObject f; f["type"] = "Feature"; f["geometry"] = geom; f["properties"] = props;
        features.append(f);
    }

    QJsonObject geojson;
    geojson["type"]     = "FeatureCollection";
    geojson["features"] = features;
    geojson["mapId"]    = m_mapId;

    m_saveBtn->setEnabled(false);
    auto *loading = new LoadingDialog("正在保存路网...", this);
    loading->show();
    ApiService::instance().saveEditedMap(geojson, [this, loading](int code, const QJsonObject &) {
        loading->close(); loading->deleteLater();
        m_saveBtn->setEnabled(true);
        if (code == 200) Toast::success(this, "路网保存成功");
        else QMessageBox::warning(this, "错误", "保存失败，请检查网络");
    });
}

void MapEditWidget::onEditMap()
{
    ApiService::instance().getMap(m_mapId, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonObject info = data.value("data").toObject(data);
        QDialog dlg(this);
        dlg.setWindowTitle("编辑地图属性");
        dlg.setMinimumWidth(300);
        auto *form = new QFormLayout(&dlg);
        auto *nameEdit   = new QLineEdit(info.value("mapName").toString());
        auto *remarkEdit = new QLineEdit(info.value("remark").toString());
        form->addRow("地图名称:", nameEdit);
        form->addRow("描述:",     remarkEdit);
        auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        form->addRow(btns);
        if (dlg.exec() != QDialog::Accepted) return;
        QJsonObject upd;
        upd["mapId"]  = m_mapId;
        upd["mapName"] = nameEdit->text().trimmed();
        upd["remark"]  = remarkEdit->text().trimmed();
        auto *loading = new LoadingDialog("正在保存...", this);
        loading->show();
        ApiService::instance().updateMap(upd, [this, loading, upd](int code2, const QJsonObject &) {
            loading->close(); loading->deleteLater();
            if (code2 == 200) {
                m_mapName = upd.value("mapName").toString();
                m_mapInfoLabel->setText(QString("路网编辑  %1").arg(m_mapName));
                Toast::success(this, "保存成功");
            } else {
                QMessageBox::warning(this, "错误", "保存失败");
            }
        });
    });
}

// ── Event filter ──────────────────────────────────────────────────────────────

bool MapEditWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != m_mapView->viewport())
        return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent*>(event);
        double f = we->angleDelta().y() > 0 ? 1.18 : 1.0/1.18;
        m_mapView->scale(f, f);
        updateRulers();
        return true;
    }

    if (event->type() == QEvent::MouseMove) {
        auto *me = static_cast<QMouseEvent*>(event);
        QPointF sp = m_mapView->mapToScene(me->pos());
        updateCoordLabel(sp);

        if (m_isDraggingHandle && m_activeHandle) {
            // 端点手柄(0=起点, 2=终点)拖拽时吸附到附近已有的点，使曲线端点与
            // 该点共用同一坐标；控制点手柄(1)不吸附，仅跟随鼠标调整曲率。
            if (m_handleIndex == 0 || m_handleIndex == 2) {
                SnapResult snap = findSnap(sp);
                updateSnapIndicator(snap);
                m_activeHandle->setPos(snap.valid ? snap.scenePos : sp);
            } else {
                m_activeHandle->setPos(sp);
            }
            updateCurveShape(m_handleCurveId);
            return true;
        }
        if (m_drawStatus != Drag) {
            // Show snap indicator in draw modes
            SnapResult snap = findSnap(sp);
            updateSnapIndicator(snap);
            updatePreview(snap.valid ? snap.scenePos : sp);
        } else {
            // Drag mode: hover cursor on handles
            updateSnapIndicator({});
            const double hoverTol = hitTolScene(10.0);
            for (auto &lst : m_curveHandles.values())
                for (auto *h : lst)
                    if (std::hypot(sp.x()-h->pos().x(), sp.y()-h->pos().y()) < hoverTol) {
                        m_mapView->viewport()->setCursor(Qt::SizeAllCursor);
                        return false;
                    }
            m_mapView->viewport()->setCursor(Qt::OpenHandCursor);
        }
        return false;
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent*>(event);

        // Right-click: complete curve drawing / cancel draw / return to Drag
        if (me->button() == Qt::RightButton) {
            // Curve mode with control points → finalize the curve
            if (m_drawStatus == Curve && m_curveCtrlPts.size() >= 2) {
                finalizeCurve();
                return true;
            }
            if (m_drawStatus != Drag) setDrawMode(Drag);
            else cancelDraw();
            return true;
        }

        if (me->button() != Qt::LeftButton) return false;
        QPointF sp = m_mapView->mapToScene(me->pos());

        // ─ Drag mode ─────────────────────────────────────────────────────────
        if (m_drawStatus == Drag) {
            const double tol = hitTolScene(10.0);
            for (auto lit = m_curveHandles.begin(); lit != m_curveHandles.end(); ++lit) {
                auto &handles = lit.value();
                for (int i = 0; i < handles.size(); ++i) {
                    QPointF hp = handles[i]->pos();
                    if (std::hypot(sp.x()-hp.x(), sp.y()-hp.y()) < tol) {
                        m_activeHandle     = handles[i];
                        m_handleCurveId    = lit.key();
                        m_handleIndex      = i;
                        m_isDraggingHandle = true;
                        m_mapView->setDragMode(QGraphicsView::NoDrag);
                        m_mapView->viewport()->setCursor(Qt::ClosedHandCursor);
                        return true;
                    }
                }
            }
            selectFeatureAt(sp);
            return false;   // let QGraphicsView handle pan via ScrollHandDrag
        }

        // ─ Draw modes ─────────────────────────────────────────────────────────
        SnapResult snap = findSnap(sp);
        QPointF usePos  = snap.valid ? snap.scenePos : sp;
        QString useId   = snap.valid ? snap.id       : QString();

        switch (m_drawStatus) {
        case Point:
            if (hasOverlap(sp)) { selectFeatureAt(sp); }  // too close → select
            else { addPoint(sp); }
            break;
        case Charge:
            if (hasOverlap(sp)) { selectFeatureAt(sp); }
            else { addCharge(sp); }
            break;
        case HighPrecision:
            if (hasOverlap(sp)) { selectFeatureAt(sp); }
            else { addHighPrecisionPoint(sp); }
            break;
        case Line:
            if (!m_drawHasP1) {
                m_drawP1   = usePos;
                m_drawP1Id = useId;
                m_drawHasP1 = true;
            } else {
                addLine(m_drawP1, usePos, m_drawP1Id, useId);
                // Continuous: last endpoint becomes new P1
                m_drawP1   = usePos;
                m_drawP1Id = useId;
                clearPreview();
            }
            break;
        case Curve: {
            // 多阶贝塞尔：累积控制点
            m_curveCtrlPts.append(usePos);
            m_curveCtrlIds.append(useId);
            // 第一个点也同步到 m_drawP1（便于兼容旧逻辑）
            if (m_curveCtrlPts.size() == 1) {
                m_drawP1   = usePos;
                m_drawP1Id = useId;
                m_drawHasP1 = true;
            }
            break;
        }
        default: break;
        }
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && m_isDraggingHandle) {
            // Commit to JSON
            auto &lst = m_curveHandles[m_handleCurveId];
            if (lst.size() == 3) {
                for (int i = 0; i < m_curves.size(); ++i) {
                    QJsonObject cv = m_curves[i].toObject();
                    if (cv.value("id").toString() != m_handleCurveId) continue;
                    QPointF w1 = sceneToWorld(lst[0]->pos().x(), lst[0]->pos().y());
                    QPointF wc = sceneToWorld(lst[1]->pos().x(), lst[1]->pos().y());
                    QPointF w2 = sceneToWorld(lst[2]->pos().x(), lst[2]->pos().y());
                    cv["x1"]=w1.x(); cv["y1"]=w1.y(); cv["cx"]=wc.x();
                    cv["cy"]=wc.y(); cv["x2"]=w2.x(); cv["y2"]=w2.y();
                    // 端点手柄落点若吸附到某个已有点，记录其 id 作为关联关系；
                    // 否则视为脱离原有关联，清空对应 startid/endid。
                    if (m_handleIndex == 0 || m_handleIndex == 2) {
                        SnapResult snap = findSnap(lst[m_handleIndex]->pos());
                        const QString &key = (m_handleIndex == 0) ? "startid" : "endid";
                        cv[key] = snap.valid ? snap.id : QString();
                    }
                    m_curves[i] = cv; break;
                }
            }
            m_isDraggingHandle = false;
            m_activeHandle = nullptr;
            m_mapView->setDragMode(QGraphicsView::ScrollHandDrag);
            m_mapView->viewport()->setCursor(Qt::OpenHandCursor);
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        cancelDraw();
        return true;
    }

    return QWidget::eventFilter(obj, event);
}

// ── Coordinate helpers ────────────────────────────────────────────────────────

void MapEditWidget::updateCoordLabel(const QPointF &sp)
{
    QPointF w = sceneToWorld(sp.x(), sp.y());
    m_coordLabel->setText(QString("X %1   Y %2")
                          .arg(w.x(), 7, 'f', 2).arg(w.y(), 7, 'f', 2));
    m_coordLabel->adjustSize();
    QSize vs = m_mapView->viewport()->size();
    m_coordLabel->move(8, vs.height() - m_coordLabel->height() - 8);
}

QPointF MapEditWidget::worldToScene(double wx, double wy) const
{
    double mapW = m_worldR - m_worldL, mapH = m_worldT - m_worldB;
    if (m_imgW <= 0 || m_imgH <= 0 || mapW <= 0 || mapH <= 0) return {wx, wy};
    return { (wx - m_worldL) / mapW * m_imgW,
             (m_worldT - wy) / mapH * m_imgH };
}

QPointF MapEditWidget::sceneToWorld(double sx, double sy) const
{
    double mapW = m_worldR - m_worldL, mapH = m_worldT - m_worldB;
    if (m_imgW <= 0 || m_imgH <= 0 || mapW <= 0 || mapH <= 0) return {sx, sy};
    return { sx / m_imgW * mapW + m_worldL,
             m_worldT - sy / m_imgH * mapH };
}

// ── MQTT / car list ───────────────────────────────────────────────────────────

void MapEditWidget::loadCarList()
{
    ApiService::instance().listCarAll([this](int code, const QJsonObject &data) {
        if (code != 200) return;
        m_carList = data.value("rows").toArray();

        // blockSignals: addItem() 会在第一条item加入时立即触发 currentIndexChanged，
        // 与下面手动选中后的 initMqtt() 重复订阅同一 topic（debugLog 已证实这一点）。
        m_carCombo->blockSignals(true);
        m_carCombo->clear();
        for (const auto &v : m_carList) {
            QJsonObject car = v.toObject();
            m_carCombo->addItem(car.value("carName").toString(),
                                car.value("carId").toVariant().toString());
        }
        // 默认优先选中全局配置的设备（与"地图管理-地图视图"页一致），而不是
        // API 返回列表里随便排第一的车——那台车未必在线/未必是真正要看的设备。
        QString preferredId = AppConfig::instance().agvId();
        int idx = preferredId.isEmpty() ? -1 : m_carCombo->findData(preferredId);
        debugLog(QString("[loadCarList] count=%1 preferredAgvId=%2 foundIdx=%3")
            .arg(m_carCombo->count()).arg(preferredId).arg(idx));
        m_carCombo->setCurrentIndex(idx >= 0 ? idx : (m_carCombo->count() > 0 ? 0 : -1));
        m_carCombo->blockSignals(false);

        if (m_carCombo->count() > 0) { m_currentCarId = m_carCombo->currentData().toString(); initMqtt(); }
    });
    connect(m_carCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        disconnectMqtt();
        m_currentCarId = m_carCombo->currentData().toString();
        initMqtt();
    });
}

void MapEditWidget::initMqtt()
{
    if (m_currentCarId.isEmpty()) return;
    debugLog(QString("[initMqtt] subscribing /rob/%1/info  (m_mapId=%2)").arg(m_currentCarId, m_mapId));
    MqttService::instance().subscribeTopic(
        QString("/rob/%1/info").arg(m_currentCarId), 0,
        [this](const QString &, const QByteArray &payload) {
            static int logCount = 0;
            QJsonParseError err;
            auto doc = QJsonDocument::fromJson(payload, &err);
            if (err.error != QJsonParseError::NoError) return;
            QJsonObject d = doc.object();
            // 与 Vue edit.vue::handleCarInfoMessage 一致：car 的位姿只在它当前所在地图
            // 与本页编辑的地图一致时才采用，否则坐标来自另一张地图，毫无意义。
            QJsonValue mapIdV = d.contains("map_id") ? d.value("map_id") : d.value("mapId");
            bool mapIdMismatch = !mapIdV.isUndefined() && mapIdV.toVariant().toString() != m_mapId;
            if (logCount < 5) {
                debugLog(QString("[/info msg #%1] raw=[%2]").arg(logCount).arg(QString::fromUtf8(payload).left(400)));
                debugLog(QString("[/info msg #%1] msg.map_id=%2 m_mapId=%3 mismatch=%4")
                    .arg(logCount).arg(mapIdV.toVariant().toString(), m_mapId).arg(mapIdMismatch));
            }
            if (mapIdMismatch) { logCount++; return; }

            m_carX   = d.value("carX").toDouble(d.value("car_x").toDouble());
            m_carY   = d.value("carY").toDouble(d.value("car_y").toDouble());
            m_carYaw = d.value("carYaw").toDouble(d.value("car_yaw").toDouble());
            m_hasCarPose = true;
            if (logCount < 5) {
                QPointF sp = worldToScene(m_carX, m_carY);
                debugLog(QString("[/info msg #%1] carX=%2 carY=%3 carYaw=%4 -> scene=(%5,%6)  world=[L%7 B%8 R%9 T%10] img=%11x%12")
                    .arg(logCount).arg(m_carX).arg(m_carY).arg(m_carYaw).arg(sp.x()).arg(sp.y())
                    .arg(m_worldL).arg(m_worldB).arg(m_worldR).arg(m_worldT).arg(m_imgW).arg(m_imgH));
            }
            logCount++;
            updateRobotMarker();
        });
}

void MapEditWidget::disconnectMqtt()
{
    if (!m_currentCarId.isEmpty())
        MqttService::instance().unsubscribeTopic(
            QString("/rob/%1/info").arg(m_currentCarId));
    m_hasCarPose = false;
    updateRobotMarker();
}

// 与 worldToScene 共用同一坐标转换，避免与栅格/路网产生位置偏差
void MapEditWidget::updateRobotMarker()
{
    if (!m_robotItem) return;
    if (!m_showRobotPosition || !m_hasCarPose) {
        m_robotItem->setVisible(false);
        return;
    }
    QPointF sp = worldToScene(m_carX, m_carY);
    m_robotItem->setPos(sp);

    // ENU: yaw=0→东, yaw=π/2→北, 逆时针为正；三角形默认朝北(0,-10)。
    // 与 MapViewWidget::renderRobot 保持同一约定：screenAngle = 90 - yawDeg
    double yawRad = (qAbs(m_carYaw) > M_PI * 2) ? qDegreesToRadians(m_carYaw) : m_carYaw;
    m_robotItem->setRotation(90.0 - qRadiansToDegrees(yawRad));
    m_robotItem->setVisible(true);
}
