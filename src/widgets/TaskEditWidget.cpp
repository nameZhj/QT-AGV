#include "TaskEditWidget.h"
#include "MapViewWidget.h"
#include "core/ApiService.h"
#include "core/AppConfig.h"
#include "core/LoadingDialog.h"
#include "core/Toast.h"
#include "core/MapPgmUtil.h"
#include "core/MotionConfigManager.h"
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QGraphicsPixmapItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QWheelEvent>
#include <QJsonDocument>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTimeEdit>
#include <QFrame>
#include <QPainter>
#include <QToolTip>
#include <QTimer>
#include <QInputDialog>
#include <cmath>

// ── Icon helpers ──────────────────────────────────────────────────────────────

namespace {

QPixmap drawTaskIcon(const QString &name, const QColor &col, int sz = 22)
{
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    double c = sz * 0.5;

    if (name == "pan") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        double arm = sz*0.34, head = sz*0.17;
        auto arw = [&](double dx, double dy) {
            QPointF tip(c+dx*arm, c+dy*arm), base(c+dx*2.5, c+dy*2.5);
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
    else if (name == "settings") {
        // 齿轮图标
        p.setPen(Qt::NoPen); p.setBrush(col);
        // 外圈齿轮齿
        QPainterPath gear;
        double outerR = sz*0.42, innerR = sz*0.30, holeR = sz*0.14;
        int teeth = 8;
        for (int i = 0; i < teeth; ++i) {
            double a0 = (i * 360.0 / teeth) * M_PI / 180.0;
            double a1 = ((i + 0.3) * 360.0 / teeth) * M_PI / 180.0;
            double a2 = ((i + 0.7) * 360.0 / teeth) * M_PI / 180.0;
            double a3 = ((i + 1.0) * 360.0 / teeth) * M_PI / 180.0;
            if (i == 0) gear.moveTo(c + qCos(a0)*innerR, c + qSin(a0)*innerR);
            gear.lineTo(c + qCos(a1)*innerR, c + qSin(a1)*innerR);
            gear.lineTo(c + qCos(a1)*outerR, c + qSin(a1)*outerR);
            gear.lineTo(c + qCos(a2)*outerR, c + qSin(a2)*outerR);
            gear.lineTo(c + qCos(a2)*innerR, c + qSin(a2)*innerR);
            gear.lineTo(c + qCos(a3)*innerR, c + qSin(a3)*innerR);
        }
        gear.closeSubpath();
        p.drawPath(gear);
        // 中心孔
        p.setBrush(Qt::transparent);
        p.setPen(QPen(col, 0));
        p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        p.drawEllipse(QPointF(c,c), holeR, holeR);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }
    return pm;
}

QIcon makeTaskToolIcon(const QString &name)
{
    QIcon icon;
    icon.addPixmap(drawTaskIcon(name, QColor("#7a95b0")), QIcon::Normal,  QIcon::Off);
    icon.addPixmap(drawTaskIcon(name, QColor("#c8ddef")), QIcon::Active,  QIcon::Off);
    icon.addPixmap(drawTaskIcon(name, QColor("white")),   QIcon::Normal,  QIcon::On);
    icon.addPixmap(drawTaskIcon(name, QColor("white")),   QIcon::Active,  QIcon::On);
    return icon;
}

struct TaskInstantTip : QObject {
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

// 数字步进器默认在鼠标悬停时就会响应滚轮改变数值，鼠标只是经过(并非想调整这个
// 控件)时很容易被意外改掉——拦截并消费滚轮事件，使其完全不响应滚轮。
struct NoWheelFilter : QObject {
    using QObject::QObject;
    bool eventFilter(QObject *, QEvent *e) override {
        return e->type() == QEvent::Wheel;
    }
};

QString taskTtHtml(const QString &title, const QString &body)
{
    QString b = body; b.replace('\n', QLatin1String("<br>"));
    return QString("<div style='min-width:150px;'>"
                   "<div style='font-size:13px;font-weight:600;color:#ddeeff;"
                   "margin-bottom:3px;'>%1</div>"
                   "<div style='color:#90adc8;font-size:11px;line-height:1.6;'>%2</div>"
                   "</div>").arg(title, b);
}

// ── 动作指令目录 ──────────────────────────────────────────────────────────────
// 与 Vue task/edit.vue 完全一致：数据来源于
// utils/pageSetting/carAction/{base,lift}.json，每个动作的参数直接铺平合并进
// action 对象本身（不是嵌套的 "params" 字段），action_type 是固定的整数编码，
// 不是随意起的字符串——这两点是设备真正能识别这条指令的硬性前提。
struct ActionParamField { QString label; QString param; double defaultValue; };
struct ActionDef { int actionType; QString name; QList<ActionParamField> fields; };

const QList<ActionDef> &actionCatalog()
{
    static const QList<ActionDef> cat = {
        {22, "倒行至下一个点", {
            {"倒行速度m/s", "astern_speed", -0.1}
        }},
        {21, "实时避障范围", {
            {"正向停止膨胀半径m",   "intrusion_stop_length",         0},
            {"侧向停止膨胀半径m",   "intrusion_stop_width",          0},
            {"正向减速膨胀半径m",   "intrusion_deceleration_length", 0},
            {"侧向减速膨胀半径m",   "intrusion_deceleration_width",  0},
        }},
        {5, "原地旋转", {
            {"旋转角度", "rotate_angle", 90.0},
            {"旋转速度", "rotate_speed", MotionConfigManager::instance().maxAngularSpeedDeg()}
        }},
        {20, "原地等待", {
            {"等待时间s", "delay", 0}
        }},
        {3, "顶升", {
            {"目标顶升高度", "lift_height", 0},
            {"顶升线速度",   "lift_linear", 0.01}
        }},
    };
    return cat;
}

const ActionDef *findActionDef(int actionType)
{
    for (const auto &d : actionCatalog())
        if (d.actionType == actionType) return &d;
    return nullptr;
}

// 弹窗编辑动作参数。existingValues 为空表示新增(用目录里的默认值预填)，
// 非空表示修改(用已保存的实际值预填)。用户取消返回空 QJsonObject 作为哨兵值，
// 调用方需要据此区分"取消"和"无参数动作"两种情况。
QJsonObject editActionParamsDialog(QWidget *parent, const ActionDef &def,
                                    const QJsonObject &existingValues, const QString &dialogTitle)
{
    QJsonObject action;
    action["name"] = def.name;
    action["action_type"] = def.actionType;
    if (def.fields.isEmpty()) return action;   // 与 Vue 一致：无参数的动作直接返回，不弹窗

    QDialog dlg(parent);
    dlg.setWindowTitle(dialogTitle);
    QFormLayout *form = new QFormLayout(&dlg);

    const auto &mc = MotionConfigManager::instance();
    QList<QDoubleSpinBox*> spins;
    for (const auto &f : def.fields) {
        QDoubleSpinBox *spin = new QDoubleSpinBox;
        if (f.param == "rotate_angle") {
            // 旋转角度不能为负数
            spin->setRange(0.0, 360.0);
            spin->setSingleStep(5.0);
        } else if (f.param == "rotate_speed") {
            // 旋转角速度范围以元数据 paramMax 为准，支持负值
            const double limitMaxRot = mc.paramMax(QStringLiteral("max_angular_speed"), 60.0);
            spin->setRange(-limitMaxRot, limitMaxRot);
            spin->setSingleStep(2.0);
        } else if (f.param == "astern_speed") {
            // 倒行速度范围以元数据 paramMax 为准，支持负值
            const double limitMaxSpd = mc.paramMax(QStringLiteral("max_speed"), 1.0);
            spin->setRange(-limitMaxSpd, limitMaxSpd);
            spin->setSingleStep(0.05);
        } else if (f.param == "distance") {
            // 移动距离不能为负数
            spin->setRange(0.0, 100.0);
            spin->setSingleStep(0.1);
        } else if (f.param.contains("intrusion")) {
            spin->setRange(0.0, 10.0);
            spin->setSingleStep(0.05);
        } else if (f.param == "lift_height") {
            spin->setRange(0.0, 0.15);
            spin->setSingleStep(0.01);
        } else if (f.param == "lift_linear") {
            spin->setRange(0.0, 0.1);
            spin->setSingleStep(0.01);
        } else if (f.param == "delay") {
            spin->setRange(0.0, 86400.0);
            spin->setSingleStep(1.0);
        } else {
            spin->setRange(-1e6, 1e6);
            spin->setSingleStep(0.1);
        }
        spin->setDecimals(3);
        double val = existingValues.contains(f.param) ? existingValues.value(f.param).toDouble() : f.defaultValue;
        if (f.param == "rotate_angle" || f.param == "distance") {
            val = std::max(0.0, val);
        }
        spin->setValue(val);
        form->addRow(f.label + ":", spin);
        spins.append(spin);
    }

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Ok)->setText("确定");
    btns->button(QDialogButtonBox::Cancel)->setText("取消");
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted) return QJsonObject();

    for (int i = 0; i < def.fields.size(); ++i)
        action[def.fields[i].param] = spins[i]->value();
    return action;
}

// 与 Vue handleActionParam 一致：按目录里的字段顺序拼接 "标签值"
QString formatActionParams(const QJsonObject &action)
{
    const ActionDef *def = findActionDef(action.value("action_type").toInt(-1));
    if (!def) return QString();
    QStringList parts;
    for (const auto &f : def->fields)
        parts << QString("%1%2").arg(f.label).arg(action.value(f.param).toDouble());
    return parts.join("  ");
}

} // namespace

// ── Construction ──────────────────────────────────────────────────────────────

TaskEditWidget::TaskEditWidget(const QString &taskId, QWidget *parent)
    : QWidget(parent), m_taskId(taskId)
{
    setupUI();
    loadMapList();
    loadCarList();
    if (!m_taskId.isEmpty()) {
        loadTaskData();
    }
}

TaskEditWidget::~TaskEditWidget() {}

void TaskEditWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 顶部标题栏
    QWidget *headerBar = new QWidget;
    headerBar->setFixedHeight(44);
    headerBar->setStyleSheet("background:#1e2d3d; border-bottom:1px solid rgba(255,255,255,0.06);");
    QHBoxLayout *headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(8, 4, 8, 4);
    headerLayout->setSpacing(8);
    QPushButton *backBtn = new QPushButton("← 返回");
    backBtn->setStyleSheet("QPushButton{color:white;border:none;border-radius:4px;padding:4px 12px;font-size:12px;background:transparent;}"
                           "QPushButton:hover{background:rgba(255,255,255,18);}");
    connect(backBtn, &QPushButton::clicked, this, &TaskEditWidget::goBack);
    m_mapInfoLabel = new QLabel(m_taskId.isEmpty() ? "新增任务" : "编辑任务");
    m_mapInfoLabel->setStyleSheet("color:#9ab8d8; font-weight:bold; font-size:13px;");
    headerLayout->addWidget(backBtn);
    QFrame *hdiv = new QFrame; hdiv->setFrameShape(QFrame::VLine);
    hdiv->setStyleSheet("color:rgba(255,255,255,0.08);"); headerLayout->addWidget(hdiv);
    headerLayout->addWidget(m_mapInfoLabel);
    headerLayout->addStretch();
    mainLayout->addWidget(headerBar);

    // 主体
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // 左侧：地图 + 绘制工具栏（参考Vue的button-container）
    QWidget *mapPanel = new QWidget;
    QHBoxLayout *mapOuterLayout = new QHBoxLayout(mapPanel);
    mapOuterLayout->setContentsMargins(0, 0, 0, 0);
    mapOuterLayout->setSpacing(0);

    // 绘制工具栏（垂直排列，在地图左侧）
    static const char *TBSTYLE =
        "background:#1a2230;";
    static const char *BTNSTYLE =
        "QPushButton{background:transparent;border:none;border-radius:7px;padding:0;}"
        "QPushButton:hover{background:rgba(100,160,220,0.14);}"
        "QPushButton:checked{background:rgba(41,128,185,210);}";

    QWidget *toolbar = new QWidget;
    toolbar->setFixedWidth(50);
    toolbar->setStyleSheet(TBSTYLE);
    QVBoxLayout *toolbarLayout = new QVBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(5, 10, 5, 10);
    toolbarLayout->setSpacing(2);

    auto makeToolBtn = [&](const QString &iconName, const QString &tip, const QString &detail) {
        auto *b = new QPushButton;
        b->setIcon(makeTaskToolIcon(iconName));
        b->setIconSize(QSize(22, 22));
        b->setCheckable(true);
        b->setFixedSize(40, 40);
        b->setStyleSheet(BTNSTYLE);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(taskTtHtml(tip, detail));
        b->installEventFilter(new TaskInstantTip(b));
        return b;
    };

    m_dragBtn   = makeToolBtn("pan",   "拖拽",   "平移地图 / 选择要素");
    m_targetBtn = makeToolBtn("point", "任务点", "点击放置任务目标点\n右键取消");
    m_dragBtn->setChecked(true);
    toolbarLayout->addWidget(m_dragBtn,   0, Qt::AlignHCenter);
    toolbarLayout->addWidget(m_targetBtn, 0, Qt::AlignHCenter);

    connect(m_dragBtn, &QPushButton::clicked, [this]() {
        m_drawStatus = Drag;
        m_mapView->setDragMode(QGraphicsView::ScrollHandDrag);
        m_mapView->viewport()->setCursor(Qt::OpenHandCursor);
        m_dragBtn->setChecked(true);
        m_targetBtn->setChecked(false);
        updateSnapIndicator({});
    });
    connect(m_targetBtn, &QPushButton::clicked, [this]() {
        m_drawStatus = Target;
        m_mapView->setDragMode(QGraphicsView::NoDrag);
        m_mapView->viewport()->setCursor(Qt::CrossCursor);
        m_targetBtn->setChecked(true);
        m_dragBtn->setChecked(false);
    });

    // Separator
    auto *sep1 = new QFrame; sep1->setFrameShape(QFrame::HLine);
    sep1->setFixedHeight(1);
    sep1->setStyleSheet("background:rgba(255,255,255,0.07);border:none;margin:4px 4px;");
    toolbarLayout->addWidget(sep1);

    // 任务设置按钮 (移到左侧工具栏)
    m_taskSettingsBtn = makeToolBtn("settings", "任务设置", "打开任务设置对话框");
    m_taskSettingsBtn->setCheckable(false);
    connect(m_taskSettingsBtn, &QPushButton::clicked, this, [this]() {
        if (m_taskSettingsDialog) m_taskSettingsDialog->exec();
    });
    toolbarLayout->addWidget(m_taskSettingsBtn, 0, Qt::AlignHCenter);

    m_gridBtn = makeToolBtn("grid", "网格", "切换网格显示");
    m_gridBtn->setCheckable(true);
    connect(m_gridBtn, &QPushButton::toggled, this, [this](bool on){ m_showGrid=on; renderMap(); });
    toolbarLayout->addWidget(m_gridBtn, 0, Qt::AlignHCenter);

    // 反光柱开关
    auto *reflectorLabel = new QLabel("反光柱");
    reflectorLabel->setStyleSheet("color:#9ab8d8;font-size:10px;");
    reflectorLabel->setAlignment(Qt::AlignHCenter);
    toolbarLayout->addWidget(reflectorLabel, 0, Qt::AlignHCenter);
    m_reflectorBtn = new ToggleSwitch;
    m_reflectorBtn->setToolTip("切换反光柱显示");
    connect(m_reflectorBtn, &ToggleSwitch::toggled, this, [this](bool on) {
        m_showReflectors = on;
        renderMap();
    });
    toolbarLayout->addWidget(m_reflectorBtn, 0, Qt::AlignHCenter);

    toolbarLayout->addStretch();

    auto *sep2 = new QFrame; sep2->setFrameShape(QFrame::HLine);
    sep2->setFixedHeight(1);
    sep2->setStyleSheet("background:rgba(255,255,255,0.07);border:none;margin:4px 4px;");
    toolbarLayout->addWidget(sep2);

    m_saveBtn = makeToolBtn("save", "保存", "保存当前任务");
    m_saveBtn->setIcon(QIcon(drawTaskIcon("save", QColor("white"))));
    m_saveBtn->setStyleSheet(
        "QPushButton{background:rgba(39,174,96,165);border:none;border-radius:7px;padding:0;}"
        "QPushButton:hover{background:rgba(46,204,113,195);}");
    connect(m_saveBtn, &QPushButton::clicked, this, &TaskEditWidget::onSaveDraw);
    toolbarLayout->addWidget(m_saveBtn, 0, Qt::AlignHCenter);

    mapOuterLayout->addWidget(toolbar);

    // 地图视图 + 刻度尺
    m_scene = new QGraphicsScene(this);
    m_mapView = new MapCanvas(m_scene, this);
    connect(m_mapView, &MapCanvas::transformChanged, this, [this]() { updateRulers(); });
    m_mapView->setDragMode(QGraphicsView::ScrollHandDrag);
    m_mapView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_mapView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_mapView->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_mapView->setFrameStyle(QFrame::NoFrame);
    m_mapView->setBackgroundBrush(QBrush(QColor("#111d2b")));
    m_mapView->viewport()->installEventFilter(this);
    m_mapView->viewport()->setMouseTracking(true);

    m_coordLabel = new QLabel("X: 0.00  Y: 0.00");
    m_coordLabel->setStyleSheet(
        "background:rgba(10,16,26,195);color:#00c8a0;"
        "padding:3px 10px;border-radius:3px;font-size:11px;font-family:Consolas,monospace;");
    m_coordLabel->setParent(m_mapView);
    m_coordLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_coordLabel->adjustSize();
    m_coordLabel->raise();

    m_topRuler  = new RulerWidget(RulerWidget::Horizontal);
    m_leftRuler = new RulerWidget(RulerWidget::Vertical);

    QWidget *mapContainer = new QWidget;
    QVBoxLayout *mapContVBox = new QVBoxLayout(mapContainer);
    mapContVBox->setContentsMargins(0,0,0,0); mapContVBox->setSpacing(0);
    mapContVBox->addWidget(m_topRuler);
    QWidget *mapInner = new QWidget;
    QHBoxLayout *mapInnerHBox = new QHBoxLayout(mapInner);
    mapInnerHBox->setContentsMargins(0,0,0,0); mapInnerHBox->setSpacing(0);
    mapInnerHBox->addWidget(m_leftRuler);
    mapInnerHBox->addWidget(m_mapView, 1);
    mapContVBox->addWidget(mapInner, 1);

    m_tablePanel = nullptr;   // no longer used as map overlay

    mapOuterLayout->addWidget(mapContainer, 1);

    splitter->addWidget(mapPanel);

    // ── 右侧面板 ──────────────────────────────────────────────────────────────
    static const char *RP = "background:#16202e;";
    static const char *RC =
        "QGroupBox{font-weight:bold;font-size:11px;color:#4a7098;"
        "border:1px solid rgba(255,255,255,0.07);border-radius:7px;"
        "margin-top:12px;padding-top:4px;background:#1a2638;}"
        "QGroupBox::title{subcontrol-origin:margin;left:10px;"
        "padding:0 5px;background:#16202e;color:#5888a8;}"
        "QLabel{background:transparent;color:#90b0cc;font-size:12px;}"
        "QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox,QTimeEdit{"
        "border:1px solid rgba(255,255,255,0.10);border-radius:4px;"
        "padding:4px 7px;background:#1e2d40;color:#b0c8e0;font-size:12px;}"
        "QComboBox QAbstractItemView{background:#1e2d40;color:#b0c8e0;"
        "selection-background-color:#2a4060;}"
        // 步进器一旦自定义了 border/padding，必须显式给出 up-button/down-button 的
        // subcontrol 矩形，否则 Qt 实际命中区域和绘制位置会错位，导致靠近输入框
        // 一侧的按钮区域点击无响应。
        "QSpinBox,QDoubleSpinBox{padding-right:16px;}"
        "QSpinBox::up-button,QDoubleSpinBox::up-button{"
        "subcontrol-origin:border;subcontrol-position:top right;"
        "width:16px;border-left:1px solid rgba(255,255,255,0.10);"
        "border-bottom:1px solid rgba(255,255,255,0.10);"
        "border-top-right-radius:4px;background:#243248;}"
        "QSpinBox::up-button:hover,QDoubleSpinBox::up-button:hover{background:#2d4060;}"
        "QSpinBox::up-button:pressed,QDoubleSpinBox::up-button:pressed{background:#1e4080;}"
        "QSpinBox::down-button,QDoubleSpinBox::down-button{"
        "subcontrol-origin:border;subcontrol-position:bottom right;"
        "width:16px;border-left:1px solid rgba(255,255,255,0.10);"
        "border-top:1px solid rgba(255,255,255,0.10);"
        "border-bottom-right-radius:4px;background:#243248;}"
        "QSpinBox::down-button:hover,QDoubleSpinBox::down-button:hover{background:#2d4060;}"
        "QSpinBox::down-button:pressed,QDoubleSpinBox::down-button:pressed{background:#1e4080;}"
        "QSpinBox::up-arrow,QDoubleSpinBox::up-arrow{"
        "width:0;height:0;border-left:3px solid transparent;"
        "border-right:3px solid transparent;border-bottom:4px solid #90b0cc;}"
        "QSpinBox::up-arrow:hover,QDoubleSpinBox::up-arrow:hover{border-bottom-color:#e0e8f0;}"
        "QSpinBox::down-arrow,QDoubleSpinBox::down-arrow{"
        "width:0;height:0;border-left:3px solid transparent;"
        "border-right:3px solid transparent;border-top:4px solid #90b0cc;}"
        "QSpinBox::down-arrow:hover,QDoubleSpinBox::down-arrow:hover{border-top-color:#e0e8f0;}"
        "QCheckBox{color:#90b0cc;}"
        "QCheckBox::indicator{width:14px;height:14px;border:1px solid rgba(255,255,255,0.15);"
        "border-radius:3px;background:#1e2d40;}"
        "QCheckBox::indicator:checked{background:#2471a3;border-color:#3498db;}"
        "QPushButton{border:1px solid rgba(255,255,255,0.10);border-radius:5px;"
        "padding:5px 10px;font-size:12px;background:#1e2d40;color:#90b0cc;}"
        "QPushButton:hover{background:#263850;}"
        "QTextEdit{border:1px solid rgba(255,255,255,0.10);border-radius:4px;"
        "padding:4px 7px;background:#1e2d40;color:#b0c8e0;font-size:12px;}";
    static const char *TBL =
        "QTableWidget{background:#1a2638;border:none;gridline-color:transparent;"
        "color:#a8c0d8;font-size:12px;outline:0;}"
        "QHeaderView::section{background:#131d2a;color:#4a7098;border:none;"
        "border-bottom:1px solid rgba(255,255,255,0.07);padding:4px 6px;font-size:11px;font-weight:bold;}"
        "QTableWidget::item{padding:4px 6px;border-bottom:1px solid rgba(255,255,255,0.04);}"
        "QTableWidget::item:selected{background:rgba(41,128,185,0.38);color:white;}"
        "QTableWidget::item:hover:!selected{background:rgba(100,160,220,0.10);}"
        "QScrollBar:vertical{background:#131d2a;width:5px;border:none;}"
        "QScrollBar::handle:vertical{background:#2a4060;border-radius:2px;min-height:20px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}";

    QWidget *rightPanel = new QWidget;
    rightPanel->setStyleSheet(RP);

    QScrollArea *rsa = new QScrollArea;
    rsa->setWidgetResizable(true);
    rsa->setFrameStyle(QFrame::NoFrame);
    rsa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rsa->setStyleSheet("background:transparent;border:none;"
                       "QScrollBar:vertical{background:#131d2a;width:5px;border:none;}"
                       "QScrollBar::handle:vertical{background:#2a4060;border-radius:2px;}");
    QWidget *rc2 = new QWidget; rc2->setStyleSheet("background:#16202e;");
    QVBoxLayout *rl = new QVBoxLayout(rc2);
    rl->setContentsMargins(8,8,8,8);
    rl->setSpacing(6);
    rsa->setWidget(rc2);
    QVBoxLayout *rpo = new QVBoxLayout(rightPanel);
    rpo->setContentsMargins(0,0,0,0);
    rpo->addWidget(rsa);

    // 地图选择
    {
        auto *g = new QGroupBox("选择地图"); g->setStyleSheet(RC);
        auto *gl = new QVBoxLayout(g); gl->setSpacing(5);
        m_mapCombo = new QComboBox;
        m_mapCombo->addItem("正在加载地图...", "");
        connect(m_mapCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &TaskEditWidget::onMapComboChanged);
        gl->addWidget(m_mapCombo);
        rl->addWidget(g);
    }

    // 执行设备：已移除下拉框，默认使用当前carId (AppConfig::instance().agvId())
    // 保留 m_carCombo 仅为兼容现有保存逻辑，但不显示在UI中
    m_carCombo = new QComboBox;  // 隐藏的combo，仅用于数据存储

    // 任务设置：弹窗形式，由左侧工具栏的"任务设置"按钮触发
    {
        m_taskSettingsDialog = new QDialog(this);
        m_taskSettingsDialog->setWindowTitle("任务设置");
        m_taskSettingsDialog->setMinimumWidth(320);
        m_taskSettingsDialog->setStyleSheet(RC);
        auto *fl = new QFormLayout(m_taskSettingsDialog);
        fl->setSpacing(6); fl->setLabelAlignment(Qt::AlignRight);
        m_taskNameEdit = new QLineEdit;
        m_remarkEdit = new QTextEdit; m_remarkEdit->setMaximumHeight(52);
        m_isLoopCheck = new QCheckBox("循环任务");
        m_executionCountSpin = new QSpinBox; m_executionCountSpin->setMinimum(1); m_executionCountSpin->setValue(1);
        m_speedSpin = new QDoubleSpinBox; m_speedSpin->setMinimum(0); m_speedSpin->setSingleStep(0.1); m_speedSpin->setValue(0.5);
        m_timingCombo = new QComboBox;
        m_timingCombo->addItem("关闭","close"); m_timingCombo->addItem("每天","everyDay"); m_timingCombo->addItem("每周","everyWeek");
        m_startTimeEdit = new QTimeEdit;
        fl->addRow("任务名称:", m_taskNameEdit);
        fl->addRow("描述:", m_remarkEdit);
        fl->addRow(m_isLoopCheck);
        fl->addRow("执行次数:", m_executionCountSpin);
        fl->addRow("速度(m/s):", m_speedSpin);
        fl->addRow("定时:", m_timingCombo);
        fl->addRow("时间:", m_startTimeEdit);
        auto *closeBtn = new QPushButton("完成");
        closeBtn->setStyleSheet(
            "QPushButton{background:rgba(36,113,163,180);color:white;border:none;border-radius:5px;padding:6px 16px;}"
            "QPushButton:hover{background:rgba(52,152,219,200);}");
        connect(closeBtn, &QPushButton::clicked, m_taskSettingsDialog, &QDialog::accept);
        fl->addRow(closeBtn);
    }

    // 目标点列表
    {
        auto *g = new QGroupBox("目标点列表"); g->setStyleSheet(RC);
        auto *gl = new QVBoxLayout(g); gl->setSpacing(5);

        m_targetTable = new QTableWidget;
        m_targetTable->setColumnCount(4);
        m_targetTable->setHorizontalHeaderLabels({"序号", "编号", "X", "Y"});
        m_targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_targetTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_targetTable->setSelectionMode(QAbstractItemView::SingleSelection);
        m_targetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_targetTable->setMinimumHeight(220);
        m_targetTable->setStyleSheet(TBL);
        connect(m_targetTable, &QTableWidget::itemSelectionChanged, this, &TaskEditWidget::onActionTargetSelectionChanged);
        gl->addWidget(m_targetTable, 1);

        QHBoxLayout *tbh = new QHBoxLayout;
        auto mkBtn = [&](const QString &txt, const char *bg, const char *hov) {
            auto *b = new QPushButton(txt);
            b->setStyleSheet(QString("QPushButton{background:%1;color:white;border:none;border-radius:5px;padding:4px 10px;font-size:11px;}"
                                     "QPushButton:hover{background:%2;}").arg(bg, hov));
            return b;
        };
        auto *upBtn   = mkBtn("↑ 上移",  "rgba(41,128,185,160)",  "rgba(52,152,219,190)");
        auto *downBtn = mkBtn("↓ 下移",  "rgba(41,128,185,160)",  "rgba(52,152,219,190)");
        auto *delBtn  = mkBtn("删除",     "rgba(192,57,43,160)",   "rgba(231,76,60,190)");
        connect(upBtn,   &QPushButton::clicked, this, &TaskEditWidget::onMoveTargetUp);
        connect(downBtn, &QPushButton::clicked, this, &TaskEditWidget::onMoveTargetDown);
        connect(delBtn,  &QPushButton::clicked, this, &TaskEditWidget::onDeleteTarget);
        tbh->addWidget(upBtn); tbh->addWidget(downBtn); tbh->addStretch(); tbh->addWidget(delBtn);
        gl->addLayout(tbh);

        // 目标点属性：选中列表某行后显示，可编辑名称/朝向(坐标由路网决定，不可手动修改)
        m_targetPropsPanel = new QWidget;
        auto *tpl = new QFormLayout(m_targetPropsPanel);
        tpl->setSpacing(4); tpl->setLabelAlignment(Qt::AlignRight);
        m_targetIdEdit = new QLineEdit; m_targetIdEdit->setReadOnly(true);
        m_targetIdEdit->setStyleSheet("background:#131d2a;color:#5a7088;");
        m_targetNameEdit = new QLineEdit;
        m_targetXSpin = new QDoubleSpinBox; m_targetXSpin->setRange(-100000, 100000); m_targetXSpin->setDecimals(3);
        m_targetXSpin->setReadOnly(true);   // 任务点坐标由路网决定，不允许手动修改
        m_targetXSpin->setStyleSheet("background:#131d2a;color:#5a7088;");
        m_targetYSpin = new QDoubleSpinBox; m_targetYSpin->setRange(-100000, 100000); m_targetYSpin->setDecimals(3);
        m_targetYSpin->setReadOnly(true);   // 任务点坐标由路网决定，不允许手动修改
        m_targetYSpin->setStyleSheet("background:#131d2a;color:#5a7088;");
        m_targetYawSpin = new QDoubleSpinBox; m_targetYawSpin->setRange(-100000, 100000); m_targetYawSpin->setDecimals(3);
        // 鼠标只是经过这几个步进器(并非有意调整)时不应该被滚轮意外改值
        for (auto *spin : {m_targetXSpin, m_targetYSpin, m_targetYawSpin})
            spin->installEventFilter(new NoWheelFilter(spin));
        tpl->addRow("编号:", m_targetIdEdit);
        tpl->addRow("名称:", m_targetNameEdit);
        tpl->addRow("X:", m_targetXSpin);
        tpl->addRow("Y:", m_targetYSpin);
        tpl->addRow("设备朝向:", m_targetYawSpin);

        QHBoxLayout *targetPropsBtnRow = new QHBoxLayout;
        m_applyTargetBtn = new QPushButton("应用");
        m_applyTargetBtn->setStyleSheet(
            "QPushButton{background:rgba(36,113,163,180);color:white;border:none;border-radius:5px;padding:5px 10px;}"
            "QPushButton:hover{background:rgba(52,152,219,200);}");
        connect(m_applyTargetBtn, &QPushButton::clicked, this, &TaskEditWidget::onApplyTargetProps);
        m_cancelTargetBtn = new QPushButton("取消");
        m_cancelTargetBtn->setStyleSheet(
            "QPushButton{background:rgba(60,72,88,180);color:#d0dce8;border:1px solid rgba(255,255,255,0.10);"
            "border-radius:5px;padding:5px 14px;font-size:12px;}"
            "QPushButton:hover{background:rgba(80,95,115,200);border-color:rgba(255,255,255,0.20);}"
            "QPushButton:pressed{background:rgba(40,50,65,220);}");
        connect(m_cancelTargetBtn, &QPushButton::clicked, this, [this]() {
            m_targetTable->clearSelection();
            m_targetPropsPanel->setVisible(false);
        });
        targetPropsBtnRow->addWidget(m_applyTargetBtn);
        targetPropsBtnRow->addWidget(m_cancelTargetBtn);
        tpl->addRow(targetPropsBtnRow);
        m_targetPropsPanel->setVisible(false);
        gl->addWidget(m_targetPropsPanel);

        rl->addWidget(g);
    }

    // 动作指令
    {
        auto *g = new QGroupBox("动作指令"); g->setStyleSheet(RC);
        auto *gl = new QVBoxLayout(g); gl->setSpacing(5);

        QHBoxLayout *abh = new QHBoxLayout;
        m_actionCombo = new QComboBox;
        for (const auto &def : actionCatalog())
            m_actionCombo->addItem(def.name, def.actionType);
        m_addActionBtn = new QPushButton("添加");
        m_addActionBtn->setStyleSheet("QPushButton{background:rgba(41,128,185,160);color:white;border:none;"
                                      "border-radius:5px;padding:5px 12px;font-size:12px;}"
                                      "QPushButton:hover{background:rgba(52,152,219,190);}");
        connect(m_addActionBtn, &QPushButton::clicked, this, &TaskEditWidget::onAddAction);
        abh->addWidget(m_actionCombo,1); abh->addWidget(m_addActionBtn);
        gl->addLayout(abh);

        m_actionTable = new QTableWidget;
        m_actionTable->setColumnCount(2);
        m_actionTable->setHorizontalHeaderLabels({"动作", "参数"});
        m_actionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_actionTable->setMaximumHeight(120);
        m_actionTable->setStyleSheet(TBL);
        connect(m_actionTable, &QTableWidget::cellDoubleClicked, this, &TaskEditWidget::onEditAction);
        gl->addWidget(m_actionTable);

        QHBoxLayout *actBtnRow = new QHBoxLayout;
        m_editActionBtn = new QPushButton("修改");
        m_deleteActionBtn = new QPushButton("删除");
        m_editActionBtn->setStyleSheet("QPushButton{background:rgba(41,128,185,160);color:white;border:none;"
                                       "border-radius:5px;padding:5px 12px;font-size:12px;}"
                                       "QPushButton:hover{background:rgba(52,152,219,190);}");
        m_deleteActionBtn->setStyleSheet("QPushButton{background:rgba(192,57,43,160);color:white;border:none;"
                                         "border-radius:5px;padding:5px 12px;font-size:12px;}"
                                         "QPushButton:hover{background:rgba(231,76,60,190);}");
        connect(m_editActionBtn, &QPushButton::clicked, this, &TaskEditWidget::onEditAction);
        connect(m_deleteActionBtn, &QPushButton::clicked, this, &TaskEditWidget::onDeleteAction);
        actBtnRow->addWidget(m_editActionBtn); actBtnRow->addWidget(m_deleteActionBtn); actBtnRow->addStretch();
        gl->addLayout(actBtnRow);

        rl->addWidget(g);
    }

    rl->addStretch();
    splitter->addWidget(rightPanel);
    splitter->setSizes({700, 290});

    mainLayout->addWidget(splitter, 1);
}

void TaskEditWidget::loadMapList()
{
    ApiService::instance().listMapAll({}, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonArray rows = data.value("rows").toArray();
        m_mapCombo->clear();
        m_mapCombo->addItem("请选择地图", "");

        // Filter: only show maps that have route data (using loadEditedMap API)
        int total = rows.size();
        if (total == 0) return;
        auto *remaining = new int(total);
        for (const auto &v : rows) {
            QJsonObject m = v.toObject();
            QString id   = jsonIdStr(m.value("mapId"));
            QString name = m.value("mapName").toString();
            ApiService::instance().loadEditedMap([this, id, name, remaining](int rc, const QJsonObject &rd) {
                (*remaining)--;
                bool last = (*remaining == 0);
                bool hasRoute = false;
                if (rc == 200) {
                    QString jsonStr = rd.value("json").toString();
                    if (!jsonStr.isEmpty()) {
                        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
                        if (doc.isObject()) {
                            QJsonArray features = doc.object().value("features").toArray();
                            hasRoute = !features.isEmpty();
                        }
                    }
                }
                if (hasRoute) m_mapCombo->addItem(name, id);
                if (last) {
                    delete remaining;
                    if (!m_mapId.isEmpty()) {
                        for (int i = 0; i < m_mapCombo->count(); ++i) {
                            if (m_mapCombo->itemData(i).toString() == m_mapId) {
                                QSignalBlocker blk(m_mapCombo);
                                m_mapCombo->setCurrentIndex(i);
                                break;
                            }
                        }
                    }
                }
            });
        }
    });
}

void TaskEditWidget::onMapComboChanged(int /*index*/)
{
    QString mapId = m_mapCombo->currentData().toString();
    if (mapId.isEmpty() || mapId == m_mapId) return;
    m_mapId = mapId;
    m_mapImage = QImage();
    m_points = QJsonArray();
    m_lines  = QJsonArray();
    m_curves = QJsonArray();
    m_reflectorPoles = QJsonArray();
    m_hasInitialFit = false;
    loadMapData();
    loadRouteData();
    loadReflectorData();
}

void TaskEditWidget::loadCarList()
{
    ApiService::instance().listCarAll([this](int code, const QJsonObject &data) {
        if (code != 200) return;
        m_carList = data.value("rows").toArray();
        // 设备下拉框已移除UI显示，但保留m_carCombo用于数据兼容
        // 默认使用当前carId (AppConfig::instance().agvId())
        m_carCombo->clear();
        QString curAgvId = AppConfig::instance().agvId();
        for (const auto &item : m_carList) {
            QJsonObject car = item.toObject();
            QString carId = jsonIdStr(car.value("carId"));
            m_carCombo->addItem(car.value("carName").toString(), carId);
            if (carId == curAgvId) {
                m_carCombo->setCurrentIndex(m_carCombo->count() - 1);
            }
        }

        // 新增任务时：通过当前carId获取设备绑定的地图
        if (m_taskId.isEmpty() && !curAgvId.isEmpty()) {
            ApiService::instance().getCar(curAgvId, [this](int code, const QJsonObject &data) {
                if (code != 200) return;
                QJsonObject carData = data.value("data").toObject(data);
                QString mapId = jsonIdStr(carData.value("mapId"));
                if (mapId.isEmpty() || mapId == m_mapId) return;
                m_mapId = mapId;
                m_mapImage = QImage();
                m_points = QJsonArray();
                m_lines = QJsonArray();
                m_curves = QJsonArray();
                m_reflectorPoles = QJsonArray();
                m_hasInitialFit = false;
                loadMapData();
                loadRouteData();
                loadReflectorData();
            });
        }
    });
}

void TaskEditWidget::loadTaskData()
{
    ApiService::instance().getTask(m_taskId, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        m_taskForm = data.value("data").toObject(data);

        m_taskNameEdit->setText(m_taskForm.value("taskName").toString());
        m_remarkEdit->setText(m_taskForm.value("remark").toString());
        m_mapId = jsonIdStr(m_taskForm.value("mapId"));
        // Sync map combo selection
        for (int i = 0; i < m_mapCombo->count(); ++i) {
            if (m_mapCombo->itemData(i).toString() == m_mapId) {
                QSignalBlocker blocker(m_mapCombo);
                m_mapCombo->setCurrentIndex(i);
                break;
            }
        }

        // 设置选中的设备
        QString carId = m_taskForm.value("carId").toString();
        for (int i = 0; i < m_carCombo->count(); ++i) {
            if (m_carCombo->itemData(i).toString() == carId) {
                m_carCombo->setCurrentIndex(i);
                break;
            }
        }

        // 加载任务详情 (task_json = {"taskInfo": {type:FeatureCollection, features:[...], isLoop, executionCount, speed, timing, timingWeek, startTime, ...}})
        ApiService::instance().getTaskInfo(m_taskId, [this](int code, const QJsonObject &data) {
            if (code != 200) return;
            QJsonObject info = data.value("data").toObject(data);
            QString taskJson = info.value("task_json").toString();
            if (!taskJson.isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(taskJson.toUtf8());
                m_form = doc.object();
                QJsonObject taskInfo = m_form.value("taskInfo").toObject();
                m_executionCountSpin->setValue(taskInfo.value("executionCount").toInt(1));
                m_speedSpin->setValue(taskInfo.value("speed").toDouble(0.5));
                m_isLoopCheck->setChecked(taskInfo.value("isLoop").toBool(false));

                int timingIdx = m_timingCombo->findData(taskInfo.value("timing").toString("close"));
                if (timingIdx >= 0) m_timingCombo->setCurrentIndex(timingIdx);
                QString startTimeStr = taskInfo.value("startTime").toString();
                if (!startTimeStr.isEmpty())
                    m_startTimeEdit->setTime(QTime::fromString(startTimeStr, "HH:mm:ss"));

                // 目标点以 GeoJSON Feature 形式存储：geometry.coordinates = [x,y,(0)]，其余字段在 properties 中
                m_targets = QJsonArray();
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
                // 之前漏调用，导致编辑已有任务时目标点列表一直是空的，
                // 即便 m_targets 已经正确加载、画布上也画出了红点。
                refreshTargetTable();
            }

            if (!m_mapId.isEmpty()) {
                loadMapData();
                loadRouteData();
                loadReflectorData();
            }
        });
    });
}

void TaskEditWidget::loadMapData()
{
    ApiService::instance().getMap(m_mapId, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonObject mapInfo = data.value("data").toObject(data);
        m_mapName = mapInfo.value("mapName").toString();
        m_mapInfoLabel->setText(QString("%1 [%2]").arg(m_mapName, m_mapId));

        // 提取坐标转换参数
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
        // originX 字段（Vue 的 getMap 流程从不读取该字段）。仅用于 pgm/yaml 拉取失败时
        // 的兜底（thumbnail.png 方案）。
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
            thumbUrl = QString("%1%2_%3/%4/thumbnail.png").arg(mapHost, m_mapName, m_mapId, tilemapPath);

        // 兜底方案：thumbnail.png + getMap() 返回的 mapWidth/mapHeight/offsetX/offsetY。
        // 这些字段是后端为生成瓦片金字塔对图片 padding 后派生的，并不是地图的真实物理
        // 尺寸与零点，只在 pgm/yaml 拉取失败时使用。
        auto useThumbnailFallback = [this, thumbUrl, apiOriginX, apiOriginY, apiPhysW, apiPhysH]() {
            if (thumbUrl.isEmpty()) return;
            m_worldL = apiOriginX;
            m_worldB = apiOriginY;
            m_worldR = apiOriginX + apiPhysW;
            m_worldT = apiOriginY + apiPhysH;
            ApiService::instance().downloadFile(thumbUrl, [this](bool ok, const QByteArray &data) {
                if (!ok) return;
                m_mapImage.loadFromData(data);
                m_imgW = m_mapImage.width();
                m_imgH = m_mapImage.height();
                renderMap();
                QTimer::singleShot(0, this, [this]{ fitView(); });
            });
        };

        // 主路径：直接拉取并解析原始 carto_map.pgm + carto_map.yaml（与 MapEditWidget/
        // MapViewWidget/TaskWidget 一致）。yaml 的 origin 本身就是 pgm 左下角像素对应的
        // 世界坐标，不需要取负，也不依赖 padding 后的 mapWidth/mapHeight/offsetX/offsetY。
        QString resBase = AppConfig::instance().agvMapResourceHost() + m_mapName + "_" + m_mapId + "/";
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
                        renderMap();
                        QTimer::singleShot(0, this, [this]{ fitView(); });
                    });
            });
    });
}

void TaskEditWidget::loadRouteData()
{
    ApiService::instance().loadEditedMap([this](int code, const QJsonObject &data) {
        if (code != 200) return;
        m_points = QJsonArray();
        m_lines  = QJsonArray();
        m_curves = QJsonArray();
        m_charges = QJsonArray();
        m_highPrecisionPoints = QJsonArray();

        QString jsonStr = data.value("json").toString();
        if (jsonStr.isEmpty()) return;
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (!doc.isObject()) return;
        QJsonObject root = doc.object();

        // 兼容旧版（修复前）保存的扁平格式: {points:[...], lines:[...], curves:[...]}
        if (!root.contains("features") && (root.contains("points") || root.contains("lines") || root.contains("curves"))) {
            m_points = root.value("points").toArray();
            m_lines  = root.value("lines").toArray();
            m_curves = root.value("curves").toArray();
            if (m_imgW > 0 && m_imgH > 0) {
                renderMap();
                QTimer::singleShot(0, this, [this]{ fitView(); });
            }
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
                // 充电桩/高精度点: 保留所有 properties(epc_x/epc_y/epc_theta/yaw/yawFix/theta 等)
                // 与 Vue loadFeaturesFromJson 行为一致，避免任务保存时丢失 epc 等定位参数
                if (id.startsWith("c_") || id.startsWith("hp_")) {
                    for (auto it = props.begin(); it != props.end(); ++it) {
                        const QString &k = it.key();
                        if (k == "id" || k == "name") continue;  // 已设置
                        pt[k] = it.value();
                    }
                    if (id.startsWith("hp_"))
                        pt["visible"] = props.value("visible").toBool(true);
                }
                if (id.startsWith("c_"))
                    m_charges.append(pt);
                else if (id.startsWith("hp_")) {
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
                cv["direct"] = props.value("direct").toInt(100);
                cv["startid"] = props.value("startid").toString();
                cv["endid"]   = props.value("endid").toString();
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
                } else if (coords.size() == 4) {
                    cv["cx1"] = coords[1].toArray()[0].toDouble();
                    cv["cy1"] = coords[1].toArray()[1].toDouble();
                    cv["cx2"] = coords[2].toArray()[0].toDouble();
                    cv["cy2"] = coords[2].toArray()[1].toDouble();
                }
                m_curves.append(cv);
            }
        }
        // 如果地图图片已加载，立即重新渲染路网
        if (m_imgW > 0 && m_imgH > 0) {
            renderMap();
            QTimer::singleShot(0, this, [this]{ fitView(); });
        }
    });
}

void TaskEditWidget::loadReflectorData()
{
    if (m_mapId.isEmpty()) return;
    ApiService::instance().getReflectorColumnInfo(m_mapId, [this](int code, const QJsonObject &data) {
        if (code != 200) {
            m_mapInfoLabel->setText(QString("%1 [%2] (反光柱加载失败)").arg(m_mapName, m_mapId));
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
        m_mapInfoLabel->setText(QString("%1 [%2] (反光柱: %3)").arg(m_mapName).arg(m_mapId).arg(m_reflectorPoles.size()));
        if (m_showReflectors) renderMap();
    });
}

void TaskEditWidget::fitView()
{
    QRectF r = m_scene->sceneRect();
    if (!r.isEmpty()) {
        m_mapView->fitInView(r, Qt::KeepAspectRatio);
        m_hasInitialFit = true;
        updateRulers();
    }
}

void TaskEditWidget::updateRulers()
{
    if (m_topRuler)
        m_topRuler->syncWithView(m_mapView, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
    if (m_leftRuler)
        m_leftRuler->syncWithView(m_mapView, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
}

void TaskEditWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (!m_hasInitialFit)
        QTimer::singleShot(0, this, [this]{ fitView(); });
}

void TaskEditWidget::renderMap()
{
    m_scene->clear();
    m_targetItems.clear();
    m_selectedTargetItem = nullptr;
    m_snapIndicator = nullptr;   // scene->clear() 已删除该项，置空避免悬空指针

    // 渲染底图
    if (!m_mapImage.isNull()) {
        QGraphicsPixmapItem *bgItem = m_scene->addPixmap(QPixmap::fromImage(m_mapImage));
        bgItem->setTransformationMode(Qt::SmoothTransformation);
        bgItem->setZValue(-100);
    }

    double pxPerMeterX = (m_imgW > 0 && (m_worldR > m_worldL)) ? (m_imgW / (m_worldR - m_worldL)) : 20.0;
    double pxPerMeterY = (m_imgH > 0 && (m_worldT > m_worldB)) ? (m_imgH / (m_worldT - m_worldB)) : 20.0;

    // 渲染路网点
    for (const auto &item : m_points) {
        QJsonObject pt = item.toObject();
        QPointF sp = worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble());
        QPainterPath crossPath;
        QPainterPath path = MapPgmUtil::createPointPath(pxPerMeterX, pxPerMeterY, nullptr, nullptr, &crossPath);
        auto *e = m_scene->addPath(path, QPen(QColor(41, 128, 185, 240), 0.8), QBrush(QColor(210, 230, 250, 235)));
        e->setPos(sp); e->setZValue(10);

        QPen crossPen(QColor("#E74C3C"), 0.8, Qt::SolidLine);
        crossPen.setCapStyle(Qt::FlatCap);
        auto *crossItem = m_scene->addPath(crossPath, crossPen);
        crossItem->setParentItem(e);
        crossItem->setPos(0, 0);
        crossItem->setAcceptedMouseButtons(Qt::NoButton);
        crossItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
    }

    // 渲染充电点
    for (const auto &item : m_charges) {
        QJsonObject pt = item.toObject();
        QPointF sp = worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble());
        QPainterPath crossPath;
        QPainterPath path = MapPgmUtil::createPointPath(pxPerMeterX, pxPerMeterY, nullptr, nullptr, &crossPath);
        auto *e = m_scene->addPath(path, QPen(QColor(217, 119, 6, 240), 0.8), QBrush(QColor(254, 243, 199, 235)));
        e->setPos(sp); e->setZValue(10);

        QPen crossPen(QColor("#E74C3C"), 0.8, Qt::SolidLine);
        crossPen.setCapStyle(Qt::FlatCap);
        auto *crossItem = m_scene->addPath(crossPath, crossPen);
        crossItem->setParentItem(e);
        crossItem->setPos(0, 0);
        crossItem->setAcceptedMouseButtons(Qt::NoButton);
        crossItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
    }

    // 渲染高精度点
    for (const auto &item : m_highPrecisionPoints) {
        QJsonObject pt = item.toObject();
        QPointF sp = worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble());
        QPainterPath crossPath;
        QPainterPath path = MapPgmUtil::createPointPath(pxPerMeterX, pxPerMeterY, nullptr, nullptr, &crossPath);
        auto *e = m_scene->addPath(path, QPen(QColor(220, 38, 38, 240), 0.8), QBrush(QColor(254, 226, 226, 235)));
        e->setPos(sp); e->setZValue(10);

        QPen crossPen(QColor("#E74C3C"), 0.8, Qt::SolidLine);
        crossPen.setCapStyle(Qt::FlatCap);
        auto *crossItem = m_scene->addPath(crossPath, crossPen);
        crossItem->setParentItem(e);
        crossItem->setPos(0, 0);
        crossItem->setAcceptedMouseButtons(Qt::NoButton);
        crossItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
    }

    // 渲染线
    QPen linePen(QColor("#1296DB"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    for (const auto &item : m_lines) {
        QJsonObject ln = item.toObject();
        QPointF a = worldToScene(ln.value("x1").toDouble(), ln.value("y1").toDouble());
        QPointF b = worldToScene(ln.value("x2").toDouble(), ln.value("y2").toDouble());
        int direct = ln.value("direct").toInt(10);
        auto *li = m_scene->addLine(a.x(), a.y(), b.x(), b.y(), linePen);
        li->setZValue(5);

        QPointF dir = b - a;
        double len = std::hypot(dir.x(), dir.y());
        if (len > 1e-3) {
            dir /= len;
            QPointF mid = (a + b) / 2.0;
            QList<QPolygonF> arrows = MapPgmUtil::createDirectionArrows(mid, dir, direct, len);
            for (const auto &poly : arrows) {
                auto *arr = m_scene->addPolygon(poly, QPen(Qt::NoPen), QBrush(linePen.color()));
                arr->setParentItem(li);
                arr->setZValue(6);
                arr->setAcceptedMouseButtons(Qt::NoButton);
            }
        }
    }

    QPen curvePen(QColor("#9B59B6"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    for (const auto &item : m_curves) {
        QJsonObject cv = item.toObject();
        int direct = cv.value("direct").toInt(10);
        QJsonArray ctrlArr = cv.value("controlPoints").toArray();
        QPainterPath path;
        QList<QPointF> scenePts;
        QGraphicsPathItem *pi = nullptr;
        if (ctrlArr.size() >= 2) {
            // 多阶贝塞尔: 使用 MapPgmUtil::getBezierPath
            for (const auto &pv : ctrlArr) {
                QJsonObject pt = pv.toObject();
                scenePts.append(worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble()));
            }
            path = MapPgmUtil::getBezierPath(scenePts);
            pi = m_scene->addPath(path, curvePen);
            pi->setZValue(5);
        } else {
            // 兼容旧格式: 3点二次贝塞尔
            QPointF p1 = worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble());
            QPointF pc = worldToScene(cv.value("cx").toDouble(), cv.value("cy").toDouble());
            QPointF p2 = worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble());
            scenePts.append(p1); scenePts.append(pc); scenePts.append(p2);
            path.moveTo(p1); path.quadTo(pc, p2);
            pi = m_scene->addPath(path, curvePen);
            pi->setZValue(5);
        }

        if (scenePts.size() >= 2 && pi) {
            double cLen = std::hypot(scenePts.last().x() - scenePts.first().x(),
                                     scenePts.last().y() - scenePts.first().y());
            QList<QPolygonF> arrows = MapPgmUtil::createBezierDirectionArrows(scenePts, direct, cLen);
            for (const auto &poly : arrows) {
                auto *arr = m_scene->addPolygon(poly, QPen(Qt::NoPen), QBrush(curvePen.color()));
                arr->setParentItem(pi);
                arr->setZValue(6);
                arr->setAcceptedMouseButtons(Qt::NoButton);
            }
        }
    }

    // 渲染任务目标点 (世界坐标→场景像素坐标)
    // 距离较近的点合并为一个点，标号用逗号分隔并按顺序显示: "1,2,3"
    {
        // 先计算所有点的场景坐标
        QList<QPointF> scenePositions;
        for (int i = 0; i < m_targets.size(); ++i) {
            QJsonObject target = m_targets[i].toObject();
            QPointF sp = worldToScene(target.value("x").toDouble(), target.value("y").toDouble());
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
                // 判断 j 是否属于当前组(与组内任一点距离小于阈值)
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

        // 逐组渲染
        for (const QList<int> &indices : groups) {
            // 取组内所有点的平均位置作为显示位置
            QPointF center(0, 0);
            for (int idx : indices) center += scenePositions[idx];
            center /= indices.size();

            // 只画一个圆点
            QGraphicsEllipseItem *ellipse = m_scene->addEllipse(center.x() - 4, center.y() - 4, 8, 8,
                QPen(Qt::red, 1.5), QBrush(QColor(255, 0, 0, 128)));
            ellipse->setZValue(20);

            // 收集标号按 targetNumber 排序
            QList<QPair<int, QString>> numbered;
            for (int idx : indices) {
                QJsonObject target = m_targets[idx].toObject();
                QString id = target.value("id").toString();
                int targetNumber = target.value("targetNumber").toInt(idx + 1);
                m_targetItems[id] = ellipse;   // 多个 id 共享同一图元(点击任一都命中)
                numbered.append(qMakePair(targetNumber, id));
            }
            std::sort(numbered.begin(), numbered.end(),
                      [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
                          return a.first < b.first;
                      });

            // 拼标号: "1,2,3"
            QStringList nums;
            for (const auto &p : numbered)
                nums << QString::number(p.first);
            QString labelText = nums.join(",");

            QGraphicsTextItem *label = m_scene->addText(labelText);
            label->setPos(center.x() + 6, center.y() - 6);
            label->setZValue(21);
        }
    }

    // 网格和坐标轴
    if (m_showGrid && m_imgW > 0 && m_imgH > 0 && m_worldR > m_worldL) {
        const double gs = 1.0;
        double xMin = m_worldL, xMax = m_worldR;
        double yMin = m_worldB, yMax = m_worldT;
        QPen gp(QColor(80,110,160,80),0.5), ag(QColor(120,160,240,130),0.8);
        for (double wx=std::ceil(xMin/gs)*gs; wx<=xMax+1e-9; wx+=gs) {
            QPointF a=worldToScene(wx,yMin),b=worldToScene(wx,yMax);
            m_scene->addLine(a.x(),a.y(),b.x(),b.y(), std::abs(wx)<1e-6?ag:gp)->setZValue(-50);
        }
        for (double wy=std::ceil(yMin/gs)*gs; wy<=yMax+1e-9; wy+=gs) {
            QPointF a=worldToScene(xMin,wy),b=worldToScene(xMax,wy);
            m_scene->addLine(a.x(),a.y(),b.x(),b.y(), std::abs(wy)<1e-6?ag:gp)->setZValue(-50);
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
            auto *e = m_scene->addEllipse(-5, -5, 10, 10,
                QPen(QColor("#caa020"), 1.5), QBrush(QColor("#f0c040")));
            e->setPos(sp);
            e->setZValue(8);
        }
    }

    m_scene->setSceneRect(0, 0, m_imgW, m_imgH);
    updateRulers();
    updateTargetSelectionHighlight();
}

// 把"当前选中目标点"的变色同步到画布；renderMap() 重建场景后旧图元已被
// 销毁，需要按当前表格选中行重新找到新图元并改色。
void TaskEditWidget::updateTargetSelectionHighlight()
{
    static const QColor kSelectColor("#00e676");   // 醒目的亮绿色
    static const QPen   kNormalPen(Qt::red, 1.5);
    static const QBrush kNormalBrush(QColor(255, 0, 0, 128));

    if (m_selectedTargetItem) {
        if (auto *e = qgraphicsitem_cast<QGraphicsEllipseItem*>(m_selectedTargetItem)) {
            e->setPen(kNormalPen);
            e->setBrush(kNormalBrush);
        }
        m_selectedTargetItem = nullptr;
    }
    int row = m_targetTable->currentRow();
    if (row < 0 || row >= m_targets.size()) return;

    QString id = m_targets[row].toObject().value("id").toString();
    QGraphicsItem *item = m_targetItems.value(id);
    if (!item) return;

    if (auto *e = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
        e->setPen(QPen(kSelectColor.darker(130), 2.4));
        e->setBrush(kSelectColor);
    }
    m_selectedTargetItem = item;
}

void TaskEditWidget::addTargetPoint(const QPointF &scenePos)
{
    // 任务点必须落在路网(点/线/曲线)上: 无论点击位置离路网多远，都吸附到
    // 整个地图上最近的路网元素位置。
    SnapResult snap = findNearestRoadSnap(scenePos);
    if (!snap.valid) return;   // 路网为空，无法吸附
    QPointF world = snap.worldPos;

    // 查找所吸附的地图节点（普通节点 / 充电桩 / 高精度节点）
    QJsonObject sourcePt;
    if (snap.type == "charge") {
        for (const auto &v : m_charges) {
            if (v.toObject().value("id").toString() == snap.id) {
                sourcePt = v.toObject();
                break;
            }
        }
    } else if (snap.type == "highPrecision") {
        for (const auto &v : m_highPrecisionPoints) {
            if (v.toObject().value("id").toString() == snap.id) {
                sourcePt = v.toObject();
                break;
            }
        }
    } else if (snap.type == "point") {
        for (const auto &v : m_points) {
            if (v.toObject().value("id").toString() == snap.id) {
                sourcePt = v.toObject();
                break;
            }
        }
    }

    QJsonObject target;
    // targetNumber 从 0 开始，与 Vue addTargetExtraProps 保持一致
    int newIdx = m_targets.size();          // 0-based index
    int newNum = newIdx + 1;               // 1-based for id/name display
    QString id = QString("t_%1").arg(newNum);
    target["id"]                  = id;
    target["name"]                = QString("T%1").arg(newNum);
    target["targetNumber"]        = newIdx;  // Vue: 0-based
    target["x"]                   = QString::number(world.x(), 'f', 3).toDouble();
    target["y"]                   = QString::number(world.y(), 'f', 3).toDouble();
    target["action"]              = "None";
    target["targetFunctionality"] = QJsonArray();
    target["roll"]                = 0.0;
    target["pitch"]               = 0.0;
    target["yaw"]                 = 0.0;
    target["offset"]              = 0.0;
    target["isyawfix"]            = false;
    target["taskid"]              = 0;
    target["robotYaw"]            = 0.0;
    target["actions"]             = QJsonArray();

    // 与 Vue addTargetExtraProps 保持一致：任务目标点本身不存 epc 字段。
    // 设备端通过坐标匹配路网节点来获取 epc 定位参数，不需要目标点自带 epc 字段。
    // 仅继承 robotYaw 供用户在属性面板查看和编辑方向。
    if (!sourcePt.isEmpty()) {
        double yawVal = 0.0;
        if (sourcePt.contains("yaw")) {
            yawVal = sourcePt.value("yaw").toDouble();
        }
        // 更新 robotYaw 供 UI 展示；yaw 保持 0.0 与 Vue 一致
        target["robotYaw"] = yawVal;
    }

    m_targets.append(target);
    renderMap();
    refreshTargetTable();
}

void TaskEditWidget::refreshTargetTable()
{
    m_targetTable->setRowCount(m_targets.size());
    for (int i = 0; i < m_targets.size(); ++i) {
        QJsonObject target = m_targets[i].toObject();
        QPointF wp = worldToScene(target.value("x").toDouble(), target.value("y").toDouble());
        QPointF wc = sceneToWorld(wp.x(), wp.y());  // just use stored world coords
        m_targetTable->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        m_targetTable->setItem(i, 1, new QTableWidgetItem(target.value("id").toString()));
        m_targetTable->setItem(i, 2, new QTableWidgetItem(QString::number(target.value("x").toDouble(),'f',2)));
        m_targetTable->setItem(i, 3, new QTableWidgetItem(QString::number(target.value("y").toDouble(),'f',2)));
    }
}

static QJsonValue numOrStr(const QString &s) {
    bool ok; qlonglong n = s.toLongLong(&ok);
    return ok ? QJsonValue((double)n) : QJsonValue(s);
}

void TaskEditWidget::onSaveDraw()
{
    // 基本验证
    if (m_mapId.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择地图");
        return;
    }
    if (m_taskNameEdit->text().trimmed().isEmpty()) {
        QInputDialog dlg(this);
        dlg.setWindowTitle("请输入任务名称");
        dlg.setLabelText("任务名称:");
        dlg.setTextValue("");
        dlg.setOkButtonText("完成");
        dlg.setCancelButtonText("取消");
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        QString name = dlg.textValue().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, "提示", "任务名称不能为空");
            return;
        }
        m_taskNameEdit->setText(name);
    }

    // 构建目标点 GeoJSON (对应 Vue 的 exportTargetFeaturesObj())
    QJsonArray features;
    for (const auto &tv : m_targets) {
        QJsonObject t = tv.toObject();
        QJsonArray coords;
        coords.append(t.value("x").toDouble());
        coords.append(t.value("y").toDouble());
        coords.append(0.0);
        QJsonObject geom; geom["type"] = "Point"; geom["coordinates"] = coords;
        QJsonObject props = t;
        props.remove("x"); props.remove("y");
        QJsonObject f; f["type"] = "Feature"; f["geometry"] = geom; f["properties"] = props;
        features.append(f);
    }

    // taskInfo 是一个对象 (GeoJSON FeatureCollection + 任务元数据)，而不是字符串
    QJsonObject taskInfo;
    taskInfo["type"]              = "FeatureCollection";
    taskInfo["features"]          = features;
    taskInfo["taskFunctionality"] = QJsonArray();
    taskInfo["isLoop"]            = m_isLoopCheck->isChecked();
    taskInfo["executionCount"]    = m_executionCountSpin->value();
    taskInfo["speed"]             = m_speedSpin->value();
    taskInfo["timing"]            = m_timingCombo->currentData().toString();
    taskInfo["timingWeek"]        = QJsonArray();
    taskInfo["startTime"]         = m_startTimeEdit->time().toString("HH:mm:ss");

    QJsonObject taskData;
    taskData["mapId"]    = numOrStr(m_mapId);
    taskData["carId"]    = numOrStr(m_carCombo->currentData().toString());
    taskData["taskName"] = m_taskNameEdit->text().trimmed();
    taskData["remark"]   = m_remarkEdit->toPlainText();
    taskData["taskInfo"] = taskInfo;

    m_saveBtn->setEnabled(false);
    auto *loading = new LoadingDialog("正在保存任务...", this);
    loading->show();

    if (m_taskId.isEmpty()) {
        ApiService::instance().addTask(taskData, [this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            m_saveBtn->setEnabled(true);
            if (code == 200) {
                Toast::success(this, "保存成功");
                emit goBack();
            } else {
                QMessageBox::warning(this, "错误", "保存失败");
            }
        });
    } else {
        taskData["taskId"]     = numOrStr(m_taskId);
        taskData["taskNumber"] = m_taskForm.value("taskNumber");
        taskData["taskPath"]   = m_taskForm.value("taskPath");
        ApiService::instance().updateTask(taskData, [this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            m_saveBtn->setEnabled(true);
            if (code == 200) {
                Toast::success(this, "保存成功");
                // 保持最新编辑状态渲染，并返回任务列表
                refreshTargetTable();
                renderMap();
                emit goBack();
            } else {
                QMessageBox::warning(this, "错误", "保存失败");
            }
        });
    }
}

void TaskEditWidget::onCarChanged(int index)
{
    QString carId = m_carCombo->currentData().toString();
    if (carId.isEmpty()) return;

    // 编辑模式：设备只影响执行者，不改变任务已绑定的地图
    if (!m_taskId.isEmpty()) return;

    // 新增模式：通过 getCar 获取设备绑定的地图
    ApiService::instance().getCar(carId, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonObject carData = data.value("data").toObject(data);
        QString mapId = jsonIdStr(carData.value("mapId"));
        if (mapId.isEmpty() || mapId == m_mapId) return;
        m_mapId = mapId;
        m_mapImage = QImage();
        m_points = QJsonArray();
        m_lines = QJsonArray();
        m_curves = QJsonArray();
        m_hasInitialFit = false;
        loadMapData();
        loadRouteData();
    });
}

void TaskEditWidget::onTableToggle()
{
    // 要素列表按钮已移除，目标点列表始终显示在右侧面板
    m_tableVisible = true;
    refreshTargetTable();
}

void TaskEditWidget::onDeleteTarget()
{
    int row = m_targetTable->currentRow();
    if (row < 0) return;
    m_targets.removeAt(row);
    // 重新编号: targetNumber 从 0 开始 (与 Vue 一致)
    for (int i = 0; i < m_targets.size(); ++i) {
        QJsonObject target = m_targets[i].toObject();
        target["targetNumber"] = i;   // 0-based
        m_targets[i] = target;
    }
    renderMap();
    refreshTargetTable();
}

void TaskEditWidget::onMoveTargetUp()
{
    int row = m_targetTable->currentRow();
    if (row <= 0) return;
    QJsonValue tmp = m_targets[row];
    m_targets[row] = m_targets[row - 1];
    m_targets[row - 1] = tmp;
    // 重新编号: targetNumber 从 0 开始 (与 Vue 一致)
    for (int i = 0; i < m_targets.size(); ++i) {
        QJsonObject target = m_targets[i].toObject();
        target["targetNumber"] = i;   // 0-based
        m_targets[i] = target;
    }
    renderMap();
    refreshTargetTable();
    m_targetTable->selectRow(row - 1);
}

void TaskEditWidget::onMoveTargetDown()
{
    int row = m_targetTable->currentRow();
    if (row < 0 || row >= m_targets.size() - 1) return;
    QJsonValue tmp = m_targets[row];
    m_targets[row] = m_targets[row + 1];
    m_targets[row + 1] = tmp;
    // 重新编号: targetNumber 从 0 开始 (与 Vue 一致)
    for (int i = 0; i < m_targets.size(); ++i) {
        QJsonObject target = m_targets[i].toObject();
        target["targetNumber"] = i;   // 0-based
        m_targets[i] = target;
    }
    renderMap();
    refreshTargetTable();
    m_targetTable->selectRow(row + 1);
}

// 与 Vue task/edit.vue 一致的动作数据格式：{name, action_type, <参数铺平合并>}
void TaskEditWidget::onAddAction()
{
    int row = m_targetTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "提示", "请先选择一个目标点");
        return;
    }

    int actionType = m_actionCombo->currentData().toInt();
    const ActionDef *def = findActionDef(actionType);
    if (!def) return;

    QJsonObject action = editActionParamsDialog(this, *def, QJsonObject(),
                                                 QString("添加指令：%1").arg(def->name));
    // 无参数动作直接返回 {name,action_type}（非空），用户取消才是空对象
    if (action.isEmpty() && !def->fields.isEmpty()) return;  // 取消

    QJsonObject target = m_targets[row].toObject();
    QJsonArray actions = target.value("actions").toArray();
    actions.append(action);
    target["actions"] = actions;
    m_targets[row] = target;

    refreshActionTable();
}

void TaskEditWidget::onDeleteAction()
{
    int targetRow = m_targetTable->currentRow();
    int actionRow = m_actionTable->currentRow();
    if (targetRow < 0 || actionRow < 0) return;

    QJsonObject target = m_targets[targetRow].toObject();
    QJsonArray actions = target.value("actions").toArray();
    if (actionRow >= actions.size()) return;
    actions.removeAt(actionRow);
    target["actions"] = actions;
    m_targets[targetRow] = target;

    refreshActionTable();
}

void TaskEditWidget::onEditAction()
{
    int targetRow = m_targetTable->currentRow();
    int actionRow = m_actionTable->currentRow();
    if (targetRow < 0 || actionRow < 0) return;

    QJsonObject target = m_targets[targetRow].toObject();
    QJsonArray actions = target.value("actions").toArray();
    if (actionRow >= actions.size()) return;

    QJsonObject existing = actions[actionRow].toObject();
    const ActionDef *def = findActionDef(existing.value("action_type").toInt(-1));
    if (!def) return;   // 旧格式/未知动作类型，不提供编辑（避免写出错误数据）
    if (def->fields.isEmpty()) return;  // 无参数动作没有可改的内容

    QJsonObject action = editActionParamsDialog(this, *def, existing,
                                                 QString("修改指令：%1").arg(def->name));
    if (action.isEmpty()) return;  // 取消

    actions[actionRow] = action;
    target["actions"] = actions;
    m_targets[targetRow] = target;

    refreshActionTable();
}

// 显示"当前选中目标点"的动作列表；目标点切换/增删改后都要调用，
// 否则动作表会停留在之前选中目标点的旧数据上。
void TaskEditWidget::refreshActionTable()
{
    int row = m_targetTable->currentRow();
    QJsonArray actions;
    if (row >= 0 && row < m_targets.size())
        actions = m_targets[row].toObject().value("actions").toArray();

    m_actionTable->setRowCount(actions.size());
    for (int i = 0; i < actions.size(); ++i) {
        QJsonObject act = actions[i].toObject();
        m_actionTable->setItem(i, 0, new QTableWidgetItem(act.value("name").toString()));
        m_actionTable->setItem(i, 1, new QTableWidgetItem(formatActionParams(act)));
    }
}

void TaskEditWidget::onActionTargetSelectionChanged()
{
    refreshActionTable();
    refreshTargetPropsPanel();
    updateTargetSelectionHighlight();
}

// 显示/隐藏并填充选中目标点的可编辑属性
void TaskEditWidget::refreshTargetPropsPanel()
{
    int row = m_targetTable->currentRow();
    if (row < 0 || row >= m_targets.size()) {
        m_targetPropsPanel->setVisible(false);
        return;
    }
    QJsonObject target = m_targets[row].toObject();
    m_targetIdEdit->setText(target.value("id").toString());
    m_targetNameEdit->setText(target.value("name").toString());
    m_targetXSpin->setValue(target.value("x").toDouble());
    m_targetYSpin->setValue(target.value("y").toDouble());
    m_targetYawSpin->setValue(target.value("robotYaw").toDouble());
    m_targetPropsPanel->setVisible(true);
}

void TaskEditWidget::onApplyTargetProps()
{
    int row = m_targetTable->currentRow();
    if (row < 0 || row >= m_targets.size()) return;

    QJsonObject target = m_targets[row].toObject();
    target["name"]     = m_targetNameEdit->text();
    target["x"]        = m_targetXSpin->value();
    target["y"]        = m_targetYSpin->value();
    target["robotYaw"] = m_targetYawSpin->value();
    target["yaw"]      = m_targetYawSpin->value();
    m_targets[row] = target;

    renderMap();
    refreshTargetTable();
    m_targetTable->selectRow(row);
}

// 在 Drag(拖拽/选择)模式下，命中测试场景中的目标点图元，用于"点击画布选中目标点"
QGraphicsEllipseItem *TaskEditWidget::targetItemAt(const QPointF &scenePos) const
{
    const double tol = 8.0;
    for (auto *item : m_scene->items(QRectF(scenePos.x()-tol, scenePos.y()-tol, tol*2, tol*2))) {
        auto *ellipse = qgraphicsitem_cast<QGraphicsEllipseItem*>(item);
        if (ellipse && m_targetItems.values().contains(ellipse)) return ellipse;
    }
    return nullptr;
}

void TaskEditWidget::updateCoordinateDisplay(const QPointF &pos)
{
    QPointF world = sceneToWorld(pos.x(), pos.y());
    m_coordLabel->setText(QString("X: %1  Y: %2").arg(world.x(), 0, 'f', 2).arg(world.y(), 0, 'f', 2));
}

QPointF TaskEditWidget::worldToScene(double wx, double wy) const
{
    double mapW = m_worldR - m_worldL, mapH = m_worldT - m_worldB;
    if (m_imgW <= 0 || m_imgH <= 0 || mapW <= 0 || mapH <= 0) return {wx, wy};
    return { (wx - m_worldL) / mapW * m_imgW,
             (m_worldT - wy) / mapH * m_imgH };
}

QPointF TaskEditWidget::sceneToWorld(double sx, double sy) const
{
    double mapW = m_worldR - m_worldL, mapH = m_worldT - m_worldB;
    if (m_imgW <= 0 || m_imgH <= 0 || mapW <= 0 || mapH <= 0) return {sx, sy};
    return { sx / m_imgW * mapW + m_worldL,
             m_worldT - sy / m_imgH * mapH };
}

// 点到线段最近点
static QPointF nearestOnSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    QPointF ab = b - a;
    double len2 = QPointF::dotProduct(ab, ab);
    if (len2 < 1e-12) return a;
    double t = QPointF::dotProduct(p - a, ab) / len2;
    t = std::clamp(t, 0.0, 1.0);
    return a + ab * t;
}

TaskEditWidget::SnapResult TaskEditWidget::findNearestRoadSnap(const QPointF &scenePos) const
{
    SnapResult best;
    double bestDist = std::numeric_limits<double>::max();

    auto consider = [&](const QPointF &candidateScene, const QString &type, const QString &id, const QPointF &world) {
        double d = std::hypot(scenePos.x() - candidateScene.x(),
                              scenePos.y() - candidateScene.y());
        if (d < bestDist) {
            bestDist   = d;
            best.valid = true;
            best.type  = type;
            best.id    = id;
            best.scenePos = candidateScene;
            best.worldPos = world;   // 直接使用源点的原始 world 坐标，避免 round-trip 浮点误差
        }
    };

    // 1) 路网点 / 充电桩 / 高精度点
    auto checkPoints = [&](const QJsonArray &arr, const QString &type) {
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            double wx = o.value("x").toDouble();
            double wy = o.value("y").toDouble();
            QPointF sp = worldToScene(wx, wy);
            consider(sp, type, o.value("id").toString(), QPointF(wx, wy));
        }
    };
    checkPoints(m_points, "point");
    checkPoints(m_charges, "charge");
    checkPoints(m_highPrecisionPoints, "highPrecision");

    // 注: 任务点只能吸附在路网点上，不再吸附路网线段和曲线
    // (原逻辑保留注释，便于后续恢复)
    // // 2) 路网线段: 找点到线段的最近点
    // for (const auto &v : m_lines) { ... }
    // // 3) 路网曲线: 在贝塞尔曲线上采样找最近点
    // for (const auto &v : m_curves) { ... }

    return best;
}

void TaskEditWidget::updateSnapIndicator(const SnapResult &r)
{
    if (m_snapIndicator) {
        m_scene->removeItem(m_snapIndicator);
        delete m_snapIndicator;
        m_snapIndicator = nullptr;
    }
    if (!r.valid) return;
    const double rad = 10.0;
    m_snapIndicator = m_scene->addEllipse(-rad, -rad, rad * 2, rad * 2,
                                          QPen(QColor("#00e676"), 2.0),
                                          QBrush(QColor(0, 230, 118, 28)));
    m_snapIndicator->setPos(r.scenePos);
    m_snapIndicator->setZValue(200);
}

bool TaskEditWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != m_mapView->viewport())
        return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::Wheel) {
        return false;
    }

    if (event->type() == QEvent::MouseMove) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        QPointF sp = m_mapView->mapToScene(me->pos());
        updateCoordinateDisplay(sp);
        // 绘制任务点模式下，实时显示最近路网吸附位置
        if (m_drawStatus == Target) {
            SnapResult snap = findNearestRoadSnap(sp);
            updateSnapIndicator(snap);
        }
        return false;
    }

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::RightButton) {
            // Right-click cancels draw mode
            m_drawStatus = Drag;
            m_mapView->setDragMode(QGraphicsView::ScrollHandDrag);
            m_mapView->viewport()->setCursor(Qt::OpenHandCursor);
            if (m_dragBtn)   m_dragBtn->setChecked(true);
            if (m_targetBtn) m_targetBtn->setChecked(false);
            updateSnapIndicator({});
            return true;
        }
        if (me->button() == Qt::LeftButton && m_drawStatus == Target) {
            addTargetPoint(m_mapView->mapToScene(me->pos()));
            return true;
        }
        // 拖拽/选择模式下点击目标点：选中对应列表行，而不是开始平移地图
        if (me->button() == Qt::LeftButton && m_drawStatus == Drag) {
            QPointF scenePos = m_mapView->mapToScene(me->pos());
            // 收集点击位置附近所有的目标点(支持重叠点循环切换)
            const double tol = 8.0;
            QList<QGraphicsEllipseItem*> hits;
            for (auto *item : m_scene->items(QRectF(scenePos.x()-tol, scenePos.y()-tol, tol*2, tol*2))) {
                auto *ellipse = qgraphicsitem_cast<QGraphicsEllipseItem*>(item);
                if (ellipse && m_targetItems.values().contains(ellipse))
                    hits.append(ellipse);
            }
            if (!hits.isEmpty()) {
                // 循环切换: 找到当前选中点在命中列表中的位置，选下一个
                int curRow = m_targetTable->currentRow();
                QString curId;
                if (curRow >= 0 && curRow < m_targets.size())
                    curId = m_targets[curRow].toObject().value("id").toString();

                int nextIdx = 0;
                if (!curId.isEmpty()) {
                    QString curItemId = m_targetItems.key(m_selectedTargetItem);
                    for (int i = 0; i < hits.size(); ++i) {
                        if (hits[i] == m_selectedTargetItem) {
                            nextIdx = (i + 1) % hits.size();
                            break;
                        }
                    }
                }
                QString id = m_targetItems.key(hits[nextIdx]);
                for (int i = 0; i < m_targets.size(); ++i) {
                    if (m_targets[i].toObject().value("id").toString() == id) {
                        m_targetTable->selectRow(i);
                        return true;
                    }
                }
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}
