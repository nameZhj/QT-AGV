#include "MapEditWidget.h"
#include "MapViewWidget.h"
#include "core/ApiService.h"
#include "core/AppConfig.h"
#include "core/MqttService.h"
#include "core/RobotInfoManager.h"
#include "core/LoadingDialog.h"
#include "core/Toast.h"
#include "core/MapPgmUtil.h"
#include "core/RouteFeatureSelect.h"
#include "core/ToggleSwitch.h"
#include "core/MotionConfigManager.h"
#include "core/AlignConfigManager.h"
#include "DirectionPadWidget.h"
#include "core/TeleopController.h"
#include <QPointer>
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
#include <QDirIterator>
#include <QMessageBox>
#include <QGraphicsPixmapItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
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
#include <QMenu>
#include <QAction>
#include <QProgressDialog>
#include <QFileDialog>
#include <cmath>
#include <QPainter>
#include <QSvgRenderer>
#include <QToolTip>
#include <QShortcut>
#include <QKeySequence>
#include <QDebug>

// ── Style constants ───────────────────────────────────────────────────────────

static const char *S_TOOLBAR =
    "background:#191e2b; border-right:1px solid #262f42;";

static const char *S_TOOLBTN =
    "QPushButton{"
    "  background:transparent;"
    "  border:none; border-radius:6px; padding:0;"
    "}"
    "QPushButton:hover{"
    "  background:rgba(255,255,255,0.08);"
    "}"
    "QPushButton:checked{"
    "  background:#1296DB;"
    "}";

static const char *S_TOPBAR =
    "background:#1b212f; border-bottom:1px solid #262e3f;";

static const char *S_RIGHTPANEL =
    "background:#151822; border-left:1px solid #262e3f;";

static const char *S_CARD =
    "QGroupBox{"
    "  font-weight:bold; font-size:11px; color:#5a789a;"
    "  border:1px solid #263348; border-radius:6px;"
    "  margin-top:10px; padding:4px; background:#1b2230;"
    "}"
    "QGroupBox::title{"
    "  subcontrol-origin:margin; left:8px;"
    "  padding:0 4px; background:#151822; color:#7090b0;"
    "}"
    "QLabel{background:transparent; color:#8fa6bf; font-size:12px;}"
    "QLineEdit,QComboBox,QDoubleSpinBox{"
    "  border:1px solid #2b384e; border-radius:4px;"
    "  padding:2px 6px; background:#131722; color:#cfddea; font-size:12px;"
    "}"
    "QLineEdit:focus,QComboBox:focus,QDoubleSpinBox:focus{border:1px solid #1296DB;}"
    "QComboBox QAbstractItemView{background:#171d2a;color:#cfddea;selection-background-color:#1296DB;border:1px solid #2b384e;}"
    "QCheckBox{color:#8fa6bf;font-size:12px;spacing:6px;}"
    "QCheckBox::indicator{width:14px;height:14px;border:1px solid #32435c;border-radius:3px;background:#131722;}"
    "QCheckBox::indicator:checked{background:#1296DB;border:1px solid #1296DB;}"
    "QPushButton{"
    "  border:1px solid #2b384e; border-radius:4px;"
    "  padding:4px 8px; font-size:12px; background:#1a2230; color:#8fa6bf;"
    "}"
    "QPushButton:hover{background:#253044; color:#fff;}";

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

// 严格判断点是否为高精度点(工作站)或充电桩点(特殊工作站)
bool isSpecialWorkstationPoint(const QJsonObject &obj)
{
    QString id = obj.value("id").toString();
    QString name = obj.value("name").toString();
    QString typeName = obj.value("typeName").toString();
    QString type = obj.value("type").toString();
    QString label = obj.value("label").toString();

    if (id.startsWith("hp_", Qt::CaseInsensitive) || id.startsWith("c_", Qt::CaseInsensitive) ||
        id.startsWith("ap_", Qt::CaseInsensitive) || id.startsWith("cp_", Qt::CaseInsensitive)) {
        return true;
    }
    if (typeName.compare("ActionPoint", Qt::CaseInsensitive) == 0 ||
        typeName.compare("ChargePoint", Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (type.compare("highPrecision", Qt::CaseInsensitive) == 0 ||
        type.compare("charge", Qt::CaseInsensitive) == 0 ||
        type.compare("highPrecisionEpc", Qt::CaseInsensitive) == 0 ||
        type.compare("chargeEpc", Qt::CaseInsensitive) == 0 ||
        type.compare("ActionPoint", Qt::CaseInsensitive) == 0 ||
        type.compare("ChargePoint", Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (name.startsWith("AP", Qt::CaseInsensitive) ||
        name.startsWith("CP", Qt::CaseInsensitive) ||
        name.startsWith("TL", Qt::CaseInsensitive)) {
        return true;
    }
    if (name.contains("工作站") || name.contains("充电") ||
        label.contains("工作站") || label.contains("充电")) {
        return true;
    }
    if (obj.contains("epc_x") || obj.contains("epcX") ||
        obj.contains("epc_y") || obj.contains("epcY") ||
        obj.contains("epc_theta") || obj.contains("epcTheta") ||
        obj.contains("epc_Yaw") || obj.contains("epc_yaw")) {
        return true;
    }
    return false;
}

// 强制将特殊工作站/工作站的四个避障参数全部置 0 (平铺字段及 actions 数组)
void forceZeroObstacleParams(QJsonObject &obj)
{
    obj["intrusion_stop_length"]         = 0.0;
    obj["intrusion_stop_width"]          = 0.0;
    obj["intrusion_deceleration_length"] = 0.0;
    obj["intrusion_deceleration_width"]  = 0.0;
    obj["is_custom_obstacle"]            = false;

    if (obj.contains("actions") && obj.value("actions").isArray()) {
        QJsonArray acts = obj.value("actions").toArray();
        bool modified = false;
        for (int i = 0; i < acts.size(); ++i) {
            QJsonObject act = acts[i].toObject();
            if (act.value("action_type").toInt() == 21 || act.value("name").toString() == "实时避障范围") {
                act["name"] = "实时避障范围";
                act["action_type"] = 21;
                act["intrusion_stop_length"]         = 0.0;
                act["intrusion_stop_width"]          = 0.0;
                act["intrusion_deceleration_length"] = 0.0;
                act["intrusion_deceleration_width"]  = 0.0;
                act["is_custom_obstacle"]            = false;
                acts[i] = act;
                modified = true;
            }
        }
        if (modified) {
            obj["actions"] = acts;
        }
    }
}

QPixmap drawToolIcon(const QString &name, const QColor &col, int sz = 22)
{
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    double c = sz * 0.5;

    if (name == "pan") {
        // 指针光标 (ToolArrow) 风格: 经典斜向箭头
        QPainterPath ptr;
        ptr.moveTo(sz*0.22, sz*0.14);
        ptr.lineTo(sz*0.22, sz*0.80);
        ptr.lineTo(sz*0.42, sz*0.60);
        ptr.lineTo(sz*0.62, sz*0.82);
        ptr.lineTo(sz*0.74, sz*0.72);
        ptr.lineTo(sz*0.53, sz*0.51);
        ptr.lineTo(sz*0.78, sz*0.46);
        ptr.closeSubpath();
        p.setPen(QPen(col.darker(120), 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(col);
        p.drawPath(ptr);
    }
    else if (name == "point") {
        // 站点 Mark: 紧凑矩形车标与内部十字
        QRectF rc(sz*0.15, sz*0.28, sz*0.70, sz*0.44);
        p.setPen(QPen(col, 1.4)); p.setBrush(col.lighter(160));
        p.drawRoundedRect(rc, 2, 2);
        p.setPen(QPen(QColor("#e74c3c"), 1.2));
        p.drawLine(QPointF(c-sz*0.11, c), QPointF(c+sz*0.11, c));
        p.drawLine(QPointF(c, c-sz*0.11), QPointF(c, c+sz*0.11));
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
    else if (name == "devicepoint") {
        // 设备打点: 定位针图标，代表用设备当前位置打点
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath pin;
        double r = sz*0.28;
        pin.moveTo(c - r, c - sz*0.06);
        pin.cubicTo(c - r, c - sz*0.30, c + r, c - sz*0.30, c + r, c - sz*0.06);
        pin.lineTo(c, sz*0.88);
        pin.closeSubpath();
        p.setBrush(col);
        p.drawPath(pin);
        p.setPen(Qt::NoPen); p.setBrush(col.lighter(140));
        p.drawEllipse(QPointF(c, c - sz*0.12), sz*0.10, sz*0.10);
    }
    else if (name == "brush") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(sz*0.25, sz*0.75), QPointF(sz*0.75, sz*0.25));
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(QPointF(sz*0.22, sz*0.78), sz*0.12, sz*0.12);
        p.drawEllipse(QPointF(sz*0.78, sz*0.22), sz*0.08, sz*0.08);
    }
    else if (name == "eraser") {
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QRectF er(sz*0.22, sz*0.35, sz*0.56, sz*0.36);
        p.drawRoundedRect(er, 2, 2);
        p.drawLine(QPointF(sz*0.48, sz*0.35), QPointF(sz*0.48, sz*0.71));
    }
    else if (name == "undo") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush);
        QRectF r(sz*0.22, sz*0.22, sz*0.56, sz*0.56);
        p.drawArc(r, 45 * 16, 220 * 16);
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.20, sz*0.28) << QPointF(sz*0.40, sz*0.18) << QPointF(sz*0.35, sz*0.42);
        p.drawPolygon(arrow);
    }
    else if (name == "redo") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush);
        QRectF r(sz*0.22, sz*0.22, sz*0.56, sz*0.56);
        p.drawArc(r, -45 * 16, -220 * 16);
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.80, sz*0.28) << QPointF(sz*0.60, sz*0.18) << QPointF(sz*0.65, sz*0.42);
        p.drawPolygon(arrow);
    }
    else if (name == "teleop") {
        // 遥控手柄图标：手柄外壳 + 十字键 + 按键点
        p.setPen(QPen(col, 1.4)); p.setBrush(Qt::NoBrush);
        QRectF body(sz*0.14, sz*0.26, sz*0.72, sz*0.48);
        p.drawRoundedRect(body, sz*0.18, sz*0.18);
        p.setPen(QPen(col, 1.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(sz*0.34, sz*0.40), QPointF(sz*0.34, sz*0.60));
        p.drawLine(QPointF(sz*0.24, sz*0.50), QPointF(sz*0.44, sz*0.50));
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(QPointF(sz*0.68, sz*0.44), sz*0.05, sz*0.05);
        p.drawEllipse(QPointF(sz*0.60, sz*0.54), sz*0.05, sz*0.05);
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
    // Load SVG robot icon at 180×330 (matching 60×110 aspect ratio)
    QSvgRenderer renderer(QString(":/ic-robot1.svg"));
    if (renderer.isValid()) {
        m_robotPixmap = QPixmap(180, 330);
        m_robotPixmap.fill(Qt::transparent);
        QPainter rp(&m_robotPixmap);
        rp.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        renderer.render(&rp);
    }

    setupUI();
    // 先加载地图数据(含坐标系)，完成后再加载路网/反光柱/设备列表，
    // 避免路网点在坐标系尚未建立时渲染导致显示混乱
    loadMapData([this]() {
        loadRouteData();
        loadReflectorData();
        loadCarList();
    });
}

MapEditWidget::~MapEditWidget()
{
    disconnectMqtt();
}

bool MapEditWidget::checkUnsavedChanges()
{
    if (!m_dirty) return true;
    QMessageBox box(QMessageBox::Question, "未保存的修改",
        "路网有未保存的修改，是否保存？", QMessageBox::NoButton, this);
    QPushButton *saveBtn    = box.addButton("保存并退出", QMessageBox::AcceptRole);
    QPushButton *discardBtn = box.addButton("放弃修改", QMessageBox::DestructiveRole);
    box.addButton("取消", QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == saveBtn) {
        onSaveMap([this](bool success) {
            if (success) {
                emit goBack();
            }
        });
        return false;   // 异步保存进行中，阻止立即退出；保存成功后再在回调中 emit goBack
    } else if (box.clickedButton() == discardBtn) {
        m_dirty = false;
        return true;   // 放弃修改，直接离开
    }
    return false;       // 取消，留在当前编辑页面
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
    connect(backBtn, &QPushButton::clicked, this, [this]() {
        if (!checkUnsavedChanges()) return;
        emit goBack();
    });
    tl->addWidget(backBtn);

    QFrame *div = new QFrame; div->setFrameShape(QFrame::VLine);
    div->setStyleSheet("color:rgba(255,255,255,0.08);"); tl->addWidget(div);

    m_mapInfoLabel = new QLabel("编辑地图");
    m_mapInfoLabel->setStyleSheet("color:#9ab8d8;font-weight:bold;font-size:13px;");
    tl->addWidget(m_mapInfoLabel);
    tl->addStretch();

    // 撤销 / 重做 (针对栅格地图修图)
    m_undoBtn = topBtn("撤销");
    m_undoBtn->setIcon(makeToolIcon("undo"));
    m_undoBtn->setToolTip(ttHtml("撤销", "撤销上一步底图修图"));
    m_undoBtn->setEnabled(false);
    connect(m_undoBtn, &QPushButton::clicked, this, &MapEditWidget::onUndo);
    tl->addWidget(m_undoBtn);

    m_redoBtn = topBtn("重做");
    m_redoBtn->setIcon(makeToolIcon("redo"));
    m_redoBtn->setToolTip(ttHtml("重做", "重做上一步底图修图"));
    m_redoBtn->setEnabled(false);
    connect(m_redoBtn, &QPushButton::clicked, this, &MapEditWidget::onRedo);
    tl->addWidget(m_redoBtn);

    // 笔刷粗细
    QLabel *brushSizeLbl = new QLabel("笔刷:");
    brushSizeLbl->setStyleSheet("color:#9ab8d8;font-size:12px;margin-left:4px;");
    tl->addWidget(brushSizeLbl);

    m_brushSizeCombo = new QComboBox;
    m_brushSizeCombo->addItem("2 px", 2);
    m_brushSizeCombo->addItem("4 px", 4);
    m_brushSizeCombo->addItem("8 px", 8);
    m_brushSizeCombo->addItem("16 px", 16);
    m_brushSizeCombo->addItem("32 px", 32);
    m_brushSizeCombo->setCurrentIndex(1); // 默认 4px
    m_brushSizeCombo->setStyleSheet(
        "QComboBox{background:#1e2d40; color:#c8ddef; border:1px solid rgba(255,255,255,0.12); border-radius:4px; padding:2px 6px; font-size:12px;}"
        "QComboBox::drop-down{border:none; width:16px;}"
        "QComboBox QAbstractItemView{background:#1a2332; color:#c8ddef; selection-background-color:#2980b9;}");
    connect(m_brushSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_brushSize = m_brushSizeCombo->itemData(idx).toInt();
    });
    tl->addWidget(m_brushSizeCombo);

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
        fitView();
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
        debugLog(QString("[设备位置] 开关=%1 图元=%2 车图标=%3 已收到位姿=%4 车号=%5")
            .arg(on ? "开" : "关")
            .arg(m_robotItem ? "已创建" : "未创建")
            .arg(m_robotPixmap.isNull() ? "加载失败" : "正常")
            .arg(m_hasCarPose ? "是" : "否")
            .arg(m_currentCarId));
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

    m_importSmapBtn = topBtn("导入 .smap路网", "#2e7d32");
    m_importSmapBtn->setToolTip(ttHtml("导入 .smap", "选择并导入 .smap 地图路网文件"));
    m_importSmapBtn->installEventFilter(new InstantTip(m_importSmapBtn));
    connect(m_importSmapBtn, &QPushButton::clicked, this, &MapEditWidget::onImportSmap);
    tl->addWidget(m_importSmapBtn);

    m_saveBtn = topBtn("保存地图", "#2471a3");
    m_saveBtn->setToolTip(ttHtml("保存地图 (Ctrl+S)", "同时保存路网数据与修改后的栅格地图底图"));
    connect(m_saveBtn, &QPushButton::clicked, this, [this]() { onSaveMap(); });
    tl->addWidget(m_saveBtn);

    m_teleopBtn = topBtn("抢占控制权", "#1e6fd4");
    m_teleopBtn->setIcon(makeToolIcon("teleop"));
    m_teleopBtn->setToolTip(ttHtml("抢占控制权",
        "点击抢占控制权（暂停导航并接管键盘/舵盘控制），再次点击恢复导航\n"
        "键盘: W A S D 移动 | Space 刹车 | Ctrl 加速 | P 升降上 | L 升降下"));
    m_teleopBtn->installEventFilter(new InstantTip(m_teleopBtn));
    connect(m_teleopBtn, &QPushButton::clicked, this, &MapEditWidget::onTeleopClicked);
    tl->addWidget(m_teleopBtn);

    QPushButton *helpBtn = topBtn("帮助 (F1)", "#34495e");
    helpBtn->setToolTip(ttHtml("帮助 (F1)", "查看键盘快捷键与地图编辑操作指南"));
    helpBtn->installEventFilter(new InstantTip(helpBtn));
    connect(helpBtn, &QPushButton::clicked, this, &MapEditWidget::showHelpDialog);
    tl->addWidget(helpBtn);

    root->addWidget(topBar);

    // ── 站点类型子工具条: 站点 / 工作站 / 特殊工作站 ────────────────────────────
    // 对应现有三类点位: 普通点(LM) / 高精度点(AP, typeName=ActionPoint) / 充电桩点(CP_, typeName=ChargePoint)
    m_stationBar = new QWidget;
    m_stationBar->setStyleSheet("background:#1e2d3d; border-bottom:1px solid rgba(255,255,255,0.06);");
    {
        QHBoxLayout *sbl = new QHBoxLayout(m_stationBar);
        sbl->setContentsMargins(8, 3, 8, 3);
        sbl->setSpacing(6);

        QLabel *sbTitle = new QLabel("站点");
        sbTitle->setStyleSheet("color:#9ab8d8; font-size:12px; font-weight:bold; background:transparent; border:none;");
        sbl->addWidget(sbTitle);

        auto mkStationBtn = [&](const QString &text, int mode) {
            auto *b = new QPushButton(text);
            b->setCheckable(true);
            b->setFocusPolicy(Qt::NoFocus);
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedHeight(24);
            b->setStyleSheet(
                "QPushButton{background:transparent; color:#9ab8d8; border:1px solid transparent;"
                "border-radius:4px; padding:2px 12px; font-size:12px;}"
                "QPushButton:hover{background:rgba(255,255,255,14);}"
                "QPushButton:checked{background:#2b4a6f; color:#ffffff; border:1px solid #3d6a9a;}");
            connect(b, &QPushButton::clicked, this, [this, mode]() { setDrawMode(mode); });
            sbl->addWidget(b);
            m_stationBtns[mode] = b;
        };
        mkStationBtn("站点",       Point);          // 普通点 (LM)
        mkStationBtn("工作站",     HighPrecision);  // 高精度点 (AP / ActionPoint)
        mkStationBtn("特殊工作站", Charge);         // 充电桩点 (CP_ / ChargePoint)

        sbl->addStretch();
    }
    m_stationBar->setVisible(false);
    root->addWidget(m_stationBar);

    // ── Content row ───────────────────────────────────────────────────────────
    QWidget *content = new QWidget;
    QHBoxLayout *cl  = new QHBoxLayout(content);
    cl->setContentsMargins(0,0,0,0);
    cl->setSpacing(0);

    // ── Left toolbar ──────────────────────────────────────────────────────────
    QWidget *toolbar = new QWidget;
    toolbar->setFixedWidth(46);
    toolbar->setStyleSheet(S_TOOLBAR);
    QVBoxLayout *tbLayout = new QVBoxLayout(toolbar);
    tbLayout->setContentsMargins(5,8,5,8);
    tbLayout->setSpacing(3);

    struct Tool { int mode; QString icon; QString tip; QString detail; };
    const QList<Tool> tools = {
        {Drag,          "pan",          "指针 (Esc)",   "选择要素 / 拖动站点与线路 / 调整曲线控制手柄"},
        {-1,            "",             "",             ""},
        {Line,          "line",         "线路 (L)",     "绘制直线路径：空地自动建站，连续连线，右键/Esc结束"},
        {Curve,         "curve",        "曲线 (B)",     "绘制多阶贝塞尔曲线：起点→控制点→终点，自动吸附/建站"},
        {-1,            "",             "",             ""},
        {Point,         "point",        "站点 (M)",     "放置站点：站点 LM（蓝）/ 工作站 AP（红）/ 特殊工作站 CP_（金）\n在顶部“站点”子工具条中切换类型"},
        {-1,            "",             "",             ""},
        {Eraser,        "eraser",       "橡皮擦 (E)",   "擦除栅格涂改：恢复可通行区域"},
        {Brush,         "brush",        "画笔 (P)",     "涂黑栅格地图：标记障碍物/墙体"},
        {-1,            "",             "",             ""},
        {-2,            "devicepoint",  "设备打点",     "添加设备当前位置为普通站点"},
        {-3,            "charge",       "充电打点",     "通过设备相对定位添加充电桩站点 CP\n(等待设备返回 epc 坐标)"},
        {-4,            "highprecision","精定位打点",   "通过设备相对定位添加高精定位站点 TL\n(等待设备返回 epc 坐标)"},
    };

    m_toolBtns.clear();
    for (const auto &t : tools) {
        if (t.mode == -1) {
            auto *sep = new QFrame; sep->setFrameShape(QFrame::HLine);
            sep->setFixedHeight(1);
            sep->setStyleSheet("background:#262f42; border:none; margin:3px 2px;");
            tbLayout->addWidget(sep);
            continue;
        }
        auto *b = new QPushButton;
        b->setIcon(makeToolIcon(t.icon));
        b->setIconSize(QSize(20, 20));
        b->setCheckable(t.mode >= 0);
        b->setFixedSize(36, 36);
        b->setStyleSheet(S_TOOLBTN);
        b->setToolTip(ttHtml(t.tip, t.detail));
        b->setCursor(Qt::PointingHandCursor);
        b->installEventFilter(new InstantTip(b));
        if (t.mode == -2) {
            connect(b, &QPushButton::clicked, this, [this]() {
                if (!m_hasCarPose) {
                    Toast::info(this, "未收到设备位置信息，无法打点");
                    return;
                }
                QPointF sp = worldToScene(m_carX, m_carY);
                addPoint(sp, m_carYaw, m_carTheta);
                Toast::success(this, QString("已在设备位置(%1, %2)添加普通站点\n朝向角: %3°")
                    .arg(m_carX, 0, 'f', 2).arg(m_carY, 0, 'f', 2).arg(m_carTheta, 0, 'f', 1));
            });
        } else if (t.mode == -3) {
            connect(b, &QPushButton::clicked, this, [this]() { addDevicePoint("charge"); });
        } else if (t.mode == -4) {
            connect(b, &QPushButton::clicked, this, [this]() { addDevicePoint("highPrecision"); });
        } else {
            int mode = t.mode;
            connect(b, &QPushButton::clicked, this, [this, mode]() { setDrawMode(mode); });
        }
        tbLayout->addWidget(b, 0, Qt::AlignHCenter);
        if (t.mode >= 0) m_toolBtns[t.mode] = b;
    }

    tbLayout->addStretch();

    auto *sep2 = new QFrame; sep2->setFrameShape(QFrame::HLine);
    sep2->setFixedHeight(1);
    sep2->setStyleSheet("background:#262f42; border:none; margin:3px 2px;");
    tbLayout->addWidget(sep2);

    m_tableModeBtn = new QPushButton;
    m_tableModeBtn->setIcon(makeToolIcon("list"));
    m_tableModeBtn->setIconSize(QSize(20, 20));
    m_tableModeBtn->setCheckable(true);
    m_tableModeBtn->setFixedSize(36, 36);
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
    connect(m_mapView, &MapCanvas::transformChanged, this, [this]() { updateRulers(); });
    m_mapView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    m_mapView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_mapView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_mapView->setAlignment(Qt::AlignCenter);
    m_mapView->setFrameStyle(QFrame::NoFrame);
    m_mapView->setBackgroundBrush(QColor("#0f1520"));
    // NoDrag: 平移由 MapCanvas(Ctrl+左键旋转/中键/Alt+左键平移)和 eventFilter 手动处理，
    // 避免 ScrollHandDrag 拦截鼠标事件导致手柄拖拽失效
    m_mapView->setDragMode(QGraphicsView::NoDrag);
    m_mapView->installEventFilter(this);
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

    // 虚拟遥控舵盘 (浮动于编辑地图左下角，支持鼠标直观遥控与实时按键状态反馈)
    m_directionPad = new DirectionPadWidget(m_mapView);
    m_directionPad->move(16, qMax(16, m_mapView->height() - m_directionPad->height() - 16));
    m_directionPad->raise();

    connect(m_mapView, &MapCanvas::canvasResized, this, [this](const QSize &sz) {
        if (m_directionPad) {
            m_directionPad->move(16, sz.height() - m_directionPad->height() - 16);
            m_directionPad->raise();
        }
    });

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

    auto *editSplitter = new QSplitter(Qt::Horizontal);
    editSplitter->setChildrenCollapsible(false);
    editSplitter->setHandleWidth(5);
    editSplitter->setStyleSheet(
        "QSplitter::handle{background:#232733;}"
        "QSplitter::handle:hover{background:#3b82f6;}"
        "QSplitter::handle:pressed{background:#1d4ed8;}");
    editSplitter->addWidget(mapArea);

    // ── Right panel ───────────────────────────────────────────────────────────
    QWidget *rightOuter = new QWidget;
    rightOuter->setMinimumWidth(260);
    rightOuter->setMaximumWidth(650);
    rightOuter->setStyleSheet(S_RIGHTPANEL);

    QScrollArea *sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameStyle(QFrame::NoFrame);
    sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sa->setStyleSheet(
        "QScrollArea{background:transparent; border:none;}"
        "QScrollBar:vertical{background:#131722;width:5px;border:none;}"
        "QScrollBar::handle:vertical{background:#2a364d;border-radius:2px;}");

    QWidget *rc = new QWidget;
    rc->setStyleSheet("background:#151822;");
    QVBoxLayout *rl = new QVBoxLayout(rc);
    rl->setContentsMargins(8,8,8,8);
    rl->setSpacing(8);
    sa->setWidget(rc);

    QVBoxLayout *rol = new QVBoxLayout(rightOuter);
    rol->setContentsMargins(0,0,0,0);
    rol->addWidget(sa);

    // 卡片容器辅助函数
    auto mkCard = [&](const QString &title) -> QPair<QWidget*, QVBoxLayout*> {
        auto *container = new QWidget;
        container->setStyleSheet("background:#1b2230; border:1px solid #263348; border-radius:6px;");
        auto *l = new QVBoxLayout(container);
        l->setContentsMargins(10,8,10,10);
        l->setSpacing(5);
        auto *lbl = new QLabel(title);
        lbl->setStyleSheet("color:#627e9e; font-size:11px; font-weight:bold; background:transparent; border:none;");
        l->addWidget(lbl);
        return {container, l};
    };

    auto mkFormRow = [](const QString &label, QWidget *editor, const QString &unit = QString()) -> QWidget* {
        auto *w = new QWidget;
        w->setStyleSheet("background:transparent; border:none;");
        auto *h = new QHBoxLayout(w);
        h->setContentsMargins(0,1,0,1);
        h->setSpacing(6);
        auto *lbl = new QLabel(label);
        lbl->setMinimumWidth(68);
        lbl->setMaximumWidth(84);
        lbl->setStyleSheet("color:#7e96b0; font-size:11px; background:transparent; border:none;");
        h->addWidget(lbl);
        h->addWidget(editor, 1);
        if (!unit.isEmpty()) {
            auto *unitLbl = new QLabel(unit);
            unitLbl->setMinimumWidth(26);
            unitLbl->setStyleSheet("color:#8fa6bf; font-size:11px; background:transparent; border:none;");
            h->addWidget(unitLbl);
        }
        return w;
    };

    // Map info
    {
        auto [card, l] = mkCard("地图信息");
        auto *idLbl = new QLabel("ID: " + m_mapId);
        idLbl->setStyleSheet("color:#4a7098; font-size:11px; background:transparent; border:none;");
        idLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        idLbl->setWordWrap(true);
        l->addWidget(idLbl);
        rl->addWidget(card);
    }

    // Device (hidden - kept for MQTT robot pose tracking, no UI panel)
    {
        m_carCombo = new QComboBox;
        m_carCombo->hide();
    }

    // Properties (hidden until selection)
    {
        auto [card, pl] = mkCard("要素属性");

        // 顶栏: 标题 + Badge
        auto *headerBar = new QWidget;
        headerBar->setStyleSheet("background:transparent; border:none;");
        auto *hbl = new QHBoxLayout(headerBar);
        hbl->setContentsMargins(0,0,0,4);
        m_propHeaderTitle = new QLabel("图元属性");
        m_propHeaderTitle->setStyleSheet("color:#e2ecf7; font-size:13px; font-weight:bold; background:transparent; border:none;");
        m_propHeaderBadge = new QLabel("LM");
        m_propHeaderBadge->setStyleSheet("background:#1e3a5f; color:#60a5fa; border:1px solid #2563eb; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;");
        hbl->addWidget(m_propHeaderTitle);
        hbl->addStretch();
        hbl->addWidget(m_propHeaderBadge);
        pl->addWidget(headerBar);

        // ==========================================
        // 1. 站点属性组 (WidgetMarkPPTW)
        // ==========================================
        m_stationPropGroup = new QWidget;
        m_stationPropGroup->setStyleSheet("background:transparent; border:none;");
        auto *sl = new QVBoxLayout(m_stationPropGroup);
        sl->setContentsMargins(0,0,0,0);
        sl->setSpacing(4);

        // 基础信息
        m_stationTypeCombo = new QComboBox;
        m_stationTypeCombo->addItem("普通站点 (LM)", "point");
        m_stationTypeCombo->addItem("充电桩 (CP)", "charge");
        m_stationTypeCombo->addItem("高精定位点 (TL)", "highPrecision");
        sl->addWidget(mkFormRow("站点类型:", m_stationTypeCombo));

        m_stationIdEdit = new QLineEdit;
        m_stationIdEdit->setReadOnly(true);
        m_stationIdEdit->setStyleSheet("background:#12151e; color:#627284; border:1px solid #232b3b; border-radius:3px; padding:2px 5px; font-size:11px;");
        sl->addWidget(mkFormRow("站点编号:", m_stationIdEdit));

        m_stationLabelEdit = new QLineEdit;
        sl->addWidget(mkFormRow("站点名称:", m_stationLabelEdit));

        // 空间坐标
        m_stationXSpin = new QDoubleSpinBox;
        m_stationXSpin->setRange(-99999.0, 99999.0);
        m_stationXSpin->setDecimals(3);
        sl->addWidget(mkFormRow("X 坐标:", m_stationXSpin, "m"));

        m_stationYSpin = new QDoubleSpinBox;
        m_stationYSpin->setRange(-99999.0, 99999.0);
        m_stationYSpin->setDecimals(3);
        sl->addWidget(mkFormRow("Y 坐标:", m_stationYSpin, "m"));

        m_stationYawSpin = new QDoubleSpinBox;
        m_stationYawSpin->setRange(-360.0, 360.0);
        m_stationYawSpin->setDecimals(1);
        sl->addWidget(mkFormRow("朝向角度:", m_stationYawSpin, "°"));

        // 站点特性复选框
        auto *attrCard = new QWidget;
        attrCard->setStyleSheet("background:#161c28; border:1px solid #232d3e; border-radius:4px;");
        auto *acl = new QVBoxLayout(attrCard);
        acl->setContentsMargins(6,6,6,6);
        acl->setSpacing(6);
        m_stationDockableChk = new QCheckBox("允许停靠 (rdsDockable)");
        m_stationDockableChk->setChecked(true);
        m_stationRotateChk = new QCheckBox("允许转身 (allowRotate)");
        m_stationRotateChk->setChecked(true);
        m_stationPgvChk = new QCheckBox("使用下视 PGV (useDownPGV)");
        acl->addWidget(m_stationDockableChk);
        acl->addWidget(m_stationRotateChk);
        acl->addWidget(m_stationPgvChk);
        sl->addWidget(attrCard);

        // EPC 相对定位数据
        m_stationEpcGroup = new QWidget;
        m_stationEpcGroup->setStyleSheet("background:#141a26; border:1px solid #222b3c; border-radius:4px;");
        auto *el = new QVBoxLayout(m_stationEpcGroup);
        el->setContentsMargins(6,6,6,6);
        el->setSpacing(3);
        auto *epcTitle = new QLabel("设备相对校准 (EPC)");
        epcTitle->setStyleSheet("color:#627e9e; font-size:10px; font-weight:bold; border:none;");
        el->addWidget(epcTitle);
        m_stationEpcXLabel = new QLabel("--");
        m_stationEpcXLabel->setStyleSheet("color:#79a2c9; font-size:11px; border:none;");
        m_stationEpcYLabel = new QLabel("--");
        m_stationEpcYLabel->setStyleSheet("color:#79a2c9; font-size:11px; border:none;");
        el->addWidget(mkFormRow("epc X:", m_stationEpcXLabel));
        el->addWidget(mkFormRow("epc Y:", m_stationEpcYLabel));
        sl->addWidget(m_stationEpcGroup);
        m_stationEpcGroup->setVisible(false);

        pl->addWidget(m_stationPropGroup);

        // ==========================================
        // 2. 线路属性组 (WidgetBezierPPTW)
        // ==========================================
        m_linePropGroup = new QWidget;
        m_linePropGroup->setStyleSheet("background:transparent; border:none;");
        auto *ll = new QVBoxLayout(m_linePropGroup);
        ll->setContentsMargins(0,0,0,0);
        ll->setSpacing(4);

        // 基础信息
        m_lineTypeCombo = new QComboBox;
        m_lineTypeCombo->addItem("直线", "line");
        m_lineTypeCombo->addItem("贝塞尔曲线", "curve");
        ll->addWidget(mkFormRow("线路类型:", m_lineTypeCombo));

        m_lineIdEdit = new QLineEdit;
        m_lineIdEdit->setReadOnly(true);
        m_lineIdEdit->setStyleSheet("background:#12151e; color:#627284; border:1px solid #232b3b; border-radius:3px; padding:2px 5px; font-size:11px;");
        ll->addWidget(mkFormRow("线路编号:", m_lineIdEdit));

        m_lineNameEdit = new QLineEdit;
        ll->addWidget(mkFormRow("线路名称:", m_lineNameEdit));

        // 起止端点
        m_lineStartLabel = new QLabel("--");
        m_lineStartLabel->setStyleSheet("background:#12151e; color:#79a2c9; border:1px solid #232b3b; border-radius:3px; padding:2px 5px; font-size:11px;");
        ll->addWidget(mkFormRow("起点 (Pos1):", m_lineStartLabel));

        m_lineEndLabel = new QLabel("--");
        m_lineEndLabel->setStyleSheet("background:#12151e; color:#79a2c9; border:1px solid #232b3b; border-radius:3px; padding:2px 5px; font-size:11px;");
        ll->addWidget(mkFormRow("终点 (Pos2):", m_lineEndLabel));

        // 运动方向
        auto *dirRow = new QWidget;
        dirRow->setStyleSheet("background:transparent; border:none;");
        auto *dirl = new QHBoxLayout(dirRow);
        dirl->setContentsMargins(0,0,0,0);
        dirl->setSpacing(4);
        m_lineDirectCombo = new QComboBox;
        m_lineDirectCombo->addItem("双向 ↔", 10);
        m_lineDirectCombo->addItem("单向前进 →", 0);
        m_lineDirectCombo->addItem("单向后退 ←", 1);
        m_lineFlipDirBtn = new QPushButton("⇄ 翻转");
        m_lineFlipDirBtn->setToolTip("翻转单向行进方向或交换端点");
        m_lineFlipDirBtn->setFixedWidth(64);
        m_lineFlipDirBtn->setStyleSheet(
            "QPushButton{background:#1e2736; border:1px solid #2e3d54; border-radius:3px; color:#8fa6bf; font-size:11px; padding:2px;}"
            "QPushButton:hover{background:#2a374c; color:#fff;}");
        connect(m_lineFlipDirBtn, &QPushButton::clicked, this, [this]() {
            int curDirect = m_lineDirectCombo->currentData().toInt();
            if (curDirect == 0)      m_lineDirectCombo->setCurrentIndex(2); // forward -> backward
            else if (curDirect == 1) m_lineDirectCombo->setCurrentIndex(1); // backward -> forward
            else {
                // 双向时交换起止显示文本
                QString t1 = m_lineStartLabel->text();
                m_lineStartLabel->setText(m_lineEndLabel->text());
                m_lineEndLabel->setText(t1);
            }
        });
        dirl->addWidget(m_lineDirectCombo, 1);
        dirl->addWidget(m_lineFlipDirBtn);
        ll->addWidget(mkFormRow("运动方向:", dirRow));

        // 运动控制参数卡片
        auto *motionCard = new QWidget;
        motionCard->setStyleSheet("background:#161c28; border:1px solid #232d3e; border-radius:4px;");
        auto *mcl = new QVBoxLayout(motionCard);
        mcl->setContentsMargins(6,6,6,6);
        mcl->setSpacing(3);
        auto *mTitle = new QLabel("运动控制参数");
        mTitle->setStyleSheet("color:#627e9e; font-size:10px; font-weight:bold; border:none;");
        mcl->addWidget(mTitle);

        const auto &mc = MotionConfigManager::instance();

        double minSpd = mc.paramMin("max_speed", 0.0);
        double maxSpd = mc.paramMax("max_speed", 1.0);
        m_lineMaxSpeedSpin = new QDoubleSpinBox;
        m_lineMaxSpeedSpin->setRange(minSpd, maxSpd);
        m_lineMaxSpeedSpin->setSingleStep(0.05);
        m_lineMaxSpeedSpin->setValue(mc.maxSpeed());
        m_lineMaxSpeedSpin->setToolTip(QString("受全局配置范围控制 (范围: [%1, %2] m/s)").arg(minSpd, 0, 'f', 2).arg(maxSpd, 0, 'f', 2));
        mcl->addWidget(mkFormRow("最大速度:", m_lineMaxSpeedSpin, "m/s"));

        double minAcc = mc.paramMin("max_acceleration", 0.3);
        double maxAcc = mc.paramMax("max_acceleration", 0.5);
        m_lineMaxAccSpin = new QDoubleSpinBox;
        m_lineMaxAccSpin->setRange(minAcc, maxAcc);
        m_lineMaxAccSpin->setSingleStep(0.05);
        m_lineMaxAccSpin->setValue(mc.maxAcceleration());
        m_lineMaxAccSpin->setToolTip(QString("受全局配置范围控制 (范围: [%1, %2] m/s²)").arg(minAcc, 0, 'f', 2).arg(maxAcc, 0, 'f', 2));
        mcl->addWidget(mkFormRow("最大加速度:", m_lineMaxAccSpin, "m/s²"));

        double minDec = mc.paramMin("max_deceleration", 0.3);
        double maxDec = mc.paramMax("max_deceleration", 0.5);
        m_lineMaxDecSpin = new QDoubleSpinBox;
        m_lineMaxDecSpin->setRange(minDec, maxDec);
        m_lineMaxDecSpin->setSingleStep(0.05);
        m_lineMaxDecSpin->setValue(mc.maxDeceleration());
        m_lineMaxDecSpin->setToolTip(QString("受全局配置范围控制 (范围: [%1, %2] m/s²)").arg(minDec, 0, 'f', 2).arg(maxDec, 0, 'f', 2));
        QWidget *rowMaxDec = mkFormRow("最大减速度:", m_lineMaxDecSpin, "m/s²");
        rowMaxDec->setVisible(false); // 隐藏最大减速度 UI
        mcl->addWidget(rowMaxDec);

        double minRot = mc.paramMin("max_angular_speed", 5.0);
        double maxRot = mc.paramMax("max_angular_speed", 60.0);
        m_lineMaxRotSpin = new QDoubleSpinBox;
        m_lineMaxRotSpin->setRange(minRot, maxRot);
        m_lineMaxRotSpin->setSingleStep(5.0);
        m_lineMaxRotSpin->setValue(mc.maxAngularSpeedDeg());
        m_lineMaxRotSpin->setToolTip(QString("受全局配置范围控制 (范围: [%1, %2] °/s)").arg(minRot, 0, 'f', 1).arg(maxRot, 0, 'f', 1));
        mcl->addWidget(mkFormRow("最大角速度:", m_lineMaxRotSpin, "°/s"));

        double minRotAcc = mc.paramMin("max_angular_acceleration", 10.0);
        double maxRotAcc = mc.paramMax("max_angular_acceleration", 30.0);
        m_lineMaxRotAccSpin = new QDoubleSpinBox;
        m_lineMaxRotAccSpin->setRange(minRotAcc, maxRotAcc);
        m_lineMaxRotAccSpin->setSingleStep(5.0);
        m_lineMaxRotAccSpin->setValue(mc.maxAngularAccelerationDeg());
        m_lineMaxRotAccSpin->setToolTip(QString("受全局配置范围控制 (范围: [%1, %2] °/s²)").arg(minRotAcc, 0, 'f', 1).arg(maxRotAcc, 0, 'f', 1));
        mcl->addWidget(mkFormRow("最大角加减速度:", m_lineMaxRotAccSpin, "°/s²"));

        m_lineReachDistSpin = new QDoubleSpinBox;
        m_lineReachDistSpin->setRange(0.01, qMax(1.0, mc.arrivalDistanceTolerance()));
        m_lineReachDistSpin->setSingleStep(0.01);
        m_lineReachDistSpin->setValue(mc.arrivalDistanceTolerance());
        mcl->addWidget(mkFormRow("到达距离精度:", m_lineReachDistSpin, "m"));

        m_lineReachAngleSpin = new QDoubleSpinBox;
        m_lineReachAngleSpin->setRange(0.1, qMax(45.0, mc.arrivalAngleToleranceDeg()));
        m_lineReachAngleSpin->setSingleStep(0.1);
        m_lineReachAngleSpin->setValue(mc.arrivalAngleToleranceDeg());
        mcl->addWidget(mkFormRow("到达角度精度:", m_lineReachAngleSpin, "°"));

        m_lineRoadWidthSpin = new QDoubleSpinBox;
        m_lineRoadWidthSpin->setRange(0.5, qMax(10.0, mc.routeWidth()));
        m_lineRoadWidthSpin->setSingleStep(0.5);
        m_lineRoadWidthSpin->setValue(mc.routeWidth());
        mcl->addWidget(mkFormRow("路线宽度:", m_lineRoadWidthSpin, "m"));

        // 监听全局运动控制参数变化，动态调整各输入控件的范围与 ToolTip
        connect(&MotionConfigManager::instance(), &MotionConfigManager::configChanged, this, [this]() {
            const auto &cfg = MotionConfigManager::instance();
            if (m_lineMaxSpeedSpin) {
                double minSpd = cfg.paramMin("max_speed", 0.0);
                double maxSpd = cfg.paramMax("max_speed", 1.0);
                m_lineMaxSpeedSpin->setRange(minSpd, maxSpd);
                m_lineMaxSpeedSpin->setToolTip(QString("受全局配置范围控制 (范围: [%1, %2] m/s)").arg(minSpd, 0, 'f', 2).arg(maxSpd, 0, 'f', 2));
                if (m_lineMaxSpeedSpin->value() > maxSpd) m_lineMaxSpeedSpin->setValue(maxSpd);
                if (m_lineMaxSpeedSpin->value() < minSpd) m_lineMaxSpeedSpin->setValue(minSpd);
            }
            if (m_lineMaxAccSpin) {
                double minAcc = cfg.paramMin("max_acceleration", 0.3);
                double maxAcc = cfg.paramMax("max_acceleration", 0.5);
                m_lineMaxAccSpin->setRange(minAcc, maxAcc);
                m_lineMaxAccSpin->setToolTip(QString("受全局配置范围控制 (范围: [%1, %2] m/s²)").arg(minAcc, 0, 'f', 2).arg(maxAcc, 0, 'f', 2));
                if (m_lineMaxAccSpin->value() > maxAcc) m_lineMaxAccSpin->setValue(maxAcc);
                if (m_lineMaxAccSpin->value() < minAcc) m_lineMaxAccSpin->setValue(minAcc);
            }
            if (m_lineMaxDecSpin) {
                double minDec = cfg.paramMin("max_deceleration", 0.3);
                double maxDec = cfg.paramMax("max_deceleration", 0.5);
                m_lineMaxDecSpin->setRange(minDec, maxDec);
                m_lineMaxDecSpin->setToolTip(QString("受全局配置范围控制 (范围: [%1, %2] m/s²)").arg(minDec, 0, 'f', 2).arg(maxDec, 0, 'f', 2));
                if (m_lineMaxDecSpin->value() > maxDec) m_lineMaxDecSpin->setValue(maxDec);
                if (m_lineMaxDecSpin->value() < minDec) m_lineMaxDecSpin->setValue(minDec);
            }
            if (m_lineMaxRotSpin) {
                double minRot = cfg.paramMin("max_angular_speed", 5.0);
                double maxRot = cfg.paramMax("max_angular_speed", 60.0);
                m_lineMaxRotSpin->setRange(minRot, maxRot);
                m_lineMaxRotSpin->setToolTip(QString("受全局配置范围控制 (范围: [%1, %2] °/s)").arg(minRot, 0, 'f', 1).arg(maxRot, 0, 'f', 1));
                if (m_lineMaxRotSpin->value() > maxRot) m_lineMaxRotSpin->setValue(maxRot);
                if (m_lineMaxRotSpin->value() < minRot) m_lineMaxRotSpin->setValue(minRot);
            }
            if (m_lineMaxRotAccSpin) {
                double minRotAcc = cfg.paramMin("max_angular_acceleration", 10.0);
                double maxRotAcc = cfg.paramMax("max_angular_acceleration", 30.0);
                m_lineMaxRotAccSpin->setRange(minRotAcc, maxRotAcc);
                m_lineMaxRotAccSpin->setToolTip(QString("受全局配置范围控制 (范围: [%1, %2] °/s²)").arg(minRotAcc, 0, 'f', 1).arg(maxRotAcc, 0, 'f', 1));
                if (m_lineMaxRotAccSpin->value() > maxRotAcc) m_lineMaxRotAccSpin->setValue(maxRotAcc);
                if (m_lineMaxRotAccSpin->value() < minRotAcc) m_lineMaxRotAccSpin->setValue(minRotAcc);
            }
            if (m_lineReachDistSpin) {
                m_lineReachDistSpin->setRange(0.01, qMax(1.0, cfg.arrivalDistanceTolerance()));
            }
            if (m_lineReachAngleSpin) {
                m_lineReachAngleSpin->setRange(0.1, qMax(45.0, cfg.arrivalAngleToleranceDeg()));
            }
            if (m_lineRoadWidthSpin) {
                m_lineRoadWidthSpin->setRange(0.5, qMax(10.0, cfg.routeWidth()));
            }
        });

        ll->addWidget(motionCard);

        // 实时避障范围卡片 (对齐 base.json 规范)
        auto *obsCard = new QWidget;
        obsCard->setStyleSheet("background:#161c28; border:1px solid #232d3e; border-radius:4px;");
        auto *ocl = new QVBoxLayout(obsCard);
        ocl->setContentsMargins(6,6,6,6);
        ocl->setSpacing(3);
        auto *oTitle = new QLabel("实时避障范围");
        oTitle->setStyleSheet("color:#627e9e; font-size:10px; font-weight:bold; border:none;");
        ocl->addWidget(oTitle);

        m_lineObsStopLenSpin = new QDoubleSpinBox;
        m_lineObsStopLenSpin->setRange(0.00, 5.00);
        m_lineObsStopLenSpin->setSingleStep(0.05);
        m_lineObsStopLenSpin->setValue(0.00);
        m_lineObsStopLenSpin->setToolTip("正向停止膨胀半径 (intrusion_stop_length)");
        ocl->addWidget(mkFormRow("正向停止膨胀:", m_lineObsStopLenSpin, "m"));

        m_lineObsStopWidSpin = new QDoubleSpinBox;
        m_lineObsStopWidSpin->setRange(0.00, 5.00);
        m_lineObsStopWidSpin->setSingleStep(0.05);
        m_lineObsStopWidSpin->setValue(0.00);
        m_lineObsStopWidSpin->setToolTip("侧向停止膨胀半径 (intrusion_stop_width)");
        ocl->addWidget(mkFormRow("侧向停止膨胀:", m_lineObsStopWidSpin, "m"));

        m_lineObsDecelLenSpin = new QDoubleSpinBox;
        m_lineObsDecelLenSpin->setRange(0.00, 5.00);
        m_lineObsDecelLenSpin->setSingleStep(0.05);
        m_lineObsDecelLenSpin->setValue(0.00);
        m_lineObsDecelLenSpin->setToolTip("正向减速膨胀半径 (intrusion_deceleration_length)");
        ocl->addWidget(mkFormRow("正向减速膨胀:", m_lineObsDecelLenSpin, "m"));

        m_lineObsDecelWidSpin = new QDoubleSpinBox;
        m_lineObsDecelWidSpin->setRange(0.00, 5.00);
        m_lineObsDecelWidSpin->setSingleStep(0.05);
        m_lineObsDecelWidSpin->setValue(0.00);
        m_lineObsDecelWidSpin->setToolTip("侧向减速膨胀半径 (intrusion_deceleration_width)");
        ocl->addWidget(mkFormRow("侧向减速膨胀:", m_lineObsDecelWidSpin, "m"));

        ll->addWidget(obsCard);
        pl->addWidget(m_linePropGroup);

        // ==========================================
        // 3. 操作按钮栏
        // ==========================================
        auto *btnRow = new QHBoxLayout;
        btnRow->setSpacing(6);
        m_propApplyBtn = new QPushButton("应用修改");
        m_propDeleteBtn = new QPushButton("删除图元");
        m_propApplyBtn->setCursor(Qt::PointingHandCursor);
        m_propDeleteBtn->setCursor(Qt::PointingHandCursor);
        m_propApplyBtn->setStyleSheet(
            "QPushButton{background:#1296DB; color:#fff; font-weight:bold; border:none; border-radius:4px; padding:6px 12px; font-size:12px;}"
            "QPushButton:hover{background:#23a2e6;}");
        m_propDeleteBtn->setStyleSheet(
            "QPushButton{background:#c0392b; color:#fff; font-weight:bold; border:none; border-radius:4px; padding:6px 12px; font-size:12px;}"
            "QPushButton:hover{background:#d9534f;}");
        connect(m_propApplyBtn,  &QPushButton::clicked, this, &MapEditWidget::applyPropsEdit);
        connect(m_propDeleteBtn, &QPushButton::clicked, this, &MapEditWidget::deleteSelected);
        btnRow->addWidget(m_propApplyBtn, 1);
        btnRow->addWidget(m_propDeleteBtn, 1);
        pl->addLayout(btnRow);

        m_propsPanel = card;
        m_propsPanel->setVisible(false);
        rl->addWidget(m_propsPanel);
    }

    // Feature list (collapsible)
    {
        auto [card, tl2] = mkCard("要素列表");

        m_tableTypeCombo = new QComboBox;
        m_tableTypeCombo->addItem("普通站点", "point");
        m_tableTypeCombo->addItem("线段",     "line");
        m_tableTypeCombo->addItem("曲线",     "curve");
        m_tableTypeCombo->addItem("充电点",   "charge");
        m_tableTypeCombo->addItem("精定位点", "highPrecision");
        m_tableTypeCombo->setStyleSheet("background:#1e2d40; color:#b0c8e0; border:1px solid rgba(255,255,255,0.10); border-radius:3px; padding:2px 4px; font-size:12px;");
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
            "border-radius:4px;padding:5px;font-size:11px;}"
            "QPushButton:hover{background:rgba(231,76,60,190);}");
        connect(delBtn, &QPushButton::clicked, this, &MapEditWidget::onDeleteFeature);
        tl2->addWidget(delBtn);

        m_tablePanel = card;
        m_tablePanel->setVisible(false);
        rl->addWidget(m_tablePanel);
    }

    rl->addStretch();
    editSplitter->addWidget(rightOuter);
    editSplitter->setStretchFactor(0, 1);
    editSplitter->setStretchFactor(1, 0);
    editSplitter->setSizes({960, 300});
    cl->addWidget(editSplitter, 1);
    root->addWidget(content, 1);

    setFocusPolicy(Qt::StrongFocus);
    setupShortcuts();

    // 抢占控制权与遥控状态同步
    connect(&TeleopController::instance(), &TeleopController::activeChanged,
            this, &MapEditWidget::onTeleopActiveChanged);
    onTeleopActiveChanged(TeleopController::instance().isActive());
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

MapEditWidget::SnapResult MapEditWidget::findSnap(const QPointF &sp, double worldTol) const
{
    SnapResult best;
    // worldTol 为地图世界单位容差(默认0.3)，转换为场景坐标距离用于比较
    // worldToScene 是线性变换，取一个单位向量的场景长度作为缩放因子
    QPointF w0 = worldToScene(0.0, 0.0);
    QPointF w1 = worldToScene(1.0, 0.0);
    double scenePerWorld = std::hypot(w1.x()-w0.x(), w1.y()-w0.y());
    double bestDist = (scenePerWorld > 1e-6) ? worldTol * scenePerWorld : worldTol;

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

    // 也吸附到线/曲线的端点
    auto checkEndpoint = [&](double wx, double wy, const QString &id, const QString &type) {
        QPointF snap = worldToScene(wx, wy);
        double dist = std::hypot(sp.x()-snap.x(), sp.y()-snap.y());
        if (dist < bestDist) {
            bestDist = dist;
            best.valid    = true;
            best.type     = type;
            best.id       = id;
            best.scenePos = snap;
            best.worldPos = {wx, wy};
        }
    };
    for (const auto &v : m_lines) {
        QJsonObject o = v.toObject();
        QString id = o.value("id").toString();
        checkEndpoint(o.value("x1").toDouble(), o.value("y1").toDouble(), id, "line");
        checkEndpoint(o.value("x2").toDouble(), o.value("y2").toDouble(), id, "line");
    }
    for (const auto &v : m_curves) {
        QJsonObject o = v.toObject();
        QString id = o.value("id").toString();
        checkEndpoint(o.value("x1").toDouble(), o.value("y1").toDouble(), id, "curve");
        checkEndpoint(o.value("x2").toDouble(), o.value("y2").toDouble(), id, "curve");
    }

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
}

void MapEditWidget::updateRulers()
{
    if (m_topRuler)
        m_topRuler->syncWithView(m_mapView, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
    if (m_leftRuler)
        m_leftRuler->syncWithView(m_mapView, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
}

// ── Data loading ──────────────────────────────────────────────────────────────

void MapEditWidget::loadMapData(std::function<void()> onDone)
{
    // 优先检查本地缓存 maps/{m_mapId}
    QString localDir = QCoreApplication::applicationDirPath() + "/maps/" + m_mapId;
    QDirIterator yIt(localDir, QStringList() << "*.yaml" << "*.yml", QDir::Files, QDirIterator::Subdirectories);
    QDirIterator pIt(localDir, QStringList() << "*.pgm", QDir::Files, QDirIterator::Subdirectories);
    QString yPath, pPath;
    while (yIt.hasNext()) {
        QString f = yIt.next();
        if (f.endsWith(".map.yaml", Qt::CaseInsensitive) || f.contains("carto_map", Qt::CaseInsensitive) || yPath.isEmpty()) {
            yPath = f;
        }
    }
    while (pIt.hasNext()) {
        QString f = pIt.next();
        if (f.endsWith(".map.pgm", Qt::CaseInsensitive) || f.contains("carto_map", Qt::CaseInsensitive) || pPath.isEmpty()) {
            pPath = f;
        }
    }

    if (!yPath.isEmpty() && !pPath.isEmpty() && QFileInfo(yPath).size() > 0 && QFileInfo(pPath).size() > 0) {
        QFile yf(yPath);
        if (yf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString yamlText = QString::fromUtf8(yf.readAll());
            yf.close();
            double resolution = 0, originX = 0, originY = 0; bool negate = false;
            if (MapPgmUtil::parseMapYaml(yamlText, resolution, originX, originY, negate) && resolution > 0) {
                QFile pf(pPath);
                if (pf.open(QIODevice::ReadOnly)) {
                    QByteArray pgmRaw = pf.readAll();
                    pf.close();
                    QImage img = MapPgmUtil::pgmToImage(pgmRaw, negate);
                    if (!img.isNull()) {
                        m_mapImage = img.convertToFormat(QImage::Format_RGB32);
                        m_originalMapImage = m_mapImage;
                        m_undoStack.clear();
                        m_redoStack.clear();
                        updateHistoryButtons();
                        m_imgW = img.width();
                        m_imgH = img.height();
                        m_worldL = originX;
                        m_worldB = originY;
                        m_worldR = m_worldL + img.width()  * resolution;
                        m_worldT = m_worldB + img.height() * resolution;

                        QFile metaFile(localDir + "/meta.json");
                        if (metaFile.open(QIODevice::ReadOnly)) {
                            QJsonObject meta = QJsonDocument::fromJson(metaFile.readAll()).object();
                            m_mapName = meta.value("mapName").toString();
                        }
                        if (m_mapName.isEmpty()) {
                            QFileInfo yfi(yPath);
                            QString base = yfi.completeBaseName();
                            if (base.endsWith(".map", Qt::CaseInsensitive)) base.chop(4);
                            m_mapName = base;
                        }
                        m_mapInfoLabel->setText(QString("路网编辑  %1").arg(m_mapName));

                        renderMap();
                        if (onDone) onDone();
                        return;
                    }
                }
            }
        }
    }

    ApiService::instance().getMap(m_mapId, [this, onDone](int code, const QJsonObject &data) {
        if (code != 200) { if (onDone) onDone(); return; }
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
        auto useThumbnailFallback = [this, thumbUrl, apiOriginX, apiOriginY, apiPhysW, apiPhysH, onDone]() {
            debugLog(QString("[loadMapData] FALLBACK to thumbnail.png url=%1").arg(thumbUrl));
            if (thumbUrl.isEmpty()) { if (onDone) onDone(); return; }
            m_worldL = apiOriginX;
            m_worldB = apiOriginY;
            m_worldR = apiOriginX + apiPhysW;
            m_worldT = apiOriginY + apiPhysH;
            ApiService::instance().downloadFile(thumbUrl, [this, onDone](bool ok, const QByteArray &raw) {
                debugLog(QString("[loadMapData] thumbnail download ok=%1 bytes=%2").arg(ok).arg(raw.size()));
                if (!ok) { if (onDone) onDone(); return; }
                m_mapImage.loadFromData(raw);
                m_mapImage = m_mapImage.convertToFormat(QImage::Format_RGB32);
                m_originalMapImage = m_mapImage;
                m_undoStack.clear();
                m_redoStack.clear();
                updateHistoryButtons();
                m_imgW = m_mapImage.width();
                m_imgH = m_mapImage.height();
                debugLog(QString("[loadMapData] FINAL(thumbnail) imgW=%1 imgH=%2 worldL=%3 worldB=%4 worldR=%5 worldT=%6")
                    .arg(m_imgW).arg(m_imgH).arg(m_worldL).arg(m_worldB).arg(m_worldR).arg(m_worldT));
                renderMap();
                if (onDone) onDone();
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
            [this, pgmUrl, useThumbnailFallback, onDone](bool yamlOk, const QByteArray &yamlRaw) {
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
                    [this, resolution, originX, originY, negate, useThumbnailFallback, onDone]
                    (bool pgmOk, const QByteArray &pgmRaw) {
                        debugLog(QString("[loadMapData] pgm download ok=%1 bytes=%2").arg(pgmOk).arg(pgmRaw.size()));
                        if (!pgmOk) { useThumbnailFallback(); return; }
                        QImage img = MapPgmUtil::pgmToImage(pgmRaw, negate);
                        debugLog(QString("[loadMapData] pgm parsed image isNull=%1 w=%2 h=%3")
                            .arg(img.isNull()).arg(img.width()).arg(img.height()));
                        if (img.isNull()) { useThumbnailFallback(); return; }

                        m_mapImage = img.convertToFormat(QImage::Format_RGB32);
                        m_originalMapImage = m_mapImage;
                        m_undoStack.clear();
                        m_redoStack.clear();
                        updateHistoryButtons();
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
                        // fitView 由 loadRouteData 完成后统一调用，避免多次 fitView 闪烁
                        if (onDone) onDone();
                    });
            });
    });
}

void MapEditWidget::loadRouteData()
{
    ApiService::instance().loadEditedMap([this](int code, const QJsonObject &data) {
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
                if (id.startsWith("c_", Qt::CaseInsensitive) || props.value("typeName").toString() == "ChargePoint" || props.value("type").toString() == "charge" || props.value("name").toString().startsWith("CP", Qt::CaseInsensitive) || props.value("name").toString().contains("充电")) {
                    forceZeroObstacleParams(obj);
                    m_charges.append(obj);
                } else if (id.startsWith("hp_", Qt::CaseInsensitive) || props.value("typeName").toString() == "ActionPoint" || props.value("type").toString() == "highPrecision" || props.value("name").toString().startsWith("AP", Qt::CaseInsensitive) || props.value("name").toString().startsWith("TL", Qt::CaseInsensitive) || isSpecialWorkstationPoint(obj)) {
                    forceZeroObstacleParams(obj);
                    m_highPrecisionPoints.append(obj);
                } else {
                    m_points.append(obj);
                }
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
                    // 支持任意阶贝塞尔曲线: 构建 controlPoints 数组
                    QJsonArray ctrlArr;
                    for (const auto &cv : coords) {
                        QJsonArray c = cv.toArray();
                        if (c.size() < 2) continue;
                        QJsonObject pt;
                        pt["x"] = c[0].toDouble();
                        pt["y"] = c[1].toDouble();
                        ctrlArr.append(pt);
                    }
                    obj["controlPoints"] = ctrlArr;
                    // 同步起终点字段(兼容旧版渲染/逻辑)
                    QJsonArray c0 = coords[0].toArray();
                    QJsonArray cN = coords.last().toArray();
                    obj["x1"] = c0[0].toDouble(); obj["y1"] = c0[1].toDouble();
                    obj["x2"] = cN[0].toDouble(); obj["y2"] = cN[1].toDouble();
                    // 兼容旧格式: 若为3点二次贝塞尔，则填充 cx/cy
                    if (coords.size() == 3) {
                        QJsonArray c1 = coords[1].toArray();
                        obj["cx"] = c1[0].toDouble(); obj["cy"] = c1[1].toDouble();
                    }
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
        debugLog(QString("[loadReflectorData] code=%1 fullResp=%2")
            .arg(code)
            .arg(QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact))));
        if (code != 200) {
            m_mapInfoLabel->setText(QString("路网编辑  %1 (反光柱加载失败)").arg(m_mapName));
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
        m_mapInfoLabel->setText(QString("路网编辑  %1 (反光柱: %2)").arg(m_mapName).arg(m_reflectorPoles.size()));
        
        debugLog(QString("[loadReflectorData] poles count=%1").arg(m_reflectorPoles.size()));
        if (!m_reflectorPoles.isEmpty()) {
            QJsonObject first = m_reflectorPoles[0].toObject();
            debugLog(QString("[loadReflectorData] first feature=%2")
                .arg(QString::fromUtf8(QJsonDocument(first).toJson(QJsonDocument::Compact))));
        }
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
    m_previewAids.clear();      // scene->clear() 已删除这些 items，清空指针避免 double-free
    m_selItem       = nullptr;
    m_snapIndicator = nullptr;   // scene cleared
    m_robotItem     = nullptr;  // scene cleared
    m_mapBgItem     = nullptr;

    if (!m_mapImage.isNull()) {
        m_mapBgItem = m_scene->addPixmap(QPixmap::fromImage(m_mapImage));
        m_mapBgItem->setTransformationMode(Qt::SmoothTransformation);
        m_mapBgItem->setZValue(-100);
    }

    double pxPerMeterX = (m_imgW > 0 && (m_worldR > m_worldL)) ? (m_imgW / (m_worldR - m_worldL)) : 20.0;
    double pxPerMeterY = (m_imgH > 0 && (m_worldT > m_worldB)) ? (m_imgH / (m_worldT - m_worldB)) : 20.0;

    auto addNode = [&](const QJsonObject &pt, const QColor &fillCol, const QColor &borderCol, const QString &type, bool visible = true) {
        QPointF sp = worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble());
        QString id = pt.value("id").toString();
        QString nameText = pt.value("name").toString();
        if (nameText.isEmpty()) nameText = id;

        double rectW = 0, rectH = 0;
        QPainterPath crossPath;
        QPainterPath path = MapPgmUtil::createPointPath(pxPerMeterX, pxPerMeterY, &rectW, &rectH, &crossPath, 0.0);

        QFont f("Segoe UI");
        double ptSize = qBound(4.0, rectH * 0.35, 7.0);
        f.setPointSizeF(ptSize);
        f.setBold(true);
        QFontMetrics fm(f);
        int tw = fm.horizontalAdvance(nameText);

        QPen pen(borderCol, 0.8);
        auto *item = m_scene->addPath(path, pen, QBrush(fillCol));
        item->setPos(sp);
        item->setZValue(10);
        item->setData(0, type); item->setData(1, id); item->setData(2, fillCol);
        item->setVisible(visible);

        // 内部红色实线精确定位小十字 (居中位于 (0,0)，与路线精准交汇，不遮挡文字)
        QPen crossPen(QColor("#E74C3C"), 0.8, Qt::SolidLine);
        crossPen.setCapStyle(Qt::FlatCap);
        auto *crossItem = m_scene->addPath(crossPath, crossPen);
        crossItem->setParentItem(item);
        crossItem->setPos(0, 0);
        crossItem->setAcceptedMouseButtons(Qt::NoButton);
        crossItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
        crossItem->setData(0, type); crossItem->setData(1, id); crossItem->setData(2, fillCol);

        // 获取朝向角 (弧度制)
        double yawVal = 0.0;
        if (pt.contains("yaw")) yawVal = pt.value("yaw").toDouble();
        else if (pt.contains("robotYaw")) yawVal = pt.value("robotYaw").toDouble();
        else if (pt.contains("theta")) yawVal = pt.value("theta").toDouble() * M_PI / 180.0;

        // 1. 地图中所有的点显示的朝向都和它设置的角度一致:
        //    ENU 坐标系逆时针为正，Qt 场景坐标系顺时针为正，旋转角为 -yawVal * 180 / M_PI
        double rotDeg = -yawVal * 180.0 / M_PI;
        item->setRotation(rotDeg);

        // 2. 在方框（站点图标）的十字中心加个简单小巧的线体箭头表示方向:
        //    item 已对齐朝向角度，在本地坐标系下从十字原点 (0, 0) 沿正 X 轴伸出小线段并带纤细折角箭头
        double arrowLen = qBound(3.5, rectW * 0.26, 6.5);
        double wingLen  = qBound(1.8, arrowLen * 0.40, 2.6);
        double wingHalf = qBound(1.2, wingLen * 0.65, 1.8);

        QPainterPath arrowLinePath;
        arrowLinePath.moveTo(0.0, 0.0);
        arrowLinePath.lineTo(arrowLen, 0.0);
        arrowLinePath.moveTo(arrowLen - wingLen, -wingHalf);
        arrowLinePath.lineTo(arrowLen, 0.0);
        arrowLinePath.lineTo(arrowLen - wingLen, wingHalf);

        QPen arrowPen(QColor("#E74C3C"), 0.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        auto *arrowItem = m_scene->addPath(arrowLinePath, arrowPen);
        arrowItem->setParentItem(item);
        arrowItem->setPos(0, 0);
        arrowItem->setZValue(12);
        arrowItem->setAcceptedMouseButtons(Qt::NoButton);
        arrowItem->setFlag(QGraphicsItem::ItemIsSelectable, false);

        m_featureItems[type + "_" + id] = item;

        // 站点名称：水平正向居中显示，反向旋转抵消父级旋转以保持永远水平易读
        auto *lbl = m_scene->addText(nameText, f);
        lbl->document()->setDocumentMargin(0);
        lbl->setDefaultTextColor(QColor("#1A1A1A"));
        lbl->setParentItem(item);
        lbl->setTransformOriginPoint(tw / 2.0, fm.height() / 2.0);
        lbl->setRotation(-rotDeg);
        double textY = (rectH >= 12.0) ? (rectH * 0.16) : (rectH / 2.0 + 1.0);
        lbl->setPos(-tw / 2.0, textY);
        lbl->setAcceptedMouseButtons(Qt::NoButton);
        lbl->setFlag(QGraphicsItem::ItemIsSelectable, false);
        lbl->setVisible(visible);
    };

    for (const auto &v : m_points)              addNode(v.toObject(), QColor(210, 230, 250, 235), QColor(41, 128, 185, 240), "point");
    for (const auto &v : m_charges)             addNode(v.toObject(), QColor(254, 243, 199, 235), QColor(217, 119, 6, 240), "charge");
    for (const auto &v : m_highPrecisionPoints) addNode(v.toObject(), QColor(254, 226, 226, 235), QColor(220, 38, 38, 240), "highPrecision", true);

    for (const auto &v : m_lines) {
        QJsonObject ln = v.toObject();
        QPointF a = worldToScene(ln.value("x1").toDouble(), ln.value("y1").toDouble());
        QPointF b = worldToScene(ln.value("x2").toDouble(), ln.value("y2").toDouble());
        QString id     = ln.value("id").toString();
        int direct     = ln.value("direct").toInt(10);
        // 直线：清爽天蓝色，线宽 1.5px (抗锯齿圆头连接)
        QPen pen(QColor("#1296DB"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        auto *li = m_scene->addLine(a.x(), a.y(), b.x(), b.y(), pen);
        li->setZValue(5); li->setData(0, "line"); li->setData(1, id); li->setData(2, pen.color());
        m_featureItems["line_" + id] = li;

        // 线路导向标 (单向或双向)
        QPointF dir = b - a;
        double len = std::hypot(dir.x(), dir.y());
        if (len > 1e-3) {
            dir /= len;
            QPointF mid = (a + b) / 2.0;
            QList<QPolygonF> arrows = MapPgmUtil::createDirectionArrows(mid, dir, direct, len);
            for (const auto &arr : arrows) {
                auto *arrItem = m_scene->addPolygon(arr, QPen(Qt::NoPen), QBrush(pen.color()));
                arrItem->setParentItem(li);
                arrItem->setZValue(6);
                arrItem->setAcceptedMouseButtons(Qt::NoButton);
            }
        }
    }

    for (const auto &v : m_curves) {
        QJsonObject cv = v.toObject();
        QString id     = cv.value("id").toString();
        int direct     = cv.value("direct").toInt(10);
        // 3阶贝塞尔曲线：鲜明典雅紫色，线宽 1.5px (抗锯齿圆头连接)
        QPen pen(QColor("#9B59B6"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

        // 支持多阶贝塞尔曲线: 优先使用 controlPoints 数组
        QJsonArray ctrlArr = cv.value("controlPoints").toArray();
        QPainterPath path;
        QList<QPointF> curveScenePts;
        if (!ctrlArr.isEmpty()) {
            for (const auto &pv : ctrlArr) {
                QPointF sp = worldToScene(pv.toObject().value("x").toDouble(),
                                          pv.toObject().value("y").toDouble());
                curveScenePts.append(sp);
            }
            path = MapPgmUtil::getBezierPath(curveScenePts);
        } else {
            // 兼容旧格式: cx/cy 二次贝塞尔
            QPointF p1 = worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble());
            QPointF pc = worldToScene(cv.value("cx").toDouble(),  cv.value("cy").toDouble());
            QPointF p2 = worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble());
            curveScenePts.append(p1); curveScenePts.append(pc); curveScenePts.append(p2);
            path.moveTo(p1); path.quadTo(pc, p2);
        }
        auto *pi = m_scene->addPath(path, pen);
        pi->setZValue(5); pi->setData(0, "curve"); pi->setData(1, id); pi->setData(2, pen.color());
        m_featureItems["curve_" + id] = pi;

        if (curveScenePts.size() >= 2) {
            double cLen = std::hypot(curveScenePts.last().x() - curveScenePts.first().x(),
                                     curveScenePts.last().y() - curveScenePts.first().y());
            QList<QPolygonF> arrows = MapPgmUtil::createBezierDirectionArrows(curveScenePts, direct, cLen);
            for (const auto &arr : arrows) {
                auto *arrItem = m_scene->addPolygon(arr, QPen(Qt::NoPen), QBrush(pen.color()));
                arrItem->setParentItem(pi);
                arrItem->setZValue(6);
                arrItem->setAcceptedMouseButtons(Qt::NoButton);
            }
        }

        rebuildCurveHandles(id);
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
            auto *e = m_scene->addEllipse(-5, -5, 10, 10,
                QPen(QColor("#caa020"), 1.5), QBrush(QColor("#f0c040")));
            e->setPos(sp);
            e->setZValue(8);
        }
    }

    if (m_showGrid) updateGrid();
    m_scene->setSceneRect(0, 0, m_imgW, m_imgH);
    updateRulers();

    // 实时设备位置标记：110*60 浅蓝车头矩形，与真实地图比例尺吻合
    if (!m_robotPixmap.isNull()) {
        m_robotItem = m_scene->addPixmap(m_robotPixmap);
        m_robotItem->setZValue(50);
        m_robotItem->setVisible(false);
    }
    updateRobotMarker();

    // Handle visibility matches current mode
    bool showH = (m_drawStatus == Drag);
    for (auto &lst : m_curveHandles.values())
        for (auto *h : lst) if (h) h->setVisible(showH);
}

void MapEditWidget::fitView()
{
    QRectF r = m_scene->sceneRect();
    if (r.isEmpty()) return;
    // 手动计算缩放比例使短边铺满，再居中，避免 fitInView 的边距/margin 问题
    m_mapView->resetTransform();
    QRectF vr = m_mapView->viewport()->rect();
    if (vr.isEmpty()) return;
    double sx = vr.width()  / r.width();
    double sy = vr.height() / r.height();
    double s  = qMin(sx, sy);   // 短边铺满
    m_mapView->scale(s, s);
    // 居中: 将场景中心移到视口中心
    QPointF center = r.center();
    m_mapView->centerOn(center);
    m_hasInitialFit = true;
    updateRulers();
}

void MapEditWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_directionPad && m_mapView) {
        m_directionPad->move(16, qMax(16, m_mapView->height() - m_directionPad->height() - 16));
        m_directionPad->raise();
    }
    if (!m_hasInitialFit && m_imgW > 0 && m_imgH > 0)
        QTimer::singleShot(50, this, [this]{ fitView(); });
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

    // 支持多阶贝塞尔曲线: 优先使用 controlPoints 数组
    QJsonArray ctrlArr = cv.value("controlPoints").toArray();
    QList<QPointF> scenePts;
    if (!ctrlArr.isEmpty()) {
        for (const auto &pv : ctrlArr)
            scenePts.append(worldToScene(pv.toObject().value("x").toDouble(),
                                         pv.toObject().value("y").toDouble()));
    } else {
        // 兼容旧格式: cx/cy 二次贝塞尔
        scenePts.append(worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble()));
        scenePts.append(worldToScene(cv.value("cx").toDouble(),  cv.value("cy").toDouble()));
        scenePts.append(worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble()));
    }

    // 起终点已关联节点(startid/endid 非空)时，该位置由节点本身显示，
    // 不再绘制端点手柄，避免覆盖原有点；中间控制点始终绘制橙色手柄。
    QString startId = cv.value("startid").toString();
    QString endId   = cv.value("endid").toString();

    QList<QGraphicsEllipseItem*> handles;
    // 用 nullptr 占位端点，保持 handles 索引与 scenePts 一致(0=起点, last=终点)，
    // 便于 updateCurveShape / 手柄拖拽等按索引访问的逻辑不受影响。
    for (int i = 0; i < scenePts.size(); ++i) {
        bool isEndpoint = (i == 0 || i == scenePts.size() - 1);
        bool hasNode    = isEndpoint && ((i == 0) ? !startId.isEmpty() : !endId.isEmpty());
        if (hasNode) {
            handles.append(nullptr);   // 端点由节点显示，不绘制手柄
            continue;
        }
        // 端点未关联节点时用天蓝色，中间控制点用白底蓝边精致圆点
        QColor col = isEndpoint ? QColor("#5DADE2") : QColor("#3498DB");
        auto *h = m_scene->addEllipse(-4, -4, 8, 8,
                                       QPen(QColor("#2980B9"), 1.2),
                                       QBrush(isEndpoint ? QColor("#D4E6F1") : Qt::white));
        h->setPos(scenePts[i]);
        h->setZValue(50);
        h->setData(Qt::UserRole,   id);
        h->setData(Qt::UserRole+1, i);
        h->setVisible(m_drawStatus == Drag);
        handles.append(h);
    }
    // 控制臂(相邻控制点之间的虚线参考线)
    auto arm = QPen(QColor(170, 185, 215, 180), 1, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
    for (int i = 0; i + 1 < scenePts.size(); ++i) {
        m_scene->addLine(scenePts[i].x(), scenePts[i].y(),
                         scenePts[i+1].x(), scenePts[i+1].y(), arm)->setZValue(49);
    }

    m_curveHandles[id] = handles;
}

void MapEditWidget::removeAllHandles()
{
    for (auto &lst : m_curveHandles.values())
        for (auto *h : lst) { if (h) { m_scene->removeItem(h); delete h; } }
    m_curveHandles.clear();
}

void MapEditWidget::updateCurveShape(const QString &id)
{
    auto it = m_curveHandles.find(id);
    if (it == m_curveHandles.end() || it.value().size() < 2) return;
    // 端点手柄可能为 nullptr(已关联节点时由节点显示)，需从曲线 JSON 取端点坐标
    QJsonObject cv;
    for (const auto &v : m_curves) {
        auto o = v.toObject();
        if (o.value("id").toString() == id) { cv = o; break; }
    }
    QPointF startScene = worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble());
    QPointF endScene   = worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble());
    // 支持任意阶贝塞尔曲线: 收集所有控制点位置
    QList<QPointF> pts;
    const auto &lst = it.value();
    for (int i = 0; i < lst.size(); ++i) {
        if (lst[i]) pts.append(lst[i]->pos());
        else if (i == 0)            pts.append(startScene);
        else if (i == lst.size()-1) pts.append(endScene);
        else                        pts.append(QPointF());   // 不应发生
    }
    QPainterPath path = MapPgmUtil::getBezierPath(pts);
    auto *pi = qgraphicsitem_cast<QGraphicsPathItem*>(m_featureItems.value("curve_" + id));
    if (pi) {
        pi->setPath(path);
        // 同步更新曲线中点箭头
        for (auto *child : pi->childItems()) {
            delete child;
        }
        if (pts.size() >= 2) {
            int direct = cv.value("direct").toInt(10);
            QColor col = pi->pen().color();
            double cLen = std::hypot(pts.last().x() - pts.first().x(), pts.last().y() - pts.first().y());
            QList<QPolygonF> arrows = MapPgmUtil::createBezierDirectionArrows(pts, direct, cLen);
            for (const auto &arr : arrows) {
                auto *arrItem = m_scene->addPolygon(arr, QPen(Qt::NoPen), QBrush(col));
                arrItem->setParentItem(pi);
                arrItem->setZValue(6);
                arrItem->setAcceptedMouseButtons(Qt::NoButton);
            }
        }
    }
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
            QPen pen(QColor("#9B59B6"), 1.5, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
            m_previewItem = m_scene->addPath(path, pen);
            m_previewItem->setZValue(100);
            // 显示控制点连线(辅助参考线)
            QPen aidPen(QColor(170, 185, 215, 180), 1.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
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
        QPen pen(m_drawStatus == Curve ? QColor("#9B59B6") : QColor("#5DADE2"), 1.5, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
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

// ── 栅格地图涂绘与历史 ────────────────────────────────────────────────────────

void MapEditWidget::updateHistoryButtons()
{
    if (m_undoBtn) m_undoBtn->setEnabled(!m_undoStack.isEmpty());
    if (m_redoBtn) m_redoBtn->setEnabled(!m_redoStack.isEmpty());
}

void MapEditWidget::pushMapUndo()
{
    if (m_mapImage.isNull()) return;
    m_undoStack.append(m_mapImage);
    if (m_undoStack.size() > 30) m_undoStack.removeFirst();
    m_redoStack.clear();
    m_dirty = true;
    updateHistoryButtons();
}

void MapEditWidget::undoMap()
{
    if (m_undoStack.isEmpty()) return;
    m_redoStack.append(m_mapImage);
    if (m_redoStack.size() > 30) m_redoStack.removeFirst();
    m_mapImage = m_undoStack.takeLast();
    if (m_mapBgItem) {
        m_mapBgItem->setPixmap(QPixmap::fromImage(m_mapImage));
        m_mapBgItem->setTransformationMode(Qt::SmoothTransformation);
    }
    m_dirty = true;
    updateHistoryButtons();
}

void MapEditWidget::redoMap()
{
    if (m_redoStack.isEmpty()) return;
    m_undoStack.append(m_mapImage);
    if (m_undoStack.size() > 30) m_undoStack.removeFirst();
    m_mapImage = m_redoStack.takeLast();
    if (m_mapBgItem) {
        m_mapBgItem->setPixmap(QPixmap::fromImage(m_mapImage));
        m_mapBgItem->setTransformationMode(Qt::SmoothTransformation);
    }
    m_dirty = true;
    updateHistoryButtons();
}

void MapEditWidget::onUndo()
{
    undoMap();
}

void MapEditWidget::onRedo()
{
    redoMap();
}

void MapEditWidget::paintStroke(const QPointF &from, const QPointF &to, bool isPoint)
{
    if (m_mapImage.isNull()) return;

    if (m_mapImage.format() != QImage::Format_RGB32 && m_mapImage.format() != QImage::Format_ARGB32_Premultiplied) {
        m_mapImage = m_mapImage.convertToFormat(QImage::Format_RGB32);
    }

    QPainter painter(&m_mapImage);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 画笔 (Brush): 涂黑 (RGB 0, 0, 0 / 标记障碍物/墙体)
    // 橡皮 (Eraser): 擦除为纯白 (RGB 255, 255, 255 / 标记为可通行自由空间)
    QColor color = (m_drawStatus == Brush) ? QColor(0, 0, 0) : QColor(255, 255, 255);
    QPen pen(color, m_brushSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    if (isPoint) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(color));
        double r = m_brushSize / 2.0;
        painter.drawEllipse(to, r, r);
    } else {
        painter.setPen(pen);
        painter.setBrush(QBrush(color));
        painter.drawLine(from, to);
    }
    painter.end();

    if (m_mapBgItem) {
        m_mapBgItem->setPixmap(QPixmap::fromImage(m_mapImage));
        m_mapBgItem->setTransformationMode(Qt::SmoothTransformation);
    }
}

void MapEditWidget::setDrawMode(int mode)
{
    cancelDraw();
    m_drawStatus = static_cast<DrawStatus>(mode);
    for (auto it = m_toolBtns.begin(); it != m_toolBtns.end(); ++it) {
        it.value()->setChecked(it.key() == mode);
    }
    if (m_drawStatus == Drag) {
        m_mapView->viewport()->setCursor(Qt::OpenHandCursor);
    } else if (m_drawStatus == Brush || m_drawStatus == Eraser) {
        m_mapView->viewport()->setCursor(Qt::CrossCursor);
    } else {
        m_mapView->viewport()->setCursor(Qt::CrossCursor);
    }
    bool showH = (m_drawStatus == Drag);
    for (auto &lst : m_curveHandles.values())
        for (auto *h : lst) if (h) h->setVisible(showH);

    // 站点类型子工具条: 站点类工具(站点/工作站/特殊工作站)激活时显示
    bool isStationMode = (m_drawStatus == Point || m_drawStatus == HighPrecision || m_drawStatus == Charge);
    if (m_stationBar) m_stationBar->setVisible(isStationMode);
    for (auto it = m_stationBtns.begin(); it != m_stationBtns.end(); ++it)
        it.value()->setChecked(it.key() == m_drawStatus);
}

void MapEditWidget::cancelDraw()
{
    m_drawHasP1 = false;
    m_drawP1 = {};
    m_drawP1Id.clear();
    m_curveCtrlPts.clear();
    m_curveCtrlIds.clear();
    m_isPainting = false;
    clearPreview();
    updateSnapIndicator({});
}

// ── Feature addition ──────────────────────────────────────────────────────────

// 从数组中找出前缀匹配的最大数字编号，返回 max+1，保证不重复
int MapEditWidget::nextIdNum(const QJsonArray &arr, const QString &prefix)
{
    int maxN = 0;
    for (const auto &v : arr) {
        QString id = v.toObject().value("id").toString();
        if (id.startsWith(prefix)) {
            bool ok = false;
            int n = id.mid(prefix.length()).toInt(&ok);
            if (ok && n > maxN) maxN = n;
        }
    }
    return maxN + 1;
}

void MapEditWidget::addPoint(const QPointF &sp, double yaw, double theta)
{
    QPointF w = sceneToWorld(sp.x(), sp.y());
    int n = nextIdNum(m_points, "p_");
    QJsonObject pt;
    pt["id"] = QString("p_%1").arg(n);
    pt["name"] = QString("LM%1").arg(n);
    pt["typeName"] = "LandMark";
    pt["x"] = w.x();
    pt["y"] = w.y();
    pt["yaw"] = yaw;            // 绝对朝向角 (弧度制)
    pt["theta"] = theta;        // 角度制朝向角
    pt["robotYaw"] = 0.0;       // 对齐规范: robotYaw 设为 0.0
    pt["roll"] = 0.0;
    pt["pitch"] = 0.0;
    pt["offset"] = 0.0;
    pt["isyawfix"] = false;
    pt["rdsDockable"] = true;
    pt["allowRotate"] = true;
    pt["useDownPGV"] = false;
    const auto &mc = MotionConfigManager::instance();
    pt["intrusion_stop_length"]         = mc.defaultObsStopLength();
    pt["intrusion_stop_width"]          = mc.defaultObsStopWidth();
    pt["intrusion_deceleration_length"] = mc.defaultObsDecelLength();
    pt["intrusion_deceleration_width"]  = mc.defaultObsDecelWidth();
    pt["is_custom_obstacle"]            = false;
    m_points.append(pt);
    m_dirty = true;
    renderMap();
    if (m_tableVisible && m_currentTable == "point") refreshTable();
}

void MapEditWidget::addCharge(const QPointF &sp, double epcX, double epcY, double epcTheta, double yaw, double theta)
{
    QPointF w = sceneToWorld(sp.x(), sp.y());
    int n = nextIdNum(m_charges, "c_");
    QJsonObject ch;
    ch["id"] = QString("c_%1").arg(n); ch["name"] = QString("CP_%1").arg(n);
    // 类型名称: 仅新增 typeName 字段, 不改变原有 id 与类型
    ch["typeName"] = "ChargePoint";
    ch["x"] = w.x(); ch["y"] = w.y();
    // 特殊工作站(充电桩)相对定位末端坐标: 存入 epc_x, epc_y, epc_theta，并补充存入 epc_Yaw/epc_yaw 保证后台兼容
    ch["epc_x"] = epcX;
    ch["epc_y"] = epcY;
    ch["epc_theta"] = epcTheta;
    ch["epcX"] = epcX;
    ch["epcY"] = epcY;
    ch["epcTheta"] = epcTheta;
    ch["epc_Yaw"] = epcTheta;
    ch["epc_yaw"] = epcTheta;
    ch["yaw"] = yaw;            // Vue charge.js: yaw 存设备打点时的绝对姿态角 carYaw (弧度制)
    ch["theta"] = theta;        // Vue charge.js: theta 存设备打点时的角度制朝向角 (如 180°/0°)
    ch["roll"] = 0.0;
    ch["pitch"] = 0.0;
    ch["offset"] = 0.0;
    ch["isyawfix"] = false;
    // Vue charge.js addChargeExtraProps: robotYaw 始终为 0
    ch["robotYaw"] = 0.0;
    ch["rdsDockable"] = true;
    ch["allowRotate"] = true;
    ch["useDownPGV"] = false;
    forceZeroObstacleParams(ch);
    m_charges.append(ch);
    m_dirty = true;
    renderMap();
    if (m_tableVisible && m_currentTable == "charge") refreshTable();
}

void MapEditWidget::addHighPrecisionPoint(const QPointF &sp, double epcX, double epcY, double epcTheta, double yaw, double theta)
{
    QPointF w = sceneToWorld(sp.x(), sp.y());
    int n = nextIdNum(m_highPrecisionPoints, "hp_");
    QJsonObject hp;
    hp["id"] = QString("hp_%1").arg(n); hp["name"] = QString("AP%1").arg(n);
    // 类型名称: 仅新增 typeName 字段, 不改变原有 id 与类型
    hp["typeName"] = "ActionPoint";
    hp["x"] = w.x(); hp["y"] = w.y();
    // 工作站(高精定位点)相对定位末端坐标: 存入 epc_x, epc_y, epc_theta，并补充存入 epc_Yaw/epc_yaw 保证后台兼容
    hp["epc_x"] = epcX;
    hp["epc_y"] = epcY;
    hp["epc_theta"] = epcTheta;
    hp["epcX"] = epcX;
    hp["epcY"] = epcY;
    hp["epcTheta"] = epcTheta;
    hp["epc_Yaw"] = epcTheta;
    hp["epc_yaw"] = epcTheta;
    hp["yaw"] = yaw;            // Vue highPrecision.js: yaw 存设备打点时的绝对姿态角 carYaw (弧度制)
    hp["theta"] = theta;        // Vue highPrecision.js: theta 存设备打点时的角度制朝向角 (如 180°/0°)
    hp["roll"] = 0.0;
    hp["pitch"] = 0.0;
    hp["offset"] = 0.0;
    hp["isyawfix"] = false;
    // Vue highPrecision.js addHighPrecisionExtraProps: robotYaw 始终为 0
    hp["robotYaw"] = 0.0;
    hp["rdsDockable"] = true;
    hp["allowRotate"] = true;
    hp["useDownPGV"] = false;
    forceZeroObstacleParams(hp);
    m_highPrecisionPoints.append(hp);
    m_dirty = true;
    renderMap();
    if (m_tableVisible && m_currentTable == "highPrecision") refreshTable();
}

// 设备打点: 通过设备相对定位(epc)添加充电桩/高精度点
// 参考 Vue edit.vue submitPointForm():
//   1. MQTT 发布 /rob/{id}/pointType 让设备记录当前相对位置
//   2. 等待 1500ms 让设备更新 epc_x/epc_y/epc_theta 到 /rob/{id}/info
//   3. 用设备当前位置(carX, carY) + epc 坐标添加点
void MapEditWidget::addDevicePoint(const QString &type)
{
    if (m_devicePointPending) {
        Toast::info(this, "上一次设备打点尚未完成，请稍候");
        return;
    }
    if (!m_hasCarPose) {
        Toast::info(this, "未收到设备位置信息，无法打点");
        return;
    }
    if (m_currentCarId.isEmpty()) {
        Toast::info(this, "未选择设备，无法打点");
        return;
    }

    m_devicePointPending = true;
    QString carId = m_currentCarId;

    // 1. 发布 /rob/{id}/pointType 命令让设备记录当前相对位置
    QJsonObject data;
    data["point_type"] = (type == "charge") ? "charge" : "highPrecision";
    data["sender"] = "Web";
    MqttService::instance().publish(
        QString("/rob/%1/pointType").arg(carId),
        QJsonDocument(data).toJson(QJsonDocument::Compact));

    // 2. 阻塞式进度条：等待 3s 让设备更新 epc 坐标
    //    QProgressDialog 模态阻塞 UI，QTimer 每 100ms 推进进度
    const int kWaitMs = 3000;
    const int kStepMs = 100;
    auto *prog = new QProgressDialog(
        QString("正在获取设备相对定位 (epc)...\n剩余 %1 秒").arg(kWaitMs / 1000),
        QString(), 0, kWaitMs / kStepMs, this);
    prog->setWindowTitle("设备打点");
    prog->setWindowModality(Qt::WindowModal);
    prog->setMinimumDuration(0);   // 立即显示
    prog->setAutoClose(false);
    prog->setAutoReset(false);
    prog->setValue(0);
    prog->setCancelButton(nullptr);  // 不允许取消

    auto *tick = new QTimer(prog);
    tick->setInterval(kStepMs);
    int elapsed = 0;
    QObject::connect(tick, &QTimer::timeout, this, [prog, tick, kStepMs, kWaitMs, &elapsed, type, this]() {
        elapsed += kStepMs;
        int remainSec = (kWaitMs - elapsed + 999) / 1000;
        prog->setLabelText(QString("正在获取设备相对定位 (epc)...\n剩余 %1 秒").arg(remainSec));
        prog->setValue(elapsed / kStepMs);

        if (elapsed >= kWaitMs) {
            tick->stop();
            prog->accept();

            m_devicePointPending = false;
            if (!m_hasCarPose) {
                Toast::error(this, "设备位置信息丢失，打点失败");
                return;
            }

            QPointF sp = worldToScene(m_carX, m_carY);
            if (type == "charge") {
                addCharge(sp, m_epcX, m_epcY, m_epcTheta, m_carYaw, m_carTheta);
                Toast::success(this, QString("已在设备位置(%1, %2)添加充电点\nepc(%3, %4)")
                    .arg(m_carX, 0, 'f', 2).arg(m_carY, 0, 'f', 2)
                    .arg(m_epcX, 0, 'f', 2).arg(m_epcY, 0, 'f', 2));
            } else if (type == "highPrecision") {
                addHighPrecisionPoint(sp, m_epcX, m_epcY, m_epcTheta, m_carYaw, m_carTheta);
                Toast::success(this, QString("已在设备位置(%1, %2)添加精定位点\nepc(%3, %4)")
                    .arg(m_carX, 0, 'f', 2).arg(m_carY, 0, 'f', 2)
                    .arg(m_epcX, 0, 'f', 2).arg(m_epcY, 0, 'f', 2));
            }
        }
    });
    tick->start();
    prog->exec();
    prog->deleteLater();
}

void MapEditWidget::addLine(const QPointF &a, const QPointF &b,
                            const QString &startId, const QString &endId)
{
    if (!startId.isEmpty() && !endId.isEmpty() && hasRouteBetween(startId, endId, a, b)) {
        Toast::warning(this, "两站点之间已存在连线，不可重复绘制");
        return;
    }
    QPointF wa = sceneToWorld(a.x(), a.y()), wb = sceneToWorld(b.x(), b.y());
    int n = nextIdNum(m_lines, "l_");
    const auto &mc = MotionConfigManager::instance();
    QJsonObject ln;
    ln["id"] = QString("l_%1").arg(n); ln["name"] = QString("线%1").arg(n);
    ln["x1"] = wa.x(); ln["y1"] = wa.y(); ln["x2"] = wb.x(); ln["y2"] = wb.y();
    ln["direct"] = 10; ln["startid"] = startId; ln["endid"] = endId; // 默认双向 ↔
    ln["maxspeed"]                 = mc.maxSpeed();
    ln["max_acceleration"]         = mc.maxAcceleration();
    ln["maxacc"]                   = mc.maxAcceleration();
    ln["max_deceleration"]         = mc.maxDeceleration();
    ln["maxdec"]                   = mc.maxDeceleration();
    ln["max_angular_speed"]        = mc.maxAngularSpeedDeg();
    ln["maxrot"]                   = mc.maxAngularSpeedDeg();
    ln["max_angular_acceleration"] = mc.maxAngularAccelerationDeg();
    ln["maxrotacc"]                = mc.maxAngularAccelerationDeg();
    ln["reachdist"]  = mc.arrivalDistanceTolerance();
    ln["reachangle"] = mc.arrivalAngleToleranceDeg();
    ln["width"]      = mc.routeWidth();
    ln["intrusion_stop_length"]         = mc.defaultObsStopLength();
    ln["intrusion_stop_width"]          = mc.defaultObsStopWidth();
    ln["intrusion_deceleration_length"] = mc.defaultObsDecelLength();
    ln["intrusion_deceleration_width"]  = mc.defaultObsDecelWidth();
    ln["is_custom_obstacle"]            = false;
    m_lines.append(ln);
    m_dirty = true;
    renderMap();
    if (m_tableVisible && m_currentTable == "line") refreshTable();
}

void MapEditWidget::addCurve(const QPointF &a, const QPointF &b,
                             const QString &startId, const QString &endId)
{
    if (!startId.isEmpty() && !endId.isEmpty() && hasRouteBetween(startId, endId, a, b)) {
        Toast::warning(this, "两站点之间已存在连线，不可重复绘制");
        return;
    }
    QPointF mid  = (a + b) / 2;
    QPointF wa   = sceneToWorld(a.x(), a.y());
    QPointF wc   = sceneToWorld(mid.x(), mid.y());
    QPointF wb   = sceneToWorld(b.x(), b.y());
    int n = nextIdNum(m_curves, "b_");
    const auto &mc = MotionConfigManager::instance();
    QJsonObject cv;
    cv["id"] = QString("b_%1").arg(n); cv["name"] = QString("曲%1").arg(n);
    cv["x1"] = wa.x(); cv["y1"] = wa.y();
    cv["cx"] = wc.x(); cv["cy"] = wc.y();
    cv["x2"] = wb.x(); cv["y2"] = wb.y();
    cv["direct"] = 10; cv["startid"] = startId; cv["endid"] = endId; // 默认双向 ↔
    cv["maxspeed"]                 = mc.maxSpeed();
    cv["max_acceleration"]         = mc.maxAcceleration();
    cv["maxacc"]                   = mc.maxAcceleration();
    cv["max_deceleration"]         = mc.maxDeceleration();
    cv["maxdec"]                   = mc.maxDeceleration();
    cv["max_angular_speed"]        = mc.maxAngularSpeedDeg();
    cv["maxrot"]                   = mc.maxAngularSpeedDeg();
    cv["max_angular_acceleration"] = mc.maxAngularAccelerationDeg();
    cv["maxrotacc"]                = mc.maxAngularAccelerationDeg();
    cv["reachdist"]  = mc.arrivalDistanceTolerance();
    cv["reachangle"] = mc.arrivalAngleToleranceDeg();
    cv["width"]      = mc.routeWidth();
    cv["intrusion_stop_length"]         = mc.defaultObsStopLength();
    cv["intrusion_stop_width"]          = mc.defaultObsStopWidth();
    cv["intrusion_deceleration_length"] = mc.defaultObsDecelLength();
    cv["intrusion_deceleration_width"]  = mc.defaultObsDecelWidth();
    cv["is_custom_obstacle"]            = false;
    m_curves.append(cv);
    m_dirty = true;
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
    // 起点: 已在首次点击时通过 ensureNodeAt 处理；此处做一次校验，若意外无 id
    // 则补吸附/创建(例如从外部状态进入 finalize)。
    if (m_curveCtrlIds.isEmpty() || m_curveCtrlIds.first().isEmpty()) {
        SnapResult head = ensureNodeAt(m_curveCtrlPts.first());
        m_curveCtrlPts.first() = head.scenePos;
        m_curveCtrlIds.first() = head.id;
    }
    // 终点: 强制吸到附近已有点，否则在该位置创建一个新节点，保证曲线两端
    // 始终关联到节点 id，而不是悬空端点。
    if (m_curveCtrlIds.size() < 2 || m_curveCtrlIds.last().isEmpty()) {
        SnapResult tail = ensureNodeAt(m_curveCtrlPts.last());
        m_curveCtrlPts.last() = tail.scenePos;
        m_curveCtrlIds.last() = tail.id;
    }

    QString startId = m_curveCtrlIds.first();
    QString endId   = m_curveCtrlIds.last();
    if (startId == endId) {
        Toast::warning(this, "起点与终点不能为同一站点");
        cancelDraw();
        return;
    }
    if (hasRouteBetween(startId, endId, m_curveCtrlPts.first(), m_curveCtrlPts.last())) {
        Toast::warning(this, "两站点之间已存在连线，不可重复绘制");
        cancelDraw();
        return;
    }

    // 保存数据后立即清空状态，防止 addBezierCurve → renderMap 链中重入
    QList<QPointF> pts = m_curveCtrlPts;
    QList<QString> ids = m_curveCtrlIds;
    m_curveCtrlPts.clear();
    m_curveCtrlIds.clear();
    m_drawHasP1 = false;
    m_drawP1 = {};
    m_drawP1Id.clear();
    // 注意: addBezierCurve → renderMap → scene->clear() 会删除 m_previewAids 中的 items
    // 所以先清空 m_previewAids 防止 clearPreview 二次删除
    m_previewAids.clear();
    if (m_previewItem) { m_scene->removeItem(m_previewItem); delete m_previewItem; m_previewItem = nullptr; }

    addBezierCurve(pts, ids);
}

void MapEditWidget::addBezierCurve(const QList<QPointF> &scenePts,
                                   const QList<QString> &snapIds)
{
    if (scenePts.size() < 2) return;
    QString startId = snapIds.isEmpty()     ? QString() : snapIds.first();
    QString endId   = (snapIds.size() >= 2) ? snapIds.last() : QString();
    if (!startId.isEmpty() && !endId.isEmpty() && hasRouteBetween(startId, endId, scenePts.first(), scenePts.last())) {
        Toast::warning(this, "两站点之间已存在连线，不可重复绘制");
        return;
    }
    int n = nextIdNum(m_curves, "b_");
    QJsonObject cv;
    cv["id"]   = QString("b_%1").arg(n);
    cv["name"] = QString("曲%1").arg(n);
    cv["direct"] = 10; // 默认双向 ↔

    // 起终点(世界坐标)
    QPointF wStart = sceneToWorld(scenePts.first().x(), scenePts.first().y());
    QPointF wEnd   = sceneToWorld(scenePts.last().x(),  scenePts.last().y());
    cv["x1"] = wStart.x(); cv["y1"] = wStart.y();
    cv["x2"] = wEnd.x();   cv["y2"] = wEnd.y();
    cv["startid"] = startId;
    cv["endid"]   = endId;

    const auto &mc = MotionConfigManager::instance();
    cv["maxspeed"]                 = mc.maxSpeed();
    cv["max_acceleration"]         = mc.maxAcceleration();
    cv["maxacc"]                   = mc.maxAcceleration();
    cv["max_deceleration"]         = mc.maxDeceleration();
    cv["maxdec"]                   = mc.maxDeceleration();
    cv["max_angular_speed"]        = mc.maxAngularSpeedDeg();
    cv["maxrot"]                   = mc.maxAngularSpeedDeg();
    cv["max_angular_acceleration"] = mc.maxAngularAccelerationDeg();
    cv["maxrotacc"]                = mc.maxAngularAccelerationDeg();
    cv["reachdist"]  = mc.arrivalDistanceTolerance();
    cv["reachangle"] = mc.arrivalAngleToleranceDeg();
    cv["width"]      = mc.routeWidth();
    cv["intrusion_stop_length"]         = mc.defaultObsStopLength();
    cv["intrusion_stop_width"]          = mc.defaultObsStopWidth();
    cv["intrusion_deceleration_length"] = mc.defaultObsDecelLength();
    cv["intrusion_deceleration_width"]  = mc.defaultObsDecelWidth();
    cv["is_custom_obstacle"]            = false;

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
    m_dirty = true;
    renderMap();
    if (m_tableVisible && m_currentTable == "curve") refreshTable();
}

MapEditWidget::SnapResult MapEditWidget::ensureNodeAt(const QPointF &sp)
{
    // 曲线端点必须落在某个节点上: 先尝试吸附到附近已有的节点/高精度点/充电桩，
    // 命中则直接复用该点；未命中则在该位置创建一个新的普通节点，保证线首尾
    // 始终关联到一个节点 id，而不是悬空一个无 id 的端点。
    SnapResult snap = findSnap(sp);
    if (snap.valid) return snap;

    // 附近无点 → 创建新节点。addPoint 内部会 renderMap，此处不重复渲染。
    addPoint(sp);
    // addPoint 会按 m_points 当前数量生成 id，取最后追加的元素 id 作为关联 id
    SnapResult r;
    if (!m_points.isEmpty()) {
        QJsonObject o = m_points.last().toObject();
        r.valid    = true;
        r.type     = "point";
        r.id       = o.value("id").toString();
        r.worldPos = QPointF(o.value("x").toDouble(), o.value("y").toDouble());
        r.scenePos = worldToScene(r.worldPos.x(), r.worldPos.y());
    }
    return r;
}

bool MapEditWidget::hasRouteBetween(const QString &startId, const QString &endId,
                                    const QPointF &sceneStart, const QPointF &sceneEnd) const
{
    const bool hasIds = !startId.isEmpty() && !endId.isEmpty() && (startId != endId);
    const bool hasCoords = !sceneStart.isNull() && !sceneEnd.isNull();
    QPointF wa, wb;
    if (hasCoords) {
        wa = sceneToWorld(sceneStart.x(), sceneStart.y());
        wb = sceneToWorld(sceneEnd.x(), sceneEnd.y());
    }

    auto matchEndpoints = [&](const QString &s, const QString &e,
                              double x1, double y1, double x2, double y2) -> bool {
        // 1. 优先按站点 ID 匹配（无向：A-B 或 B-A 均视为同一路段）
        if (hasIds && !s.isEmpty() && !e.isEmpty()) {
            if ((s == startId && e == endId) || (s == endId && e == startId)) {
                return true;
            }
        }
        // 2. 坐标容差兜底匹配 (容差 0.15 米，防止历史导入数据缺少 ID)
        if (hasCoords) {
            const double tol = 0.15;
            bool directMatch = (std::hypot(wa.x() - x1, wa.y() - y1) < tol &&
                                std::hypot(wb.x() - x2, wb.y() - y2) < tol);
            bool reverseMatch = (std::hypot(wa.x() - x2, wa.y() - y2) < tol &&
                                 std::hypot(wb.x() - x1, wb.y() - y1) < tol);
            if (directMatch || reverseMatch) {
                return true;
            }
        }
        return false;
    };

    // 检查直线
    for (const auto &v : m_lines) {
        QJsonObject ln = v.toObject();
        QString s = ln.value("startid").toString();
        if (s.isEmpty()) s = ln.value("startId").toString();
        if (s.isEmpty()) s = ln.value("sId").toString();

        QString e = ln.value("endid").toString();
        if (e.isEmpty()) e = ln.value("endId").toString();
        if (e.isEmpty()) e = ln.value("eId").toString();

        double x1 = ln.value("x1").toDouble();
        double y1 = ln.value("y1").toDouble();
        double x2 = ln.value("x2").toDouble();
        double y2 = ln.value("y2").toDouble();

        if (matchEndpoints(s, e, x1, y1, x2, y2)) {
            return true;
        }
    }

    // 检查曲线
    for (const auto &v : m_curves) {
        QJsonObject cv = v.toObject();
        QString s = cv.value("startid").toString();
        if (s.isEmpty()) s = cv.value("startId").toString();
        if (s.isEmpty()) s = cv.value("sId").toString();

        QString e = cv.value("endid").toString();
        if (e.isEmpty()) e = cv.value("endId").toString();
        if (e.isEmpty()) e = cv.value("eId").toString();

        double x1 = cv.value("x1").toDouble();
        double y1 = cv.value("y1").toDouble();
        double x2 = cv.value("x2").toDouble();
        double y2 = cv.value("y2").toDouble();

        if (matchEndpoints(s, e, x1, y1, x2, y2)) {
            return true;
        }
    }

    return false;
}

// ── Feature selection ─────────────────────────────────────────────────────────

void MapEditWidget::selectFeatureAt(const QPointF &sp)
{
    // 命中逻辑与选中效果统一由 RouteFeatureSelect 提供，与"地图预览"共用同一份实现
    QGraphicsItem *target = RouteFeatureSelect::pickItem(m_scene, sp, hitTolScene(12.0));
    if (target) {
        showFeatureProperties(target->data(0).toString(), target->data(1).toString());
        return;
    }
    clearSelection();
}

void MapEditWidget::clearSelection()
{
    RouteFeatureSelect::clearSelectColor(m_selItem);
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
    QString realType = type;   // 实际点类型(点击 epc 点时映射回父类型)
    QString realId   = id;
    // 点击 epc 点 → 选中对应的父点(charge/highPrecision)
    if (type == "chargeEpc" || type == "highPrecisionEpc") {
        // epc 点的 id 与父点相同，只是 type 不同
        realType = (type == "chargeEpc") ? "charge" : "highPrecision";
    }
    if      (realType == "point")         obj = find(m_points);
    else if (realType == "line")          obj = find(m_lines);
    else if (realType == "curve")         obj = find(m_curves);
    else if (realType == "charge")        obj = find(m_charges);
    else if (realType == "highPrecision") obj = find(m_highPrecisionPoints);
    if (obj.isEmpty()) return;

    m_selType = realType; m_selId = realId;
    QGraphicsItem *newSel = m_featureItems.value(realType + "_" + realId);
    if (m_selItem && m_selItem != newSel) RouteFeatureSelect::clearSelectColor(m_selItem);
    m_selItem = newSel;
    RouteFeatureSelect::applySelectColor(m_selItem);

    bool isPointKind = (realType == "point" || realType == "highPrecision" || realType == "charge");
    if (isPointKind) {
        m_propHeaderTitle->setText("站点属性");
        QString badgeText = (realType == "point") ? "LM" : ((realType == "charge") ? "CP" : "TL");
        QString badgeStyle = (realType == "point")
            ? "background:#1e3a5f; color:#60a5fa; border:1px solid #2563eb; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;"
            : ((realType == "charge")
                ? "background:#4d3800; color:#fbbf24; border:1px solid #d97706; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;"
                : "background:#4d1414; color:#f87171; border:1px solid #dc2626; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;");
        m_propHeaderBadge->setText(badgeText);
        m_propHeaderBadge->setStyleSheet(badgeStyle);

        m_stationPropGroup->setVisible(true);
        m_linePropGroup->setVisible(false);

        int idx = m_stationTypeCombo->findData(realType);
        if (idx >= 0) m_stationTypeCombo->setCurrentIndex(idx);
        m_stationIdEdit->setText(realId);
        m_stationLabelEdit->setText(obj.value("name").toString());
        m_stationXSpin->setValue(obj.value("x").toDouble());
        m_stationYSpin->setValue(obj.value("y").toDouble());

        double yawVal = 0.0;
        if (obj.contains("yaw")) yawVal = obj.value("yaw").toDouble();
        else if (obj.contains("robotYaw")) yawVal = obj.value("robotYaw").toDouble();
        else if (obj.contains("theta")) yawVal = obj.value("theta").toDouble() * M_PI / 180.0;
        m_stationYawSpin->setValue(yawVal * 180.0 / M_PI);

        m_stationDockableChk->setChecked(obj.value("rdsDockable").toBool(true));
        m_stationRotateChk->setChecked(obj.value("allowRotate").toBool(true));
        m_stationPgvChk->setChecked(obj.value("useDownPGV").toBool(false));

        bool isDevicePoint = (realType == "charge" || realType == "highPrecision");
        // 工作站与特殊工作站的 xy 与朝向角均支持自由编辑
        m_stationXSpin->setReadOnly(false);
        m_stationYSpin->setReadOnly(false);
        m_stationYawSpin->setReadOnly(false);

        QString rwStyle = "background:#131722; color:#cfddea; border:1px solid #2b384e; border-radius:4px; padding:2px 6px; font-size:12px;";
        m_stationXSpin->setStyleSheet(rwStyle);
        m_stationYSpin->setStyleSheet(rwStyle);
        m_stationYawSpin->setStyleSheet(rwStyle);

        if (isDevicePoint) {
            double epcX = obj.contains("epc_x") ? obj.value("epc_x").toDouble()
                                                 : obj.value("epcX").toDouble();
            double epcY = obj.contains("epc_y") ? obj.value("epc_y").toDouble()
                                                 : obj.value("epcY").toDouble();
            m_stationEpcXLabel->setText(QString::number(epcX, 'f', 4) + " m");
            m_stationEpcYLabel->setText(QString::number(epcY, 'f', 4) + " m");
            m_stationEpcGroup->setVisible(true);
        } else {
            m_stationEpcGroup->setVisible(false);
        }
    } else {
        // Line or Curve
        m_propHeaderTitle->setText("线路属性");
        m_propHeaderBadge->setText(realType == "line" ? "直线" : "贝塞尔曲线");
        m_propHeaderBadge->setStyleSheet("background:#17344f; color:#38bdf8; border:1px solid #0284c7; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;");

        m_stationPropGroup->setVisible(false);
        m_linePropGroup->setVisible(true);

        m_lineTypeCombo->setCurrentIndex(realType == "line" ? 0 : 1);
        m_lineIdEdit->setText(realId);
        m_lineNameEdit->setText(obj.value("name").toString());

        QString sId = obj.value("startid").toString();
        QString eId = obj.value("endid").toString();
        m_lineStartLabel->setText(sId.isEmpty() ? "--" : sId);
        m_lineEndLabel->setText(eId.isEmpty() ? "--" : eId);

        int direct = obj.value("direct").toInt(10);
        int dIdx = (direct == 10) ? 0 : ((direct == 0) ? 1 : 2);
        m_lineDirectCombo->setCurrentIndex(dIdx);

        const auto &mc = MotionConfigManager::instance();
        double minSpd = mc.paramMin("max_speed", 0.0);
        double maxSpd = mc.paramMax("max_speed", 1.0);
        double minAcc = mc.paramMin("max_acceleration", 0.3);
        double maxAcc = mc.paramMax("max_acceleration", 0.5);
        double minDec = mc.paramMin("max_deceleration", 0.3);
        double maxDec = mc.paramMax("max_deceleration", 0.5);
        double minRot = mc.paramMin("max_angular_speed", 5.0);
        double maxRot = mc.paramMax("max_angular_speed", 60.0);
        double minRotAcc = mc.paramMin("max_angular_acceleration", 10.0);
        double maxRotAcc = mc.paramMax("max_angular_acceleration", 30.0);

        m_lineMaxSpeedSpin->setRange(minSpd, maxSpd);
        m_lineMaxAccSpin->setRange(minAcc, maxAcc);
        m_lineMaxDecSpin->setRange(minDec, maxDec);
        m_lineMaxRotSpin->setRange(minRot, maxRot);
        m_lineMaxRotAccSpin->setRange(minRotAcc, maxRotAcc);
        m_lineReachDistSpin->setRange(0.01, qMax(1.0, mc.arrivalDistanceTolerance()));
        m_lineReachAngleSpin->setRange(0.1, qMax(45.0, mc.arrivalAngleToleranceDeg()));
        m_lineRoadWidthSpin->setRange(0.5, qMax(10.0, mc.routeWidth()));

        // 若图元有保存的属性（用户修改过）则以图元为准；若未修改则以“参数”中的全局当前值为准
        double defSpd = mc.maxSpeed();
        double defAcc = mc.maxAcceleration();
        double defDec = mc.maxDeceleration();
        double defRot = mc.maxAngularSpeedDeg();
        double defRotAcc = mc.maxAngularAccelerationDeg();

        double curSpd = obj.contains("maxspeed") ? obj.value("maxspeed").toDouble() : defSpd;
        double acc = obj.contains("max_acceleration") ? obj.value("max_acceleration").toDouble() : (obj.contains("maxacc") ? obj.value("maxacc").toDouble() : defAcc);
        double dec = obj.contains("max_deceleration") ? obj.value("max_deceleration").toDouble() : (obj.contains("maxdec") ? obj.value("maxdec").toDouble() : defDec);
        double rot = obj.contains("max_angular_speed") ? obj.value("max_angular_speed").toDouble() : (obj.contains("maxrot") ? obj.value("maxrot").toDouble() : defRot);
        double rotAcc = obj.contains("max_angular_acceleration") ? obj.value("max_angular_acceleration").toDouble() : (obj.contains("maxrotacc") ? obj.value("maxrotacc").toDouble() : defRotAcc);

        m_lineMaxSpeedSpin->setValue(qBound(minSpd, curSpd, maxSpd));
        m_lineMaxAccSpin->setValue(qBound(minAcc, acc, maxAcc));
        m_lineMaxDecSpin->setValue(qBound(minDec, dec, maxDec));
        m_lineMaxRotSpin->setValue(qBound(minRot, rot, maxRot));
        m_lineMaxRotAccSpin->setValue(qBound(minRotAcc, rotAcc, maxRotAcc));
        m_lineReachDistSpin->setValue(obj.value("reachdist").toDouble(mc.arrivalDistanceTolerance()));
        m_lineReachAngleSpin->setValue(obj.value("reachangle").toDouble(mc.arrivalAngleToleranceDeg()));
        m_lineRoadWidthSpin->setValue(obj.value("width").toDouble(mc.routeWidth()));

        // 实时避障范围定向膨胀参数 (m，若已存在则直接加载真实值，不存在才使用默认值)
        double sl = obj.contains("intrusion_stop_length") ? obj.value("intrusion_stop_length").toDouble(0.0) : mc.defaultObsStopLength();
        double sw = obj.contains("intrusion_stop_width") ? obj.value("intrusion_stop_width").toDouble(0.0) : mc.defaultObsStopWidth();
        double dl = obj.contains("intrusion_deceleration_length") ? obj.value("intrusion_deceleration_length").toDouble(0.0) : mc.defaultObsDecelLength();
        double dw = obj.contains("intrusion_deceleration_width") ? obj.value("intrusion_deceleration_width").toDouble(0.0) : mc.defaultObsDecelWidth();

        m_lineObsStopLenSpin->setValue(sl);
        m_lineObsStopWidSpin->setValue(sw);
        m_lineObsDecelLenSpin->setValue(dl);
        m_lineObsDecelWidSpin->setValue(dw);
    }

    m_propsPanel->setVisible(true);

    if (m_tableVisible) {
        int idx = m_tableTypeCombo->findData(realType);
        if (idx >= 0 && m_tableTypeCombo->currentIndex() != idx)
            m_tableTypeCombo->setCurrentIndex(idx);
        for (int r = 0; r < m_featureTable->rowCount(); ++r) {
            auto *it = m_featureTable->item(r, 0);
            if (it && it->text() == realId) { m_featureTable->selectRow(r); break; }
        }
    }
}

void MapEditWidget::syncLineObstaclesToEndpoints()
{
    // 1. 识别特殊工作站（充电桩）与工作站（高精度点）的 ID 集合
    QSet<QString> specialIds;
    for (const auto &v : m_charges) {
        QJsonObject o = v.toObject();
        QString id = o.value("id").toString();
        if (!id.isEmpty()) specialIds.insert(id);
    }
    for (const auto &v : m_highPrecisionPoints) {
        QJsonObject o = v.toObject();
        QString id = o.value("id").toString();
        if (!id.isEmpty()) specialIds.insert(id);
    }
    for (const auto &v : m_points) {
        QJsonObject o = v.toObject();
        if (isSpecialWorkstationPoint(o)) {
            QString id = o.value("id").toString();
            if (!id.isEmpty()) specialIds.insert(id);
        }
    }

    // 2. 收集普通站点的避障参数 (如果线路连接了高精度点或充电桩点，另一端普通站点避障参数值和线保持一致)
    struct ObstacleParam {
        double stopLen = 0.0;
        double stopWid = 0.0;
        double decelLen = 0.0;
        double decelWid = 0.0;
        bool isCustom = false;
    };
    QMap<QString, ObstacleParam> normalStationObsMap;

    auto isNodeSpecial = [&](const QString &nodeId) -> bool {
        if (nodeId.isEmpty()) return false;
        if (specialIds.contains(nodeId)) return true;
        if (nodeId.startsWith("hp_", Qt::CaseInsensitive) || nodeId.startsWith("c_", Qt::CaseInsensitive) ||
            nodeId.startsWith("ap_", Qt::CaseInsensitive) || nodeId.startsWith("cp_", Qt::CaseInsensitive)) return true;
        return false;
    };

    auto collectLineObstacle = [&](const QJsonArray &arr) {
        for (const auto &v : arr) {
            QJsonObject ln = v.toObject();
            bool lnCustom = ln.value("is_custom_obstacle").toBool(ln.value("custom_obstacle").toBool(false));
            const auto &mc = MotionConfigManager::instance();
            double sl = ln.value("intrusion_stop_length").toDouble(lnCustom ? 0.0 : mc.defaultObsStopLength());
            double sw = ln.value("intrusion_stop_width").toDouble(lnCustom ? 0.0 : mc.defaultObsStopWidth());
            double dl = ln.value("intrusion_deceleration_length").toDouble(lnCustom ? 0.0 : mc.defaultObsDecelLength());
            double dw = ln.value("intrusion_deceleration_width").toDouble(lnCustom ? 0.0 : mc.defaultObsDecelWidth());
            if (!lnCustom && !ln.contains("intrusion_stop_length") && !ln.contains("intrusion_stop_width") &&
                !ln.contains("intrusion_deceleration_length") && !ln.contains("intrusion_deceleration_width")) {
                sl = mc.defaultObsStopLength();
                sw = mc.defaultObsStopWidth();
                dl = mc.defaultObsDecelLength();
                dw = mc.defaultObsDecelWidth();
            }

            QString sId = ln.contains("startid") ? ln.value("startid").toString() : ln.value("sId").toString();
            if (sId.isEmpty()) sId = ln.value("startId").toString();
            QString eId = ln.contains("endid") ? ln.value("endid").toString() : ln.value("eId").toString();
            if (eId.isEmpty()) eId = ln.value("endId").toString();

            bool sIsSpecial = isNodeSpecial(sId);
            bool eIsSpecial = isNodeSpecial(eId);

            auto updateStation = [&](const QString &nodeId) {
                if (nodeId.isEmpty() || isNodeSpecial(nodeId)) return;
                ObstacleParam &param = normalStationObsMap[nodeId];
                param.stopLen  = qMax(param.stopLen, sl);
                param.stopWid  = qMax(param.stopWid, sw);
                param.decelLen = qMax(param.decelLen, dl);
                param.decelWid = qMax(param.decelWid, dw);
                if (lnCustom) param.isCustom = true;
            };

            if (sIsSpecial && !eIsSpecial) {
                // 线的起点是高精度点/充电桩点，另一端是普通站点 -> 站点避障参数与线保持一致
                updateStation(eId);
            } else if (!sIsSpecial && eIsSpecial) {
                // 线的终点是高精度点/充电桩点，另一端是普通站点 -> 站点避障参数与线保持一致
                updateStation(sId);
            } else if (!sIsSpecial && !eIsSpecial) {
                // 线的两端都是普通站点 -> 普通站点避障参数与线保持一致
                updateStation(sId);
                updateStation(eId);
            }
            // 若两端都是特殊工作站/充电桩点，则都不更新
        }
    };

    collectLineObstacle(m_lines);
    collectLineObstacle(m_curves);

    // 补齐线路的默认避障参数（仅在完全未配置字段时才赋默认值，0.0 是合法避障范围）
    auto fillLineDefaults = [](QJsonArray &arr) {
        const auto &mc = MotionConfigManager::instance();
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject o = arr[i].toObject();
            bool isCustom = o.value("is_custom_obstacle").toBool(o.value("custom_obstacle").toBool(false));
            if (!isCustom && !o.contains("intrusion_stop_length")) {
                o["intrusion_stop_length"]         = mc.defaultObsStopLength();
                o["intrusion_stop_width"]          = mc.defaultObsStopWidth();
                o["intrusion_deceleration_length"] = mc.defaultObsDecelLength();
                o["intrusion_deceleration_width"]  = mc.defaultObsDecelWidth();
                o["is_custom_obstacle"]            = false;
                arr[i] = o;
            }
        }
    };
    fillLineDefaults(m_lines);
    fillLineDefaults(m_curves);

    // 3. 将避障参数同步赋给普通站点 (m_points)，若 m_points 中有特殊工作站，强制为 0
    for (int i = 0; i < m_points.size(); ++i) {
        QJsonObject pt = m_points[i].toObject();
        QString ptId = pt.value("id").toString();
        if (isSpecialWorkstationPoint(pt) || specialIds.contains(ptId)) {
            // 特殊工作站(充电桩)与工作站(高精度点)避障参数永远为 0
            forceZeroObstacleParams(pt);
            m_points[i] = pt;
            continue;
        }

        bool ptCustom = pt.value("is_custom_obstacle").toBool(pt.value("custom_obstacle").toBool(false));
        if (ptCustom) {
            // 用户单独修改过站点的避障参数，不被相连线或默认值覆盖
            m_points[i] = pt;
            continue;
        }

        const auto &mc = MotionConfigManager::instance();
        double sl = mc.defaultObsStopLength(), sw = mc.defaultObsStopWidth(), dl = mc.defaultObsDecelLength(), dw = mc.defaultObsDecelWidth();
        bool isCustom = false;
        if (normalStationObsMap.contains(ptId)) {
            const auto &param = normalStationObsMap.value(ptId);
            sl = param.stopLen;
            sw = param.stopWid;
            dl = param.decelLen;
            dw = param.decelWid;
            isCustom = param.isCustom;
        } else if (pt.contains("intrusion_stop_length")) {
            sl = pt.value("intrusion_stop_length").toDouble(0.0);
            sw = pt.value("intrusion_stop_width").toDouble(0.0);
            dl = pt.value("intrusion_deceleration_length").toDouble(0.0);
            dw = pt.value("intrusion_deceleration_width").toDouble(0.0);
        }

        // 1. properties 顶层平铺字段赋值
        pt["intrusion_stop_length"]         = sl;
        pt["intrusion_stop_width"]          = sw;
        pt["intrusion_deceleration_length"] = dl;
        pt["intrusion_deceleration_width"]  = dw;
        pt["is_custom_obstacle"]            = isCustom;

        // 2. Vue actions 动作列表 (action_type: 21, name: "实时避障范围")
        QJsonArray actions = pt.value("actions").toArray();
        bool foundAction = false;
        for (int j = 0; j < actions.size(); ++j) {
            QJsonObject act = actions[j].toObject();
            if (act.value("action_type").toInt() == 21 || act.value("name").toString() == "实时避障范围") {
                act["name"] = "实时避障范围";
                act["action_type"] = 21;
                act["intrusion_stop_length"]         = sl;
                act["intrusion_stop_width"]          = sw;
                act["intrusion_deceleration_length"] = dl;
                act["intrusion_deceleration_width"]  = dw;
                act["is_custom_obstacle"]            = isCustom;
                actions[j] = act;
                foundAction = true;
                break;
            }
        }
        if (!foundAction) {
            QJsonObject act;
            act["name"] = "实时避障范围";
            act["action_type"] = 21;
            act["intrusion_stop_length"]         = sl;
            act["intrusion_stop_width"]          = sw;
            act["intrusion_deceleration_length"] = dl;
            act["intrusion_deceleration_width"]  = dw;
            act["is_custom_obstacle"]            = isCustom;
            actions.append(act);
        }
        pt["actions"] = actions;
        m_points[i] = pt;
    }

    // 4. 把高精度点（工作站）或充电桩点（特殊工作站）的四个避障参数全部强制置为 0
    auto resetSpecialPoints = [](QJsonArray &arr) {
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject pt = arr[i].toObject();
            forceZeroObstacleParams(pt);
            arr[i] = pt;
        }
    };

    resetSpecialPoints(m_charges);
    resetSpecialPoints(m_highPrecisionPoints);
}

void MapEditWidget::onSaveMap(std::function<void(bool success)> onFinished)
{
    // 同步线路避障参数到端点（与 Vue actions 规范完全一致）
    syncLineObstaclesToEndpoints();

    // 1. Flush any dragged handle positions to JSON (支持任意阶贝塞尔)
    for (auto it = m_curveHandles.begin(); it != m_curveHandles.end(); ++it) {
        auto &lst = it.value();
        if (lst.size() < 2) continue;
        const QString &id = it.key();
        for (int i = 0; i < m_curves.size(); ++i) {
            QJsonObject cv = m_curves[i].toObject();
            if (cv.value("id").toString() != id) continue;

            auto handlePos = [&](int idx) -> QPointF {
                if (idx >= 0 && idx < lst.size() && lst[idx]) return lst[idx]->pos();
                if (idx == 0)
                    return worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble());
                return worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble());
            };

            QJsonArray ctrlArr = cv.value("controlPoints").toArray();
            if (ctrlArr.isEmpty() && lst.size() == 3) {
                QList<QPointF> wPts;
                for (int k = 0; k < lst.size(); ++k)
                    wPts.append(sceneToWorld(handlePos(k).x(), handlePos(k).y()));
                cv["x1"] = wPts[0].x(); cv["y1"] = wPts[0].y();
                cv["cx"] = wPts[1].x(); cv["cy"] = wPts[1].y();
                cv["x2"] = wPts[2].x(); cv["y2"] = wPts[2].y();
            } else {
                for (int j = 0; j < lst.size() && j < ctrlArr.size(); ++j) {
                    QPointF wp = sceneToWorld(handlePos(j).x(), handlePos(j).y());
                    QJsonObject pt = ctrlArr[j].toObject();
                    pt["x"] = wp.x(); pt["y"] = wp.y();
                    ctrlArr[j] = pt;
                }
                cv["controlPoints"] = ctrlArr;
                if (!ctrlArr.isEmpty()) {
                    QPointF wFirst = sceneToWorld(handlePos(0).x(), handlePos(0).y());
                    QPointF wLast  = sceneToWorld(handlePos(lst.size()-1).x(), handlePos(lst.size()-1).y());
                    cv["x1"] = wFirst.x(); cv["y1"] = wFirst.y();
                    cv["x2"] = wLast.x();  cv["y2"] = wLast.y();
                }
            }
            m_curves[i] = cv; break;
        }
    }

    // 2. Build GeoJSON FeatureCollection
    QJsonArray features;

    auto makePointFeat = [](const QJsonObject &obj) -> QJsonObject {
        QJsonObject props = obj;
        if (isSpecialWorkstationPoint(props)) {
            // 保存路网时再次强制将高精度点/工作站与充电桩点/特殊工作站避障参数改为 0
            forceZeroObstacleParams(props);
        }
        QJsonArray coords;
        coords.append(props.value("x").toDouble());
        coords.append(props.value("y").toDouble());
        coords.append(0.0);
        QJsonObject geom; geom["type"] = "Point"; geom["coordinates"] = coords;
        props.remove("x"); props.remove("y");
        QJsonObject f; f["type"] = "Feature"; f["geometry"] = geom; f["properties"] = props;
        return f;
    };

    for (const auto &v : m_points) {
        QJsonObject o = v.toObject();
        features.append(makePointFeat(o));
    }
    for (const auto &v : m_charges) {
        QJsonObject o = v.toObject();
        forceZeroObstacleParams(o);
        features.append(makePointFeat(o));
    }
    for (const auto &v : m_highPrecisionPoints) {
        QJsonObject o = v.toObject();
        forceZeroObstacleParams(o);
        features.append(makePointFeat(o));
    }

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
        QJsonArray coords;
        QJsonArray ctrlArr = cv.value("controlPoints").toArray();
        if (!ctrlArr.isEmpty()) {
            for (const auto &pv : ctrlArr) {
                QJsonObject pt = pv.toObject();
                QJsonArray c;
                c.append(pt.value("x").toDouble());
                c.append(pt.value("y").toDouble());
                coords.append(c);
            }
        } else {
            QJsonArray c0, c1, c2;
            c0.append(cv.value("x1").toDouble()); c0.append(cv.value("y1").toDouble());
            c1.append(cv.value("cx").toDouble()); c1.append(cv.value("cy").toDouble());
            c2.append(cv.value("x2").toDouble()); c2.append(cv.value("y2").toDouble());
            coords.append(c0); coords.append(c1); coords.append(c2);
        }

        QJsonObject geom; geom["type"] = "MultiPoint"; geom["coordinates"] = coords;
        QJsonObject props = cv;
        props.remove("x1"); props.remove("y1"); props.remove("cx"); props.remove("cy");
        props.remove("x2"); props.remove("y2"); props.remove("controlPoints");
        QJsonObject f; f["type"] = "Feature"; f["geometry"] = geom; f["properties"] = props;
        features.append(f);
    }

    QJsonObject geojson;
    geojson["type"]     = "FeatureCollection";
    geojson["features"] = features;
    geojson["mapId"]    = m_mapId;
    geojson["mapName"]  = m_mapName;

    // 3. Prepare PGM raster data (仅在底图实际修改时才上传栅格地图)
    QByteArray pgmData;
    bool rasterModified = (!m_mapImage.isNull() && (m_mapImage != m_originalMapImage || !m_undoStack.isEmpty()));
    if (rasterModified) {
        pgmData = MapPgmUtil::imageToPgm(m_mapImage, false);
    }

    if (m_saveBtn) m_saveBtn->setEnabled(false);
    QPointer<LoadingDialog> loading = new LoadingDialog("正在保存地图与路网...", window(), Qt::ApplicationModal);
    loading->show();

    // 4. Concurrently save route and upload PGM
    struct SaveState {
        bool routeDone = false;
        bool routeOk = false;
        QString routeErr;
        bool pgmDone = false;
        bool pgmOk = false;
        QString pgmErr;
    };
    auto state = std::make_shared<SaveState>();
    if (pgmData.isEmpty()) {
        state->pgmDone = true;
        state->pgmOk = true;
    }

    QPointer<MapEditWidget> self = this;

    auto onComplete = [self, loading, state, onFinished]() {
        if (loading) {
            loading->close();
            loading->deleteLater();
        }
        if (!self) return;

        if (self->m_saveBtn) self->m_saveBtn->setEnabled(true);

        bool allOk = (state->routeOk && state->pgmOk);
        if (allOk) {
            Toast::success(self, "地图与路网保存成功");
            self->m_dirty = false;
            self->m_originalMapImage = self->m_mapImage;
            self->m_undoStack.clear();
            self->m_redoStack.clear();
            self->updateHistoryButtons();
            if (onFinished) {
                onFinished(true);
            } else {
                self->loadRouteData();
            }
        } else {
            if (!state->routeOk && !state->pgmOk) {
                Toast::error(self, QString("保存失败: 路网(%1), 栅格(%2)").arg(state->routeErr, state->pgmErr));
            } else if (!state->routeOk) {
                Toast::info(self, QString("栅格已保存，但路网保存失败: %1").arg(state->routeErr));
            } else {
                Toast::info(self, QString("路网已保存，但栅格地图上传失败: %1").arg(state->pgmErr));
            }
            if (onFinished) {
                onFinished(false);
            }
        }
    };

    // A. Save Route
    ApiService::instance().saveEditedMap(geojson, [self, state, onComplete](int code, const QJsonObject &resp) {
        if (!self) return;
        state->routeDone = true;
        state->routeOk = (code == 200);
        if (code != 200) {
            QString err = resp.value("msg").toString();
            if (err.isEmpty()) err = resp.value("message").toString();
            if (err.isEmpty()) err = QString("HTTP %1").arg(code);
            state->routeErr = err;
        }
        if (state->pgmDone) {
            onComplete();
        }
    });

    // B. Save PGM raster (if available)
    if (!pgmData.isEmpty()) {
        ApiService::instance().uploadMapPgm(m_mapId, pgmData, [self, state, onComplete](int code, const QJsonObject &resp) {
            if (!self) return;
            state->pgmDone = true;
            state->pgmOk = (code == 200);
            if (code != 200) {
                QString err = resp.value("msg").toString();
                if (err.isEmpty()) err = resp.value("message").toString();
                if (err.isEmpty()) err = QString("HTTP %1").arg(code);
                state->pgmErr = err;
            }
            if (state->routeDone) {
                onComplete();
            }
        });
    }
}

void MapEditWidget::applyPropsEdit()
{
    if (m_selType.isEmpty()) return;
    bool isPointKind = (m_selType == "point" || m_selType == "highPrecision" || m_selType == "charge");

    if (isPointKind) {
        QString newName  = m_stationLabelEdit->text().trimmed();
        QString newKind  = m_stationTypeCombo->currentData().toString();
        double newX      = m_stationXSpin->value();
        double newY      = m_stationYSpin->value();
        double newYawDeg = m_stationYawSpin->value();
        double newYawRad = newYawDeg * M_PI / 180.0;
        bool dockable    = m_stationDockableChk->isChecked();
        bool rotate      = m_stationRotateChk->isChecked();
        bool pgv         = m_stationPgvChk->isChecked();

        if (newKind != m_selType) {
            // 类型切换处理
            QJsonObject pointObj;
            auto take = [&](QJsonArray &arr) {
                for (int i = 0; i < arr.size(); ++i) {
                    QJsonObject o = arr[i].toObject();
                    if (o.value("id").toString() == m_selId) {
                        pointObj = o; arr.removeAt(i); return true;
                    }
                }
                return false;
            };
            if      (m_selType == "point")         take(m_points);
            else if (m_selType == "charge")        take(m_charges);
            else if (m_selType == "highPrecision") take(m_highPrecisionPoints);

            if (!pointObj.isEmpty()) {
                QString newId, defaultName;
                if      (newKind == "point")         { int nn = nextIdNum(m_points, "p_");              newId = QString("p_%1").arg(nn);  defaultName = QString("LM%1").arg(nn); }
                else if (newKind == "charge")        { int nn = nextIdNum(m_charges, "c_");             newId = QString("c_%1").arg(nn);  defaultName = QString("CP%1").arg(nn); }
                else                                  { int nn = nextIdNum(m_highPrecisionPoints, "hp_"); newId = QString("hp_%1").arg(nn); defaultName = QString("TL%1").arg(nn); }

                pointObj["id"]   = newId;
                pointObj["name"] = newName.isEmpty() ? defaultName : newName;
                pointObj["x"]    = newX;
                pointObj["y"]    = newY;
                pointObj["yaw"]  = newYawRad;
                pointObj["theta"] = newYawDeg;
                pointObj["robotYaw"] = 0.0;
                pointObj["rdsDockable"] = dockable;
                pointObj["allowRotate"] = rotate;
                pointObj["useDownPGV"]  = pgv;

                if      (newKind == "point")         m_points.append(pointObj);
                else if (newKind == "charge")        m_charges.append(pointObj);
                else                                  m_highPrecisionPoints.append(pointObj);

                m_selType = newKind;
                m_selId   = newId;
            }
        } else {
            // 普通更新
            auto upd = [&](QJsonArray &arr) {
                for (int i = 0; i < arr.size(); ++i) {
                    QJsonObject o = arr[i].toObject();
                    if (o.value("id").toString() != m_selId) continue;
                    o["name"]        = newName;
                    o["x"]           = newX;
                    o["y"]           = newY;
                    o["yaw"]         = newYawRad;
                    o["theta"]       = newYawDeg;
                    o["robotYaw"]    = 0.0;
                    o["rdsDockable"] = dockable;
                    o["allowRotate"] = rotate;
                    o["useDownPGV"]  = pgv;

                    // 联动更新所有相连线路与曲线的端点坐标
                    for (int li = 0; li < m_lines.size(); ++li) {
                        QJsonObject ln = m_lines[li].toObject();
                        bool changed = false;
                        if (ln.value("startid").toString() == m_selId) { ln["x1"] = newX; ln["y1"] = newY; changed = true; }
                        if (ln.value("endid").toString() == m_selId)   { ln["x2"] = newX; ln["y2"] = newY; changed = true; }
                        if (changed) m_lines[li] = ln;
                    }
                    for (int ci = 0; ci < m_curves.size(); ++ci) {
                        QJsonObject cv = m_curves[ci].toObject();
                        bool changed = false;
                        if (cv.value("startid").toString() == m_selId) {
                            cv["x1"] = newX; cv["y1"] = newY;
                            QJsonArray ctrlArr = cv.value("controlPoints").toArray();
                            if (!ctrlArr.isEmpty()) {
                                QJsonObject cp0 = ctrlArr[0].toObject();
                                cp0["x"] = newX; cp0["y"] = newY;
                                ctrlArr[0] = cp0;
                                cv["controlPoints"] = ctrlArr;
                            }
                            changed = true;
                        }
                        if (cv.value("endid").toString() == m_selId) {
                            cv["x2"] = newX; cv["y2"] = newY;
                            QJsonArray ctrlArr = cv.value("controlPoints").toArray();
                            if (!ctrlArr.isEmpty()) {
                                int lastIdx = ctrlArr.size() - 1;
                                QJsonObject cpLast = ctrlArr[lastIdx].toObject();
                                cpLast["x"] = newX; cpLast["y"] = newY;
                                ctrlArr[lastIdx] = cpLast;
                                cv["controlPoints"] = ctrlArr;
                            }
                            changed = true;
                        }
                        if (changed) m_curves[ci] = cv;
                    }

                    arr[i] = o;
                    break;
                }
            };
            if      (m_selType == "point")         upd(m_points);
            else if (m_selType == "charge")        upd(m_charges);
            else if (m_selType == "highPrecision") upd(m_highPrecisionPoints);
        }
    } else {
        // Line or Curve
        QString newName   = m_lineNameEdit->text().trimmed();
        int newDirect     = m_lineDirectCombo->currentData().toInt();
        const auto &mc = MotionConfigManager::instance();
        double minSpd = mc.paramMin("max_speed", 0.0);
        double limitMaxSpd = mc.paramMax("max_speed", 1.0);
        double minAcc = mc.paramMin("max_acceleration", 0.3);
        double limitMaxAcc = mc.paramMax("max_acceleration", 0.5);
        double minDec = mc.paramMin("max_deceleration", 0.3);
        double limitMaxDec = mc.paramMax("max_deceleration", 0.5);
        double minRot = mc.paramMin("max_angular_speed", 5.0);
        double limitMaxRot = mc.paramMax("max_angular_speed", 60.0);
        double minRotAcc = mc.paramMin("max_angular_acceleration", 10.0);
        double limitMaxRotAcc = mc.paramMax("max_angular_acceleration", 30.0);

        double maxSpeed   = qBound(minSpd, m_lineMaxSpeedSpin->value(), limitMaxSpd);
        double maxAcc     = qBound(minAcc, m_lineMaxAccSpin->value(), limitMaxAcc);
        double maxDec     = qBound(minDec, m_lineMaxDecSpin->value(), limitMaxDec);
        double maxRot     = qBound(minRot, m_lineMaxRotSpin->value(), limitMaxRot);
        double maxRotAcc  = qBound(minRotAcc, m_lineMaxRotAccSpin->value(), limitMaxRotAcc);
        double reachDist  = m_lineReachDistSpin->value();
        double reachAngle = m_lineReachAngleSpin->value();
        double roadWidth  = m_lineRoadWidthSpin->value();
        double obsStopLen  = m_lineObsStopLenSpin->value();
        double obsStopWid  = m_lineObsStopWidSpin->value();
        double obsDecelLen = m_lineObsDecelLenSpin->value();
        double obsDecelWid = m_lineObsDecelWidSpin->value();

        auto updLine = [&](QJsonArray &arr) {
            for (int i = 0; i < arr.size(); ++i) {
                QJsonObject o = arr[i].toObject();
                if (o.value("id").toString() != m_selId) continue;
                o["name"]       = newName;
                o["direct"]     = newDirect;
                o["maxspeed"]                  = maxSpeed;
                o["max_acceleration"]          = maxAcc;
                o["maxacc"]                    = maxAcc;
                o["max_deceleration"]          = maxDec;
                o["maxdec"]                    = maxDec;
                o["max_angular_speed"]         = maxRot;
                o["maxrot"]                    = maxRot;
                o["max_angular_acceleration"]  = maxRotAcc;
                o["maxrotacc"]                 = maxRotAcc;
                o["reachdist"]  = reachDist;
                o["reachangle"] = reachAngle;
                o["width"]      = roadWidth;
                o["intrusion_stop_length"]         = obsStopLen;
                o["intrusion_stop_width"]          = obsStopWid;
                o["intrusion_deceleration_length"] = obsDecelLen;
                o["intrusion_deceleration_width"]  = obsDecelWid;
                o["is_custom_obstacle"]            = true;
                arr[i] = o;
                break;
            }
        };
        if (m_selType == "line")       updLine(m_lines);
        else if (m_selType == "curve") updLine(m_curves);
        syncLineObstaclesToEndpoints();
    }

    renderMap();
    showFeatureProperties(m_selType, m_selId);
    if (m_tableVisible) refreshTable();
    m_dirty = true;
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
    m_dirty = true;
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
    m_dirty = true;
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

// ── Save / Edit / Import map ──────────────────────────────────────────────────

void MapEditWidget::onImportSmap()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "导入 .smap 地图路网文件",
        QString(),
        "SMAP 地图文件 (*.smap);;所有文件 (*.*)");
    if (filePath.isEmpty()) return;

    if (!m_points.isEmpty() || !m_lines.isEmpty() || !m_curves.isEmpty() || !m_charges.isEmpty() || !m_highPrecisionPoints.isEmpty()) {
        auto ret = QMessageBox::question(
            this,
            "导入确认",
            "导入 .smap 将覆盖当前未保存的路网数据，是否继续？",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }

    QJsonArray newPoints, newLines, newCurves;
    QString smapMapName;
    QString errMsg;
    QJsonObject smapHeader;

    bool ok = MapPgmUtil::parseSmapFile(filePath, newPoints, newLines, newCurves, smapMapName, &errMsg, &smapHeader);
    if (!ok) {
        Toast::error(this, QString("导入失败: %1").arg(errMsg));
        return;
    }

    // 清除现有选区与控制手柄
    clearSelection();
    removeAllHandles();

    m_points = QJsonArray();
    m_charges = QJsonArray();
    m_highPrecisionPoints = QJsonArray();

    // 根据点位类型分别归类到普通站点、充电点、精定位点
    for (const auto &pv : newPoints) {
        QJsonObject p = pv.toObject();
        QString type = p.value("type").toString("point");
        if (type == "charge") {
            m_charges.append(p);
        } else if (type == "highPrecision") {
            m_highPrecisionPoints.append(p);
        } else {
            m_points.append(p);
        }
    }

    m_lines   = newLines;
    m_curves  = newCurves;
    m_dirty   = true;

    // 检查导入路网的边界范围，若超出当前底图范围则提示并自动适配视图
    if (m_imgW > 0 && (!newPoints.isEmpty() || !newLines.isEmpty() || !newCurves.isEmpty())) {
        double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
        auto updateBounds = [&](double x, double y) {
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        };
        for (const auto &v : newPoints) {
            QJsonObject o = v.toObject();
            updateBounds(o.value("x").toDouble(), o.value("y").toDouble());
        }
        for (const auto &v : newLines) {
            QJsonObject o = v.toObject();
            updateBounds(o.value("x1").toDouble(), o.value("y1").toDouble());
            updateBounds(o.value("x2").toDouble(), o.value("y2").toDouble());
        }

        if (minX < m_worldL || maxX > m_worldR || minY < m_worldB || maxY > m_worldT) {
            QMessageBox::information(
                this,
                "路网范围提示",
                QString("导入的 .smap 路网范围 [%1, %2]m ~ [%3, %4]m\n"
                        "超出当前底图范围 [%5, %6]m ~ [%7, %8]m。\n\n"
                        "已为您自动调整视图以完整展示导入路网。")
                    .arg(minX, 0, 'f', 2).arg(minY, 0, 'f', 2)
                    .arg(maxX, 0, 'f', 2).arg(maxY, 0, 'f', 2)
                    .arg(m_worldL, 0, 'f', 2).arg(m_worldB, 0, 'f', 2)
                    .arg(m_worldR, 0, 'f', 2).arg(m_worldT, 0, 'f', 2));
        }
    }

    renderMap();
    fitView();
    if (m_tableVisible) refreshTable();

    Toast::success(this, QString("成功导入 .smap 路网: %1 个普通站点, %2 个充电点, %3 个精定位点, %4 条双向曲线, %5 条直线")
                            .arg(m_points.size())
                            .arg(m_charges.size())
                            .arg(m_highPrecisionPoints.size())
                            .arg(m_curves.size())
                            .arg(m_lines.size()));
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
    if (event->type() == QEvent::KeyPress) {
        keyPressEvent(static_cast<QKeyEvent*>(event));
        if (event->isAccepted()) return true;
    }

    if (obj != m_mapView->viewport() && obj != m_mapView)
        return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::Wheel) {
        return false;
    }

    if (event->type() == QEvent::MouseMove) {
        auto *me = static_cast<QMouseEvent*>(event);
        QPointF sp = m_mapView->mapToScene(me->pos());
        updateCoordLabel(sp);

        // 手动平移(中键/Alt+左键): 按像素增量滚动滚动条
        if (m_panning) {
            QPoint delta = me->pos() - m_lastPanPos;
            m_lastPanPos = me->pos();
            m_mapView->horizontalScrollBar()->setValue(
                m_mapView->horizontalScrollBar()->value() - delta.x());
            m_mapView->verticalScrollBar()->setValue(
                m_mapView->verticalScrollBar()->value() - delta.y());
            updateRulers();
            return true;
        }

        if (m_isDraggingNode) {
            QPointF newWorld = sceneToWorld(sp.x(), sp.y());
            // 1. 更新被拖动节点自身的坐标
            auto updatePointPos = [&](QJsonArray &arr) {
                for (int i = 0; i < arr.size(); ++i) {
                    QJsonObject o = arr[i].toObject();
                    if (o.value("id").toString() == m_draggingNodeId) {
                        o["x"] = newWorld.x();
                        o["y"] = newWorld.y();
                        arr[i] = o;
                        break;
                    }
                }
            };
            if      (m_draggingNodeType == "point")         updatePointPos(m_points);
            else if (m_draggingNodeType == "charge")        updatePointPos(m_charges);
            else if (m_draggingNodeType == "highPrecision") updatePointPos(m_highPrecisionPoints);

            // 2. 实时拉伸/联动所有连接到该节点的直线
            for (int i = 0; i < m_lines.size(); ++i) {
                QJsonObject ln = m_lines[i].toObject();
                bool changed = false;
                if (ln.value("startid").toString() == m_draggingNodeId) {
                    ln["x1"] = newWorld.x(); ln["y1"] = newWorld.y();
                    changed = true;
                }
                if (ln.value("endid").toString() == m_draggingNodeId) {
                    ln["x2"] = newWorld.x(); ln["y2"] = newWorld.y();
                    changed = true;
                }
                if (changed) m_lines[i] = ln;
            }

            // 3. 实时拉伸/联动所有连接到该节点的曲线端点
            for (int i = 0; i < m_curves.size(); ++i) {
                QJsonObject cv = m_curves[i].toObject();
                bool changed = false;
                if (cv.value("startid").toString() == m_draggingNodeId) {
                    cv["x1"] = newWorld.x(); cv["y1"] = newWorld.y();
                    QJsonArray ctrlArr = cv.value("controlPoints").toArray();
                    if (!ctrlArr.isEmpty()) {
                        QJsonObject cp0 = ctrlArr[0].toObject();
                        cp0["x"] = newWorld.x(); cp0["y"] = newWorld.y();
                        ctrlArr[0] = cp0;
                        cv["controlPoints"] = ctrlArr;
                    }
                    changed = true;
                }
                if (cv.value("endid").toString() == m_draggingNodeId) {
                    cv["x2"] = newWorld.x(); cv["y2"] = newWorld.y();
                    QJsonArray ctrlArr = cv.value("controlPoints").toArray();
                    if (!ctrlArr.isEmpty()) {
                        int lastIdx = ctrlArr.size() - 1;
                        QJsonObject cpLast = ctrlArr[lastIdx].toObject();
                        cpLast["x"] = newWorld.x(); cpLast["y"] = newWorld.y();
                        ctrlArr[lastIdx] = cpLast;
                        cv["controlPoints"] = ctrlArr;
                    }
                    changed = true;
                }
                if (changed) m_curves[i] = cv;
            }

            // 4. 重绘场景并保持当前节点的高亮选中状态
            renderMap();
            m_dirty = true;
            QString key = m_draggingNodeType + "_" + m_draggingNodeId;
            if (m_featureItems.contains(key)) {
                m_selItem = m_featureItems.value(key);
                RouteFeatureSelect::applySelectColor(m_selItem);
            }
            return true;
        }

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

        if (m_isPainting && (m_drawStatus == Brush || m_drawStatus == Eraser)) {
            paintStroke(m_lastPaintImgPos, sp, false);
            m_lastPaintImgPos = sp;
            return true;
        }

        if (m_drawStatus != Drag) {
            // Show snap indicator in draw modes
            SnapResult snap = findSnap(sp);
            updateSnapIndicator(snap);
            updatePreview(snap.valid ? snap.scenePos : sp);
        } else {
            // Drag mode: hover cursor on handles or stations
            updateSnapIndicator({});
            const double hoverTol = hitTolScene(10.0);
            for (auto &lst : m_curveHandles.values())
                for (auto *h : lst) {
                    if (!h) continue;
                    if (std::hypot(sp.x()-h->pos().x(), sp.y()-h->pos().y()) < hoverTol) {
                        m_mapView->viewport()->setCursor(Qt::SizeAllCursor);
                        return false;
                    }
                }
            SnapResult nodeSnap = findSnap(sp, 0.4);
            if (nodeSnap.valid && (nodeSnap.type == "point" || nodeSnap.type == "charge" || nodeSnap.type == "highPrecision")) {
                m_mapView->viewport()->setCursor(Qt::SizeAllCursor);
                return false;
            }
            m_mapView->viewport()->setCursor(Qt::OpenHandCursor);
        }
        return false;
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent*>(event);

        // Right-click: complete curve drawing / cancel line chain / return to Drag
        if (me->button() == Qt::RightButton) {
            if (m_drawStatus == Curve && m_curveCtrlPts.size() >= 2) {
                finalizeCurve();
                return true;
            }
            if (m_drawStatus == Line && m_drawHasP1) {
                cancelDraw();
                clearPreview();
                return true;
            }
            if (m_drawStatus != Drag) setDrawMode(Drag);
            else cancelDraw();
            return true;
        }

        // Ctrl + 左键 → 放行给 MapCanvas 处理拖拽旋转(不在 eventFilter 中消费)
        if (me->button() == Qt::LeftButton && (me->modifiers() & Qt::ControlModifier)) {
            return false;
        }

        // 中键 或 Alt+左键 → 手动平移(替代 ScrollHandDrag，避免与手柄拖拽冲突)
        if (me->button() == Qt::MiddleButton ||
            (me->button() == Qt::LeftButton && (me->modifiers() & Qt::AltModifier))) {
            m_panning = true;
            m_lastPanPos = me->pos();
            m_mapView->viewport()->setCursor(Qt::ClosedHandCursor);
            return true;
        }

        if (me->button() != Qt::LeftButton) return false;
        QPointF sp = m_mapView->mapToScene(me->pos());

        if (m_drawStatus == Brush || m_drawStatus == Eraser) {
            pushMapUndo();
            m_isPainting = true;
            m_lastPaintImgPos = sp;
            paintStroke(sp, sp, true);
            return true;
        }

        // ─ Drag mode ─────────────────────────────────────────────────────────
        if (m_drawStatus == Drag) {
            const double tol = hitTolScene(10.0);
            for (auto lit = m_curveHandles.begin(); lit != m_curveHandles.end(); ++lit) {
                auto &handles = lit.value();
                for (int i = 0; i < handles.size(); ++i) {
                    if (!handles[i]) continue;   // 端点由节点显示，无手柄
                    QPointF hp = handles[i]->pos();
                    if (std::hypot(sp.x()-hp.x(), sp.y()-hp.y()) < tol) {
                        m_activeHandle     = handles[i];
                        m_handleCurveId    = lit.key();
                        m_handleIndex      = i;
                        m_isDraggingHandle = true;
                        m_mapView->viewport()->setCursor(Qt::ClosedHandCursor);
                        return true;
                    }
                }
            }

            // 优先检查是否点击在站点节点上（普通站点/充电点/精定位点），触发节点拖动
            SnapResult nodeSnap = findSnap(sp, 0.4);
            if (!nodeSnap.valid) {
                for (auto *it : m_scene->items(QRectF(sp.x()-tol, sp.y()-tol, tol*2, tol*2))) {
                    QGraphicsItem *target = it;
                    if (!target->data(0).isValid() && target->parentItem()) target = target->parentItem();
                    QString type = target->data(0).toString();
                    QString id = target->data(1).toString();
                    if (type == "point" || type == "charge" || type == "highPrecision") {
                        nodeSnap.valid = true;
                        nodeSnap.type = type;
                        nodeSnap.id = id;
                        nodeSnap.scenePos = target->pos();
                        nodeSnap.worldPos = sceneToWorld(target->pos().x(), target->pos().y());
                        break;
                    }
                }
            }
            if (nodeSnap.valid && (nodeSnap.type == "point" || nodeSnap.type == "charge" || nodeSnap.type == "highPrecision")) {
                m_isDraggingNode    = true;
                m_draggingNodeType  = nodeSnap.type;
                m_draggingNodeId    = nodeSnap.id;
                m_dragStartScenePos = nodeSnap.scenePos;
                m_dragStartWorldPos = nodeSnap.worldPos;
                showFeatureProperties(nodeSnap.type, nodeSnap.id);
                m_mapView->viewport()->setCursor(Qt::ClosedHandCursor);
                return true;
            }

            selectFeatureAt(sp);
            return false;
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
            else {
                const double epcX = AlignConfigManager::instance().epcX();
                addCharge(sp, epcX, 0.0, 0.0);
            }
            break;
        case HighPrecision:
            if (hasOverlap(sp)) { selectFeatureAt(sp); }
            else {
                const double epcX = AlignConfigManager::instance().epcX();
                addHighPrecisionPoint(sp, epcX, 0.0, 0.0);
            }
            break;
        case Line: {
            if (!m_drawHasP1) {
                // 起点: 优先吸附到已有点，未命中则在空地自动创建新普通站点(LM)
                SnapResult head = ensureNodeAt(usePos);
                m_drawP1   = head.scenePos;
                m_drawP1Id = head.id;
                m_drawHasP1 = true;
            } else {
                // 终点: 优先吸附到已有点，未命中则在空地自动创建新普通站点(LM)
                SnapResult tail = ensureNodeAt(usePos);
                if (tail.id == m_drawP1Id) {
                    Toast::warning(this, "起点与终点不能为同一站点");
                    clearPreview();
                } else if (hasRouteBetween(m_drawP1Id, tail.id, m_drawP1, tail.scenePos)) {
                    Toast::warning(this, "两站点之间已存在连线，不可重复绘制");
                    clearPreview();
                } else {
                    addLine(m_drawP1, tail.scenePos, m_drawP1Id, tail.id);
                    // 连续连线模式: 刚绘制好的终点自动作为下一条线段的起点
                    m_drawP1   = tail.scenePos;
                    m_drawP1Id = tail.id;
                    m_drawHasP1 = true;
                    clearPreview();
                }
            }
            break;
        }
        case Curve: {
            // 三阶贝塞尔曲线：最多 4 个控制点 (P0 起点, P1 控制点1, P2 控制点2, P3 终点)
            int n = m_curveCtrlPts.size();
            if (n == 0) {
                // 起点: 强制吸附或创建节点
                SnapResult head = ensureNodeAt(usePos);
                m_curveCtrlPts.append(head.scenePos);
                m_curveCtrlIds.append(head.id);
                m_drawP1   = head.scenePos;
                m_drawP1Id = head.id;
                m_drawHasP1 = true;
            } else if (n < 3) {
                // 第 2 点 (P1) 或 第 3 点 (P2): 自由中间控制点
                m_curveCtrlPts.append(usePos);
                m_curveCtrlIds.append(useId);
            } else if (n == 3) {
                // 第 4 点 (P3): 达到三阶上限（4控制点），强制吸附或创建终点节点并自动完成绘制
                SnapResult tail = ensureNodeAt(usePos);
                QString startId = m_curveCtrlIds.isEmpty() ? QString() : m_curveCtrlIds.first();
                if (!startId.isEmpty() && tail.id == startId) {
                    Toast::warning(this, "起点与终点不能为同一站点");
                    cancelDraw();
                } else if (!startId.isEmpty() && hasRouteBetween(startId, tail.id, m_curveCtrlPts.first(), tail.scenePos)) {
                    Toast::warning(this, "两站点之间已存在连线，不可重复绘制");
                    cancelDraw();
                } else {
                    m_curveCtrlPts.append(tail.scenePos);
                    m_curveCtrlIds.append(tail.id);
                    finalizeCurve();
                }
            }
            break;
        }
        default: break;
        }
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && m_isPainting) {
            m_isPainting = false;
            return true;
        }
        // 手动平移释放
        if (m_panning &&
            (me->button() == Qt::MiddleButton || me->button() == Qt::LeftButton)) {
            m_panning = false;
            m_mapView->viewport()->setCursor(Qt::OpenHandCursor);
            return true;
        }
        if (me->button() == Qt::LeftButton && m_isDraggingNode) {
            m_isDraggingNode = false;
            m_dirty = true;
            renderMap();
            showFeatureProperties(m_draggingNodeType, m_draggingNodeId);
            if (m_tableVisible) refreshTable();
            m_mapView->viewport()->setCursor(Qt::OpenHandCursor);
            return true;
        }
        if (me->button() == Qt::LeftButton && m_isDraggingHandle) {
            // Commit to JSON: 支持任意阶贝塞尔曲线
            // 端点手柄可能为 nullptr(已关联节点时由节点显示)，其位置取自曲线 JSON 的
            // x1/y1、x2/y2；仅非空手柄(中间控制点或未关联节点的端点)随拖拽更新。
            auto &lst = m_curveHandles[m_handleCurveId];
            auto handlePos = [&](int idx) {
                if (lst[idx]) return lst[idx]->pos();
                // 端点无手柄 → 从 JSON 取端点坐标
                for (const auto &v : m_curves) {
                    auto o = v.toObject();
                    if (o.value("id").toString() != m_handleCurveId) continue;
                    if (idx == 0)
                        return worldToScene(o.value("x1").toDouble(), o.value("y1").toDouble());
                    return worldToScene(o.value("x2").toDouble(), o.value("y2").toDouble());
                }
                return QPointF();
            };
            for (int i = 0; i < m_curves.size(); ++i) {
                QJsonObject cv = m_curves[i].toObject();
                if (cv.value("id").toString() != m_handleCurveId) continue;

                // 更新 controlPoints 数组
                QJsonArray ctrlArr = cv.value("controlPoints").toArray();
                if (ctrlArr.isEmpty() && lst.size() == 3) {
                    // 兼容旧格式: 从 cx/cy 构建 controlPoints
                    QList<QPointF> wPts;
                    for (int k = 0; k < lst.size(); ++k)
                        wPts.append(sceneToWorld(handlePos(k).x(), handlePos(k).y()));
                    cv["x1"] = wPts[0].x(); cv["y1"] = wPts[0].y();
                    cv["cx"] = wPts[1].x(); cv["cy"] = wPts[1].y();
                    cv["x2"] = wPts[2].x(); cv["y2"] = wPts[2].y();
                } else {
                    // 多阶贝塞尔: 逐个更新 controlPoints(跳过无手柄的端点，保持其坐标)
                    for (int j = 0; j < lst.size() && j < ctrlArr.size(); ++j) {
                        QPointF wp = sceneToWorld(handlePos(j).x(), handlePos(j).y());
                        QJsonObject pt = ctrlArr[j].toObject();
                        pt["x"] = wp.x(); pt["y"] = wp.y();
                        ctrlArr[j] = pt;
                    }
                    cv["controlPoints"] = ctrlArr;
                    // 同步起终点字段
                    if (!ctrlArr.isEmpty()) {
                        QPointF wFirst = sceneToWorld(handlePos(0).x(), handlePos(0).y());
                        QPointF wLast  = sceneToWorld(handlePos(lst.size()-1).x(),
                                                     handlePos(lst.size()-1).y());
                        cv["x1"] = wFirst.x(); cv["y1"] = wFirst.y();
                        cv["x2"] = wLast.x();  cv["y2"] = wLast.y();
                    }
                }
                // 被拖拽的端点手柄落点若吸附到某个已有点，记录其 id
                if (m_handleIndex == 0 || m_handleIndex == lst.size() - 1) {
                    SnapResult snap = findSnap(lst[m_handleIndex]->pos());
                    const QString &key = (m_handleIndex == 0) ? "startid" : "endid";
                    cv[key] = snap.valid ? snap.id : QString();
                }
                m_curves[i] = cv; break;
            }
            m_isDraggingHandle = false;
            m_activeHandle = nullptr;
            m_mapView->setDragMode(QGraphicsView::NoDrag);
            m_mapView->viewport()->setCursor(Qt::OpenHandCursor);
            m_dirty = true;   // 拖拽手柄修改了曲线
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
        if (code != 200) {
            // 车列表接口失败时不能静默退出: 否则 m_currentCarId 始终为空、initMqtt() 永不执行，
            // "设备位置"开关打开后也收不到任何位姿。此处退化为用全局配置的设备号订阅。
            debugLog(QString("[loadCarList] 请求失败 code=%1 -> 退化为使用全局设备号订阅").arg(code));
            initMqtt();
            return;
        }
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

        // 无论列表是否为空都要订阅位姿: initMqtt() 内部会退化为全局配置/默认设备号，
        // 否则列表为空时"设备位置"开关永远收不到位姿
        if (m_carCombo->count() > 0) m_currentCarId = m_carCombo->currentData().toString();
        initMqtt();
    });
    connect(m_carCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        disconnectMqtt();
        m_currentCarId = m_carCombo->currentData().toString();
        initMqtt();
    });
}

void MapEditWidget::initMqtt()
{
    QString serverIp = AppConfig::instance().serverIp();
    if (serverIp.isEmpty()) return;
    if (!MqttService::instance().isConnected()) {
        MqttService::instance().connectToHost(serverIp, 1883);
    }

    auto handleInfoPayload = [this](const QString &topic, const QByteArray &payload) {
        static int logCount = 0;
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(payload, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
        QJsonObject d = doc.object();
        QJsonObject dataObj = d.value("data").toObject();
        QJsonObject poseObj = d.value("pose").toObject();

        RobotInfoManager::instance().updateFromPayload(topic, payload);

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

        // 地图匹配校验 (若报文中指定了 mapId)
        QJsonValue mapIdV = d.contains("map_id") ? d.value("map_id") : d.value("mapId");
        if (mapIdV.isUndefined() && !dataObj.isEmpty()) {
            mapIdV = dataObj.contains("map_id") ? dataObj.value("map_id") : dataObj.value("mapId");
        }
        int msgMapId = mapIdV.toVariant().toInt();
        int curMapId = m_mapId.toInt();
        bool mapIdMismatch = (msgMapId > 0 && curMapId > 0 && msgMapId != curMapId);
        if (mapIdMismatch) { logCount++; return; }

        m_carX     = dv({"carX", "car_x", "x", "current_x", "posX"});
        m_carY     = dv({"carY", "car_y", "y", "current_y", "posY"});
        m_carYaw   = dv({"carYaw", "car_yaw", "yaw", "carTheta", "theta", "angle"});
        m_carTheta = dv({"theta", "car_theta", "carTheta", "angle"}, qRadiansToDegrees(m_carYaw));
        m_hasCarPose = true;

        // 设备相对定位末端坐标(epc): 用于充电桩/高精度点打点
        if (d.contains("epc_x") || d.contains("epc_y") || (!dataObj.isEmpty() && (dataObj.contains("epc_x") || dataObj.contains("epc_y")))) {
            m_epcX     = dv({"epc_x", "epcX"}, 0);
            m_epcY     = dv({"epc_y", "epcY"}, 0);
            m_epcTheta = dv({"epc_theta", "epc_Theta", "epcTheta"}, 0);
            m_hasEpcPose = true;
        }
        if (logCount < 5) {
            QPointF sp = worldToScene(m_carX, m_carY);
            debugLog(QString("[/info msg #%1] topic=%2 carX=%3 carY=%4 carYaw=%5 -> scene=(%6,%7)")
                .arg(logCount).arg(topic).arg(m_carX).arg(m_carY).arg(m_carYaw).arg(sp.x()).arg(sp.y()));
        }
        logCount++;
        updateRobotMarker();
    };

    // 只有 "/rob/1/info"
    m_currentCarId = QStringLiteral("1");
    MqttService::instance().subscribeTopic(QStringLiteral("/rob/1/info"), 0, handleInfoPayload);
}

void MapEditWidget::disconnectMqtt()
{
    MqttService::instance().unsubscribeTopic(QStringLiteral("/rob/1/info"));
    m_hasCarPose = false;
    m_hasEpcPose = false;
    m_devicePointPending = false;
    updateRobotMarker();
}

// 与 worldToScene 共用同一坐标转换，避免与栅格/路网产生位置偏差
void MapEditWidget::updateRobotMarker()
{
    if (!m_robotItem) return;
    if (!m_showRobotPosition || !m_hasCarPose) {
        // 开关已开但仍无位姿时记录一次，便于排查订阅/上报问题 (限频避免刷屏)
        static int s_warnCount = 0;
        if (m_showRobotPosition && !m_hasCarPose && s_warnCount < 3) {
            ++s_warnCount;
            debugLog(QString("[设备位置] 开关已打开但未收到设备位姿 (车号=%1)").arg(m_currentCarId));
        }
        m_robotItem->setVisible(false);
        return;
    }
    QPointF sp = worldToScene(m_carX, m_carY);

    // 地图比例尺计算: ppm (pixels per meter)
    double mapW = m_worldR - m_worldL;
    double mapH = m_worldT - m_worldB;
    double ppm = 20.0; // 默认备用比例尺 0.05m/px
    if (m_imgW > 0 && mapW > 0 && m_imgH > 0 && mapH > 0) {
        ppm = ((double)m_imgW / mapW + (double)m_imgH / mapH) / 2.0;
    } else if (m_imgW > 0 && mapW > 0) {
        ppm = (double)m_imgW / mapW;
    }

    // 真实尺寸: 车长 1.0m (纵向/车头朝向), 车宽 0.5m (横向)
    const double kRobotLength = 1.0;
    const double kRobotWidth  = 0.5;
    double sceneW = kRobotWidth * ppm;
    double sceneH = kRobotLength * ppm;

    double srcW   = m_robotPixmap.width();
    double srcH   = m_robotPixmap.height();
    double pivotX = srcW / 2.0;
    double pivotY = srcH / 2.0;
    double scale  = (srcW > 0) ? (sceneW / srcW) : 1.0;

    m_robotItem->setScale(scale);
    m_robotItem->setTransformOriginPoint(pivotX, pivotY);
    m_robotItem->setPos(sp.x() - pivotX, sp.y() - pivotY);

    // ENU: yaw=0→东, yaw=π/2→北, 逆时针为正；车头默认朝北。
    // 与 MapViewWidget::renderRobot 保持同一约定：screenAngle = 90 - yawDeg
    double yawRad = (qAbs(m_carYaw) > M_PI * 2) ? qDegreesToRadians(m_carYaw) : m_carYaw;
    m_robotItem->setRotation(90.0 - qRadiansToDegrees(yawRad));
    m_robotItem->setVisible(true);
}

void MapEditWidget::setupShortcuts()
{
    // 撤销 / 重做 (底图涂绘历史)
    auto *undoSc = new QShortcut(QKeySequence::Undo, this, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(undoSc, &QShortcut::activated, this, &MapEditWidget::onUndo);

    auto *redoSc = new QShortcut(QKeySequence::Redo, this, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(redoSc, &QShortcut::activated, this, &MapEditWidget::onRedo);

    auto *redoSc2 = new QShortcut(QKeySequence("Ctrl+Shift+Z"), this, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(redoSc2, &QShortcut::activated, this, &MapEditWidget::onRedo);

    // 保存地图
    auto *saveSc = new QShortcut(QKeySequence::Save, this, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(saveSc, &QShortcut::activated, this, [this]() { onSaveMap(); });

    auto *saveSc2 = new QShortcut(QKeySequence("Alt+S"), this, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(saveSc2, &QShortcut::activated, this, [this]() { onSaveMap(); });

    // 快捷帮助
    auto *helpSc = new QShortcut(QKeySequence::HelpContents, this, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(helpSc, &QShortcut::activated, this, &MapEditWidget::showHelpDialog);
}

void MapEditWidget::keyPressEvent(QKeyEvent *event)
{
    QWidget *focused = QApplication::focusWidget();
    bool inInputWidget = (qobject_cast<QLineEdit*>(focused) ||
                          qobject_cast<QAbstractSpinBox*>(focused) ||
                          qobject_cast<QComboBox*>(focused));

    if (!inInputWidget) {
        if (TeleopController::instance().isActive()) {
            int k = event->key();
            if (k == Qt::Key_W || k == Qt::Key_A || k == Qt::Key_S || k == Qt::Key_D ||
                k == Qt::Key_Space || k == Qt::Key_P || k == Qt::Key_L) {
                // 抢占遥控期间，WASD/Space/P/L 交由全局遥控器处理，不触发地图编辑快捷键
                return;
            }
        }

        if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
            deleteSelected();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            cancelDraw();
            clearSelection();
            setDrawMode(Drag);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_M) {
            setDrawMode(Point);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_C) {
            setDrawMode(Charge);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_T) {
            setDrawMode(HighPrecision);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_P || event->key() == Qt::Key_L) {
            setDrawMode(Line);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_B) {
            setDrawMode(Curve);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_E) {
            setDrawMode(Eraser);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Space) {
            // 循环切换工具: 拖拽 -> 普通站点 -> 充电点 -> 精定位点 -> 线段 -> 三阶曲线 -> 画笔 -> 橡皮
            int nextMode = Drag;
            switch (m_drawStatus) {
            case Drag:          nextMode = Point; break;
            case Point:         nextMode = Charge; break;
            case Charge:        nextMode = HighPrecision; break;
            case HighPrecision: nextMode = Line; break;
            case Line:          nextMode = Curve; break;
            case Curve:         nextMode = Brush; break;
            case Brush:         nextMode = Eraser; break;
            case Eraser:        nextMode = Drag; break;
            default:            nextMode = Drag; break;
            }
            setDrawMode(nextMode);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_F1) {
            showHelpDialog();
            event->accept();
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

void MapEditWidget::showHelpDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("地图编辑与快捷操作指南");
    dlg.setMinimumWidth(560);
    dlg.setStyleSheet(
        "QDialog { background: #16202e; color: #d0e0f0; font-size: 13px; }"
        "QGroupBox { font-weight: bold; border: 1px solid rgba(255,255,255,0.12); border-radius: 6px; margin-top: 12px; padding: 12px; background: #1a2638; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; color: #5dade2; }"
        "QLabel { background: transparent; color: #b8d2e8; }"
        "QPushButton { background: #2471a3; color: white; border: none; border-radius: 4px; padding: 6px 18px; font-weight: bold; font-size: 12px; }"
        "QPushButton:hover { background: #2980b9; }");

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);
    layout->setContentsMargins(18, 18, 18, 18);

    // 1. 快捷键
    QGroupBox *scBox = new QGroupBox("⌨️ 常用键盘快捷键", &dlg);
    QVBoxLayout *scLayout = new QVBoxLayout(scBox);
    scLayout->setSpacing(6);
    auto addRow = [&](const QString &key, const QString &desc) {
        QHBoxLayout *row = new QHBoxLayout;
        QLabel *kLbl = new QLabel(key);
        kLbl->setFixedWidth(130);
        kLbl->setStyleSheet("color: #f1c40f; font-family: Consolas, monospace; font-weight: bold;");
        QLabel *dLbl = new QLabel(desc);
        dLbl->setStyleSheet("color: #ecf0f1;");
        row->addWidget(kLbl);
        row->addWidget(dLbl, 1);
        scLayout->addLayout(row);
    };
    addRow("Esc", "指针模式：选择要素 / 拖拽移动站点与线路");
    addRow("M", "站点工具：点击放置站点 LM（蓝）");
    addRow("C", "站点工具-特殊工作站：点击放置充电桩点 CP_（金，typeName=ChargePoint）");
    addRow("T", "站点工具-工作站：点击放置高精度点 AP（红，typeName=ActionPoint）");
    addRow("P / L", "路径/线段工具：空地自动建站，连续连线");
    addRow("B", "三阶曲线工具：起点 ➔ 控制点 ➔ 终点自动吸附/建站");
    addRow("E", "橡皮擦：擦除栅格涂改，恢复可通行区域");
    addRow("Delete / Backspace", "删除当前选中的路网要素（站点、线段、曲线）");
    addRow("Ctrl + Z", "撤销底图画笔 / 橡皮擦涂改");
    addRow("Ctrl + Y / Ctrl+Shift+Z", "重做底图涂改操作");
    addRow("Ctrl + S / Alt + S", "保存地图底图与路网结构");
    addRow("空格键 (Space)", "循环切换绘图工具");
    addRow("F1", "打开此地图编辑与快捷操作指南");
    layout->addWidget(scBox);

    // 2. 鼠标交互
    QGroupBox *mouseBox = new QGroupBox("🖱️ 鼠标交互与视图操作", &dlg);
    QVBoxLayout *mLayout = new QVBoxLayout(mouseBox);
    mLayout->setSpacing(6);
    auto addMouseRow = [&](const QString &act, const QString &desc) {
        QHBoxLayout *row = new QHBoxLayout;
        QLabel *aLbl = new QLabel(act);
        aLbl->setFixedWidth(130);
        aLbl->setStyleSheet("color: #3498db; font-weight: bold;");
        QLabel *dLbl = new QLabel(desc);
        dLbl->setStyleSheet("color: #ecf0f1;");
        row->addWidget(aLbl);
        row->addWidget(dLbl, 1);
        mLayout->addLayout(row);
    };
    addMouseRow("鼠标左键拖拽", "拖动站点（关联的所有线路与曲线实时跟随拉伸）/ 拖动手柄");
    addMouseRow("鼠标右键", "路径/曲线绘制中断连续连线；或返回指针模式");
    addMouseRow("中键 / Alt+左键", "平移拖拽地图画布视图");
    addMouseRow("鼠标滚轮", "以光标所在位置为中心缩放地图");
    layout->addWidget(mouseBox);

    // 3. 图元与配色说明
    QGroupBox *colorBox = new QGroupBox("📍 路网图元与配色规范", &dlg);
    QVBoxLayout *cLayout = new QVBoxLayout(colorBox);
    cLayout->setSpacing(6);
    auto addColorRow = [&](const QString &badge, const QString &col, const QString &desc) {
        QHBoxLayout *row = new QHBoxLayout;
        QLabel *bLbl = new QLabel(badge);
        bLbl->setFixedWidth(130);
        bLbl->setStyleSheet(QString("color: %1; font-weight: bold;").arg(col));
        QLabel *dLbl = new QLabel(desc);
        dLbl->setStyleSheet("color: #ecf0f1;");
        row->addWidget(bLbl);
        row->addWidget(dLbl, 1);
        cLayout->addLayout(row);
    };
    addColorRow("■ 普通站点 (蓝色)", "#3A8FD0", "标准 AGV 停止位姿（长0.5m x 宽0.25m，含定位十字）");
    addColorRow("■ 充电点 (金色)", "#FFB800", "充电桩对接站点（支持手动放置或设备相对定位打点）");
    addColorRow("■ 精定位点 (红色)", "#FF4D4F", "机台移载 / PGV 二维码 / 反光柱高精度对接站点");
    addColorRow("━ 直线 / 曲线", "#50C878", "标准双向导轨线路，居中带有微型燕尾指示箭头");
    layout->addWidget(colorBox);

    // 底部按钮
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    QPushButton *okBtn = new QPushButton("确定", &dlg);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnLayout->addWidget(okBtn);
    layout->addLayout(btnLayout);
    dlg.exec();
}

void MapEditWidget::onTeleopClicked()
{
    TeleopController::instance().setActive(!TeleopController::instance().isActive());
}

void MapEditWidget::onTeleopActiveChanged(bool active)
{
    if (!m_teleopBtn) return;
    // 状态切换注意不要修改文字，保持“抢占控制权”，只修改逻辑与高亮样式
    if (active) {
        m_teleopBtn->setStyleSheet(
            "QPushButton{color:white;border:none;border-radius:4px;"
            "padding:4px 12px;font-size:12px;background:#c0392b;}"
            "QPushButton:hover{background:#e74c3c;}");
        Toast::info(this, "已抢占控制权：WASD 移动 | Space 刹车 | Ctrl 加速 | P/L 升降");
    } else {
        m_teleopBtn->setStyleSheet(
            "QPushButton{color:white;border:none;border-radius:4px;"
            "padding:4px 12px;font-size:12px;background:#1e6fd4;}"
            "QPushButton:hover{background:#2980e8;}");
    }
}
