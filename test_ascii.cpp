#include "MapViewWidget.h"
#include "core/AppConfig.h"
#include "core/ApiService.h"
#include "core/MqttService.h"
#include "core/LoadingDialog.h"
#include "core/Toast.h"
#include "core/MapPgmUtil.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QLabel>
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QGraphicsPixmapItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsTextItem>
#include <QGraphicsRectItem>
#include <QJsonDocument>
#include <QDesktopServices>
#include <QApplication>
#include <QGridLayout>
#include <QSvgRenderer>
#include <QtMath>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QLineEdit>
#include <QFrame>
#include <QDateTime>
#include <QToolButton>
#include <functional>

// ?? Toolbar icon helpers ?????????????????????????????????????????????????????
namespace {

QPixmap drawMapToolIcon(const QString &name, const QColor &col, int sz = 20)
{
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    double c = sz * 0.5;

    if (name == "add") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(c, sz*0.12), QPointF(c, sz*0.88));
        p.drawLine(QPointF(sz*0.12, c), QPointF(sz*0.88, c));
    } else if (name == "swap") {
        p.setPen(QPen(col, 1.7, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(sz*0.18, sz*0.36), QPointF(sz*0.82, sz*0.36));
        p.drawLine(QPointF(sz*0.18, sz*0.64), QPointF(sz*0.82, sz*0.64));
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF a1; a1 << QPointF(sz*0.82,sz*0.36) << QPointF(sz*0.66,sz*0.26) << QPointF(sz*0.66,sz*0.46);
        QPolygonF a2; a2 << QPointF(sz*0.18,sz*0.64) << QPointF(sz*0.34,sz*0.54) << QPointF(sz*0.34,sz*0.74);
        p.drawPolygon(a1); p.drawPolygon(a2);
    } else if (name == "target") {
        p.setPen(QPen(col, 1.6)); p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(c,c), sz*0.36, sz*0.36);
        p.drawEllipse(QPointF(c,c), sz*0.16, sz*0.16);
        p.setPen(QPen(col, 1.4));
        p.drawLine(QPointF(c, sz*0.02), QPointF(c, sz*0.16));
        p.drawLine(QPointF(c, sz*0.84), QPointF(c, sz*0.98));
        p.drawLine(QPointF(sz*0.02, c), QPointF(sz*0.16, c));
        p.drawLine(QPointF(sz*0.84, c), QPointF(sz*0.98, c));
    } else if (name == "route") {
        p.setPen(QPen(col, 1.7, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(sz*0.14, sz*0.82);
        path.cubicTo(sz*0.14, sz*0.40, sz*0.86, sz*0.60, sz*0.86, sz*0.18);
        p.drawPath(path);
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(QPointF(sz*0.14, sz*0.82), sz*0.09, sz*0.09);
        p.drawEllipse(QPointF(sz*0.86, sz*0.18), sz*0.09, sz*0.09);
    } else if (name == "edit") {
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(sz*0.20, sz*0.80), QPointF(sz*0.62, sz*0.38));
        p.setBrush(col);
        QPolygonF tip; tip << QPointF(sz*0.16,sz*0.88) << QPointF(sz*0.20,sz*0.68) << QPointF(sz*0.32,sz*0.80);
        p.drawPolygon(tip);
        p.drawRect(QRectF(sz*0.62, sz*0.20, sz*0.18, sz*0.18));
    } else if (name == "tag") {
        p.setPen(QPen(col, 1.5)); p.setBrush(Qt::NoBrush);
        QPolygonF tag;
        tag << QPointF(sz*0.14,sz*0.20) << QPointF(sz*0.58,sz*0.20)
            << QPointF(sz*0.88,sz*0.50) << QPointF(sz*0.58,sz*0.80) << QPointF(sz*0.14,sz*0.80);
        p.drawPolygon(tag);
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(QPointF(sz*0.30, sz*0.50), sz*0.06, sz*0.06);
    } else if (name == "download") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(c, sz*0.10), QPointF(c, sz*0.62));
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.32,sz*0.50) << QPointF(sz*0.68,sz*0.50) << QPointF(c,sz*0.78);
        p.drawPolygon(arrow);
        p.setPen(QPen(col, 1.6));
        p.drawLine(QPointF(sz*0.16, sz*0.88), QPointF(sz*0.84, sz*0.88));
    } else if (name == "file-download") {
        p.setPen(QPen(col, 1.4)); p.setBrush(Qt::NoBrush);
        QPolygonF file;
        file << QPointF(sz*0.22,sz*0.08) << QPointF(sz*0.62,sz*0.08) << QPointF(sz*0.78,sz*0.24)
             << QPointF(sz*0.78,sz*0.62) << QPointF(sz*0.22,sz*0.62);
        p.drawPolygon(file);
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.36,sz*0.74) << QPointF(sz*0.64,sz*0.74) << QPointF(c,sz*0.96);
        p.drawPolygon(arrow);
    } else if (name == "upload") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(c, sz*0.34), QPointF(c, sz*0.88));
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.32,sz*0.44) << QPointF(sz*0.68,sz*0.44) << QPointF(c,sz*0.16);
        p.drawPolygon(arrow);
        p.setPen(QPen(col, 1.6));
        p.drawLine(QPointF(sz*0.16, sz*0.10), QPointF(sz*0.84, sz*0.10));
    } else if (name == "trash") {
        p.setPen(QPen(col, 1.5)); p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(sz*0.24, sz*0.32, sz*0.52, sz*0.56));
        p.drawLine(QPointF(sz*0.14, sz*0.30), QPointF(sz*0.86, sz*0.30));
        p.drawLine(QPointF(sz*0.38, sz*0.30), QPointF(sz*0.42, sz*0.16));
        p.drawLine(QPointF(sz*0.42, sz*0.16), QPointF(sz*0.58, sz*0.16));
        p.drawLine(QPointF(sz*0.58, sz*0.16), QPointF(sz*0.62, sz*0.30));
        p.setPen(QPen(col, 1.3));
        p.drawLine(QPointF(sz*0.38, sz*0.42), QPointF(sz*0.38, sz*0.76));
        p.drawLine(QPointF(c, sz*0.42), QPointF(c, sz*0.76));
        p.drawLine(QPointF(sz*0.62, sz*0.42), QPointF(sz*0.62, sz*0.76));
    }

    p.end();
    return pm;
}

QIcon makeMapToolIcon(const QString &name)
{
    QIcon icon;
    icon.addPixmap(drawMapToolIcon(name, QColor("#9ab0c8")), QIcon::Normal, QIcon::Off);
    icon.addPixmap(drawMapToolIcon(name, QColor("white")),   QIcon::Active, QIcon::Off);
    return icon;
}

} // namespace

// ???????????????????????????????????????????????????????????????????????????
//  RulerWidget
// ???????????????????????????????????????????????????????????????????????????

RulerWidget::RulerWidget(Orientation ori, QWidget *parent)
    : QWidget(parent), m_ori(ori)
{
    if (ori == Horizontal) setFixedHeight(22);
    else                   setFixedWidth(44);
    setStyleSheet("background:#22262e;");
}

void RulerWidget::syncWithView(QGraphicsView *view,
                               double worldL, double worldB, double worldR, double worldT,
                               double imgW,   double imgH)
{
    m_view  = view;
    m_worldL = worldL; m_worldB = worldB;
    m_worldR = worldR; m_worldT = worldT;
    m_imgW   = imgW;   m_imgH   = imgH;
    update();
}

void RulerWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x1a, 0x1d, 0x23));
    p.setPen(QColor(0x55, 0x66, 0x77));
    p.setFont(QFont("Consolas", 7));

    double mapW = m_worldR - m_worldL;
    double mapH = m_worldT - m_worldB;
    if (!m_view || mapW <= 0 || mapH <= 0 || m_imgW <= 0 || m_imgH <= 0) {
        p.setPen(QColor(0x44, 0x55, 0x66));
        p.drawText(rect(), Qt::AlignCenter, m_ori == Horizontal ? "X (m)" : "Y (m)");
        return;
    }

    // Per-axis metres-per-scene-pixel (handles non-square maps correctly)
    double resX = mapW / m_imgW;
    double resY = mapH / m_imgH;

    // Scale from view transform
    double scale = m_view->transform().m11();

    // Tick steps in world metres
    auto pickStep = [](double pxPerM) -> double {
        double rawStep = 80.0 / pxPerM;
        static const double steps[] = {0.01,0.02,0.05,0.1,0.2,0.5,1,2,5,10,20,50,100,200,500};
        double step = steps[0];
        for (double s : steps) { step = s; if (s >= rawStep) break; }
        return step;
    };

    auto fmtLabel = [](double v, double st) -> QString {
        if (st < 0.1)  return QString::number(v, 'f', 2);
        if (st < 1.0)  return QString::number(v, 'f', 1);
        return QString::number((int)std::round(v));
    };

    if (m_ori == Horizontal) {
        double pxPerM = scale / resX;
        double step   = pickStep(pxPerM);
        QRectF vis    = m_view->mapToScene(m_view->viewport()->rect()).boundingRect();
        double wLeft  = vis.left()  * resX + m_worldL;
        double wRight = vis.right() * resX + m_worldL;
        double first  = std::ceil(wLeft / step) * step;

        int H = height();
        for (double wx = first; wx <= wRight + 1e-9; wx += step) {
            double sceneX = (wx - m_worldL) / resX;
            QPointF vp    = m_view->mapFromScene(sceneX, 0);
            int sx        = (int)vp.x();
            bool major    = qFuzzyIsNull(std::fmod(wx / step + 0.5, 5.0) - 0.5);
            p.setPen(major ? QColor(0x88,0x99,0xaa) : QColor(0x44,0x55,0x66));
            p.drawLine(sx, major ? H-10 : H-5, sx, H);
            if (major) {
                p.setPen(QColor(0x99,0xaa,0xbb));
                p.drawText(sx-22, 0, 44, H-11, Qt::AlignCenter, fmtLabel(wx, step));
            }
        }
        p.setPen(QColor(0x44,0x55,0x66));
        p.drawLine(0, H-1, width(), H-1);
    } else {
        double pxPerM = scale / resY;
        double step   = pickStep(pxPerM);
        QRectF vis    = m_view->mapToScene(m_view->viewport()->rect()).boundingRect();
        // scene Y increases downward; world Y increases upward
        double wTop    = m_worldT - vis.top()    * resY;
        double wBottom = m_worldT - vis.bottom() * resY;
        double first   = std::ceil(wBottom / step) * step;

        int W = width();
        for (double wy = first; wy <= wTop + 1e-9; wy += step) {
            double sceneY = (m_worldT - wy) / resY;
            QPointF vp    = m_view->mapFromScene(0, sceneY);
            int sy        = (int)vp.y();
            bool major    = qFuzzyIsNull(std::fmod(wy / step + 0.5, 5.0) - 0.5);
            p.setPen(major ? QColor(0x88,0x99,0xaa) : QColor(0x44,0x55,0x66));
            p.drawLine(W, sy, major ? 5 : W-5, sy);
            if (major) {
                p.setPen(QColor(0x99,0xaa,0xbb));
                p.save();
                p.translate(W-14, sy);
                p.rotate(-90);
                p.drawText(-22, -6, 44, 12, Qt::AlignCenter, fmtLabel(wy, step));
                p.restore();
            }
        }
        p.setPen(QColor(0x44,0x55,0x66));
        p.drawLine(W-1, 0, W-1, height());
    }
}

// ???????????????????????????????????????????????????????????????????????????
//  MapCanvas
// ???????????????????????????????????????????????????????????????????????????

MapCanvas::MapCanvas(QGraphicsScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
{
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setDragMode(QGraphicsView::NoDrag);
    setBackgroundBrush(QColor(0x12, 0x15, 0x1a));
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setCursor(Qt::CrossCursor);
}

void MapCanvas::wheelEvent(QWheelEvent *e)
{
    double factor = e->angleDelta().y() > 0 ? 1.15 : 1.0/1.15;
    scale(factor, factor);
    emit transformChanged();
}

void MapCanvas::setPanWithLeftButton(bool enabled)
{
    m_panWithLeftButton = enabled;
}

void MapCanvas::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton ||
        (e->button() == Qt::LeftButton &&
         (m_panWithLeftButton || (e->modifiers() & Qt::AltModifier)))) {
        m_panning = true;
        m_lastMousePos = e->pos();
        setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    QPointF sp = mapToScene(e->pos());
    emit sceneClicked(sp, e->button());
    QGraphicsView::mousePressEvent(e);
}

void MapCanvas::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_panning)
        emit sceneMoved(mapToScene(e->pos()));
    if (m_panning) {
        QPoint delta = e->pos() - m_lastMousePos;
        m_lastMousePos = e->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        emit transformChanged();
        e->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(e);
    emit transformChanged();
}

void MapCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_panning && (e->button() == Qt::MiddleButton || e->button() == Qt::LeftButton)) {
        m_panning = false;
        setCursor(Qt::CrossCursor);
        e->accept();
        return;
    }
    emit sceneReleased(mapToScene(e->pos()), e->button());
    QGraphicsView::mouseReleaseEvent(e);
}

void MapCanvas::resizeEvent(QResizeEvent *e)
{
    QGraphicsView::resizeEvent(e);
    emit canvasResized(size());
}

// ???????????????????????????????????????????????????????????????????????????
//  MapViewWidget
// ???????????????????????????????????????????????????????????????????????????

static const char *kBtnStyle =
    "QPushButton{"
    "  background:#252a35; color:#c8d0d8; border:1px solid #3a3f4a;"
    "  border-radius:4px; padding:5px 10px; font-size:12px; font-weight:600;"
    "}"
    "QPushButton:hover{background:#2d3340; border-color:#4a9eff;}"
    "QPushButton:pressed{background:#1e4080;}"
    "QPushButton:disabled{color:#445566; border-color:#2d3340;}";

static const char *kBtnPrimary =
    "QPushButton{"
    "  background:#1e6fd4; color:#fff; border:none;"
    "  border-radius:4px; padding:5px 14px; font-size:12px; font-weight:600;"
    "}"
    "QPushButton:hover{background:#2980e8;}"
    "QPushButton:pressed{background:#155db8;}";

// ?????????????????? (Qt::ToolButtonTextUnderIcon)
static const char *kToolBtnStyle =
    "QToolButton{"
    "  background:transparent; color:#9ab0c8; border:1px solid transparent;"
    "  border-radius:5px; padding:3px 8px; font-size:11px; font-weight:600;"
    "}"
    "QToolButton:hover{background:#2d3340; border-color:#3a3f4a; color:#e0e8f0;}"
    "QToolButton:pressed{background:#1e4080;}"
    "QToolButton:disabled{color:#445566;}";

static const char *kToolBtnPrimary =
    "QToolButton{"
    "  background:#1e6fd4; color:#fff; border:none;"
    "  border-radius:5px; padding:3px 8px; font-size:11px; font-weight:600;"
    "}"
    "QToolButton:hover{background:#2980e8;}"
    "QToolButton:pressed{background:#155db8;}";

MapViewWidget::MapViewWidget(QWidget *parent)
    : QWidget(parent)
{
    // Load SVG robot icon at 256?256 for crisp scaling at any size
    QSvgRenderer renderer(QString(":/ic-robot1.svg"));
    if (renderer.isValid()) {
        m_robotPixmap = QPixmap(256, 256);
        m_robotPixmap.fill(Qt::transparent);
        QPainter rp(&m_robotPixmap);
        rp.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        renderer.render(&rp);
    }
    setupUI();
}

MapViewWidget::~MapViewWidget() {}

void MapViewWidget::setupUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ?? Toolbar ????????????????????????????????????????????????????????????
    auto *toolbarWidget = new QWidget;
    toolbarWidget->setFixedHeight(60);
    toolbarWidget->setStyleSheet("background:#12151a; border-bottom:1px solid #2d3340;");
    setupToolbar(toolbarWidget);
    root->addWidget(toolbarWidget);

    // ?? Status bar ?????????????????????????????????????????????????????????
    m_statusLabel = new QLabel("????");
    m_statusLabel->setFixedHeight(24);
    m_statusLabel->setStyleSheet(
        "background:#0d1117; color:#8898a8; font-size:11px; padding:0 10px;"
        "border-bottom:1px solid #2d3340;");
    root->addWidget(m_statusLabel);

    // ?? Splitter: canvas area | right panel ????????????????????????????????
    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->setChildrenCollapsible(false);
    splitter->setStyleSheet("QSplitter::handle{background:#2d3340; width:1px;}");

    // Left: corner + top ruler + [left ruler + canvas]
    auto *mapArea = new QWidget;
    auto *mapAreaLayout = new QGridLayout(mapArea);
    mapAreaLayout->setContentsMargins(0,0,0,0);
    mapAreaLayout->setSpacing(0);

    auto *corner = new QLabel;
    corner->setFixedSize(44, 22);
    corner->setStyleSheet("background:#1a1d23;");
    mapAreaLayout->addWidget(corner, 0, 0);

    m_topRuler = new RulerWidget(RulerWidget::Horizontal);
    mapAreaLayout->addWidget(m_topRuler, 0, 1);

    m_leftRuler = new RulerWidget(RulerWidget::Vertical);
    mapAreaLayout->addWidget(m_leftRuler, 1, 0);

    m_scene = new QGraphicsScene(this);
    m_canvas = new MapCanvas(m_scene, this);
    m_canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mapAreaLayout->addWidget(m_canvas, 1, 1);
    mapAreaLayout->setRowStretch(1, 1);
    mapAreaLayout->setColumnStretch(1, 1);

    // ?? Overlays on canvas ?????????????????????????????????????????????????
    // Coordinate label (top-left) ? fixed minimum width so Y value is never clipped
    m_coordLabel = new QLabel("X: --  Y: --", m_canvas);
    m_coordLabel->setStyleSheet(
        "background:rgba(18,21,26,200); color:#7ab0f0; font-size:11px;"
        "padding:3px 8px; border-radius:3px; font-family:Consolas,monospace;");
    m_coordLabel->setMinimumWidth(240);
    m_coordLabel->move(8, 8);
    m_coordLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_coordLabel->adjustSize();

    // Grid toggle button (bottom-right overlay)
    m_gridBtn = new QPushButton("? ??", m_canvas);
    m_gridBtn->setFixedSize(72, 26);
    m_gridBtn->setCheckable(true);
    m_gridBtn->setChecked(false);
    m_gridBtn->setStyleSheet(
        "QPushButton{background:rgba(30,34,44,200);color:#7ab0f0;"
        "border:1px solid #3a4050;border-radius:4px;font-size:11px;}"
        "QPushButton:checked{background:rgba(30,107,212,200);color:#fff;border-color:#4a9eff;}"
        "QPushButton:hover{border-color:#4a9eff;}");
    connect(m_gridBtn, &QPushButton::toggled, this, [this](bool on) {
        m_showGrid = on;
        renderAxesAndGrid();
    });

    // Reposition grid button on canvas resize
    connect(m_canvas, &MapCanvas::canvasResized, this, [this](const QSize &sz) {
        m_gridBtn->move(sz.width() - m_gridBtn->width() - 8,
                        sz.height() - m_gridBtn->height() - 8);
    });

    connect(m_canvas, &MapCanvas::transformChanged, this, &MapViewWidget::onViewTransformChanged);
    connect(m_canvas, &MapCanvas::sceneClicked,  this, &MapViewWidget::onSceneClicked);
    connect(m_canvas, &MapCanvas::sceneMoved,    this, &MapViewWidget::onSceneMoved);
    connect(m_canvas, &MapCanvas::sceneReleased, this, &MapViewWidget::onSceneReleased);

    splitter->addWidget(mapArea);

    // Right panel
    auto *rightWidget = new QWidget;
    rightWidget->setFixedWidth(240);
    rightWidget->setStyleSheet("background:#1a1d23;");
    setupRightPanel(rightWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);

    root->addWidget(splitter, 1);
}

void MapViewWidget::setupToolbar(QWidget *parent)
{
    auto *lay = new QHBoxLayout(parent);
    lay->setContentsMargins(8, 0, 8, 0);
    lay->setSpacing(4);

    m_mapNameLabel = new QLabel("?????");
    m_mapNameLabel->setStyleSheet("color:#c8d0d8; font-size:13px; font-weight:600;");
    m_mapNameLabel->setMinimumWidth(80);
    lay->addWidget(m_mapNameLabel);

    auto *sep1 = new QFrame; sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color:#2d3340;"); lay->addWidget(sep1);

    auto addBtn = [&](const QString &iconName, const QString &text, const char *slot) {
        auto *b = new QToolButton(parent);
        b->setText(text);
        b->setIcon(makeMapToolIcon(iconName));
        b->setIconSize(QSize(20, 20));
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setAutoRaise(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(kToolBtnStyle);
        connect(b, SIGNAL(clicked()), this, slot);
        lay->addWidget(b);
        return b;
    };

    m_createMapBtn = addBtn("add", "????", SLOT(onCreateMap()));

    auto *sepCreate = new QFrame; sepCreate->setFrameShape(QFrame::VLine);
    sepCreate->setStyleSheet("color:#2d3340;"); lay->addWidget(sepCreate);

    m_switchMapBtn = addBtn("swap", "????", SLOT(onSwitchMap()));
    m_switchMapBtn->setStyleSheet(kToolBtnPrimary);

    m_relocBtn = addBtn("target", "???", SLOT(onRelocalize()));

    auto *sep2 = new QFrame; sep2->setFrameShape(QFrame::VLine);
    sep2->setStyleSheet("color:#2d3340;"); lay->addWidget(sep2);

    addBtn("route",         "????",   SLOT(onEditRoute()));
    addBtn("edit",          "????",   SLOT(onEditMap()));
    addBtn("tag",           "???",     SLOT(onRenameMap()));
    addBtn("file-download", "?????", SLOT(onDownloadSource()));
    addBtn("upload",        "?????", SLOT(onSendToCar()));
    addBtn("trash",         "????",   SLOT(onDeleteMap()));

    lay->addStretch();
}

void MapViewWidget::setupRightPanel(QWidget *parent)
{
    auto *lay = new QVBoxLayout(parent);
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(8);

    auto mkGroup = [](const QString &title) {
        auto *g = new QGroupBox(title);
        g->setStyleSheet(
            "QGroupBox{border:1px solid #2d3340; border-radius:4px;"
            " margin-top:14px; color:#8898a8; font-size:12px; font-weight:600;}"
            "QGroupBox::title{subcontrol-origin:margin; left:8px; top:-2px;"
            " padding:0 4px;}");
        return g;
    };

    // ?? Car info ???????????????????????????????????????????????????????????
    auto *carGroup = mkGroup("????");
    auto *carForm  = new QFormLayout(carGroup);
    carForm->setSpacing(6);
    carForm->setLabelAlignment(Qt::AlignRight);

    auto mkInfoLabel = [](const QString &val = "--") {
        auto *l = new QLabel(val);
        l->setStyleSheet("color:#c8d0d8; font-size:12px;");
        return l;
    };
    m_carNameLabel = mkInfoLabel();
    m_batteryLabel = mkInfoLabel();
    m_speedLabel   = mkInfoLabel();
    m_covLabel     = mkInfoLabel();
    m_posLabel     = mkInfoLabel();

    carForm->addRow("??:", m_carNameLabel);
    carForm->addRow("??:", m_batteryLabel);
    carForm->addRow("??:", m_speedLabel);
    carForm->addRow("???:", m_covLabel);
    carForm->addRow("??:", m_posLabel);
    lay->addWidget(carGroup);

    // ?? Map info (moved here from the status bar above the canvas) ??????????
    auto *infoGroup = mkGroup("????");
    auto *infoForm  = new QFormLayout(infoGroup);
    infoForm->setSpacing(6);
    infoForm->setLabelAlignment(Qt::AlignRight);

    m_infoNameLabel   = mkInfoLabel();
    m_infoSizeLabel   = mkInfoLabel();
    m_infoResLabel    = mkInfoLabel();
    m_infoOriginLabel = mkInfoLabel();
    m_infoNameLabel->setWordWrap(true);

    infoForm->addRow("??:",   m_infoNameLabel);
    infoForm->addRow("??:",   m_infoSizeLabel);
    infoForm->addRow("???:", m_infoResLabel);
    infoForm->addRow("??:",   m_infoOriginLabel);
    lay->addWidget(infoGroup);

    auto *hint = new QLabel(
        "<small style='color:#556677;'>"
        "????: ??????<br>"
        "Alt+?? / ??: ??<br>"
        "??: ??"
        "</small>");
    hint->setTextFormat(Qt::RichText);
    hint->setWordWrap(true);
    lay->addWidget(hint);
    lay->addStretch();
}

// ?? Show / hide ??????????????????????????????????????????????????????????????

void MapViewWidget::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    if (m_mapId.isEmpty())
        loadCurrentCarMap();
    initMqtt();
}

void MapViewWidget::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);
    disconnectMqtt();
}

// ?? Data loading ?????????????????????????????????????????????????????????????

void MapViewWidget::loadCurrentCarMap()
{
    setStatus("????????...");
    QString carId = AppConfig::instance().agvId();
    if (carId.isEmpty()) { setStatus("?????ID", "#e05555"); return; }

    ApiService::instance().getCar(carId, [this](int code, const QJsonObject &data) {
        if (code != 200) { setStatus("????????", "#e05555"); return; }
        QJsonObject car = data.value("data").toObject(data);

        // Populate right panel car name
        m_carNameLabel->setText(car.value("carName").toString("--"));

        QString mapId = jsonIdStr(car.value("mapId"));
        if (mapId.isEmpty() || mapId == "0") {
            ApiService::instance().listMap(QJsonObject{{"pageNum",1},{"pageSize",1}},
                [this](int c2, const QJsonObject &d2) {
                    if (c2 == 200) {
                        QJsonArray rows = d2.value("rows").toArray();
                        if (!rows.isEmpty())
                            loadMapById(jsonIdStr(rows.first().toObject().value("mapId")));
                        else
                            setStatus("?????", "#e05555");
                    }
                });
            return;
        }
        loadMapById(mapId);
    });
}

void MapViewWidget::loadMapById(const QString &mapId)
{
    if (mapId.isEmpty()) return;
    setStatus(QString("?????? %1...").arg(mapId));
    m_mapId = mapId;

    ApiService::instance().getMap(mapId, [this](int code, const QJsonObject &data) {
        if (code != 200) { setStatus("????????", "#e05555"); return; }
        QJsonObject mapInfo = data.value("data").toObject(data);
        applyMapData(mapInfo);
    });
}

void MapViewWidget::applyMapData(const QJsonObject &mapInfo)
{
    m_mapName      = mapInfo.value("mapName").toString();
    m_tilemapPath  = mapInfo.value("tilemapPath").toString("tilemap");

    auto tryDbl = [&](const QStringList &keys, double def) -> double {
        for (const QString &k : keys) {
            QJsonValue v = mapInfo.value(k);
            if (!v.isUndefined() && !v.isNull()) return v.toDouble(def);
        }
        return def;
    };
    // Physical map size in metres
    double physW = tryDbl({"mapWidth","map_width","width"}, 0.0);
    double physH = tryDbl({"mapHeight","map_height","height"}, 0.0);

    // World origin = bottom-left corner of map in world coords.
    // ? Vue edit.vue/show.vue ???????? -offsetX/-offsetY ?????
    // ???/?????? originX ???Vue ? getMap ???????????
    // ???????? API ?????????????????
    auto tryOrigin = [&](const QStringList &negate) -> double {
        for (const QString &k : negate) {
            QJsonValue v = mapInfo.value(k);
            if (!v.isUndefined() && !v.isNull()) return -v.toDouble();
        }
        return 0.0;
    };
    double apiOriginX = tryOrigin({"offsetX","mapOffsetX","offset_x"});
    double apiOriginY = tryOrigin({"offsetY","mapOffsetY","offset_y"});

    m_mapNameLabel->setText(m_mapName);
    m_mapReady = false;

    loadMapFromPgm(apiOriginX, apiOriginY, physW, physH);
    loadRoute();
}

// ????????????? carto_map.pgm + carto_map.yaml?? MapEditWidget ????
// ?????????????? padding ? 512*2^n ?????getMap() ???
// mapWidth/mapHeight/offsetX/offsetY ??? padding ?????????????????
// ??????? PcdToImgImpl.pgmToImg??yaml ? resolution/origin ?? ROS map_server
// ??????????pgm ????? padding???????????
void MapViewWidget::loadMapFromPgm(double apiOriginX, double apiOriginY, double apiPhysW, double apiPhysH)
{
    if (m_mapName.isEmpty() || m_mapId.isEmpty()) {
        setStatus("??????ID", "#f0a060"); return;
    }

    auto useThumbnailFallback = [this, apiOriginX, apiOriginY, apiPhysW, apiPhysH]() {
        m_worldL = apiOriginX;
        m_worldB = apiOriginY;
        m_worldR = apiOriginX + apiPhysW;
        m_worldT = apiOriginY + apiPhysH;
        loadMapImage();
    };

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

                    QPixmap pix = QPixmap::fromImage(img);
                    // ROS map_server ???yaml ? origin ???? pgm ????????
                    // ??????????????? API ? offsetX/Y ?????????
                    m_worldL = originX;
                    m_worldB = originY;
                    m_worldR = m_worldL + pix.width()  * resolution;
                    m_worldT = m_worldB + pix.height() * resolution;
                    installMapPixmap(pix);
                });
        });
}

void MapViewWidget::loadMapImage()
{
    if (m_mapName.isEmpty() || m_mapId.isEmpty()) {
        setStatus("??????ID", "#f0a060"); return;
    }

    // URL: mapHost + mapName_mapId / tilemapPath / thumbnail.png
    // tilemapPath comes from the API response (e.g. "tilemap"); fallback to "tilemap"
    QString url = AppConfig::instance().mapHost()
                  + m_mapName + "_" + m_mapId + "/"
                  + m_tilemapPath + "/thumbnail.png";

    ApiService::instance().downloadFile(url, [this](bool ok, const QByteArray &imgData) {
        if (!ok || imgData.isEmpty()) { setStatus("????????", "#e05555"); return; }

        QPixmap pix;
        if (!pix.loadFromData(imgData)) { setStatus("????????", "#e05555"); return; }
        installMapPixmap(pix);
    });
}

void MapViewWidget::installMapPixmap(const QPixmap &pix)
{
    m_imgW = pix.width();
    m_imgH = pix.height();
    m_mapReady = true;

    m_scene->clear();
    m_mapBg = m_robotItem = m_routeLayer = m_axesLayer = nullptr;

    auto *bgItem = m_scene->addPixmap(pix);
    bgItem->setZValue(-100);
    m_mapBg = bgItem;

    m_scene->setSceneRect(0, 0, m_imgW, m_imgH);
    // ???????????? ("cover" ?? "contain")
    m_canvas->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatioByExpanding);

    renderAll();
    updateRulers();
    double resX = (m_worldR - m_worldL) / m_imgW;
    m_infoNameLabel->setText(m_mapName);
    m_infoSizeLabel->setText(QString("%1?%2px").arg((int)m_imgW).arg((int)m_imgH));
    m_infoResLabel->setText(QString("%1 m/px").arg(resX, 0, 'f', 4));
    m_infoOriginLabel->setText(QString("(%1, %2)m").arg(m_worldL, 0, 'f', 2).arg(m_worldB, 0, 'f', 2));
    setStatus("??????");
}

void MapViewWidget::loadRoute()
{
    if (m_mapId.isEmpty()) return;
    QJsonObject query;
    query["mapId"] = m_mapId;
    ApiService::instance().loadEditedMap(query, [this](int code, const QJsonObject &data) {
        m_routeLoaded = (code == 200);
        if (!m_routeLoaded) return;

        m_points = QJsonArray();
        m_lines  = QJsonArray();
        m_curves = QJsonArray();

        QString jsonStr = data.value("json").toString();
        if (!jsonStr.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
            if (doc.isObject()) {
                QJsonArray features = doc.object().value("features").toArray();
                for (const auto &fv : features) {
                    QJsonObject f = fv.toObject();
                    QJsonObject props = f.value("properties").toObject();
                    QJsonObject geom  = f.value("geometry").toObject();
                    QString gtype = geom.value("type").toString();
                    QJsonArray coords = geom.value("coordinates").toArray();
                    QString ftype = props.value("featureType").toString();

                    if (gtype == "Point" && ftype == "point") {
                        QJsonObject pt;
                        pt["id"] = props.value("id").toString();
                        pt["x"] = coords[0].toDouble();
                        pt["y"] = coords[1].toDouble();
                        pt["name"] = props.value("name").toString();
                        m_points.append(pt);
                    } else if (gtype == "LineString" && ftype == "line") {
                        QJsonObject ln;
                        ln["id"] = props.value("id").toString();
                        ln["x1"] = coords[0].toArray()[0].toDouble();
                        ln["y1"] = coords[0].toArray()[1].toDouble();
                        ln["x2"] = coords[1].toArray()[0].toDouble();
                        ln["y2"] = coords[1].toArray()[1].toDouble();
                        ln["name"] = props.value("name").toString();
                        m_lines.append(ln);
                    } else if (gtype == "LineString" && ftype == "curve") {
                        QJsonObject cv;
                        cv["id"] = props.value("id").toString();
                        cv["x1"] = coords[0].toArray()[0].toDouble();
                        cv["y1"] = coords[0].toArray()[1].toDouble();
                        cv["x2"] = coords[2].toArray()[0].toDouble();
                        cv["y2"] = coords[2].toArray()[1].toDouble();
                        cv["cx"] = coords[1].toArray()[0].toDouble();
                        cv["cy"] = coords[1].toArray()[1].toDouble();
                        cv["name"] = props.value("name").toString();
                        m_curves.append(cv);
                    }
                }
            }
        }
        if (m_mapReady) renderRoute();
    });
}

// ?? Rendering ?????????????????????????????????????????????????????????????????

QPointF MapViewWidget::worldToScene(double wx, double wy) const
{
    double mapW = m_worldR - m_worldL, mapH = m_worldT - m_worldB;
    if (m_imgW <= 0 || m_imgH <= 0 || mapW <= 0 || mapH <= 0) return {wx, wy};
    return { (wx - m_worldL) / mapW * m_imgW,
             (m_worldT - wy) / mapH * m_imgH };
}

QPointF MapViewWidget::sceneToWorld(double sx, double sy) const
{
    double mapW = m_worldR - m_worldL, mapH = m_worldT - m_worldB;
    if (m_imgW <= 0 || m_imgH <= 0 || mapW <= 0 || mapH <= 0) return {sx, sy};
    return { sx / m_imgW * mapW + m_worldL,
             m_worldT - sy / m_imgH * mapH };
}

void MapViewWidget::renderAll()
{
    renderAxesAndGrid();
    renderRoute();
    renderRobot();
}

void MapViewWidget::renderAxesAndGrid()
{
    if (!m_mapReady || m_imgW <= 0 || m_imgH <= 0) return;

    QList<QGraphicsItem *> toRemove;
    for (auto *item : m_scene->items())
        if (item->data(0).toInt() == 1) toRemove.append(item);
    for (auto *item : toRemove) { m_scene->removeItem(item); delete item; }

    auto tag = [](QGraphicsItem *item, int z) {
        item->setData(0, 1); item->setZValue(z); return item;
    };

    double wLeft   = m_worldL;
    double wRight  = m_worldR;
    double wBottom = m_worldB;
    double wTop    = m_worldT;

    const QColor xCol(210,  45,  45, 230);   // red  X
    const QColor yCol( 30, 185,  60, 230);   // green Y
    const QColor oCol( 30, 140, 230, 255);   // blue  origin dot

    // ?? Compact axis indicator at the world origin (reference-image style) ?
    QPointF O = worldToScene(0.0, 0.0);
    bool inView = (O.x() > -60 && O.x() < m_imgW + 60 &&
                   O.y() > -60 && O.y() < m_imgH + 60);

    if (inView) {
        const double arm = 52.0;   // arm length in scene pixels
        const double aL  = 11.0;  // arrowhead length
        const double aW  =  4.5;  // arrowhead half-width
        const double lw  =  1.8;  // line width

        // Helper: filled triangular arrowhead at tip, pointing in direction (dx,dy)
        auto mkArrow = [&](QPointF tip, double dx, double dy, QColor col) {
            double px = -dy, py = dx;
            QPolygonF tri;
            tri << tip
                << QPointF(tip.x() - dx*aL + px*aW, tip.y() - dy*aL + py*aW)
                << QPointF(tip.x() - dx*aL - px*aW, tip.y() - dy*aL - py*aW);
            tag(m_scene->addPolygon(tri, QPen(Qt::NoPen), QBrush(col)), 12);
        };
        auto mkLabel = [&](QPointF pos, const QString &txt, QColor col) {
            auto *t = m_scene->addText(txt);
            t->setDefaultTextColor(col);
            t->setFont(QFont("Arial", 9, QFont::Bold));
            t->setPos(pos);
            tag(t, 13);
        };

        // +X arm ? pointing right in scene
        QPointF xTip(O.x() + arm, O.y());
        tag(m_scene->addLine(O.x(), O.y(), xTip.x() - aL, O.y(),
            QPen(xCol, lw)), 10);
        mkArrow(xTip, 1.0, 0.0, xCol);
        mkLabel(QPointF(xTip.x() + 2, xTip.y() - 9), "x", xCol);

        // +Y arm ? pointing up in scene (world +Y = scene -Y)
        QPointF yTip(O.x(), O.y() - arm);
        tag(m_scene->addLine(O.x(), O.y(), O.x(), yTip.y() + aL,
            QPen(yCol, lw)), 10);
        mkArrow(yTip, 0.0, -1.0, yCol);
        mkLabel(QPointF(yTip.x() + 4, yTip.y() - 2), "y", yCol);

    }

    // ?? Grid (only when toggled on) ???????????????????????????????????????
    if (!m_showGrid) return;

    const double gridStep = 1.0;
    QPen gridPen (QColor(80, 110, 160,  80), 0.5);
    QPen axisGPen(QColor(120, 160, 240, 130), 0.8);

    for (double wx = std::ceil(wLeft / gridStep) * gridStep;
         wx <= wRight + 1e-9; wx += gridStep) {
        QPointF a = worldToScene(wx, wBottom);
        QPointF b = worldToScene(wx, wTop);
        tag(m_scene->addLine(a.x(), a.y(), b.x(), b.y(),
            std::abs(wx) < 1e-6 ? axisGPen : gridPen), 9);
    }
    for (double wy = std::ceil(wBottom / gridStep) * gridStep;
         wy <= wTop + 1e-9; wy += gridStep) {
        QPointF a = worldToScene(wLeft,  wy);
        QPointF b = worldToScene(wRight, wy);
        tag(m_scene->addLine(a.x(), a.y(), b.x(), b.y(),
            std::abs(wy) < 1e-6 ? axisGPen : gridPen), 9);
    }
}

void MapViewWidget::renderRoute()
{
    if (!m_mapReady || !m_routeLoaded) return;

    for (auto *item : m_scene->items()) {
        if (item->data(0).toInt() == 2) {
            m_scene->removeItem(item);
            delete item;
        }
    }

    if (m_points.isEmpty() && m_lines.isEmpty() && m_curves.isEmpty()) return;

    auto addRoute = [](QGraphicsItem *item) {
        item->setData(0, 2);
        item->setZValue(20);
    };

    QPen linePen(QColor(80, 200, 120, 200), 1.5);
    for (const auto &v : m_lines) {
        QJsonObject ln = v.toObject();
        QPointF a = worldToScene(ln.value("x1").toDouble(), ln.value("y1").toDouble());
        QPointF b = worldToScene(ln.value("x2").toDouble(), ln.value("y2").toDouble());
        addRoute(m_scene->addLine(a.x(), a.y(), b.x(), b.y(), linePen));
    }

    QPen curvePen(QColor(180, 120, 240, 200), 1.5);
    for (const auto &v : m_curves) {
        QJsonObject cv = v.toObject();
        QPointF p1 = worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble());
        QPointF pc = worldToScene(cv.value("cx").toDouble(), cv.value("cy").toDouble());
        QPointF p2 = worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble());
        QPainterPath path;
        path.moveTo(p1);
        path.quadTo(pc, p2);
        auto *pi = m_scene->addPath(path, curvePen);
        addRoute(pi);
    }

    for (const auto &v : m_points) {
        QJsonObject pt = v.toObject();
        QPointF sp = worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble());
        auto *ell = m_scene->addEllipse(sp.x()-5, sp.y()-5, 10, 10,
            QPen(QColor(60, 160, 255), 1), QBrush(QColor(60, 160, 255, 180)));
        addRoute(ell);

        QString name = pt.value("name").toString();
        if (!name.isEmpty()) {
            auto *lbl = m_scene->addText(name);
            lbl->setDefaultTextColor(QColor(200, 220, 255));
            lbl->setPos(sp.x() + 7, sp.y() - 10);
            lbl->setFont(QFont("Microsoft YaHei UI", 8));
            addRoute(lbl);
        }
    }
}

void MapViewWidget::renderRobot()
{
    if (!m_mapReady || !m_robot.posValid) return;

    // Remove old robot items
    for (auto *item : m_scene->items()) {
        if (item->data(0).toInt() == 3) {
            m_scene->removeItem(item);
            delete item;
        }
    }

    QPointF sp = worldToScene(m_robot.wx, m_robot.wy);

    // ENU: yaw=0?East, yaw=?/2?North, CCW positive. Icon/triangle tip points North.
    // Qt rotate(a) is CW; tip (0,-10) after rotate(a) points (sin a, -cos a).
    // For ENU direction (cos yaw, -sin yaw): solve ? a = 90 - yaw. So screenAngle = 90 - yawDeg.
    double yawRad      = (qAbs(m_robot.yawRad) > M_PI * 2)
                         ? m_robot.yawRad * M_PI / 180.0 : m_robot.yawRad;
    double yawDeg      = yawRad * 180.0 / M_PI;
    double screenAngle = 90.0 - yawDeg;

    if (!m_robotPixmap.isNull()) {
        const double iconSz = 15.0;   // ????????30
        const double srcSz  = 256.0;  // ?????
        const double pivot  = srcSz / 2.0;
        auto *robotItem = m_scene->addPixmap(m_robotPixmap);
        robotItem->setScale(iconSz / srcSz);
        robotItem->setTransformOriginPoint(pivot, pivot);
        robotItem->setPos(sp.x() - pivot, sp.y() - pivot);
        robotItem->setRotation(screenAngle);
        robotItem->setData(0, 3);
        robotItem->setZValue(100);
    } else {
        // Fallback: draw triangle
        QPolygonF tri;
        tri << QPointF(0, -10) << QPointF(-6, 6) << QPointF(6, 6);
        QTransform t;
        t.translate(sp.x(), sp.y());
        t.rotate(screenAngle);
        tri = t.map(tri);
        auto *marker = m_scene->addPolygon(tri,
            QPen(QColor(255, 220, 50), 2),
            QBrush(QColor(255, 180, 0, 200)));
        marker->setData(0, 3);
        marker->setZValue(100);
    }
}

void MapViewWidget::updateRulers()
{
    m_topRuler->syncWithView(m_canvas, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
    m_leftRuler->syncWithView(m_canvas, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
}

// ?? MQTT ??????????????????????????????????????????????????????????????????????

void MapViewWidget::onMqttMessage(const QString &topic, const QByteArray &payload)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
    if (!topic.contains("/info")) return;

    QJsonObject d = doc.object();

    auto dv = [&](const QStringList &keys, double def = 0) -> double {
        for (const QString &k : keys) {
            QJsonValue v = d.value(k);
            if (!v.isUndefined() && !v.isNull()) return v.toDouble(def);
        }
        return def;
    };

    m_robot.wx       = dv({"car_x","carX","x"});
    m_robot.wy       = dv({"car_y","carY","y"});
    m_robot.yawRad   = dv({"car_yaw","carYaw","yaw"});
    m_robot.speed    = dv({"speed","linear_x","linearX"});
    m_robot.battery  = dv({"battery","power","bat"}, -1);
    m_robot.covScore = dv({"covariance_score","covarianceScore","covariance"}, -1);
    m_robot.posValid = true;

    renderRobot();
    updateInfoPanel();
}

void MapViewWidget::updateInfoPanel()
{
    if (!m_robot.posValid) return;

    // Battery
    double bat = m_robot.battery;
    if (bat < 0) {
        m_batteryLabel->setText("???");
        m_batteryLabel->setStyleSheet("color:#5cb85c; font-size:12px;");
    } else {
        int batPct = (int)(bat * 100);
        m_batteryLabel->setText(QString::number(batPct) + "%");
        QColor c = batPct < 20 ? QColor(0xe0,0x55,0x55) :
                   batPct < 40 ? QColor(0xf0,0xa0,0x30) :
                                 QColor(0x5c,0xb8,0x5c);
        m_batteryLabel->setStyleSheet(
            QString("color:%1; font-size:12px;").arg(c.name()));
    }

    m_speedLabel->setText(QString::number(m_robot.speed, 'f', 2) + " m/s");
    m_posLabel->setText(QString("%1, %2")
        .arg(m_robot.wx, 0, 'f', 2)
        .arg(m_robot.wy, 0, 'f', 2));

    if (m_robot.covScore >= 0) {
        m_covLabel->setText(QString::number(m_robot.covScore, 'f', 1) + "%");
        m_covLabel->setStyleSheet(
            m_robot.covScore <= 40
            ? "color:#e05555; font-size:12px;"
            : "color:#c8d0d8; font-size:12px;");
    } else {
        m_covLabel->setText("??");
        m_covLabel->setStyleSheet("color:#8898a8; font-size:12px;");
    }
}

// ?? View events ???????????????????????????????????????????????????????????????

void MapViewWidget::onViewTransformChanged()
{
    updateRulers();

    QPoint cursorPos = m_canvas->mapFromGlobal(QCursor::pos());
    if (m_canvas->rect().contains(cursorPos)) {
        QPointF scenePos = m_canvas->mapToScene(cursorPos);
        QPointF world = sceneToWorld(scenePos.x(), scenePos.y());
        m_coordLabel->setText(
            QString("X: %1  Y: %2")
            .arg(world.x(), 0, 'f', 2).arg(world.y(), 0, 'f', 2));
        m_coordLabel->adjustSize();
    }
}

void MapViewWidget::onSceneClicked(const QPointF &scenePos, Qt::MouseButton btn)
{
    QPointF world = sceneToWorld(scenePos.x(), scenePos.y());
    m_coordLabel->setText(
        QString("X: %1  Y: %2")
        .arg(world.x(), 0, 'f', 2).arg(world.y(), 0, 'f', 2));
    m_coordLabel->adjustSize();

    if (btn != Qt::LeftButton) return;

    if (m_relocMode) {
        if (m_robot.carId.isEmpty()) { setStatus("???ID??????", "#e05555"); return; }
        // Start drag: record press position, create dot preview
        m_relocDragging   = true;
        m_relocStartWorld = world;

        // Remove stale preview items
        if (m_poseDotItem)   { m_scene->removeItem(m_poseDotItem);   delete m_poseDotItem;   m_poseDotItem   = nullptr; }
        if (m_poseArrowItem) { m_scene->removeItem(m_poseArrowItem); delete m_poseArrowItem; m_poseArrowItem = nullptr; }

        // Dot at click position
        QPointF s = worldToScene(world.x(), world.y());
        auto *dot = new QGraphicsEllipseItem(s.x() - 6, s.y() - 6, 12, 12);
        dot->setBrush(QColor(255, 165, 0));
        dot->setPen(Qt::NoPen);
        dot->setZValue(30);
        m_poseDotItem = dot;
        m_scene->addItem(m_poseDotItem);

        setStatus("???????????????", "#4a9eff");
    }
}

void MapViewWidget::onSceneMoved(const QPointF &scenePos)
{
    // Update cursor coordinate label
    QPointF world = sceneToWorld(scenePos.x(), scenePos.y());
    m_coordLabel->setText(
        QString("X: %1  Y: %2")
        .arg(world.x(), 0, 'f', 2).arg(world.y(), 0, 'f', 2));
    m_coordLabel->adjustSize();

    if (!m_relocDragging) return;

    // Update arrow preview from start to current position
    QPointF startScene = worldToScene(m_relocStartWorld.x(), m_relocStartWorld.y());
    if (m_poseArrowItem) { m_scene->removeItem(m_poseArrowItem); delete m_poseArrowItem; m_poseArrowItem = nullptr; }

    auto *line = new QGraphicsLineItem(startScene.x(), startScene.y(), scenePos.x(), scenePos.y());
    line->setPen(QPen(QColor(255, 165, 0), 2, Qt::SolidLine, Qt::RoundCap));
    line->setZValue(30);
    m_poseArrowItem = line;
    m_scene->addItem(m_poseArrowItem);
}

void MapViewWidget::onSceneReleased(const QPointF &scenePos, Qt::MouseButton btn)
{
    if (!m_relocDragging || btn != Qt::LeftButton) return;

    m_relocDragging = false;
    QPointF endWorld = sceneToWorld(scenePos.x(), scenePos.y());

    // Clean up preview items
    if (m_poseDotItem)   { m_scene->removeItem(m_poseDotItem);   delete m_poseDotItem;   m_poseDotItem   = nullptr; }
    if (m_poseArrowItem) { m_scene->removeItem(m_poseArrowItem); delete m_poseArrowItem; m_poseArrowItem = nullptr; }

    // Compute yaw: atan2(?north, ?east) ? ROS standard (0=east, CCW positive)
    double dx = endWorld.x() - m_relocStartWorld.x();
    double dy = endWorld.y() - m_relocStartWorld.y();
    double yaw = (dx * dx + dy * dy > 0.01)   // use drag direction if dragged > ~0.1m
                 ? std::atan2(dy, dx)
                 : m_robot.yawRad;

    QString carId = m_robot.carId;
    if (carId.isEmpty()) { setStatus("???ID??????", "#e05555"); return; }

    QJsonObject poseObj;
    QJsonArray coords;
    coords.append(m_relocStartWorld.x());
    coords.append(m_relocStartWorld.y());
    coords.append(0.0);
    poseObj["coordinates"] = coords;
    poseObj["yaw"]         = yaw;

    QJsonObject msg;
    msg["action"] = "initialpose";
    msg["car_id"] = carId;
    msg["map_id"] = m_mapId;
    msg["pose"]   = poseObj;

    setStatus("???????...", "#4a9eff");
    auto *loading = new LoadingDialog("???????...", this);
    loading->show();
    ApiService::instance().sendMqtt(msg, [this, loading](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            Toast::success(this, "?????");
            setStatus("?????");
        } else {
            setStatus("?????", "#e05555");
        }
        m_relocMode = false;
        m_relocBtn->setStyleSheet(kToolBtnStyle);
        m_canvas->setPanWithLeftButton(true);
    });
}

// ?? Toolbar actions ???????????????????????????????????????????????????????????

void MapViewWidget::onCreateMap()
{
    emit navigateRequested("create_map", QString());
}

void MapViewWidget::onSwitchMap()
{
    QDialog dlg(this);
    dlg.setWindowTitle("????");
    dlg.setMinimumSize(820, 560);
    dlg.setStyleSheet(
        "QDialog{background:#1a1d23; color:#c8d0d8;}"
        "QScrollArea{background:#1a1d23; border:none;}"
        "QWidget#cards{background:#1a1d23;}"
        "QPushButton{background:#252a35; color:#c8d0d8; border:1px solid #3a3f4a;"
        "border-radius:4px; padding:5px 12px; font-size:12px;}"
        "QPushButton:hover{background:#2d3340;}"
        "QLabel{background:transparent;}");

    auto *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    // Search bar
    auto *topRow = new QHBoxLayout;
    auto *searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("?? ??????...");
    searchEdit->setStyleSheet(
        "background:#252a35; border:1px solid #3a3f4a; border-radius:4px;"
        "color:#c8d0d8; padding:5px 10px; font-size:12px;");
    topRow->addWidget(searchEdit, 1);
    root->addLayout(topRow);

    // Scrollable card grid
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *cardsWidget = new QWidget;
    cardsWidget->setObjectName("cards");
    auto *grid = new QGridLayout(cardsWidget);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setSpacing(12);
    grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    scroll->setWidget(cardsWidget);
    root->addWidget(scroll, 1);

    // Button row
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto *cancelBtn = new QPushButton("??");
    auto *okBtn     = new QPushButton("??");
    okBtn->setStyleSheet(kBtnPrimary);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    root->addLayout(btnRow);

    connect(okBtn,     &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    // Track selected map
    QString selectedMapId;
    QVector<QFrame*> allCards;
    QJsonArray mapList;

    // Build cards and load thumbnails (std::function, not auto, so it can call itself recursively after a delete)
    std::function<void(const QJsonArray &, const QString &)> buildCards =
            [&](const QJsonArray &list, const QString &filter) {
        // Clear existing
        for (auto *c : allCards) c->deleteLater();
        allCards.clear();
        while (auto *item = grid->takeAt(0)) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }

        int col = 0, row = 0;
        const int COLS = 4;
        const int CARD_W = 170, CARD_H = 208;

        for (const auto &v : list) {
            QJsonObject m = v.toObject();
            QString name  = m.value("mapName").toString();
            QString mid   = jsonIdStr(m.value("mapId"));
            QString flag  = m.value("delFlag").toString("0");

            if (!filter.isEmpty() && !name.toLower().contains(filter.toLower()))
                continue;

            // Card frame
            auto *card = new QFrame;
            card->setFixedSize(CARD_W, CARD_H);
            card->setStyleSheet(
                "QFrame{background:#252a35; border:1px solid #3a3f4a; border-radius:6px;}"
                "QFrame:hover{border-color:#4a9eff;}");
            allCards.append(card);

            auto *cl = new QVBoxLayout(card);
            cl->setContentsMargins(6, 6, 6, 6);
            cl->setSpacing(4);

            // Thumbnail area
            auto *thumbLbl = new QLabel;
            thumbLbl->setFixedSize(CARD_W-12, 130);
            thumbLbl->setAlignment(Qt::AlignCenter);
            thumbLbl->setStyleSheet("background:#12151a; border-radius:4px; color:#556677;");
            thumbLbl->setText("???...");
            cl->addWidget(thumbLbl);

            // Map name + ID
            QString dispName = name.length() > 16 ? name.left(15)+"?" : name;
            auto *nameLbl = new QLabel(QString("%1\nID: %2").arg(dispName, mid));
            nameLbl->setAlignment(Qt::AlignCenter);
            nameLbl->setStyleSheet("color:#c8d0d8; font-size:11px; font-weight:600;");
            nameLbl->setToolTip(QString("%1 [ID: %2]").arg(name, mid));
            cl->addWidget(nameLbl);

            // Status badge
            QString statusText = flag == "0" ? "??" : flag == "3" ? "???" : "??";
            QString statusColor= flag == "0" ? "#5cb85c" : flag == "3" ? "#f0a060" : "#e05555";
            auto *statusLbl = new QLabel(statusText);
            statusLbl->setAlignment(Qt::AlignCenter);
            statusLbl->setStyleSheet(
                QString("color:%1; font-size:10px;").arg(statusColor));
            cl->addWidget(statusLbl);

            // Click to select
            card->setProperty("mapId", mid);
            card->installEventFilter(&dlg);

            // Mouse press ? select
            auto *selOverlay = new QPushButton(card);
            selOverlay->setGeometry(0, 0, CARD_W, CARD_H);
            selOverlay->setFlat(true);
            selOverlay->setStyleSheet("QPushButton{background:transparent;}");
            connect(selOverlay, &QPushButton::clicked, this, [&, mid, card]() {
                selectedMapId = mid;
                for (auto *c : allCards) {
                    c->setStyleSheet(
                        "QFrame{background:#252a35; border:1px solid #3a3f4a; border-radius:6px;}"
                        "QFrame:hover{border-color:#4a9eff;}");
                }
                card->setStyleSheet(
                    "QFrame{background:#1e4080; border:2px solid #4a9eff; border-radius:6px;}");
            });
            connect(selOverlay, &QPushButton::pressed, this, [&dlg, &selectedMapId, mid, card]() {
                selectedMapId = mid;
                Q_UNUSED(card)
                (void)selectedMapId;
            });
            // Double-click accepts dialog
            selOverlay->setProperty("mapId", mid);

            // Delete button (top-right corner overlay, raised above selOverlay)
            auto *delBtn = new QPushButton("?", card);
            delBtn->setFixedSize(22, 22);
            delBtn->move(CARD_W - 28, 4);
            delBtn->setCursor(Qt::PointingHandCursor);
            delBtn->setStyleSheet(
                "QPushButton{background:rgba(180,40,40,200); color:#fff; border:none;"
                "border-radius:11px; font-size:11px; font-weight:bold; padding:0;}"
                "QPushButton:hover{background:rgba(220,50,50,235);}");
            delBtn->raise();
            connect(delBtn, &QPushButton::clicked, this, [&, mid, name]() {
                QMessageBox confirm(&dlg);
                confirm.setWindowTitle("????");
                confirm.setText(QString("???????%1?(ID: %2)?\n????????").arg(name, mid));
                QPushButton *yesBtn = confirm.addButton("??", QMessageBox::YesRole);
                confirm.addButton("??", QMessageBox::NoRole);
                confirm.exec();
                if (confirm.clickedButton() != yesBtn) return;

                ApiService::instance().delMap(mid, [&, mid](int code, const QJsonObject &) {
                    if (code != 200) {
                        QMessageBox::warning(&dlg, "??", "??????");
                        return;
                    }
                    Toast::success(this, "?????");
                    if (selectedMapId == mid) selectedMapId.clear();
                    QJsonArray newList;
                    for (const auto &v : mapList)
                        if (jsonIdStr(v.toObject().value("mapId")) != mid) newList.append(v);
                    mapList = newList;
                    buildCards(mapList, searchEdit->text());
                });
            });

            grid->addWidget(card, row, col);
            if (++col >= COLS) { col = 0; ++row; }

            // Load thumbnail asynchronously
            if (flag == "0") {
                QString mname      = m.value("mapName").toString();
                QString tilePath   = m.value("tilemapPath").toString("tilemap");
                QString thumbUrl = AppConfig::instance().mapHost()
                                   + mname + "_" + mid + "/" + tilePath + "/thumbnail.png";
                ApiService::instance().downloadFile(thumbUrl,
                    [thumbLbl, CARD_W](bool ok, const QByteArray &imgData) {
                        if (!ok || imgData.isEmpty()) {
                            thumbLbl->setText("???");
                            return;
                        }
                        QPixmap pix;
                        if (pix.loadFromData(imgData)) {
                            thumbLbl->setPixmap(pix.scaled(CARD_W-12, 130,
                                Qt::KeepAspectRatio, Qt::SmoothTransformation));
                            thumbLbl->setText("");
                        } else {
                            thumbLbl->setText("???");
                        }
                    });
            } else {
                thumbLbl->setText(flag == "3" ? "???..." : "??");
            }
        }
    };

    // Load map list
    ApiService::instance().listMapAll(QJsonObject{{"pageNum",1},{"pageSize",100}},
        [&](int code, const QJsonObject &data) {
            if (code != 200) return;
            mapList = data.value("rows").toArray();
            buildCards(mapList, "");
        });

    connect(searchEdit, &QLineEdit::textChanged, this, [&](const QString &txt) {
        buildCards(mapList, txt);
    });

    if (dlg.exec() == QDialog::Accepted && !selectedMapId.isEmpty() && selectedMapId != m_mapId) {
        QString carId = m_robot.carId;
        if (carId.isEmpty()) {
            // ????????????????????????
            m_points = QJsonArray();
            m_lines  = QJsonArray();
            m_curves = QJsonArray();
            m_routeLoaded = false;
            m_robot.posValid = false;
            loadMapById(selectedMapId);
            return;
        }

        // ???????????????????????????????????????
        auto *loading = new LoadingDialog("??????...", this);
        loading->show();
        ApiService::instance().getCar(carId, [this, selectedMapId, loading](int code, const QJsonObject &data) {
            if (code != 200) {
                loading->close();
                loading->deleteLater();
                QMessageBox::warning(this, "??", "???????????????");
                return;
            }
            QJsonObject car = data.value("data").toObject(data);
            car["mapId"] = selectedMapId;
            // ???? updateTime ????? (? Vue car/details.vue ? submitForm ??)?
            // ???? getCar ???? updateTime ???????????????????
            // ???? 40 ?????? "crud ????"?
            car["updateTime"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
            ApiService::instance().updateCar(car, [this, selectedMapId, loading](int code2, const QJsonObject &) {
                loading->close();
                loading->deleteLater();
                if (code2 != 200) {
                    QMessageBox::warning(this, "??", "????????????");
                    return;
                }
                // ????????????????
                m_points = QJsonArray();
                m_lines  = QJsonArray();
                m_curves = QJsonArray();
                m_routeLoaded = false;
                m_robot.posValid = false;
                loadMapById(selectedMapId);
                Toast::success(this, "??????");
            });
        });
    }
}

void MapViewWidget::onRelocalize()
{
    m_relocMode = !m_relocMode;
    m_canvas->setPanWithLeftButton(!m_relocMode);
    if (m_relocMode) {
        m_relocBtn->setStyleSheet(kToolBtnPrimary);
        setStatus("??????????????? (????????)", "#4a9eff");
    } else {
        // Cancel: clean up any in-progress drag
        m_relocDragging = false;
        if (m_poseDotItem)   { m_scene->removeItem(m_poseDotItem);   delete m_poseDotItem;   m_poseDotItem   = nullptr; }
        if (m_poseArrowItem) { m_scene->removeItem(m_poseArrowItem); delete m_poseArrowItem; m_poseArrowItem = nullptr; }
        m_relocBtn->setStyleSheet(kToolBtnStyle);
        setStatus("??");
    }
}

void MapViewWidget::onEditRoute()
{
    if (m_mapId.isEmpty()) { setStatus("??????", "#f0a060"); return; }
    emit navigateRequested("map_edit", m_mapId);
}

void MapViewWidget::onEditMap()
{
    if (m_mapId.isEmpty()) { setStatus("??????", "#f0a060"); return; }
    emit navigateRequested("map_image_edit", m_mapId);
}

void MapViewWidget::onRenameMap()
{
    if (m_mapId.isEmpty()) return;
    bool ok;
    QString name = QInputDialog::getText(this, "?????", "???:",
                                         QLineEdit::Normal, m_mapName, &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    QJsonObject d;
    d["mapId"]   = m_mapId;
    d["mapName"] = name.trimmed();
    auto *loading = new LoadingDialog("???????...", this);
    loading->show();
    ApiService::instance().updateMap(d, [this, loading, name](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            m_mapName = name.trimmed();
            m_mapNameLabel->setText(m_mapName);
            setStatus("?????");
            Toast::success(this, "?????");
        } else {
            QMessageBox::warning(this, "??", "?????");
        }
    });
}

void MapViewWidget::onDownloadSource()
{
    if (m_mapId.isEmpty()) return;
    ApiService::instance().downloadPcd(
        QJsonObject{{"mapId", m_mapId}},
        [this](int code, const QJsonObject &data) {
            if (code == 200) {
                QString path = data.value("pcdPath").toString();
                if (!path.isEmpty())
                    QDesktopServices::openUrl(QUrl(AppConfig::instance().pcdHost() + path));
            } else {
                QDesktopServices::openUrl(QUrl(
                    AppConfig::instance().mapHost() + m_mapName + "_" + m_mapId));
            }
        });
}

void MapViewWidget::onSendToCar()
{
    if (m_mapId.isEmpty()) return;
    auto *loading = new LoadingDialog("?????????...", this);
    loading->show();
    ApiService::instance().sendMap2Car(
        QJsonObject{{"mapId", m_mapId}},
        [this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) { setStatus("????????"); Toast::success(this, "????????"); }
            else QMessageBox::warning(this, "??", "??????");
        });
}

void MapViewWidget::onDeleteMap()
{
    if (m_mapId.isEmpty()) return;
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("????");
    msgBox.setText(QString("???????%1??\n????????").arg(m_mapName));
    QPushButton *yesBtn = msgBox.addButton("??", QMessageBox::YesRole);
    msgBox.addButton("??", QMessageBox::NoRole);
    msgBox.exec();
    if (msgBox.clickedButton() != yesBtn) return;

    auto *loading = new LoadingDialog("??????...", this);
    loading->show();
    ApiService::instance().delMap(m_mapId, [this, loading](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            m_mapId.clear(); m_mapName.clear();
            m_scene->clear();
            m_mapBg = m_robotItem = m_routeLayer = m_axesLayer = nullptr;
            m_mapNameLabel->setText("???");
            setStatus("???????????");
            Toast::success(this, "?????");
        } else {
            QMessageBox::warning(this, "??", "??????");
        }
    });
}

void MapViewWidget::setStatus(const QString &msg, const QString &color)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(
        QString("background:#0d1117; color:%1; font-size:11px; padding:0 10px;"
                "border-bottom:1px solid #2d3340;").arg(color));
}

void MapViewWidget::initMqtt()
{
    QString carId = AppConfig::instance().agvId();
    if (carId.isEmpty()) return;
    if (!MqttService::instance().isConnected())
        MqttService::instance().connectToHost(AppConfig::instance().serverIp(), 1883);
    m_robot.carId = carId;
    MqttService::instance().subscribeTopic(
        QString("/rob/%1/info").arg(carId), 0,
        [this](const QString &t, const QByteArray &p) { onMqttMessage(t, p); });
}

void MapViewWidget::disconnectMqtt()
{
    QString carId = AppConfig::instance().agvId();
    if (!carId.isEmpty())
        MqttService::instance().unsubscribeTopic(QString("/rob/%1/info").arg(carId));
}
