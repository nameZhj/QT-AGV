#include "MapViewWidget.h"
#include "core/AppConfig.h"
#include "core/ApiService.h"
#include "core/MqttService.h"
#include "core/RobotInfoManager.h"
#include "core/TeleopController.h"
#include "core/LoadingDialog.h"
#include "core/Toast.h"
#include "core/MapPgmUtil.h"
#include "core/ZipUtil.h"
#include <QDirIterator>
#include "MapUploadDialog.h"
#include "SmapUploadDialog.h"
#include "RobotStatusDialog.h"
#include "DirectionPadWidget.h"
#include "MotionDialog.h"
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
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <functional>


static const QString kToolBtnStyle =
    "QToolButton{background:#252a35;color:#c8d0d8;border:1px solid #3a3f4a;"
    "border-radius:4px;padding:3px 8px;font-size:11px;font-weight:600;}"
    "QToolButton:hover{background:#2d3340;border-color:#4a9eff;}"
    "QToolButton:pressed{background:#1e4080;}"
    "QToolButton:disabled{background:#1c1f26;color:#556070;border-color:#2a2e38;}";
static const QString kToolBtnPrimary =
    "QToolButton{background:#1e6fd4;color:#fff;border:none;"
    "border-radius:4px;padding:3px 8px;font-size:11px;font-weight:600;}"
    "QToolButton:hover{background:#2980e8;}"
    "QToolButton:pressed{background:#155db8;}"
    "QToolButton:disabled{background:#1c1f26;color:#556070;border:none;}";
static const QString kToolBtnDanger =
    "QToolButton{background:#c0392b;color:#fff;border:none;"
    "border-radius:4px;padding:3px 8px;font-size:11px;font-weight:600;}"
    "QToolButton:hover{background:#e74c3c;}"
    "QToolButton:pressed{background:#a93226;}";
static const QString kBtnPrimary = "background:#2980b9; color:white; border-radius:4px; padding:6px 12px;";

// ── Toolbar icon helpers ─────────────────────────────────────────────────────
namespace {

// 与 Vue 一致：carId/mapId 在 Vue store 中是数字(parseInt)，发送 MQTT 时需要
// 以数字类型出现在 JSON 中，否则后端无法识别。此函数把数字字符串转为 QJsonValue
// 数字，非数字字符串保持为字符串。
QJsonValue numOrStr(const QString &s) {
    bool ok; qlonglong n = s.toLongLong(&ok);
    return ok ? QJsonValue((double)n) : QJsonValue(s);
}

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
    } else if (name == "map-upload") {
        // 地图上传图标：折页地图 + 向上箭头
        p.setPen(QPen(col, 1.3)); p.setBrush(Qt::NoBrush);
        QPolygonF mapSheet;
        mapSheet << QPointF(sz*0.16, sz*0.28) << QPointF(sz*0.38, sz*0.20)
                 << QPointF(sz*0.62, sz*0.28) << QPointF(sz*0.84, sz*0.20)
                 << QPointF(sz*0.84, sz*0.82) << QPointF(sz*0.62, sz*0.90)
                 << QPointF(sz*0.38, sz*0.82) << QPointF(sz*0.16, sz*0.90);
        p.drawPolygon(mapSheet);
        p.drawLine(QPointF(sz*0.38, sz*0.20), QPointF(sz*0.38, sz*0.82));
        p.drawLine(QPointF(sz*0.62, sz*0.28), QPointF(sz*0.62, sz*0.90));
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(c, sz*0.74), QPointF(c, sz*0.42));
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.34, sz*0.52) << QPointF(sz*0.66, sz*0.52) << QPointF(c, sz*0.32);
        p.drawPolygon(arrow);
    } else if (name == "smap-upload") {
        // smap 上传图标：文档轮廓 + 向上箭头 + 'S' 标
        p.setPen(QPen(col, 1.3)); p.setBrush(Qt::NoBrush);
        QPolygonF file;
        file << QPointF(sz*0.18, sz*0.14) << QPointF(sz*0.56, sz*0.14)
             << QPointF(sz*0.82, sz*0.36) << QPointF(sz*0.82, sz*0.88)
             << QPointF(sz*0.18, sz*0.88);
        p.drawPolygon(file);
        p.drawLine(QPointF(sz*0.56, sz*0.14), QPointF(sz*0.56, sz*0.36));
        p.drawLine(QPointF(sz*0.56, sz*0.36), QPointF(sz*0.82, sz*0.36));
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(sz*0.50, sz*0.76), QPointF(sz*0.50, sz*0.46));
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.36, sz*0.54) << QPointF(sz*0.64, sz*0.54) << QPointF(sz*0.50, sz*0.36);
        p.drawPolygon(arrow);
        p.setPen(col);
        QFont f("Consolas", 6, QFont::Bold);
        p.setFont(f);
        p.drawText(QRectF(sz*0.22, sz*0.20, sz*0.30, sz*0.30), Qt::AlignCenter, "S");
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
    } else if (name == "reset") {
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
    } else if (name == "refresh") {
        // 刷新图标：顺时针双向环形箭头
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush);
        QRectF r(sz*0.20, sz*0.20, sz*0.60, sz*0.60);
        p.drawArc(r, 45 * 16, 250 * 16);
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow;
        arrow << QPointF(sz*0.78, sz*0.24)
              << QPointF(sz*0.86, sz*0.44)
              << QPointF(sz*0.64, sz*0.38);
        p.drawPolygon(arrow);
    } else if (name == "status") {
        // 状态图标：方框内波形折线（心电图/脉冲波）
        p.setPen(QPen(col, 1.4)); p.setBrush(Qt::NoBrush);
        QRectF box(sz*0.14, sz*0.14, sz*0.72, sz*0.72);
        p.drawRoundedRect(box, 3, 3);
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPolygonF pulse;
        pulse << QPointF(sz*0.22, sz*0.52)
              << QPointF(sz*0.36, sz*0.52)
              << QPointF(sz*0.44, sz*0.30)
              << QPointF(sz*0.54, sz*0.74)
              << QPointF(sz*0.62, sz*0.44)
              << QPointF(sz*0.68, sz*0.52)
              << QPointF(sz*0.78, sz*0.52);
        p.drawPolyline(pulse);
    } else if (name == "move") {
        // 移动图标：四向移动十字箭头
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(c, sz*0.20), QPointF(c, sz*0.80));
        p.drawLine(QPointF(sz*0.20, c), QPointF(sz*0.80, c));
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF topA; topA << QPointF(c, sz*0.10) << QPointF(c - sz*0.10, sz*0.24) << QPointF(c + sz*0.10, sz*0.24);
        p.drawPolygon(topA);
        QPolygonF botA; botA << QPointF(c, sz*0.90) << QPointF(c - sz*0.10, sz*0.76) << QPointF(c + sz*0.10, sz*0.76);
        p.drawPolygon(botA);
        QPolygonF lftA; lftA << QPointF(sz*0.10, c) << QPointF(sz*0.24, c - sz*0.10) << QPointF(sz*0.24, c + sz*0.10);
        p.drawPolygon(lftA);
        QPolygonF rgtA; rgtA << QPointF(sz*0.90, c) << QPointF(sz*0.76, c - sz*0.10) << QPointF(sz*0.76, c + sz*0.10);
        p.drawPolygon(rgtA);
    } else if (name == "cancel") {
        // 取消图标：圆圈内打叉
        p.setPen(QPen(col, 1.4)); p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(c, c), sz*0.38, sz*0.38);
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(c - sz*0.16, c - sz*0.16), QPointF(c + sz*0.16, c + sz*0.16));
        p.drawLine(QPointF(c + sz*0.16, c - sz*0.16), QPointF(c - sz*0.16, c + sz*0.16));
    } else if (name == "pause") {
        // 暂停图标：两条垂直实心圆角柱
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawRoundedRect(QRectF(sz*0.26, sz*0.20, sz*0.16, sz*0.60), 2, 2);
        p.drawRoundedRect(QRectF(sz*0.58, sz*0.20, sz*0.16, sz*0.60), 2, 2);
    } else if (name == "teleop") {
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

// ═══════════════════════════════════════════════════════════════════════════
//  RulerWidget
// ═══════════════════════════════════════════════════════════════════════════

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

// ═══════════════════════════════════════════════════════════════════════════
//  MapCanvas
// ═══════════════════════════════════════════════════════════════════════════

MapViewWidget::MapViewWidget(QWidget *parent)
    : QWidget(parent)
{
    // Load SVG robot icon at 180×330 (matching 60×110 aspect ratio) for crisp scaling at any size
    QSvgRenderer renderer(QString(":/ic-robot1.svg"));
    if (renderer.isValid()) {
        m_robotPixmap = QPixmap(180, 330);
        m_robotPixmap.fill(Qt::transparent);
        QPainter rp(&m_robotPixmap);
        rp.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        renderer.render(&rp);
    }
    setupUI();
    connect(&RobotInfoManager::instance(), &RobotInfoManager::currentRobotInfoUpdated,
            this, &MapViewWidget::onRobotInfoUpdated);
    connect(&TeleopController::instance(), &TeleopController::activeChanged,
            this, &MapViewWidget::onTeleopActiveChanged);
    onRobotInfoUpdated(RobotInfoManager::instance().currentInfo());
}

MapViewWidget::~MapViewWidget() {}

void MapViewWidget::setupUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Toolbar ────────────────────────────────────────────────────────────
    auto *toolbarWidget = new QWidget;
    toolbarWidget->setFixedHeight(60);
    toolbarWidget->setStyleSheet("background:#12151a; border-bottom:1px solid #2d3340;");
    setupToolbar(toolbarWidget);
    root->addWidget(toolbarWidget);

    // ── Status bar ─────────────────────────────────────────────────────────
    m_statusLabel = new QLabel("准备就绪");
    m_statusLabel->setFixedHeight(24);
    m_statusLabel->setStyleSheet(
        "background:#0d1117; color:#8898a8; font-size:11px; padding:0 10px;"
        "border-bottom:1px solid #2d3340;");
    root->addWidget(m_statusLabel);

    // ── Splitter: canvas area | right panel ────────────────────────────────
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

    // ── Overlays on canvas ─────────────────────────────────────────────────
    // Coordinate label (top-left) — fixed minimum width so Y value is never clipped
    m_coordLabel = new QLabel("X: --  Y: --", m_canvas);
    m_coordLabel->setStyleSheet(
        "background:rgba(18,21,26,200); color:#7ab0f0; font-size:11px;"
        "padding:3px 8px; border-radius:3px; font-family:Consolas,monospace;");
    m_coordLabel->setMinimumWidth(240);
    m_coordLabel->move(8, 8);
    m_coordLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_coordLabel->adjustSize();

    // Grid toggle button (bottom-right overlay)
    m_gridBtn = new QPushButton("⊞ 网格", m_canvas);
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

    // Direction pad overlay (bottom-left of canvas, persistent)
    m_directionPad = new DirectionPadWidget(m_canvas);
    m_directionPad->move(16, qMax(16, m_canvas->height() - m_directionPad->height() - 16));
    m_directionPad->raise();

    // Reposition overlays on canvas resize
    connect(m_canvas, &MapCanvas::canvasResized, this, [this](const QSize &sz) {
        m_gridBtn->move(sz.width() - m_gridBtn->width() - 8,
                        sz.height() - m_gridBtn->height() - 8);
        if (m_directionPad) {
            m_directionPad->move(16, sz.height() - m_directionPad->height() - 16);
            m_directionPad->raise();
        }
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

    // 1. 启用编辑
    m_editMapBtn = addBtn("edit", "启用编辑", SLOT(onEditMap()));
    m_editMapBtn->setToolTip("进入地图编辑与拓扑标注模式");

    // 2. 加载地图 (原 smap上传)
    m_uploadSmapBtn = addBtn("smap-upload", "加载地图", SLOT(onUploadSmap()));
    m_uploadSmapBtn->setToolTip("选择本地 .smap 地图文件加载至当前视图");

    // 3. 2D建图
    m_createMapBtn = addBtn("add", "2D建图", SLOT(onCreateMap()));
    m_createMapBtn->setToolTip("开启 2D SLAM 激光扫描建图模式");

    // 4. 重定位 (点击一次即激活，修复点两次bug)
    m_relocBtn = addBtn("target", "重定位", SLOT(onRelocalize()));
    m_relocBtn->setCheckable(true);
    m_relocBtn->setToolTip("单次点击激活：在地图上点击并拖拽设定初始位姿");

    // 5. 路径导航
    m_navModeBtn = addBtn("route", "路径导航", SLOT(onToggleNavMode()));
    m_navModeBtn->setCheckable(true);
    m_navModeBtn->setToolTip("路径导航模式：点击地图上的站点发起导航任务");

    // 6. 移动 (平动转动控制弹窗)
    m_moveBtn = addBtn("move", "移动", SLOT(onToggleMoveMode()));
    m_moveBtn->setCheckable(false);
    m_moveBtn->setToolTip("平动与转动控制弹窗");

    // 7. 取消导航 (不执行时置灰不可点击)
    m_cancelNavBtn = addBtn("cancel", "取消导航", SLOT(onCancelNavigation()));
    m_cancelNavBtn->setEnabled(false);
    m_cancelNavBtn->setToolTip("取消当前执行中的导航或移动任务");

    // 8. 暂停导航 (不执行时置灰不可点击)
    m_pauseNavBtn = addBtn("pause", "暂停导航", SLOT(onPauseNavigation()));
    m_pauseNavBtn->setEnabled(false);
    m_pauseNavBtn->setToolTip("暂停或恢复当前执行中的导航任务");

    auto *sepNav = new QFrame; sepNav->setFrameShape(QFrame::VLine);
    sepNav->setStyleSheet("color:#2d3340;"); lay->addWidget(sepNav);

    // 9. 推送地图 (原 地图上传)
    m_uploadMapBtn = addBtn("map-upload", "推送地图", SLOT(onUploadMap()));
    m_uploadMapBtn->setToolTip("将当前地图推送到设备/服务端");

    // 10. 拉取地图 (原 下载原文件)
    m_downloadMapBtn = addBtn("file-download", "拉取地图", SLOT(onDownloadSource()));
    m_downloadMapBtn->setToolTip("从设备/服务端拉取下载地图原文件 (.pcd/.smap)");

    // 11. 地图管理 (原 切换地图)
    m_switchMapBtn = addBtn("swap", "地图管理", SLOT(onSwitchMap()));
    m_switchMapBtn->setStyleSheet(kToolBtnPrimary);
    m_switchMapBtn->setToolTip("地图管理：切换、重命名或删除地图");

    // 12. 抢占控制权 (从底栏移入顶栏)
    m_teleopBtn = addBtn("teleop", "抢占控制权", SLOT(onTeleopClicked()));
    m_teleopBtn->setToolTip(
        "点击抢占控制权（暂停导航并接管键盘控制），再次点击恢复导航\n"
        "键盘: W A S D 移动 | Space 刹车 | Ctrl 加速 | P 升降上 | L 升降下");

    // 13. 状态
    m_statusBtn = addBtn("status", "状态", SLOT(onShowRobotStatus()));
    m_statusBtn->setToolTip("查看机器人运行状态与详细诊断信息");

    auto *sepEnd = new QFrame; sepEnd->setFrameShape(QFrame::VLine);
    sepEnd->setStyleSheet("color:#2d3340;"); lay->addWidget(sepEnd);

    // 辅助工具：重置视图与刷新
    m_resetViewBtn = addBtn("reset", "重置视图", SLOT(onResetView()));
    m_resetViewBtn->setToolTip("复位视图缩放与中心位置");

    m_refreshBtn = addBtn("refresh", "刷新", SLOT(onRefreshMap()));
    m_refreshBtn->setToolTip("重新刷新当前地图与路网数据");

    lay->addStretch();
}

void MapViewWidget::setupRightPanel(QWidget *parent)
{
    parent->setFixedWidth(260);
    auto *outerLay = new QVBoxLayout(parent);
    outerLay->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea(parent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { background: #12151a; width: 6px; }"
        "QScrollBar::handle:vertical { background: #2d3340; border-radius: 3px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background: #3d4758; }");

    auto *container = new QWidget;
    container->setStyleSheet("background: transparent;");
    auto *lay = new QVBoxLayout(container);
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(10);

    auto mkGroup = [](const QString &title) {
        auto *g = new QGroupBox(title);
        g->setStyleSheet(
            "QGroupBox{border:1px solid #2d3340; border-radius:4px;"
            " margin-top:14px; color:#8898a8; font-size:12px; font-weight:600;}"
            "QGroupBox::title{subcontrol-origin:margin; left:8px; top:-2px;"
            " padding:0 4px;}");
        return g;
    };

    auto mkInfoLabel = [](const QString &val = "--") {
        auto *l = new QLabel(val);
        l->setStyleSheet("color:#c8d0d8; font-size:12px;");
        return l;
    };

    // ── 1. Car info ───────────────────────────────────────────────────────────
    auto *carGroup = mkGroup("设备状态");
    auto *carForm  = new QFormLayout(carGroup);
    carForm->setSpacing(6);
    carForm->setLabelAlignment(Qt::AlignRight);

    m_carNameLabel      = mkInfoLabel();
    m_deviceStatusLabel = mkInfoLabel();
    m_batteryLabel      = mkInfoLabel();
    m_speedLabel        = mkInfoLabel();
    m_covLabel          = mkInfoLabel();
    m_posLabel          = mkInfoLabel();

    carForm->addRow("设备:", m_carNameLabel);
    carForm->addRow("状态:", m_deviceStatusLabel);
    carForm->addRow("电量:", m_batteryLabel);
    carForm->addRow("速度:", m_speedLabel);
    carForm->addRow("置信度:", m_covLabel);
    carForm->addRow("坐标:", m_posLabel);
    lay->addWidget(carGroup);

    // ── 2. Station info (对标"启用编辑"右侧站点属性) ──────────────────────────
    m_stationGroup = mkGroup("站点属性");
    auto *stationMainLay = new QVBoxLayout(m_stationGroup);
    stationMainLay->setContentsMargins(8, 12, 8, 8);
    stationMainLay->setSpacing(6);

    // 顶部标题行: 类型徽章
    auto *badgeRow = new QHBoxLayout;
    badgeRow->addStretch();
    m_stationBadge = new QLabel;
    m_stationBadge->setStyleSheet("border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;");
    m_stationBadge->setVisible(false);
    badgeRow->addWidget(m_stationBadge);
    stationMainLay->addLayout(badgeRow);

    // 默认空提示
    m_stationEmptyHint = new QLabel("点击地图上的点位\n查看站点信息");
    m_stationEmptyHint->setAlignment(Qt::AlignCenter);
    m_stationEmptyHint->setStyleSheet("color:#556677; font-size:12px; padding:14px 0;");
    stationMainLay->addWidget(m_stationEmptyHint);

    // 属性表单容器
    m_stationContentWidget = new QWidget;
    m_stationContentWidget->setStyleSheet("background:transparent; border:none;");
    auto *stForm = new QFormLayout(m_stationContentWidget);
    stForm->setContentsMargins(0, 0, 0, 0);
    stForm->setSpacing(6);
    stForm->setLabelAlignment(Qt::AlignRight);

    m_stationTypeVal   = mkInfoLabel();
    m_stationIdVal     = mkInfoLabel();
    m_stationNameVal   = mkInfoLabel();
    m_stationPosVal    = mkInfoLabel();
    m_stationYawVal    = mkInfoLabel();
    m_stationDockVal   = mkInfoLabel();
    m_stationRotateVal = mkInfoLabel();
    m_stationPgvVal    = mkInfoLabel();

    stForm->addRow("类型:", m_stationTypeVal);
    stForm->addRow("编号:", m_stationIdVal);
    stForm->addRow("名称:", m_stationNameVal);
    stForm->addRow("坐标:", m_stationPosVal);
    stForm->addRow("朝向:", m_stationYawVal);
    stForm->addRow("停靠:", m_stationDockVal);
    stForm->addRow("转身:", m_stationRotateVal);
    stForm->addRow("PGV:",  m_stationPgvVal);

    // EPC 相对定位数据卡片
    m_stationEpcGroup = new QWidget;
    m_stationEpcGroup->setStyleSheet("background:#141a26; border:1px solid #222b3c; border-radius:4px; margin-top:4px;");
    auto *epcL = new QVBoxLayout(m_stationEpcGroup);
    epcL->setContentsMargins(6, 6, 6, 6);
    epcL->setSpacing(3);
    auto *epcTitle = new QLabel("设备相对校准 (EPC)");
    epcTitle->setStyleSheet("color:#627e9e; font-size:10px; font-weight:bold; border:none;");
    epcL->addWidget(epcTitle);
    auto *epcForm = new QFormLayout;
    epcForm->setContentsMargins(0, 0, 0, 0);
    epcForm->setSpacing(3);
    epcForm->setLabelAlignment(Qt::AlignRight);
    m_stationEpcPosVal = mkInfoLabel();
    m_stationEpcYawVal = mkInfoLabel();
    epcForm->addRow("相对坐标:", m_stationEpcPosVal);
    epcForm->addRow("相对偏角:", m_stationEpcYawVal);
    epcL->addLayout(epcForm);
    stForm->addRow(m_stationEpcGroup);
    m_stationEpcGroup->setVisible(false);

    stationMainLay->addWidget(m_stationContentWidget);
    m_stationContentWidget->setVisible(false);
    lay->addWidget(m_stationGroup);

    // ── 3. Map info ──────────────────────────────────────────────────────────
    auto *infoGroup = mkGroup("地图信息");
    auto *infoForm  = new QFormLayout(infoGroup);
    infoForm->setSpacing(6);
    infoForm->setLabelAlignment(Qt::AlignRight);

    m_infoNameLabel   = mkInfoLabel();
    m_infoSizeLabel   = mkInfoLabel();
    m_infoResLabel    = mkInfoLabel();
    m_infoOriginLabel = mkInfoLabel();
    m_infoNameLabel->setWordWrap(true);

    infoForm->addRow("名称:",   m_infoNameLabel);
    infoForm->addRow("尺寸:",   m_infoSizeLabel);
    infoForm->addRow("分辨率:", m_infoResLabel);
    infoForm->addRow("原点:",   m_infoOriginLabel);
    lay->addWidget(infoGroup);

    auto *hint = new QLabel(
        "<small style='color:#556677;'>"
        "左键点击点位: 查看站点信息<br>"
        "路径导航模式: 点击点位导航<br>"
        "Alt+拖拽 / 中键: 平移<br>"
        "滚轮/触控板双指: 缩放<br>"
        "Ctrl+滚轮/触控板: 旋转"
        "</small>");
    hint->setTextFormat(Qt::RichText);
    hint->setWordWrap(true);
    lay->addWidget(hint);
    lay->addStretch();

    scrollArea->setWidget(container);
    outerLay->addWidget(scrollArea);
}

void MapViewWidget::showStationDetails(const QJsonObject &pt, const QString &type)
{
    QString id = pt.value("id").toString();
    QString name = pt.value("name").toString();
    if (name.isEmpty()) name = id;
    m_selectedStationId = id;

    // 徽章与类型展示 (对标 MapEditWidget)
    QString typeText = "普通站点 (LM)";
    QString badgeText = "LM";
    QString badgeStyle = "background:#1e3a5f; color:#60a5fa; border:1px solid #2563eb; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;";
    if (type == "charge" || type == "chargeEpc") {
        typeText = "充电桩 (CP)";
        badgeText = "CP";
        badgeStyle = "background:#4d3800; color:#fbbf24; border:1px solid #d97706; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;";
    } else if (type == "highPrecision" || type == "highPrecisionEpc") {
        typeText = "高精定位点 (TL)";
        badgeText = "TL";
        badgeStyle = "background:#4d1414; color:#f87171; border:1px solid #dc2626; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;";
    }

    if (m_stationBadge) {
        m_stationBadge->setText(badgeText);
        m_stationBadge->setStyleSheet(badgeStyle);
        m_stationBadge->setVisible(true);
    }

    if (m_stationTypeVal)   m_stationTypeVal->setText(typeText);
    if (m_stationIdVal)     m_stationIdVal->setText(id);
    if (m_stationNameVal)   m_stationNameVal->setText(name);

    double x = pt.value("x").toDouble();
    double y = pt.value("y").toDouble();
    if (m_stationPosVal) {
        m_stationPosVal->setText(QString("(%1, %2) m").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3));
    }

    double yawVal = 0.0;
    if (pt.contains("yaw")) yawVal = pt.value("yaw").toDouble();
    else if (pt.contains("robotYaw")) yawVal = pt.value("robotYaw").toDouble();
    else if (pt.contains("theta")) yawVal = pt.value("theta").toDouble() * M_PI / 180.0;
    if (m_stationYawVal) {
        m_stationYawVal->setText(QString("%1° (%2 rad)").arg(yawVal * 180.0 / M_PI, 0, 'f', 1).arg(yawVal, 0, 'f', 3));
    }

    if (m_stationDockVal) {
        m_stationDockVal->setText(pt.value("rdsDockable").toBool(true) ? "允许停靠" : "禁止停靠");
    }
    if (m_stationRotateVal) {
        m_stationRotateVal->setText(pt.value("allowRotate").toBool(true) ? "允许转身" : "禁止转身");
    }
    if (m_stationPgvVal) {
        m_stationPgvVal->setText(pt.value("useDownPGV").toBool(false) ? "使用PGV" : "不使用PGV");
    }

    // EPC 相对定位数据
    bool isDevicePoint = (type == "charge" || type == "highPrecision" || type == "chargeEpc" || type == "highPrecisionEpc");
    if (isDevicePoint && (pt.contains("epc_x") || pt.contains("epcX") || pt.contains("epc_y") || pt.contains("epcY"))) {
        double epcX = pt.contains("epc_x") ? pt.value("epc_x").toDouble() : pt.value("epcX").toDouble();
        double epcY = pt.contains("epc_y") ? pt.value("epc_y").toDouble() : pt.value("epcY").toDouble();
        double epcTheta = pt.contains("epc_theta") ? pt.value("epc_theta").toDouble() : pt.value("epcTheta").toDouble();
        if (m_stationEpcPosVal) {
            m_stationEpcPosVal->setText(QString("(%1, %2) m").arg(epcX, 0, 'f', 3).arg(epcY, 0, 'f', 3));
        }
        if (m_stationEpcYawVal) {
            m_stationEpcYawVal->setText(QString("%1° (%2 rad)").arg(epcTheta * 180.0 / M_PI, 0, 'f', 1).arg(epcTheta, 0, 'f', 3));
        }
        if (m_stationEpcGroup) m_stationEpcGroup->setVisible(true);
    } else {
        if (m_stationEpcGroup) m_stationEpcGroup->setVisible(false);
    }

    if (m_stationEmptyHint)     m_stationEmptyHint->setVisible(false);
    if (m_stationContentWidget) m_stationContentWidget->setVisible(true);

    // 在地图上绘制翠绿色高亮选中光环
    QPointF sp = worldToScene(x, y);
    if (!m_selectedPointRing && m_scene) {
        m_selectedPointRing = new QGraphicsEllipseItem;
        m_selectedPointRing->setPen(QPen(QColor("#2ecc71"), 2.0));
        m_selectedPointRing->setBrush(QBrush(QColor(46, 204, 113, 70)));
        m_selectedPointRing->setZValue(25);
        m_scene->addItem(m_selectedPointRing);
    }
    if (m_selectedPointRing) {
        m_selectedPointRing->setRect(sp.x() - 14, sp.y() - 14, 28, 28);
        m_selectedPointRing->setVisible(true);
    }
}

void MapViewWidget::clearStationDetails()
{
    m_selectedStationId.clear();
    if (m_stationBadge) {
        m_stationBadge->setText("");
        m_stationBadge->setVisible(false);
    }
    if (m_stationContentWidget) m_stationContentWidget->setVisible(false);
    if (m_stationEmptyHint)     m_stationEmptyHint->setVisible(true);
    if (m_selectedPointRing)    m_selectedPointRing->setVisible(false);
}

// ── Show / hide ──────────────────────────────────────────────────────────────

void MapViewWidget::refreshData()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    // 节流保护：500ms 内避免重复全量刷新
    if (now - m_lastRefreshMs < 500) {
        return;
    }
    m_lastRefreshMs = now;

    // 每次页面展示（菜单切换/从下级编辑页返回/首次打开）全量刷新设备数据与地图/路网
    loadCurrentCarMap();
    initMqtt();
}

void MapViewWidget::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    refreshData();
}

void MapViewWidget::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);
    // 保持 MQTT 全局持续监听，不再在隐藏时断开订阅
}

// ── Data loading ─────────────────────────────────────────────────────────────

void MapViewWidget::loadCurrentCarMap()
{
    if (m_isLoadingCar) {
        return;
    }
    m_isLoadingCar = true;

    setStatus("正在获取设备信息...");
    QString carId = AppConfig::instance().agvId();
    if (carId.isEmpty()) carId = "1";

    ApiService::instance().getCar(carId, [this, carId](int code, const QJsonObject &data) {
        m_isLoadingCar = false;
        if (code != 200) { setStatus("获取设备信息失败", "#e05555"); return; }
        QJsonObject car = data.value("data").toObject(data);

        // Populate right panel car name
        QString carName = car.value("carName").toString().trimmed();
        if (carName.isEmpty() || carName == "--") {
            carName = QString("AGV-%1").arg(carId);
        }
        m_robot.carName = carName;
        m_carNameLabel->setText(carName);
        updateInfoPanel();

        QString mapId = jsonIdStr(car.value("mapId"));
        if (mapId.isEmpty() || mapId == "0") {
            mapId = jsonIdStr(car.value("map_id"));
        }
        if (mapId.isEmpty() || mapId == "0") {
            mapId = jsonIdStr(data.value("mapId"));
        }
        if (mapId.isEmpty() || mapId == "0") {
            ApiService::instance().listMap(QJsonObject{{"pageNum",1},{"pageSize",1}},
                [this](int c2, const QJsonObject &d2) {
                    if (c2 == 200) {
                        QJsonArray rows = d2.value("rows").toArray();
                        if (!rows.isEmpty())
                            loadMapById(jsonIdStr(rows.first().toObject().value("mapId")));
                        else
                            setStatus("无可用地图", "#e05555");
                    }
                });
            return;
        }
        loadMapById(mapId);
    });
}

bool MapViewWidget::findLocalMapFiles(const QString &dirPath, QString &outYamlPath, QString &outPgmPath,
                                     const QString &preferredMapName, const QString &preferredYamlName, const QString &preferredPgmName)
{
    QDir dir(dirPath);
    if (!dir.exists()) return false;

    // 递归检索目录及子目录中的 .yaml/.yml 和 .pgm 文件
    QDirIterator yamlIt(dirPath, QStringList() << "*.yaml" << "*.yml", QDir::Files, QDirIterator::Subdirectories);
    QStringList yamlCandidates;
    while (yamlIt.hasNext()) {
        yamlCandidates << yamlIt.next();
    }

    QDirIterator pgmIt(dirPath, QStringList() << "*.pgm", QDir::Files, QDirIterator::Subdirectories);
    QStringList pgmCandidates;
    while (pgmIt.hasNext()) {
        pgmCandidates << pgmIt.next();
    }

    if (yamlCandidates.isEmpty() || pgmCandidates.isEmpty()) {
        return false;
    }

    // 1. 确定最匹配的 YAML 文件
    outYamlPath.clear();
    // 规则 A: 精确匹配 preferredYamlName
    if (!preferredYamlName.isEmpty()) {
        for (const QString &f : yamlCandidates) {
            QString fn = QFileInfo(f).fileName();
            if (fn.compare(preferredYamlName, Qt::CaseInsensitive) == 0 ||
                f.endsWith("/" + preferredYamlName, Qt::CaseInsensitive) ||
                f.endsWith("\\" + preferredYamlName, Qt::CaseInsensitive)) {
                outYamlPath = f;
                break;
            }
        }
    }
    // 规则 B: 包含 preferredMapName
    if (outYamlPath.isEmpty() && !preferredMapName.isEmpty()) {
        for (const QString &f : yamlCandidates) {
            if (QFileInfo(f).fileName().contains(preferredMapName, Qt::CaseInsensitive)) {
                outYamlPath = f;
                break;
            }
        }
    }
    // 规则 C: 筛选标准地图文件，按最后修改时间降序（最新优先）或版本号/时间戳数字降序
    if (outYamlPath.isEmpty()) {
        QStringList standardYamls;
        for (const QString &f : yamlCandidates) {
            if (f.endsWith(".map.yaml", Qt::CaseInsensitive) || f.contains("carto_map", Qt::CaseInsensitive)) {
                standardYamls << f;
            }
        }
        if (standardYamls.isEmpty()) standardYamls = yamlCandidates;

        std::sort(standardYamls.begin(), standardYamls.end(), [](const QString &a, const QString &b) {
            QFileInfo fa(a), fb(b);
            if (fa.lastModified() != fb.lastModified()) {
                return fa.lastModified() > fb.lastModified();
            }
            return fa.fileName() > fb.fileName(); // 字母/时间戳降序，如 20260819 > 20260803
        });
        outYamlPath = standardYamls.first();
    }

    // 2. 从 YAML 文件内提取 image 属性，严格绑定对应的 PGM 文件
    outPgmPath.clear();
    QFile yf(outYamlPath);
    if (yf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString yamlText = QString::fromUtf8(yf.readAll());
        yf.close();
        double r = 0, ox = 0, oy = 0; bool neg = false;
        QString imageFile;
        if (MapPgmUtil::parseMapYaml(yamlText, r, ox, oy, neg, &imageFile) && !imageFile.isEmpty()) {
            QFileInfo yfi(outYamlPath);
            QString candidatePgm = QFileInfo(yfi.dir(), imageFile).absoluteFilePath();
            if (QFile::exists(candidatePgm) && QFileInfo(candidatePgm).size() > 0) {
                outPgmPath = candidatePgm;
            }
        }
    }

    // 3. 若 YAML 内未声明有效 image 路径，使用名称对应规则与时间降序回退
    if (outPgmPath.isEmpty()) {
        if (!preferredPgmName.isEmpty()) {
            for (const QString &f : pgmCandidates) {
                if (QFileInfo(f).fileName().compare(preferredPgmName, Qt::CaseInsensitive) == 0) {
                    outPgmPath = f;
                    break;
                }
            }
        }
        if (outPgmPath.isEmpty()) {
            QFileInfo yfi(outYamlPath);
            QString yamlBase = yfi.completeBaseName();
            if (yamlBase.endsWith(".map", Qt::CaseInsensitive)) yamlBase.chop(4);
            for (const QString &f : pgmCandidates) {
                if (QFileInfo(f).fileName().contains(yamlBase, Qt::CaseInsensitive)) {
                    outPgmPath = f;
                    break;
                }
            }
        }
        if (outPgmPath.isEmpty()) {
            std::sort(pgmCandidates.begin(), pgmCandidates.end(), [](const QString &a, const QString &b) {
                return QFileInfo(a).lastModified() > QFileInfo(b).lastModified();
            });
            outPgmPath = pgmCandidates.first();
        }
    }

    return (!outYamlPath.isEmpty() && !outPgmPath.isEmpty() &&
            QFileInfo(outYamlPath).size() > 0 && QFileInfo(outPgmPath).size() > 0);
}

bool MapViewWidget::loadMapFromLocalFiles(const QString &yamlPath, const QString &pgmPath)
{
    QFile yf(yamlPath);
    if (!yf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[MapViewWidget] Failed to open yaml file:" << yamlPath;
        return false;
    }
    QString yamlText = QString::fromUtf8(yf.readAll());
    yf.close();

    double resolution = 0, originX = 0, originY = 0;
    bool negate = false;
    QString imageRelPath;
    if (!MapPgmUtil::parseMapYaml(yamlText, resolution, originX, originY, negate, &imageRelPath) || resolution <= 0) {
        qWarning() << "[MapViewWidget] Failed to parse yaml file:" << yamlPath;
        return false;
    }

    // 严格以 YAML 中的 image 字段优先定位 PGM
    QString actualPgmPath = pgmPath;
    if (!imageRelPath.isEmpty()) {
        QFileInfo yfi(yamlPath);
        QString resolvedPath = QFileInfo(yfi.dir(), imageRelPath).absoluteFilePath();
        if (QFile::exists(resolvedPath) && QFileInfo(resolvedPath).size() > 0) {
            actualPgmPath = resolvedPath;
        }
    }

    QFile pf(actualPgmPath);
    if (!pf.open(QIODevice::ReadOnly)) {
        qWarning() << "[MapViewWidget] Failed to open pgm file:" << actualPgmPath;
        return false;
    }
    QByteArray pgmData = pf.readAll();
    pf.close();

    QImage img = MapPgmUtil::pgmToImage(pgmData, negate);
    if (img.isNull()) {
        qWarning() << "[MapViewWidget] Failed to decode pgm image:" << actualPgmPath;
        return false;
    }

    QPixmap pix = QPixmap::fromImage(img);
    m_worldL = originX;
    m_worldB = originY;
    m_worldR = m_worldL + pix.width()  * resolution;
    m_worldT = m_worldB + pix.height() * resolution;

    qDebug().noquote() << QString("[MapViewWidget] Map loaded from YAML: %1 | PGM: %2 | Res: %3 | Origin: (%4, %5) | WorldBounds: [%6, %7, %8, %9]")
                              .arg(yamlPath, actualPgmPath)
                              .arg(resolution)
                              .arg(originX).arg(originY)
                              .arg(m_worldL).arg(m_worldB).arg(m_worldR).arg(m_worldT);

    installMapPixmap(pix);
    loadRoute();
    return true;
}

void MapViewWidget::loadMapById(const QString &mapId, const QString &mapName)
{
    if (mapId.isEmpty()) return;
    // 避免同一地图在正在请求中时被并发重复触发
    if (m_loadingMapId == mapId) {
        return;
    }
    m_loadingMapId = mapId;
    m_mapId = mapId;
    if (!mapName.isEmpty()) {
        m_mapName = mapName;
        if (m_mapNameLabel) m_mapNameLabel->setText(m_mapName);
    }

    setStatus(QString("正在加载地图 %1...").arg(mapId));

    QString localDir = QCoreApplication::applicationDirPath() + "/maps/" + mapId;

    // 先尝试从本地 meta.json 提取权威绑定的元信息
    QString metaMapName = mapName;
    QString metaYamlName;
    QString metaPgmName;
    QFile metaFile(localDir + "/meta.json");
    if (metaFile.open(QIODevice::ReadOnly)) {
        QJsonObject metaObj = QJsonDocument::fromJson(metaFile.readAll()).object();
        metaFile.close();
        if (metaMapName.isEmpty()) metaMapName = metaObj.value("mapName").toString();
        metaYamlName = metaObj.value("yamlName").toString();
        metaPgmName  = metaObj.value("fileName").toString();
    }

    auto doLoadOrDownload = [this, mapId, localDir](const QString &targetMapName, const QString &targetYamlName, const QString &targetPgmName) {
        QString yamlPath, pgmPath;

        // 1. 优先检查本地持久化缓存，若存在则精准筛选并秒级加载
        if (findLocalMapFiles(localDir, yamlPath, pgmPath, targetMapName, targetYamlName, targetPgmName)) {
            qDebug().noquote() << QString("[MapCache] Hit local cache for map %1 in %2: yaml=%3, pgm=%4")
                                      .arg(mapId, localDir, yamlPath, pgmPath);

            QFileInfo yfi(yamlPath);
            QString base = yfi.completeBaseName();
            if (base.endsWith(".map", Qt::CaseInsensitive)) base.chop(4);
            m_mapName = !targetMapName.isEmpty() ? targetMapName : base;
            if (m_mapNameLabel) m_mapNameLabel->setText(m_mapName);

            if (loadMapFromLocalFiles(yamlPath, pgmPath)) {
                m_loadingMapId.clear();
                setStatus(QString("地图 %1 加载完成 (本地缓存)").arg(m_mapName.isEmpty() ? mapId : m_mapName));
                return;
            }
            qWarning() << "[MapCache] Local cache files corrupted, falling back to download...";
        }

        // 2. 本地未缓存或损坏：调用新接口 /robot/map/download/resources/{mapid} 下载 zip 资源包
        setStatus(QString("正在下载地图资源包 (ID: %1)...").arg(mapId));
        ApiService::instance().downloadMapResources(mapId, [this, mapId, localDir, targetMapName, targetYamlName, targetPgmName](bool ok, const QByteArray &zipData) {
            if (!ok || zipData.isEmpty()) {
                if (m_loadingMapId == mapId) {
                    m_loadingMapId.clear();
                }
                setStatus("下载地图资源包失败", "#e05555");
                Toast::error(this, "下载地图资源包失败");
                return;
            }

            // 保存 zip 文件到本地缓存目录
            QDir().mkpath(localDir);
            QString zipPath = localDir + "/" + mapId + ".zip";
            QFile zf(zipPath);
            if (!zf.open(QIODevice::WriteOnly)) {
                if (m_loadingMapId == mapId) {
                    m_loadingMapId.clear();
                }
                setStatus("保存地图资源包失败", "#e05555");
                Toast::error(this, "保存地图资源包失败");
                return;
            }
            zf.write(zipData);
            zf.close();

            // 自动解压 zip 文件
            setStatus("正在解压地图资源...");
            if (!ZipUtil::extractZip(zipPath, localDir)) {
                if (m_loadingMapId == mapId) {
                    m_loadingMapId.clear();
                }
                setStatus("地图资源包解压失败", "#e05555");
                Toast::error(this, "解压地图资源包失败");
                return;
            }

            // 查找解压出的 pgm 与 yaml
            QString yPath, pPath;
            if (!findLocalMapFiles(localDir, yPath, pPath, targetMapName, targetYamlName, targetPgmName)) {
                if (m_loadingMapId == mapId) {
                    m_loadingMapId.clear();
                }
                setStatus("未找到有效地图文件", "#e05555");
                Toast::error(this, "地图资源包中缺少 pgm 或 yaml 文件");
                return;
            }

            QFileInfo yfi(yPath);
            QString base = yfi.completeBaseName();
            if (base.endsWith(".map", Qt::CaseInsensitive)) base.chop(4);
            m_mapName = !targetMapName.isEmpty() ? targetMapName : base;
            if (m_mapNameLabel) m_mapNameLabel->setText(m_mapName);

            // 存储 meta.json 锁定当前解析映射
            QFile metaFile(localDir + "/meta.json");
            if (metaFile.open(QIODevice::WriteOnly)) {
                QJsonObject metaObj;
                metaObj["mapId"] = mapId;
                metaObj["mapName"] = m_mapName;
                metaObj["yamlName"] = QFileInfo(yPath).fileName();
                metaObj["fileName"] = QFileInfo(pPath).fileName();
                metaFile.write(QJsonDocument(metaObj).toJson());
                metaFile.close();
            }

            if (loadMapFromLocalFiles(yPath, pPath)) {
                if (m_loadingMapId == mapId) {
                    m_loadingMapId.clear();
                }
                setStatus(QString("地图 %1 下载并加载完成").arg(m_mapName.isEmpty() ? mapId : m_mapName));
                Toast::success(this, "地图资源下载并加载成功");
            } else {
                if (m_loadingMapId == mapId) {
                    m_loadingMapId.clear();
                }
                setStatus("解析地图数据失败", "#e05555");
                Toast::error(this, "解析地图数据失败");
            }
        });
    };

    // 如果 meta 缺少明确的 yamlName，向后端 API 查询一次以获得权威绑定的文件名称
    if (metaYamlName.isEmpty() || metaMapName.isEmpty()) {
        ApiService::instance().getMap(mapId, [this, metaMapName, doLoadOrDownload](int code, const QJsonObject &data) {
            QString apiMapName = metaMapName;
            QString apiYamlName;
            QString apiPgmName;
            if (code == 200) {
                QJsonObject d = data.value("data").toObject(data);
                if (apiMapName.isEmpty()) apiMapName = d.value("mapName").toString();
                apiYamlName = d.value("yamlName").toString();
                apiPgmName  = d.value("fileName").toString();
            }
            doLoadOrDownload(apiMapName, apiYamlName, apiPgmName);
        });
    } else {
        doLoadOrDownload(metaMapName, metaYamlName, metaPgmName);
    }
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
    // 与 Vue edit.vue/show.vue 完全一致：永远用 -offsetX/-offsetY 计算零点，
    // 不读取/优先采用任何 originX 字段（Vue 的 getMap 流程里从不存在该字段，
    // 之前误信任它会在 API 偶然带有该字段时取到错误的零点）。
    auto tryOrigin = [&](const QStringList &negate) -> double {
        for (const QString &k : negate) {
            QJsonValue v = mapInfo.value(k);
            if (!v.isUndefined() && !v.isNull()) return -v.toDouble();
        }
        return 0.0;
    };
    double apiOriginX = tryOrigin({"offsetX","mapOffsetX","offset_x"});
    double apiOriginY = tryOrigin({"offsetY","mapOffsetY","offset_y"});

    if (m_mapNameLabel) m_mapNameLabel->setText(m_mapName);
    m_mapReady = false;

    loadMapFromPgm(apiOriginX, apiOriginY, physW, physH);
    loadRoute();
}

// 主路径：直接拉取并解析原始 carto_map.pgm + carto_map.yaml（与 MapEditWidget 一致）。
// 后端生成瓦片金字塔时会把图片 padding 成 512*2^n 的正方形，getMap() 返回的
// mapWidth/mapHeight/offsetX/offsetY 是基于 padding 后画布派生的，并不是地图的真实物理
// 尺寸与零点（见 PcdToImgImpl.pgmToImg）。yaml 的 resolution/origin 才是 ROS map_server
// 标准、无歧义的定义，pgm 本身也未经 padding，可彻底避免这个偏差。
void MapViewWidget::loadMapFromPgm(double apiOriginX, double apiOriginY, double apiPhysW, double apiPhysH)
{
    if (m_mapName.isEmpty() || m_mapId.isEmpty()) {
        setStatus("无地图名称或ID", "#f0a060"); return;
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
            // 下载并保存 yaml 到本地缓存目录
            QString localDir = QCoreApplication::applicationDirPath() + "/maps/" + m_mapName + "_" + m_mapId;
            QDir().mkpath(localDir);
            QFile yf(localDir + "/carto_map.yaml");
            if (yf.open(QIODevice::WriteOnly)) {
                yf.write(yamlRaw);
                yf.close();
            }

            ApiService::instance().downloadFile(pgmUrl,
                [this, resolution, originX, originY, negate, useThumbnailFallback, localDir]
                (bool pgmOk, const QByteArray &pgmRaw) {
                    if (!pgmOk) { useThumbnailFallback(); return; }
                    QImage img = MapPgmUtil::pgmToImage(pgmRaw, negate);
                    if (img.isNull()) { useThumbnailFallback(); return; }

                    // 下载并保存 pgm 到本地缓存目录
                    QFile pf(localDir + "/carto_map.pgm");
                    if (pf.open(QIODevice::WriteOnly)) {
                        pf.write(pgmRaw);
                        pf.close();
                    }

                    QPixmap pix = QPixmap::fromImage(img);
                    // ROS map_server 标准：yaml 的 origin 本身就是 pgm 左下角像素对应的
                    // 世界坐标，不需要再取负——那是 API 的 offsetX/Y 字段才需要的换算。
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
        setStatus("无地图名称或ID", "#f0a060"); return;
    }

    // URL: mapHost + mapName_mapId / tilemapPath / thumbnail.png
    // tilemapPath comes from the API response (e.g. "tilemap"); fallback to "tilemap"
    QString url = AppConfig::instance().mapHost()
                  + m_mapName + "_" + m_mapId + "/"
                  + m_tilemapPath + "/thumbnail.png";

    ApiService::instance().downloadFile(url, [this](bool ok, const QByteArray &imgData) {
        if (!ok || imgData.isEmpty()) { setStatus("地图图像下载失败", "#e05555"); return; }

        QString localDir = QCoreApplication::applicationDirPath() + "/maps/" + m_mapName + "_" + m_mapId;
        QDir().mkpath(localDir);
        QFile f(localDir + "/thumbnail.png");
        if (f.open(QIODevice::WriteOnly)) {
            f.write(imgData);
            f.close();
        }

        QPixmap pix;
        if (!pix.loadFromData(imgData)) { setStatus("地图图像解析失败", "#e05555"); return; }
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
    bgItem->setTransformationMode(Qt::SmoothTransformation);
    bgItem->setZValue(-100);
    m_mapBg = bgItem;

    m_scene->setSceneRect(0, 0, m_imgW, m_imgH);
    // 最短边铺满、长边居中裁切 ("cover" 而非 "contain")
    m_canvas->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatioByExpanding);

    renderAll();
    updateRulers();
    double resX = (m_worldR - m_worldL) / m_imgW;
    m_infoNameLabel->setText(m_mapName);
    m_infoSizeLabel->setText(QString("%1×%2px").arg((int)m_imgW).arg((int)m_imgH));
    m_infoResLabel->setText(QString("%1 m/px").arg(resX, 0, 'f', 4));
    m_infoOriginLabel->setText(QString("(%1, %2)m").arg(m_worldL, 0, 'f', 2).arg(m_worldB, 0, 'f', 2));
    setStatus("地图加载完成");
}

void MapViewWidget::loadRoute()
{
    if (m_mapId.isEmpty()) return;
    ApiService::instance().loadEditedMap([this](int code, const QJsonObject &data) {
        m_routeLoaded = (code == 200);
        if (!m_routeLoaded) return;

        m_points = QJsonArray();
        m_lines  = QJsonArray();
        m_curves = QJsonArray();
        m_charges = QJsonArray();
        m_highPrecisionPoints = QJsonArray();

        QString jsonStr = data.value("json").toString();
        if (!jsonStr.isEmpty()) {
            QString localDir = QCoreApplication::applicationDirPath() + "/maps/" + m_mapName + "_" + m_mapId;
            QDir().mkpath(localDir);
            QFile rf(localDir + "/route.json");
            if (rf.open(QIODevice::WriteOnly)) {
                rf.write(jsonStr.toUtf8());
                rf.close();
            }

            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
            if (doc.isObject()) {
                QJsonArray features = doc.object().value("features").toArray();
                for (const auto &fv : features) {
                    QJsonObject f = fv.toObject();
                    QJsonObject props = f.value("properties").toObject();
                    QJsonObject geom  = f.value("geometry").toObject();
                    QString gtype = geom.value("type").toString();
                    QString id    = props.value("id").toString();
                    QJsonArray coords = geom.value("coordinates").toArray();

                    if (gtype == "Point") {
                        QJsonObject pt;
                        pt["id"]   = id;
                        pt["x"]    = coords[0].toDouble();
                        pt["y"]    = coords[1].toDouble();
                        pt["name"] = props.value("name").toString();
                        // 充电桩/高精度点: 保留所有 properties(epc_x/epc_y/epc_theta/yaw/yawFix/theta 等)
                        if (id.startsWith("c_") || id.startsWith("hp_")) {
                            for (auto it = props.begin(); it != props.end(); ++it) {
                                const QString &k = it.key();
                                if (k == "id" || k == "name") continue;
                                pt[k] = it.value();
                            }
                        }
                        if (id.startsWith("c_"))
                            m_charges.append(pt);
                        else if (id.startsWith("hp_"))
                            m_highPrecisionPoints.append(pt);
                        else
                            m_points.append(pt);
                    } else if (gtype == "LineString") {
                        // 路网线段: id 以 l_ 开头
                        QJsonObject ln;
                        ln["id"]   = id;
                        if (coords.size() >= 2) {
                            QJsonArray c0 = coords[0].toArray();
                            QJsonArray c1 = coords[1].toArray();
                            ln["x1"] = c0[0].toDouble(); ln["y1"] = c0[1].toDouble();
                            ln["x2"] = c1[0].toDouble(); ln["y2"] = c1[1].toDouble();
                        }
                        ln["name"] = props.value("name").toString();
                        m_lines.append(ln);
                    } else if (gtype == "MultiPoint") {
                        // 多阶贝塞尔曲线(>=3 个点): id 以 b_ 开头
                        QJsonObject cv;
                        cv["id"]   = id;
                        cv["name"] = props.value("name").toString();
                        QJsonArray ctrlArr;
                        for (const auto &c : coords) {
                            QJsonArray ca = c.toArray();
                            if (ca.size() < 2) continue;
                            QJsonObject p; p["x"] = ca[0].toDouble(); p["y"] = ca[1].toDouble();
                            ctrlArr.append(p);
                        }
                        cv["controlPoints"] = ctrlArr;
                        if (coords.size() >= 2) {
                            QJsonArray a0 = coords[0].toArray(), aN = coords[coords.size()-1].toArray();
                            cv["x1"] = a0[0].toDouble(); cv["y1"] = a0[1].toDouble();
                            cv["x2"] = aN[0].toDouble(); cv["y2"] = aN[1].toDouble();
                        }
                        if (coords.size() == 3) {
                            QJsonArray a1 = coords[1].toArray();
                            cv["cx"] = a1[0].toDouble(); cv["cy"] = a1[1].toDouble();
                        }
                        m_curves.append(cv);
                    }
                }
            }
        }
        if (m_mapReady) renderRoute();
    });
}

// ── Rendering ─────────────────────────────────────────────────────────────────

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

    // ── Grid (only when toggled on) ───────────────────────────────────────
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
    if (m_selectedPointRing) {
        m_scene->removeItem(m_selectedPointRing);
        delete m_selectedPointRing;
        m_selectedPointRing = nullptr;
    }

    for (auto *item : m_scene->items()) {
        if (item->data(0).toInt() == 2) {
            m_scene->removeItem(item);
            delete item;
        }
    }

    if (m_points.isEmpty() && m_lines.isEmpty() && m_curves.isEmpty()
        && m_charges.isEmpty() && m_highPrecisionPoints.isEmpty()) return;

    auto addRoute = [](QGraphicsItem *item) {
        item->setData(0, 2);
        item->setZValue(20);
    };

    QPen linePen(QColor("#1296DB"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    for (const auto &v : m_lines) {
        QJsonObject ln = v.toObject();
        QPointF a = worldToScene(ln.value("x1").toDouble(), ln.value("y1").toDouble());
        QPointF b = worldToScene(ln.value("x2").toDouble(), ln.value("y2").toDouble());
        int direct = ln.value("direct").toInt(10);
        auto *lineItem = m_scene->addLine(a.x(), a.y(), b.x(), b.y(), linePen);
        addRoute(lineItem);

        // 线路导向标 (单向或双向)
        QPointF dir = b - a;
        double len = std::hypot(dir.x(), dir.y());
        if (len > 1e-3) {
            dir /= len;
            QPointF mid = (a + b) / 2.0;
            QList<QPolygonF> arrows = MapPgmUtil::createDirectionArrows(mid, dir, direct, len);
            for (const auto &poly : arrows) {
                auto *arrItem = m_scene->addPolygon(poly, QPen(Qt::NoPen), QBrush(linePen.color()));
                arrItem->setParentItem(lineItem);
                arrItem->setZValue(6);
                arrItem->setAcceptedMouseButtons(Qt::NoButton);
                addRoute(arrItem);
            }
        }
    }

    QPen curvePen(QColor("#9B59B6"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    for (const auto &v : m_curves) {
        QJsonObject cv = v.toObject();
        int direct = cv.value("direct").toInt(10);
        QPainterPath path;
        QList<QPointF> pts;
        QJsonArray ctrlArr = cv.value("controlPoints").toArray();
        if (ctrlArr.size() >= 2) {
            // 多阶贝塞尔曲线
            for (const auto &pv : ctrlArr)
                pts.append(worldToScene(pv.toObject().value("x").toDouble(),
                                        pv.toObject().value("y").toDouble()));
            path = MapPgmUtil::getBezierPath(pts);
        } else {
            // 兼容旧格式: cx/cy 二次贝塞尔
            QPointF p1 = worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble());
            QPointF pc = worldToScene(cv.value("cx").toDouble(),  cv.value("cy").toDouble());
            QPointF p2 = worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble());
            pts.append(p1); pts.append(pc); pts.append(p2);
            path.moveTo(p1); path.quadTo(pc, p2);
        }
        auto *pathItem = m_scene->addPath(path, curvePen);
        addRoute(pathItem);

        if (pts.size() >= 2) {
            double cLen = std::hypot(pts.last().x() - pts.first().x(),
                                     pts.last().y() - pts.first().y());
            QList<QPolygonF> arrows = MapPgmUtil::createBezierDirectionArrows(pts, direct, cLen);
            for (const auto &poly : arrows) {
                auto *arrItem = m_scene->addPolygon(poly, QPen(Qt::NoPen), QBrush(curvePen.color()));
                arrItem->setParentItem(pathItem);
                arrItem->setZValue(6);
                arrItem->setAcceptedMouseButtons(Qt::NoButton);
                addRoute(arrItem);
            }
        }
    }

    double pxPerMeterX = (m_imgW > 0 && (m_worldR > m_worldL)) ? (m_imgW / (m_worldR - m_worldL)) : 20.0;
    double pxPerMeterY = (m_imgH > 0 && (m_worldT > m_worldB)) ? (m_imgH / (m_worldT - m_worldB)) : 20.0;

    auto addPointData = [](QGraphicsItem *item, const QString &id, const QString &type, const QString &name, const QPointF &wPos) {
        item->setData(0, 2);                      // Route layer
        item->setData(1, id);                     // Point ID
        item->setData(2, type);                   // Point type ("point", "charge", "highPrecision")
        item->setData(3, name);                   // Point Name
        item->setData(4, wPos);                   // World position
        item->setZValue(20);
    };

    // 路网站点渲染 (普通站点 / 充电点 / 精定位点)
    auto addStationNode = [&](const QJsonObject &pt, const QColor &fillCol, const QColor &borderCol, const QString &type) {
        QPointF sp = worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble());
        QString id = pt.value("id").toString();
        QString name = pt.value("name").toString();
        if (name.isEmpty()) name = id;

        double rectW = 0, rectH = 0;
        QPainterPath crossPath;
        QPainterPath path = MapPgmUtil::createPointPath(pxPerMeterX, pxPerMeterY, &rectW, &rectH, &crossPath, 0.0);

        QFont f("Segoe UI");
        double ptSize = qBound(4.0, rectH * 0.35, 7.0);
        f.setPointSizeF(ptSize);
        f.setBold(true);
        QFontMetrics fm(f);
        int tw = fm.horizontalAdvance(name);

        auto *item = m_scene->addPath(path, QPen(borderCol, 0.8), QBrush(fillCol));
        item->setPos(sp);

        QPointF wPos(pt.value("x").toDouble(), pt.value("y").toDouble());
        addPointData(item, id, type, name, wPos);

        // 内部红色实线精确定位小十字 (居中位于 (0,0)，与路线精准交汇，不遮挡文字)
        QPen crossPen(QColor("#E74C3C"), 0.8, Qt::SolidLine);
        crossPen.setCapStyle(Qt::FlatCap);
        auto *crossItem = m_scene->addPath(crossPath, crossPen);
        crossItem->setParentItem(item);
        crossItem->setPos(0, 0);
        crossItem->setAcceptedMouseButtons(Qt::NoButton);
        crossItem->setFlag(QGraphicsItem::ItemIsSelectable, false);

        // 若配置了车头朝向角，绘制微型方向指针
        double yawVal = 0.0;
        if (pt.contains("yaw")) yawVal = pt.value("yaw").toDouble();
        else if (pt.contains("robotYaw")) yawVal = pt.value("robotYaw").toDouble();
        else if (pt.contains("theta")) yawVal = pt.value("theta").toDouble() * M_PI / 180.0;
        if (yawVal != 0.0) {
            double angleDeg = 90.0 - yawVal * 180.0 / M_PI;
            double rad = qDegreesToRadians(angleDeg);
            double pr = rectW / 2.0 + 2.5;
            QPointF pHead(std::sin(rad) * pr, -std::cos(rad) * pr);
            QPolygonF ptr;
            QPointF norm(-std::cos(rad) * 2.0, -std::sin(rad) * 2.0);
            ptr << pHead + QPointF(std::sin(rad) * 3.5, -std::cos(rad) * 3.5)
                << pHead + norm
                << pHead - norm;
            auto *pItem = m_scene->addPolygon(ptr, QPen(Qt::NoPen), QBrush(borderCol));
            pItem->setParentItem(item);
            pItem->setAcceptedMouseButtons(Qt::NoButton);
            pItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
        }

        // 站点名称：直接绘制在矩形框下部，水平居中，与上方红色十字完全分离无遮挡，深色高对比度，尺寸紧凑工整
        auto *lbl = m_scene->addText(name, f);
        lbl->document()->setDocumentMargin(0);
        lbl->setDefaultTextColor(QColor("#1A1A1A"));
        lbl->setParentItem(item);
        double textY = (rectH >= 12.0) ? (rectH * 0.16) : (rectH / 2.0 + 1.0);
        lbl->setPos(-tw / 2.0, textY);
        lbl->setAcceptedMouseButtons(Qt::NoButton);
        lbl->setFlag(QGraphicsItem::ItemIsSelectable, false);
        addRoute(lbl);
    };

    for (const auto &v : m_points)              addStationNode(v.toObject(), QColor(210, 230, 250, 235), QColor(41, 128, 185, 240), "point");
    for (const auto &v : m_charges)             addStationNode(v.toObject(), QColor(254, 243, 199, 235), QColor(217, 119, 6, 240), "charge");
    for (const auto &v : m_highPrecisionPoints) addStationNode(v.toObject(), QColor(254, 226, 226, 235), QColor(220, 38, 38, 240), "highPrecision");
}

void MapViewWidget::renderRobot()
{
    if (!m_mapReady || !m_robot.posValid) return;

    QPointF sp = worldToScene(m_robot.wx, m_robot.wy);

    // ENU: yaw=0→East, yaw=π/2→North, CCW positive. Icon/triangle tip points North.
    // Qt rotate(a) is CW; tip (0,-10) after rotate(a) points (sin a, -cos a).
    // For ENU direction (cos yaw, -sin yaw): solve → a = 90 - yaw. So screenAngle = 90 - yawDeg.
    double yawRad      = (qAbs(m_robot.yawRad) > M_PI * 2)
                         ? m_robot.yawRad * M_PI / 180.0 : m_robot.yawRad;
    double yawDeg      = yawRad * 180.0 / M_PI;
    double screenAngle = 90.0 - yawDeg;

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

    if (!m_robotPixmap.isNull()) {
        const double srcW   = m_robotPixmap.width();
        const double srcH   = m_robotPixmap.height();
        const double pivotX = srcW / 2.0;
        const double pivotY = srcH / 2.0;
        const double scale  = (srcW > 0) ? (sceneW / srcW) : 1.0;

        auto *pixItem = dynamic_cast<QGraphicsPixmapItem *>(m_robotItem);
        if (pixItem) {
            pixItem->setScale(scale);
            pixItem->setTransformOriginPoint(pivotX, pivotY);
            pixItem->setPos(sp.x() - pivotX, sp.y() - pivotY);
            pixItem->setRotation(screenAngle);
        } else {
            if (m_robotItem) {
                m_scene->removeItem(m_robotItem);
                delete m_robotItem;
                m_robotItem = nullptr;
            }
            auto *robotItem = m_scene->addPixmap(m_robotPixmap);
            robotItem->setScale(scale);
            robotItem->setTransformOriginPoint(pivotX, pivotY);
            robotItem->setPos(sp.x() - pivotX, sp.y() - pivotY);
            robotItem->setRotation(screenAngle);
            robotItem->setData(0, 3);
            robotItem->setZValue(120);
            m_robotItem = robotItem;
        }
    } else {
        // Fallback: draw 110*60 rectangle with light-blue head
        if (m_robotItem) {
            m_scene->removeItem(m_robotItem);
            delete m_robotItem;
            m_robotItem = nullptr;
        }
        double halfW = sceneW / 2.0;
        double halfH = sceneH / 2.0;
        QPolygonF rectPoly;
        rectPoly << QPointF(-halfW, -halfH)
                 << QPointF(halfW, -halfH)
                 << QPointF(halfW, halfH)
                 << QPointF(-halfW, halfH);
        QTransform t;
        t.translate(sp.x(), sp.y());
        t.rotate(screenAngle);
        rectPoly = t.map(rectPoly);
        auto *marker = m_scene->addPolygon(rectPoly,
            QPen(QColor(56, 189, 248), 1.5),
            QBrush(QColor(30, 41, 59, 220)));
        marker->setData(0, 3);
        marker->setZValue(120);
        m_robotItem = marker;
    }

    if (m_canvas && m_canvas->viewport()) {
        m_canvas->viewport()->update();
    }
}

void MapViewWidget::updateRulers()
{
    m_topRuler->syncWithView(m_canvas, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
    m_leftRuler->syncWithView(m_canvas, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
}

// ── MQTT ──────────────────────────────────────────────────────────────────────

void MapViewWidget::onMqttMessage(const QString &topic, const QByteArray &payload)
{
    QByteArray cleanPayload = payload.trimmed();
    bool inQuote = false;
    bool escape = false;
    for (int i = 0; i < cleanPayload.size(); ++i) {
        char c = cleanPayload.at(i);
        if (escape) { escape = false; continue; }
        if (c == '\\') { if (inQuote) escape = true; continue; }
        if (c == '"') { inQuote = !inQuote; continue; }
        if (!inQuote) {
            if (c == '(') cleanPayload[i] = '{';
            else if (c == ')') cleanPayload[i] = '}';
        }
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(cleanPayload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    if (!topic.contains("info")) return;

    // 过滤非当前跟踪设备的消息 (多机防干扰)
    QJsonObject d = doc.object();
    QString msgCarId;
    if (d.contains("car_id")) msgCarId = d.value("car_id").toVariant().toString().trimmed();
    else if (d.contains("carId")) msgCarId = d.value("carId").toVariant().toString().trimmed();
    else if (topic.startsWith("/rob/")) {
        QStringList parts = topic.split('/', Qt::SkipEmptyParts);
        if (parts.size() >= 2) msgCarId = parts[1].trimmed();
    }
    QString curCarId = m_robot.carId.trimmed().isEmpty() ? AppConfig::instance().agvId().trimmed() : m_robot.carId.trimmed();
    if (curCarId.isEmpty()) curCarId = "1";
    if (!msgCarId.isEmpty() && msgCarId != curCarId) {
        return;
    }

    RobotInfoManager::instance().updateFromPayload(topic, cleanPayload);
}

void MapViewWidget::onRobotInfoUpdated(const RobotInfo &info)
{
    if (info.hasPose) {
        m_robot.wx = info.car_x;
        m_robot.wy = info.car_y;
        m_robot.yawRad = info.car_yaw;
        m_robot.posValid = true;
    }
    if (info.battery >= 0.0) {
        m_robot.battery = info.batteryPercent();
    }
    if (info.actual_vel.valid) {
        m_robot.speed = info.actualLinearSpeed();
    }
    if (info.covScore >= 0.0) {
        m_robot.covScore = info.covScore;
    }
    if (!info.carName.isEmpty()) {
        m_robot.carName = info.carName;
    }

    if (m_deviceStatusLabel) {
        m_deviceStatusLabel->setText(info.statusText());
        m_deviceStatusLabel->setStyleSheet(QString("color:%1; font-size:12px; font-weight:600;").arg(info.statusColor()));
    }

    renderRobot();
    updateInfoPanel();

    if (info.status != "3" && info.status != "5") {
        m_navActive = false;
    }
    updateNavButtonsState();
}

void MapViewWidget::updateInfoPanel()
{
    if (!m_robot.carName.isEmpty()) {
        m_carNameLabel->setText(m_robot.carName);
    } else {
        QString carId = m_robot.carId.isEmpty() ? AppConfig::instance().agvId() : m_robot.carId;
        if (carId.isEmpty()) carId = "1";
        m_carNameLabel->setText(QString("AGV-%1").arg(carId));
    }

    if (!m_robot.posValid) {
        m_posLabel->setText("未定位");
        m_covLabel->setText("未定位");
        m_covLabel->setStyleSheet("color:#e05555; font-size:12px;");
        return;
    }

    // Battery
    double bat = m_robot.battery;
    if (bat < 0) {
        m_batteryLabel->setText("充电中");
        m_batteryLabel->setStyleSheet("color:#5cb85c; font-size:12px;");
    } else {
        int batPct = (bat >= 0.0 && bat <= 1.0) ? qRound(bat * 100.0) : qRound(bat);
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
        double cs = (m_robot.covScore > 1.0) ? m_robot.covScore / 100.0 : m_robot.covScore;
        m_covLabel->setText(QString::number(cs, 'f', 2));
        m_covLabel->setStyleSheet(
            cs <= 0.4
            ? "color:#e05555; font-size:12px;"
            : "color:#c8d0d8; font-size:12px;");
    } else {
        m_covLabel->setText("未知");
        m_covLabel->setStyleSheet("color:#8898a8; font-size:12px;");
    }
}

// ── View events ───────────────────────────────────────────────────────────────

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

void MapViewWidget::onResetView()
{
    if (!m_canvas || !m_scene) return;
    m_canvas->resetTransform();
    m_canvas->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatioByExpanding);
    onViewTransformChanged();
}

void MapViewWidget::onToggleNavMode()
{
    m_navMode = m_navModeBtn ? m_navModeBtn->isChecked() : false;
    if (m_navMode) {
        m_navModeBtn->setStyleSheet(kToolBtnPrimary);
        if (m_relocMode) exitRelocMode();
        if (m_moveMode && m_moveBtn) {
            m_moveMode = false;
            m_moveBtn->setChecked(false);
            m_moveBtn->setStyleSheet(kToolBtnStyle);
        }
        if (m_canvas) {
            m_canvas->setPanWithLeftButton(false);
            m_canvas->setCursor(Qt::PointingHandCursor);
        }
        setStatus("路径导航模式已开启: 请在地图上点击任意路网点发起导航", "#00bcd4");
    } else {
        if (m_navModeBtn) m_navModeBtn->setStyleSheet(kToolBtnStyle);
        if (m_canvas) {
            m_canvas->setPanWithLeftButton(!m_relocMode && !m_moveMode);
            m_canvas->setCursor(Qt::CrossCursor);
        }
        setStatus("已退出路径导航模式");
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

    // 优先检索鼠标指针所在位置的站点图元 (支持微小点击容差)
    QGraphicsItem *stationItem = nullptr;
    for (auto *it : m_scene->items(scenePos)) {
        QGraphicsItem *cur = it;
        if (!cur->data(1).isValid() && cur->parentItem()) cur = cur->parentItem();
        if (cur->data(0).toInt() == 2 && cur->data(1).isValid()) {
            stationItem = cur;
            break;
        }
    }
    if (!stationItem) {
        const double tol = 8.0;
        for (auto *it : m_scene->items(QRectF(scenePos.x() - tol, scenePos.y() - tol, tol * 2, tol * 2))) {
            QGraphicsItem *cur = it;
            if (!cur->data(1).isValid() && cur->parentItem()) cur = cur->parentItem();
            if (cur->data(0).toInt() == 2 && cur->data(1).isValid()) {
                stationItem = cur;
                break;
            }
        }
    }

    if (stationItem) {
        QString ptId   = stationItem->data(1).toString();
        QString ptType = stationItem->data(2).toString();

        auto findIn = [&](const QJsonArray &arr) -> QJsonObject {
            for (const auto &v : arr) {
                QJsonObject o = v.toObject();
                if (o.value("id").toString() == ptId) return o;
            }
            return QJsonObject();
        };

        QJsonObject ptObj;
        if (ptType == "charge") ptObj = findIn(m_charges);
        else if (ptType == "highPrecision") ptObj = findIn(m_highPrecisionPoints);
        else ptObj = findIn(m_points);

        if (!ptObj.isEmpty()) {
            showStationDetails(ptObj, ptType);
        }
    } else {
        if (!m_navMode && !m_moveMode && !m_relocMode) {
            clearStationDetails();
        }
    }

    if (m_navMode) {
        QGraphicsItem *targetItem = stationItem;
        if (!targetItem) {
            return;
        }

        QString ptId   = targetItem->data(1).toString();
        QString ptType = targetItem->data(2).toString();
        QString ptName = targetItem->data(3).toString();
        QPointF wPos   = targetItem->data(4).toPointF();

        if (ptName.isEmpty()) ptName = ptId;

        QString typeCN = (ptType == "charge") ? "充电点" : (ptType == "highPrecision" ? "精定位点" : "普通站点");

        // 弹出对话框确认导航
        QMessageBox navBox(this);
        navBox.setWindowTitle("路径导航");
        navBox.setIcon(QMessageBox::Question);
        navBox.setText(QString("是否导航到 “%1”？").arg(ptName));
        navBox.setInformativeText(
            QString("点位类型: %1\n点位编号: %2\n目标坐标: (%3, %4)")
            .arg(typeCN, ptId)
            .arg(wPos.x(), 0, 'f', 2)
            .arg(wPos.y(), 0, 'f', 2));
        QPushButton *startBtn = navBox.addButton("导航到 " + ptName, QMessageBox::AcceptRole);
        QPushButton *cancelBtn = navBox.addButton("取消", QMessageBox::RejectRole);
        navBox.setDefaultButton(startBtn);

        navBox.exec();
        if (navBox.clickedButton() == startBtn) {
            QString carId = AppConfig::instance().currentDevice().carId;
            if (carId.isEmpty()) carId = AppConfig::instance().agvId();
            if (carId.isEmpty()) carId = "1";

            int actionId = static_cast<int>(QDateTime::currentMSecsSinceEpoch() % 1000000 + 1);
            double speed = TeleopController::instance().baseLinearSpeed();
            if (speed <= 0.01) speed = 0.5;

            QString mapName = AppConfig::instance().currentDevice().mapName;
            if (mapName.isEmpty()) mapName = m_mapName;

            // 1. 控制器 8061 端口标准固定路径导航 (Table 3 set_robot_fix_path)
            ApiService::instance().navigateToStation(ptId, mapName, wPos.x(), wPos.y(), speed, actionId, [this, ptName](int code, const QJsonObject &resp) {
                if (code == 200) {
                    QString msg = resp.value("data").toObject().value("msg").toString("ok");
                    if (msg == "failed") {
                        QString err = resp.value("data").toObject().value("error_info").toString();
                        Toast::error(this, QString("控制器拒绝导航: %1").arg(err.isEmpty() ? "执行中或格式错误" : err));
                    } else {
                        Toast::success(this, QString("已开始路径导航至点位 “%1”").arg(ptName));
                    }
                } else {
                    QString msg = resp.value("msg").toString("控制器未响应");
                    Toast::error(this, QString("导航请求失败(%1): %2").arg(code).arg(msg));
                }
            });

            // 2. MQTT 协议实时发布导航目标指令
            QJsonObject mqttData;
            mqttData["command"] = "navigate";
            mqttData["point_id"] = ptId;
            mqttData["point_name"] = ptName;
            mqttData["target_x"] = wPos.x();
            mqttData["target_y"] = wPos.y();
            mqttData["sender"] = "QT-AGV";

            MqttService::instance().publish(
                QString("/rob/%1/navigation").arg(carId),
                QJsonDocument(mqttData).toJson(QJsonDocument::Compact));

            m_navActive = true;
            updateNavButtonsState();

            setStatus(QString("已下发控制器路径导航指令至点位 “%1” (%2, %3)").arg(ptName).arg(wPos.x(), 0, 'f', 2).arg(wPos.y(), 0, 'f', 2), "#00e676");
        }
        return;
    }

    if (m_moveMode) {
        QMessageBox moveBox(this);
        moveBox.setWindowTitle("自由移动");
        moveBox.setIcon(QMessageBox::Question);
        moveBox.setText(QString("是否移动至目标坐标 (%1, %2)？")
                        .arg(world.x(), 0, 'f', 2).arg(world.y(), 0, 'f', 2));
        QPushButton *startBtn = moveBox.addButton("开始移动", QMessageBox::AcceptRole);
        QPushButton *cancelBtn = moveBox.addButton("取消", QMessageBox::RejectRole);
        moveBox.setDefaultButton(startBtn);
        moveBox.exec();

        if (moveBox.clickedButton() == startBtn) {
            QString carId = AppConfig::instance().currentDevice().carId;
            if (carId.isEmpty()) carId = AppConfig::instance().agvId();
            if (carId.isEmpty()) carId = "1";

            int actionId = static_cast<int>(QDateTime::currentMSecsSinceEpoch() % 1000000 + 1);
            double speed = TeleopController::instance().baseLinearSpeed();
            if (speed <= 0.01) speed = 0.5;

            // 1. 控制器 8061 端口三方固定路径点导航 (Table 4 set_third_fix_path)
            ApiService::instance().navigateToPose(m_robot.wx, m_robot.wy, world.x(), world.y(), speed, actionId, [this, world](int code, const QJsonObject &resp) {
                if (code == 200) {
                    QString msg = resp.value("data").toObject().value("msg").toString("ok");
                    if (msg == "failed") {
                        QString err = resp.value("data").toObject().value("error_info").toString();
                        Toast::error(this, QString("控制器拒绝移动: %1").arg(err.isEmpty() ? "执行中或格式错误" : err));
                    } else {
                        Toast::success(this, QString("已开始移动至 (%1, %2)").arg(world.x(), 0, 'f', 2).arg(world.y(), 0, 'f', 2));
                    }
                } else {
                    QString msg = resp.value("msg").toString("控制器未响应");
                    Toast::error(this, QString("移动请求失败(%1): %2").arg(code).arg(msg));
                }
            });

            // 2. MQTT 协议实时发布移动目标指令
            QJsonObject mqttData;
            mqttData["command"] = "navigate";
            mqttData["point_id"] = "";
            mqttData["point_name"] = QString("坐标(%1,%2)").arg(world.x(), 0, 'f', 1).arg(world.y(), 0, 'f', 1);
            mqttData["target_x"] = world.x();
            mqttData["target_y"] = world.y();
            mqttData["sender"] = "QT-AGV";

            MqttService::instance().publish(
                QString("/rob/%1/navigation").arg(carId),
                QJsonDocument(mqttData).toJson(QJsonDocument::Compact));

            m_navActive = true;
            updateNavButtonsState();

            setStatus(QString("已下发移动指令至 (%1, %2)").arg(world.x(), 0, 'f', 2).arg(world.y(), 0, 'f', 2), "#00e676");
            Toast::success(this, QString("移动指令已下发: (%1, %2)").arg(world.x(), 0, 'f', 2).arg(world.y(), 0, 'f', 2));
        }
        return;
    }

    if (m_relocMode) {
        if (m_robot.carId.isEmpty()) { setStatus("无设备ID，无法重定位", "#e05555"); return; }
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

        setStatus("拖拽以设定朝向，松开发送重定位", "#4a9eff");
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

    // Compute yaw: atan2(Δnorth, Δeast) — ROS standard (0=east, CCW positive)
    double dx = endWorld.x() - m_relocStartWorld.x();
    double dy = endWorld.y() - m_relocStartWorld.y();
    double yaw = (dx * dx + dy * dy > 0.01)   // use drag direction if dragged > ~0.1m
                 ? std::atan2(dy, dx)
                 : m_robot.yawRad;

    QString carId = m_robot.carId.trimmed().isEmpty() ? AppConfig::instance().agvId() : m_robot.carId.trimmed();
    if (carId.isEmpty()) carId = "1";

    QJsonObject poseObj;
    QJsonArray coords;
    coords.append(m_relocStartWorld.x());
    coords.append(m_relocStartWorld.y());
    coords.append(0.0);
    poseObj["coordinates"] = coords;
    poseObj["yaw"]         = yaw;

    QJsonObject msg;
    msg["action"] = "initialpose";
    msg["car_id"] = numOrStr(carId);
    msg["map_id"] = numOrStr(m_mapId);
    msg["pose"]   = poseObj;

    setStatus("正在发送重定位...", "#4a9eff");
    auto *loading = new LoadingDialog("正在发送重定位...", this);
    loading->show();
    ApiService::instance().sendMqtt(msg, [this, loading](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            Toast::success(this, "重定位成功");
            setStatus("重定位成功");
        } else {
            setStatus("重定位失败", "#e05555");
        }
        exitRelocMode();
    });
}

// ── Toolbar actions ───────────────────────────────────────────────────────────

void MapViewWidget::onCreateMap()
{
    emit navigateRequested("create_map", QString());
}

void MapViewWidget::onUploadMap()
{
    auto *dlg = new MapUploadDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &MapUploadDialog::uploadSucceeded, this, [this]() {
        onRefreshMap();
    });
    dlg->exec();
}

void MapViewWidget::onUploadSmap()
{
    auto *dlg = new SmapUploadDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &SmapUploadDialog::uploadSucceeded, this, [this]() {
        onRefreshMap();
    });
    dlg->exec();
}

void MapViewWidget::onRefreshMap()
{
    // 手动刷新重置节流与请求锁，允许用户主动重试
    m_lastRefreshMs = 0;
    m_isLoadingCar  = false;
    m_loadingMapId.clear();

    setStatus("正在获取设备绑定地图并刷新...");
    QString carId = AppConfig::instance().agvId();
    if (carId.isEmpty()) carId = "1";

    ApiService::instance().getCar(carId, [this](int code, const QJsonObject &data) {
        if (code != 200) {
            setStatus("获取设备信息失败", "#e05555");
            Toast::error(this, "获取设备信息失败");
            return;
        }
        QJsonObject car = data.value("data").toObject(data);

        // 更新右侧面板车辆名称
        m_carNameLabel->setText(car.value("carName").toString("--"));

        QString mapId = jsonIdStr(car.value("mapId"));
        if (mapId.isEmpty() || mapId == "0") {
            mapId = jsonIdStr(car.value("map_id"));
        }
        if (mapId.isEmpty() || mapId == "0") {
            mapId = jsonIdStr(data.value("mapId"));
        }
        if (mapId.isEmpty() || mapId == "0") {
            QJsonArray maps = data.value("maps").toArray();
            if (!maps.isEmpty()) {
                mapId = jsonIdStr(maps.first().toObject().value("mapId"));
            }
        }

        if (mapId.isEmpty() || mapId == "0") {
            ApiService::instance().listMap(QJsonObject{{"pageNum", 1}, {"pageSize", 1}},
                [this](int c2, const QJsonObject &d2) {
                    if (c2 == 200) {
                        QJsonArray rows = d2.value("rows").toArray();
                        if (!rows.isEmpty()) {
                            QString fallbackMapId = jsonIdStr(rows.first().toObject().value("mapId"));
                            loadMapById(fallbackMapId);
                            Toast::success(this, "已刷新并加载可用地图");
                        } else {
                            setStatus("无可用地图", "#e05555");
                            Toast::info(this, "无可用地图");
                        }
                    } else {
                        setStatus("获取地图列表失败", "#e05555");
                    }
                });
            return;
        }

        // 刷新重置要素缓存，重新加载/下载地图与路网
        m_points = QJsonArray();
        m_lines  = QJsonArray();
        m_curves = QJsonArray();
        m_charges = QJsonArray();
        m_highPrecisionPoints = QJsonArray();
        m_routeLoaded = false;
        m_robot.posValid = false;

        loadMapById(mapId);
        Toast::success(this, "已获取绑定地图并刷新");
    });
}

void MapViewWidget::forceRefreshAfterMapping()
{
    // 重置所有节流与请求锁，确保不被500ms防抖或并发锁阻断
    m_lastRefreshMs = 0;
    m_isLoadingCar  = false;
    m_loadingMapId.clear();

    setStatus("建图完成，正在获取最新地图...");
    QString carId = AppConfig::instance().agvId();
    if (carId.isEmpty()) carId = "1";

    ApiService::instance().getCar(carId, [this](int code, const QJsonObject &data) {
        if (code != 200) {
            setStatus("获取设备信息失败", "#e05555");
            Toast::error(this, "获取设备信息失败");
            return;
        }
        QJsonObject car = data.value("data").toObject(data);

        // 更新右侧面板车辆名称
        QString carName = car.value("carName").toString().trimmed();
        if (carName.isEmpty()) carName = QString("AGV-%1").arg(AppConfig::instance().agvId());
        m_robot.carName = carName;
        m_carNameLabel->setText(carName);
        updateInfoPanel();

        // 获取当前绑定的 mapId
        QString mapId = jsonIdStr(car.value("mapId"));
        if (mapId.isEmpty() || mapId == "0") mapId = jsonIdStr(car.value("map_id"));
        if (mapId.isEmpty() || mapId == "0") mapId = jsonIdStr(data.value("mapId"));

        if (mapId.isEmpty() || mapId == "0") {
            setStatus("设备当前未绑定地图", "#e05555");
            Toast::info(this, "设备当前未绑定地图");
            return;
        }

        // 清理该 mapId 对应的本地缓存，强制重新从服务端下载最新 zip
        QString localDir = QCoreApplication::applicationDirPath() + "/maps/" + mapId;
        QDir dir(localDir);
        if (dir.exists()) {
            // 删除所有 yaml/pgm/zip/meta 文件，保留 route.json 等其他文件
            const QStringList patterns = {"*.yaml", "*.yml", "*.pgm", "*.zip", "meta.json"};
            for (const QString &pattern : patterns) {
                for (const QString &f : dir.entryList({pattern}, QDir::Files)) {
                    dir.remove(f);
                    qDebug() << "[forceRefreshAfterMapping] Removed stale cache file:" << f;
                }
            }
        }

        // 清理路网与位姿缓存
        m_points = QJsonArray();
        m_lines  = QJsonArray();
        m_curves = QJsonArray();
        m_charges = QJsonArray();
        m_highPrecisionPoints = QJsonArray();
        m_routeLoaded = false;
        m_robot.posValid = false;

        loadMapById(mapId);
        Toast::success(this, "建图完成，正在加载最新地图");
    });
}

void MapViewWidget::onSwitchMap()
{
    QDialog dlg(this);
    dlg.setWindowTitle("选择地图");
    dlg.setMinimumSize(840, 580);
    dlg.setStyleSheet(
        "QDialog{background:#181b22; color:#e2e8f0;}"
        "QScrollArea{background:#181b22; border:none;}"
        "QWidget#cards{background:#181b22;}"
        "QPushButton{background:#232936; color:#cbd5e1; border:1px solid #3b4252;"
        "border-radius:4px; padding:6px 14px; font-size:12px; font-weight:500;}"
        "QPushButton:hover{background:#2d3545; border-color:#4a9eff; color:#fff;}"
        "QLabel{background:transparent;}");

    auto *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // Top Row: Search bar + Stats label + Refresh button
    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(10);
    auto *searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("🔍 搜索地图名称或地图ID...");
    searchEdit->setStyleSheet(
        "background:#232936; border:1px solid #3b4252; border-radius:6px;"
        "color:#f8fafc; padding:7px 12px; font-size:13px;");
    topRow->addWidget(searchEdit, 1);

    auto *statsLabel = new QLabel("共 0 张地图");
    statsLabel->setStyleSheet("color:#94a3b8; font-size:12px; font-weight:500;");
    topRow->addWidget(statsLabel);

    auto *refreshListBtn = new QPushButton("🔄 刷新列表");
    topRow->addWidget(refreshListBtn);

    root->addLayout(topRow);

    // Scrollable cards area
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *cardsWidget = new QWidget;
    cardsWidget->setObjectName("cards");
    auto *grid = new QGridLayout(cardsWidget);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setSpacing(10);
    grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    scroll->setWidget(cardsWidget);
    root->addWidget(scroll, 1);

    // Bottom Row: Selection preview tip + Action buttons
    auto *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(10);
    auto *selTipLabel = new QLabel("请在上方选择地图 (支持双击快速切换)");
    selTipLabel->setStyleSheet("color:#94a3b8; font-size:12px;");
    bottomRow->addWidget(selTipLabel, 1);

    auto *cancelBtn = new QPushButton("取消");
    auto *okBtn     = new QPushButton("确认切换");
    okBtn->setStyleSheet("QPushButton{background:#2563eb; color:#fff; font-weight:600; border:none; border-radius:4px; padding:6px 18px;}"
                         "QPushButton:hover{background:#3b82f6;}");
    bottomRow->addWidget(cancelBtn);
    bottomRow->addWidget(okBtn);
    root->addLayout(bottomRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    QString selectedMapId;
    QString selectedMapName;
    QVector<QFrame*> allCards;
    QJsonArray mapList;

    // 点击确认切换逻辑：访问接口 /robot/car/edit/map/{mapid}，成功后关闭窗口并刷新地图
    auto doConfirmSwitch = [&]() {
        if (selectedMapId.isEmpty()) {
            Toast::info(&dlg, "请先选择需要切换的地图");
            return;
        }

        okBtn->setEnabled(false);
        cancelBtn->setEnabled(false);
        okBtn->setText("正在切换...");
        selTipLabel->setText(QString("正在请求切换至地图 [ID: %1]...").arg(selectedMapId));
        selTipLabel->setStyleSheet("color:#38bdf8; font-size:12px;");

        auto *loading = new LoadingDialog("正在切换地图...", &dlg);
        loading->show();

        // 访问接口 "/robot/car/edit/map/{mapid}"
        ApiService::instance().editCarMap(selectedMapId, [this, selectedMapId, selectedMapName, &dlg, okBtn, cancelBtn, selTipLabel, loading](int code, const QJsonObject &data) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                // 成功后关闭窗口
                dlg.accept();
            } else {
                okBtn->setEnabled(true);
                cancelBtn->setEnabled(true);
                okBtn->setText("确认切换");
                QString errMsg = data.value("msg").toString();
                if (errMsg.isEmpty()) {
                    errMsg = QString("服务器返回错误 (状态码: %1)").arg(code);
                }
                selTipLabel->setText(QString("<font color='#f87171'>切换失败: %1</font>").arg(errMsg));
                QMessageBox::warning(&dlg, "切换失败", QString("访问切换地图接口失败 (错误码 %1)：\n%2").arg(code).arg(errMsg));
            }
        });
    };

    connect(okBtn, &QPushButton::clicked, &dlg, [&]() {
        doConfirmSwitch();
    });

    // Build cards list
    std::function<void(const QJsonArray &, const QString &)> buildCards =
            [&](const QJsonArray &list, const QString &filter) {
        for (auto *c : allCards) c->deleteLater();
        allCards.clear();
        while (auto *item = grid->takeAt(0)) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }

        int visibleCount = 0;
        int col = 0, row = 0;
        const int COLS = 2; // 双列现代化布局，宽度适中信息饱满

        for (const auto &v : list) {
            QJsonObject m = v.toObject();
            QString name  = m.value("mapName").toString();
            QString mid   = jsonIdStr(m.value("mapId"));

            if (!filter.isEmpty()) {
                bool nameMatch = name.toLower().contains(filter.toLower());
                bool idMatch   = mid.contains(filter.trimmed());
                if (!nameMatch && !idMatch) continue;
            }

            visibleCount++;

            // 检查该地图是否本地已缓存
            QString localDir = QCoreApplication::applicationDirPath() + "/maps/" + mid;
            QString dummyY, dummyP;
            bool isCached = findLocalMapFiles(localDir, dummyY, dummyP);
            bool isCurrent = (mid == m_mapId);

            // 卡片容器
            auto *card = new QFrame;
            card->setMinimumHeight(68);
            card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

            bool isSelected = (mid == selectedMapId);
            if (isSelected) {
                card->setStyleSheet("QFrame{background:#183354; border:2px solid #3b82f6; border-radius:6px;}");
            } else if (isCurrent) {
                card->setStyleSheet("QFrame{background:#1c2738; border:1px solid #2d456b; border-radius:6px;}"
                                    "QFrame:hover{background:#23334a; border-color:#3b82f6;}");
            } else {
                card->setStyleSheet("QFrame{background:#1e2430; border:1px solid #2d3545; border-radius:6px;}"
                                    "QFrame:hover{background:#252e3d; border-color:#3b82f6;}");
            }
            allCards.append(card);

            auto *hl = new QHBoxLayout(card);
            hl->setContentsMargins(12, 10, 12, 10);
            hl->setSpacing(12);

            // 左侧地图矢量图标装饰块
            auto *iconBox = new QFrame;
            iconBox->setFixedSize(40, 40);
            iconBox->setStyleSheet("background:#141b2d; border:1px solid #1e3a8a; border-radius:6px;");
            auto *iconLyt = new QVBoxLayout(iconBox);
            iconLyt->setContentsMargins(0, 0, 0, 0);
            auto *iconLbl = new QLabel("MAP");
            iconLbl->setAlignment(Qt::AlignCenter);
            iconLbl->setStyleSheet("color:#38bdf8; font-weight:bold; font-size:11px; font-family:sans-serif;");
            iconLyt->addWidget(iconLbl);
            hl->addWidget(iconBox);

            // 中间信息区
            auto *infoLyt = new QVBoxLayout;
            infoLyt->setContentsMargins(0, 0, 0, 0);
            infoLyt->setSpacing(4);

            auto *nameLbl = new QLabel(name);
            nameLbl->setStyleSheet("color:#f8fafc; font-size:13px; font-weight:bold;");
            nameLbl->setToolTip(QString("地图名称: %1\n地图ID: %2").arg(name, mid));
            infoLyt->addWidget(nameLbl);

            // 标签行
            auto *tagRow = new QHBoxLayout;
            tagRow->setContentsMargins(0, 0, 0, 0);
            tagRow->setSpacing(6);

            // ID Badge
            auto *idBadge = new QLabel(QString("ID: %1").arg(mid));
            idBadge->setStyleSheet(
                "background:#0f172a; color:#38bdf8; border:1px solid #0284c7;"
                "border-radius:3px; padding:1px 6px; font-size:11px; font-family:monospace; font-weight:600;");
            tagRow->addWidget(idBadge);

            // 缓存状态 Badge
            auto *cacheBadge = new QLabel(isCached ? "✓ 本地已缓存" : "云端资源包");
            if (isCached) {
                cacheBadge->setStyleSheet(
                    "background:rgba(16,185,129,0.18); color:#34d399; border:1px solid rgba(16,185,129,0.4);"
                    "border-radius:3px; padding:1px 6px; font-size:11px; font-weight:500;");
            } else {
                cacheBadge->setStyleSheet(
                    "background:rgba(148,163,184,0.12); color:#94a3b8; border:1px solid rgba(148,163,184,0.3);"
                    "border-radius:3px; padding:1px 6px; font-size:11px;");
            }
            tagRow->addWidget(cacheBadge);

            // 当前使用中 Badge
            if (isCurrent) {
                auto *curBadge = new QLabel("当前使用中");
                curBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
                curBadge->setStyleSheet(
                    "background:rgba(59,130,246,0.25); color:#60a5fa; border:1px solid #3b82f6;"
                    "border-radius:3px; padding:1px 6px; font-size:11px; font-weight:bold;");
                tagRow->addWidget(curBadge);
            }

            tagRow->addStretch();
            infoLyt->addLayout(tagRow);
            hl->addLayout(infoLyt, 1);

            // 卡片整体鼠标与穿透设置
            card->setCursor(Qt::PointingHandCursor);
            iconBox->setAttribute(Qt::WA_TransparentForMouseEvents);
            iconLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            nameLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            idBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
            cacheBadge->setAttribute(Qt::WA_TransparentForMouseEvents);

            // 右侧删除按钮
            auto *delBtn = new QPushButton("✕", card);
            delBtn->setFixedSize(24, 24);
            delBtn->setCursor(Qt::PointingHandCursor);
            delBtn->setToolTip("删除此地图");
            delBtn->setStyleSheet(
                "QPushButton{background:rgba(220,38,38,0.18); color:#f87171; border:1px solid rgba(220,38,38,0.3);"
                "border-radius:12px; font-size:11px; font-weight:bold; padding:0;}"
                "QPushButton:hover{background:#ef4444; color:#fff; border:none;}");
            hl->addWidget(delBtn);

            auto updateCardSelection = [=, &selectedMapId, &selectedMapName, &allCards]() {
                selectedMapId = mid;
                selectedMapName = name;
                for (auto *c : allCards) {
                    c->setStyleSheet("QFrame{background:#1e2430; border:1px solid #2d3545; border-radius:6px;}"
                                    "QFrame:hover{background:#252e3d; border-color:#3b82f6;}");
                }
                card->setStyleSheet("QFrame{background:#183354; border:2px solid #3b82f6; border-radius:6px;}");
                selTipLabel->setText(QString("已选中地图: [ID: %1] %2 (%3)")
                                         .arg(mid, name, isCached ? "本地已缓存" : "需下载资源包"));
                selTipLabel->setStyleSheet("color:#60a5fa; font-size:12px; font-weight:500;");
            };

            // 监听卡片点击与双击
            struct CardClickFilter : public QObject {
                std::function<void()> onClick;
                std::function<void()> onDblClick;
                CardClickFilter(std::function<void()> clk, std::function<void()> dbl, QObject *parent = nullptr)
                    : QObject(parent), onClick(clk), onDblClick(dbl) {}
                bool eventFilter(QObject *obj, QEvent *event) override {
                    if (event->type() == QEvent::MouseButtonPress) {
                        if (onClick) onClick();
                    } else if (event->type() == QEvent::MouseButtonDblClick) {
                        if (onDblClick) onDblClick();
                        return true;
                    }
                    return QObject::eventFilter(obj, event);
                }
            };
            auto *filter = new CardClickFilter(updateCardSelection, [updateCardSelection, &doConfirmSwitch]() {
                updateCardSelection();
                doConfirmSwitch();
            }, card);
            card->installEventFilter(filter);

            // 删除事件绑定
            connect(delBtn, &QPushButton::clicked, this, [&, mid, name]() {
                QMessageBox confirm(&dlg);
                confirm.setWindowTitle("确认删除");
                confirm.setText(QString("确认删除地图「%1」(ID: %2)？\n此操作不可撤销。").arg(name, mid));
                QPushButton *yesBtn = confirm.addButton("确定", QMessageBox::YesRole);
                confirm.addButton("取消", QMessageBox::NoRole);
                confirm.exec();
                if (confirm.clickedButton() != yesBtn) return;

                ApiService::instance().delMap(mid, [&, mid](int code, const QJsonObject &) {
                    if (code != 200) {
                        QMessageBox::warning(&dlg, "错误", "删除地图失败");
                        return;
                    }
                    // 同步清理本地缓存目录
                    QString cDir = QCoreApplication::applicationDirPath() + "/maps/" + mid;
                    QDir(cDir).removeRecursively();

                    Toast::success(this, "地图已删除");
                    if (selectedMapId == mid) {
                        selectedMapId.clear();
                        selectedMapName.clear();
                        selTipLabel->setText("请在上方选择地图 (支持双击快速切换)");
                        selTipLabel->setStyleSheet("color:#94a3b8; font-size:12px;");
                    }
                    QJsonArray newList;
                    for (const auto &v : mapList) {
                        if (jsonIdStr(v.toObject().value("mapId")) != mid) {
                            newList.append(v);
                        }
                    }
                    mapList = newList;
                    buildCards(mapList, searchEdit->text());
                });
            });

            grid->addWidget(card, row, col);
            if (++col >= COLS) { col = 0; ++row; }
        }

        statsLabel->setText(QString("共 %1 张地图%2")
                                .arg(list.size())
                                .arg(filter.isEmpty() ? "" : QString(" (已筛选: %1)").arg(visibleCount)));
    };

    auto loadList = [&]() {
        ApiService::instance().listMapAll(QJsonObject{{"pageNum",1},{"pageSize",100}},
            [&](int code, const QJsonObject &data) {
                if (code != 200) {
                    statsLabel->setText("获取地图列表失败");
                    return;
                }
                mapList = data.value("rows").toArray();
                buildCards(mapList, searchEdit->text());
            });
    };

    loadList();

    connect(refreshListBtn, &QPushButton::clicked, this, loadList);
    connect(searchEdit, &QLineEdit::textChanged, this, [&](const QString &txt) {
        buildCards(mapList, txt);
    });

    if (dlg.exec() == QDialog::Accepted && !selectedMapId.isEmpty()) {
        exitRelocMode();
        // 成功切换地图后，清理旧的路径和点位，刷新加载新地图
        m_points = QJsonArray();
        m_lines  = QJsonArray();
        m_curves = QJsonArray();
        m_charges = QJsonArray();
        m_highPrecisionPoints = QJsonArray();
        m_routeLoaded = false;
        m_robot.posValid = false;
        loadMapById(selectedMapId, selectedMapName);
        Toast::success(this, "地图切换成功");
    }
}

void MapViewWidget::onRelocalize()
{
    m_relocMode = m_relocBtn ? m_relocBtn->isChecked() : !m_relocMode;
    if (m_relocMode) {
        if (m_relocBtn) {
            m_relocBtn->setChecked(true);
            m_relocBtn->setStyleSheet(kToolBtnPrimary);
        }
        if (m_navMode && m_navModeBtn) {
            m_navMode = false;
            m_navModeBtn->setChecked(false);
            m_navModeBtn->setStyleSheet(kToolBtnStyle);
        }
        if (m_moveMode && m_moveBtn) {
            m_moveMode = false;
            m_moveBtn->setChecked(false);
            m_moveBtn->setStyleSheet(kToolBtnStyle);
        }
        if (m_canvas) {
            m_canvas->setPanWithLeftButton(false);
        }
        setStatus("点击地图选择位置，拖拽设定朝向 (再次点击按钮取消)", "#4a9eff");
    } else {
        exitRelocMode();
        setStatus("就绪");
    }
}

void MapViewWidget::exitRelocMode()
{
    m_relocMode = false;
    m_relocDragging = false;
    if (m_relocBtn) {
        m_relocBtn->setChecked(false);
        m_relocBtn->setStyleSheet(kToolBtnStyle);
    }
    if (m_poseDotItem)   { m_scene->removeItem(m_poseDotItem);   delete m_poseDotItem;   m_poseDotItem   = nullptr; }
    if (m_poseArrowItem) { m_scene->removeItem(m_poseArrowItem); delete m_poseArrowItem; m_poseArrowItem = nullptr; }
    if (m_canvas) {
        m_canvas->setPanWithLeftButton(!m_navMode && !m_moveMode);
    }
}

void MapViewWidget::onToggleMoveMode()
{
    if (!m_motionDialog) {
        m_motionDialog = new MotionDialog(this);
    }
    m_motionDialog->show();
    m_motionDialog->raise();
    m_motionDialog->activateWindow();
}

void MapViewWidget::onCancelNavigation()
{
    QString carId = AppConfig::instance().currentDevice().carId;
    if (carId.isEmpty()) carId = AppConfig::instance().agvId();
    if (carId.isEmpty()) carId = "1";

    // 1. MQTT 广播取消
    MqttService::instance().publish(QString("/rob/%1/cancel").arg(carId), "1");
    // 2. 控制器 8061 端口取消任务接口 (Table 8 stop_task)
    ApiService::instance().stopRobotTask(true, [](int code, const QJsonObject &resp) {
        qDebug() << "[MapViewWidget] stopRobotTask 8061 code:" << code << resp;
    });

    m_navActive = false;
    updateNavButtonsState();
    setStatus("已取消导航", "#f59e0b");
    Toast::info(this, "已下发取消导航指令");
}

void MapViewWidget::onPauseNavigation()
{
    QString carId = AppConfig::instance().currentDevice().carId;
    if (carId.isEmpty()) carId = AppConfig::instance().agvId();
    if (carId.isEmpty()) carId = "1";

    const RobotInfo &info = RobotInfoManager::instance().currentInfo();
    const bool isPaused = (info.status == "5");

    if (isPaused) {
        // 1. MQTT 继续
        MqttService::instance().publish(QString("/rob/%1/navigationContinue").arg(carId), "1");
        // 2. 控制器 8061 端口恢复运行接口 (Table 7 continue)
        ApiService::instance().continueRobot([](int code, const QJsonObject &resp) {
            qDebug() << "[MapViewWidget] continueRobot 8061 code:" << code << resp;
        });
        setStatus("已继续导航", "#00e676");
        Toast::success(this, "已继续导航");
    } else {
        // 1. MQTT 暂停
        MqttService::instance().publish(QString("/rob/%1/cancel").arg(carId), "1");
        // 2. 控制器 8061 端口暂停接口 (Table 6 suspend)
        ApiService::instance().suspendRobot([](int code, const QJsonObject &resp) {
            qDebug() << "[MapViewWidget] suspendRobot 8061 code:" << code << resp;
        });
        setStatus("导航已暂停", "#f59e0b");
        Toast::info(this, "导航已暂停");
    }
}

void MapViewWidget::updateNavButtonsState()
{
    const RobotInfo &info = RobotInfoManager::instance().currentInfo();
    const bool isNav = (info.status == "3" || info.status == "5" || m_navActive);

    if (m_cancelNavBtn) {
        m_cancelNavBtn->setEnabled(isNav);
    }
    if (m_pauseNavBtn) {
        m_pauseNavBtn->setEnabled(isNav);
        if (info.status == "5") {
            m_pauseNavBtn->setText("恢复导航");
            m_pauseNavBtn->setIcon(makeMapToolIcon("route"));
        } else {
            m_pauseNavBtn->setText("暂停导航");
            m_pauseNavBtn->setIcon(makeMapToolIcon("pause"));
        }
    }
}

void MapViewWidget::onTeleopClicked()
{
    TeleopController::instance().setActive(!TeleopController::instance().isActive());
}

void MapViewWidget::onTeleopActiveChanged(bool active)
{
    if (!m_teleopBtn) return;
    if (active) {
        m_teleopBtn->setText("恢复导航");
        m_teleopBtn->setStyleSheet(kToolBtnDanger);
        Toast::info(this, "已抢占控制权：WASD 移动 | Space 刹车 | Ctrl 加速 | P/L 升降");
        ApiService::instance().suspendWork([](int code, const QJsonObject &) {
            if (code != 200) {
                qWarning("暂停导航失败 (HTTP %d)，遥控可能受影响", code);
            }
        });
    } else {
        m_teleopBtn->setText("抢占控制权");
        m_teleopBtn->setStyleSheet(kToolBtnStyle);
        ApiService::instance().continueWork([](int code, const QJsonObject &) {
            if (code != 200) {
                qWarning("恢复导航失败 (HTTP %d)", code);
            }
        });
    }
}

void MapViewWidget::onShowRobotStatus()
{
    if (!m_statusDialog) {
        m_statusDialog = new RobotStatusDialog(this);
    }
    m_statusDialog->setMapInfo(m_mapId, m_mapName);
    m_statusDialog->updateFromRobotInfo(RobotInfoManager::instance().currentInfo());
    m_statusDialog->updateFromRobotState(m_robot.carId, m_robot.wx, m_robot.wy, m_robot.yawRad,
                                         m_robot.speed, m_robot.battery, m_robot.covScore, m_robot.posValid);
    m_statusDialog->show();
    m_statusDialog->raise();
    m_statusDialog->activateWindow();
}

void MapViewWidget::onEditRoute()
{
    if (m_mapId.isEmpty()) { setStatus("请先加载地图", "#f0a060"); return; }
    emit navigateRequested("map_edit", m_mapId);
}

void MapViewWidget::onEditMap()
{
    if (m_mapId.isEmpty()) { setStatus("请先加载地图", "#f0a060"); return; }
    emit navigateRequested("map_edit", m_mapId);
}

void MapViewWidget::onRenameMap()
{
    if (m_mapId.isEmpty()) return;
    bool ok;
    QString name = QInputDialog::getText(this, "重命名地图", "新名称:",
                                         QLineEdit::Normal, m_mapName, &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    QJsonObject d;
    d["mapId"]   = m_mapId;
    d["mapName"] = name.trimmed();
    auto *loading = new LoadingDialog("正在重命名地图...", this);
    loading->show();
    ApiService::instance().updateMap(d, [this, loading, name](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            m_mapName = name.trimmed();
            if (m_mapNameLabel) m_mapNameLabel->setText(m_mapName);
            setStatus("重命名成功");
            Toast::success(this, "重命名成功");
        } else {
            QMessageBox::warning(this, "错误", "重命名失败");
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

void MapViewWidget::onDeleteMap()
{
    if (m_mapId.isEmpty()) return;
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认删除");
    msgBox.setText(QString("确认删除地图「%1」？\n此操作不可撤销。").arg(m_mapName));
    QPushButton *yesBtn = msgBox.addButton("确定", QMessageBox::YesRole);
    msgBox.addButton("取消", QMessageBox::NoRole);
    msgBox.exec();
    if (msgBox.clickedButton() != yesBtn) return;

    auto *loading = new LoadingDialog("正在删除地图...", this);
    loading->show();
    ApiService::instance().delMap(m_mapId, [this, loading](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            m_mapId.clear(); m_mapName.clear();
            m_scene->clear();
            m_mapBg = m_robotItem = m_routeLayer = m_axesLayer = nullptr;
            if (m_mapNameLabel) m_mapNameLabel->setText("已删除");
            setStatus("地图已删除，请切换地图");
            Toast::success(this, "地图已删除");
        } else {
            QMessageBox::warning(this, "错误", "删除地图失败");
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
    QString serverIp = AppConfig::instance().serverIp();
    if (serverIp.isEmpty()) return;

    disconnectMqtt();

    if (!MqttService::instance().isConnected()) {
        MqttService::instance().connectToHost(serverIp, 1883);
    }

    auto cb = [this](const QString &t, const QByteArray &p) {
        onMqttMessage(t, p);
    };

    // 订阅设备 info 话题
    MqttService::instance().subscribeTopic("/info", 0, cb);
    MqttService::instance().subscribeTopic("info", 0, cb);
    MqttService::instance().subscribeTopic("/rob/info", 0, cb);

    // 若绑定了 carId，也订阅特定设备话题
    QString carId = AppConfig::instance().agvId();
    if (carId.isEmpty()) carId = "1";
    m_robot.carId = carId;
    MqttService::instance().subscribeTopic(QString("/rob/%1/info").arg(carId), 0, cb);
}

void MapViewWidget::disconnectMqtt()
{
    MqttService::instance().unsubscribeTopic("/info");
    MqttService::instance().unsubscribeTopic("info");
    MqttService::instance().unsubscribeTopic("/rob/info");
    QString carId = AppConfig::instance().agvId();
    if (carId.isEmpty()) carId = "1";
    MqttService::instance().unsubscribeTopic(QString("/rob/%1/info").arg(carId));
}
