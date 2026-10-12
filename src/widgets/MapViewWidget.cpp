#include "MapViewWidget.h"
#include "core/AppConfig.h"
#include "core/ApiService.h"
#include "core/MqttService.h"
#include "core/RobotInfoManager.h"
#include "core/TeleopController.h"
#include "core/LoadingDialog.h"
#include "core/Toast.h"
#include "core/MotionConfigManager.h"
#include "core/MapPgmUtil.h"
#include "core/RouteFeatureSelect.h"
#include "core/ZipUtil.h"
#include <QDirIterator>
#include "MapUploadDialog.h"
#include "SmapUploadDialog.h"
#include "RobotStatusDialog.h"
#include "AudioManagerDialog.h"
#include "DirectionPadWidget.h"
#include "MotionDialog.h"
#include "EditConfigWidget.h"
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
#include <QProgressDialog>
#include <QPointer>
#include <QMenu>
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
#include <QStackedWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QCheckBox>
#include <QButtonGroup>
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
#include <QTcpSocket>
#include <QElapsedTimer>
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
    } else if (name == "resume" || name == "play") {
        // 继续图标：右向实心圆角三角形
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF tri;
        tri << QPointF(sz*0.30, sz*0.20) << QPointF(sz*0.78, sz*0.50) << QPointF(sz*0.30, sz*0.80);
        p.drawPolygon(tri);
    } else if (name == "more") {
        // 其它图标：三个横向圆点
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(QPointF(c - sz*0.26, c), sz*0.075, sz*0.075);
        p.drawEllipse(QPointF(c, c), sz*0.075, sz*0.075);
        p.drawEllipse(QPointF(c + sz*0.26, c), sz*0.075, sz*0.075);
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
    } else if (name == "config" || name == "settings") {
        // 参数/配置图标：三条滑轨 + 滑块 (Sliders)
        p.setPen(QPen(col, 1.4, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(sz*0.16, sz*0.28), QPointF(sz*0.84, sz*0.28));
        p.drawLine(QPointF(sz*0.16, sz*0.50), QPointF(sz*0.84, sz*0.50));
        p.drawLine(QPointF(sz*0.16, sz*0.72), QPointF(sz*0.84, sz*0.72));
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawRoundedRect(QRectF(sz*0.32, sz*0.20, sz*0.14, sz*0.16), 1.5, 1.5);
        p.drawRoundedRect(QRectF(sz*0.58, sz*0.42, sz*0.14, sz*0.16), 1.5, 1.5);
        p.drawRoundedRect(QRectF(sz*0.26, sz*0.64, sz*0.14, sz*0.16), 1.5, 1.5);
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
    fetchMapNameList();
    connect(&RobotInfoManager::instance(), &RobotInfoManager::currentRobotInfoUpdated,
            this, &MapViewWidget::onRobotInfoUpdated);
    connect(&TeleopController::instance(), &TeleopController::activeChanged,
            this, &MapViewWidget::onTeleopActiveChanged);
    connect(&TeleopController::instance(), &TeleopController::userPreemptedChanged,
            this, [this](bool) { updateNavButtonsState(); });
    onRobotInfoUpdated(RobotInfoManager::instance().currentInfo());
}

MapViewWidget::~MapViewWidget()
{
    disconnectMqtt();
}

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
    splitter->setHandleWidth(5);
    splitter->setStyleSheet(
        "QSplitter::handle{background:#232733;}"
        "QSplitter::handle:hover{background:#3b82f6;}"
        "QSplitter::handle:pressed{background:#1d4ed8;}");

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
    connect(m_canvas, &MapCanvas::sceneDoubleClicked, this, &MapViewWidget::onSceneDoubleClicked);
    connect(m_canvas, &MapCanvas::sceneMoved,    this, &MapViewWidget::onSceneMoved);
    connect(m_canvas, &MapCanvas::sceneReleased, this, &MapViewWidget::onSceneReleased);

    splitter->addWidget(mapArea);

    // Right panel
    auto *rightWidget = new QWidget;
    rightWidget->setMinimumWidth(200);
    rightWidget->setMaximumWidth(900);
    rightWidget->setStyleSheet("background:#1a1d23;");
    setupRightPanel(rightWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({960, 280});

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

    // 10. 拉取地图 (直接打开本地缓存文件夹)
    m_downloadMapBtn = addBtn("file-download", "拉取地图", SLOT(onDownloadSource()));
    m_downloadMapBtn->setToolTip("打开当前地图的本地缓存文件夹");

    // 11. 地图管理 (原 切换地图)
    m_switchMapBtn = addBtn("swap", "地图管理", SLOT(onSwitchMap()));
    m_switchMapBtn->setStyleSheet(kToolBtnPrimary);
    m_switchMapBtn->setToolTip("地图管理：切换、重命名或删除地图");

    // 12. 抢占控制权 (从底栏移入顶栏)
    m_teleopBtn = addBtn("teleop", "抢占控制权", SLOT(onTeleopClicked()));
    m_teleopBtn->setToolTip(
        "点击抢占控制权（暂停导航并接管键盘控制），再次点击恢复导航\n"
        "键盘: W A S D 移动 | Space 刹车 | Ctrl 加速 | P 升降上 | L 升降下");

    // 13. 参数 (从顶栏迁移，弹窗展示)
    m_configBtn = addBtn("config", "参数", SLOT(onShowConfigDialog()));
    m_configBtn->setToolTip("参数配置：查看与修改机器人各项配置参数");

    // 14. 状态
    m_statusBtn = addBtn("status", "状态", SLOT(onShowRobotStatus()));
    m_statusBtn->setToolTip("查看机器人运行状态与详细诊断信息");

    // 14. 其它 (点击弹出工具列表)
    m_moreBtn = addBtn("more", "其它", SLOT(onShowMoreMenu()));
    m_moreBtn->setToolTip("更多工具");
    m_moreMenu = new QMenu(m_moreBtn);
    m_moreMenu->setStyleSheet(
        "QMenu {"
        "  background: #1e2125;"
        "  color: #e2e8f0;"
        "  border: 1px solid #2d3340;"
        "  padding: 4px;"
        "}"
        "QMenu::item {"
        "  padding: 6px 22px;"
        "  border-radius: 3px;"
        "  font-size: 12px;"
        "}"
        "QMenu::item:selected {"
        "  background: #0ea5e9;"
        "  color: #ffffff;"
        "}"
    );
    QAction *audioManagerAct = m_moreMenu->addAction("音频管理器");
    connect(audioManagerAct, &QAction::triggered, this, &MapViewWidget::onShowAudioManager);

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
    auto *outerLay = new QVBoxLayout(parent);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    // ── 顶部标签页: 属性 / 图元 / 图层 ────────────────────────────────────────
    auto *tabBar = new QWidget(parent);
    tabBar->setFixedHeight(30);
    tabBar->setStyleSheet("background:#171b21; border-bottom:1px solid #2d3340;");
    auto *tabLay = new QHBoxLayout(tabBar);
    tabLay->setContentsMargins(6, 3, 6, 3);
    tabLay->setSpacing(4);
    auto mkTab = [&](const QString &text, int idx) -> QPushButton* {
        auto *b = new QPushButton(text);
        b->setCheckable(true);
        b->setFocusPolicy(Qt::NoFocus);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(
            "QPushButton{background:transparent; color:#8898a8; border:none; border-radius:3px;"
            "padding:3px 10px; font-size:12px;}"
            "QPushButton:hover{background:rgba(255,255,255,12);}"
            "QPushButton:checked{background:#22303f; color:#d8e4f0; font-weight:600;}");
        connect(b, &QPushButton::clicked, this, [this, idx]() { setRightTab(idx); });
        tabLay->addWidget(b);
        return b;
    };
    m_tabPropsBtn    = mkTab("属性", TabProps);
    m_tabElementsBtn = mkTab("图元", TabElements);
    m_tabLayersBtn   = mkTab("图层", TabLayers);
    tabLay->addStretch();
    outerLay->addWidget(tabBar);

    m_rightStack = new QStackedWidget(parent);
    m_rightStack->setStyleSheet("background: transparent;");
    outerLay->addWidget(m_rightStack, 1);

    // ── 属性页 (可滑动) ───────────────────────────────────────────────────────
    auto *propsPage = new QWidget;
    auto *propsLay  = new QVBoxLayout(propsPage);
    propsLay->setContentsMargins(0, 0, 0, 0);
    propsLay->setSpacing(0);
    m_rightStack->addWidget(propsPage);

    auto *scrollArea = new QScrollArea(propsPage);
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
    addDeviceGroup(carGroup);
    m_deviceHasData.insert(carGroup);   // 基础设备状态始终显示

    // ── 1b. 设备数据监视 (数据来自 MQTT /rob/{id}/info, 无数据的分区整块隐藏) ──
    auto mkValRow = [&](QFormLayout *form, const QString &label, QLabel *&outVal) {
        outVal = mkInfoLabel();
        form->addRow(label, outVal);
    };

    // 控制屏: 电量 / 实际速度 / 接收速度
    m_ctlGroup = mkGroup("控制屏");
    {
        auto *f = new QFormLayout(m_ctlGroup);
        f->setSpacing(6);
        f->setLabelAlignment(Qt::AlignRight);
        mkValRow(f, "电量:",     m_ctlBatteryVal);
        mkValRow(f, "实际速度:", m_ctlActualVal);
        mkValRow(f, "接收速度:", m_ctlCmdVal);
    }
    lay->addWidget(m_ctlGroup);
    addDeviceGroup(m_ctlGroup);
    m_ctlGroup->setVisible(false);

    // 里程
    m_mileageGroup = mkGroup("里程");
    {
        auto *f = new QFormLayout(m_mileageGroup);
        f->setSpacing(6);
        f->setLabelAlignment(Qt::AlignRight);
        mkValRow(f, "累计里程:", m_mileageTotalVal);
        mkValRow(f, "今日里程:", m_mileageTodayVal);
    }
    lay->addWidget(m_mileageGroup);
    addDeviceGroup(m_mileageGroup);
    m_mileageGroup->setVisible(false);

    // 运行时间
    m_runtimeGroup = mkGroup("运行时间");
    {
        auto *f = new QFormLayout(m_runtimeGroup);
        f->setSpacing(6);
        f->setLabelAlignment(Qt::AlignRight);
        mkValRow(f, "本次运行:", m_runtimeCurVal);
        mkValRow(f, "累计运行:", m_runtimeTotalVal);
    }
    lay->addWidget(m_runtimeGroup);
    addDeviceGroup(m_runtimeGroup);
    m_runtimeGroup->setVisible(false);

    // 障碍
    m_obstacleGroup = mkGroup("障碍");
    {
        auto *f = new QFormLayout(m_obstacleGroup);
        f->setSpacing(6);
        f->setLabelAlignment(Qt::AlignRight);
        mkValRow(f, "类型:",   m_obstacleTypeVal);
        mkValRow(f, "障碍点:", m_obstaclePointVal);
    }
    lay->addWidget(m_obstacleGroup);
    addDeviceGroup(m_obstacleGroup);
    m_obstacleGroup->setVisible(false);

    // 阻挡
    m_blockedGroup = mkGroup("阻挡");
    {
        auto *f = new QFormLayout(m_blockedGroup);
        f->setSpacing(6);
        f->setLabelAlignment(Qt::AlignRight);
        mkValRow(f, "状态:", m_blockedStateVal);
    }
    lay->addWidget(m_blockedGroup);
    addDeviceGroup(m_blockedGroup);
    m_blockedGroup->setVisible(false);

    // 位姿与机台 (EPC)
    m_poseGroup = mkGroup("位姿与机台");
    {
        auto *f = new QFormLayout(m_poseGroup);
        f->setSpacing(6);
        f->setLabelAlignment(Qt::AlignRight);
        mkValRow(f, "坐标:",     m_posePosVal);
        mkValRow(f, "朝向:",     m_poseYawVal);
        mkValRow(f, "机台角度:", m_poseThetaVal);
        mkValRow(f, "末端坐标:", m_poseEpcVal);
    }
    lay->addWidget(m_poseGroup);
    addDeviceGroup(m_poseGroup);
    m_poseGroup->setVisible(false);

    // 机器人信息
    m_robotInfoGroup = mkGroup("机器人信息");
    {
        auto *f = new QFormLayout(m_robotInfoGroup);
        f->setSpacing(6);
        f->setLabelAlignment(Qt::AlignRight);
        mkValRow(f, "车辆ID:", m_robotCarIdVal);
        mkValRow(f, "地图ID:", m_robotMapIdVal);
        mkValRow(f, "任务ID:", m_robotTaskIdVal);
        mkValRow(f, "状态:",   m_robotStatusVal);
    }
    lay->addWidget(m_robotInfoGroup);
    addDeviceGroup(m_robotInfoGroup);
    m_robotInfoGroup->setVisible(false);

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

    // 辅助构造只读带勾选框的属性行 (参考图片2: 置灰不可操作，显示属性选中状态)
    auto mkReadOnlyCheckableRow = [](QCheckBox *&chk, const QString &text, QLabel *valLbl) -> QWidget* {
        auto *w = new QWidget;
        w->setStyleSheet("background:transparent; border:none;");
        auto *h = new QHBoxLayout(w);
        h->setContentsMargins(0, 2, 0, 2);
        h->setSpacing(6);

        auto *leftCont = new QWidget;
        leftCont->setFixedWidth(118);
        leftCont->setStyleSheet("background:transparent; border:none;");
        auto *lh = new QHBoxLayout(leftCont);
        lh->setContentsMargins(0, 0, 0, 0);
        lh->setSpacing(4);

        chk = new QCheckBox;
        chk->setEnabled(false);
        chk->setFixedSize(16, 16);
        chk->setStyleSheet(
            "QCheckBox { background: transparent; }"
            "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #3d4a5d; border-radius: 2px; background: #171c26; }"
            "QCheckBox::indicator:disabled { border: 1px solid #3d4a5d; background: #171c26; }"
            "QCheckBox::indicator:checked:disabled { background: #1e6fd4; border-color: #1e6fd4; image: url(:/check.svg); }"
        );
        lh->addWidget(chk, 0, Qt::AlignVCenter);

        auto *lbl = new QLabel(text);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lbl->setStyleSheet("color: #8fa6bf; font-size: 11px; background: transparent; border: none;");
        lh->addWidget(lbl, 1, Qt::AlignVCenter);

        h->addWidget(leftCont, 0, Qt::AlignLeft | Qt::AlignVCenter);

        valLbl->setStyleSheet("color: #c8d0d8; font-size: 11px; background: transparent; border: none;");
        h->addWidget(valLbl, 1, Qt::AlignVCenter);
        return w;
    };

    // 属性表单容器
    m_stationContentWidget = new QWidget;
    m_stationContentWidget->setStyleSheet("background:transparent; border:none;");
    auto *stLay = new QVBoxLayout(m_stationContentWidget);
    stLay->setContentsMargins(0, 0, 0, 0);
    stLay->setSpacing(4);

    m_stationTypeVal   = mkInfoLabel();
    m_stationIdVal     = mkInfoLabel();
    m_stationNameVal   = mkInfoLabel();
    m_stationPosVal    = mkInfoLabel();
    m_stationYawVal    = mkInfoLabel();
    m_stationDockVal   = mkInfoLabel();
    m_stationRotateVal = mkInfoLabel();
    m_stationPgvVal    = mkInfoLabel();

    stLay->addWidget(mkReadOnlyCheckableRow(m_chkStationType,   "类型:", m_stationTypeVal));
    stLay->addWidget(mkReadOnlyCheckableRow(m_chkStationId,     "编号:", m_stationIdVal));
    stLay->addWidget(mkReadOnlyCheckableRow(m_chkStationName,   "名称:", m_stationNameVal));
    stLay->addWidget(mkReadOnlyCheckableRow(m_chkStationPos,    "坐标:", m_stationPosVal));
    stLay->addWidget(mkReadOnlyCheckableRow(m_chkStationYaw,    "朝向:", m_stationYawVal));
    stLay->addWidget(mkReadOnlyCheckableRow(m_chkStationDock,   "停靠:", m_stationDockVal));
    stLay->addWidget(mkReadOnlyCheckableRow(m_chkStationRotate, "转身:", m_stationRotateVal));
    stLay->addWidget(mkReadOnlyCheckableRow(m_chkStationPgv,    "PGV:",  m_stationPgvVal));

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
    stLay->addWidget(m_stationEpcGroup);
    m_stationEpcGroup->setVisible(false);

    stationMainLay->addWidget(m_stationContentWidget);
    m_stationContentWidget->setVisible(false);
    lay->addWidget(m_stationGroup);

    // ── 2b. Route info (对标"启用编辑"右侧线路属性) ───────────────────────────
    m_routeGroup = mkGroup("路线属性");
    auto *routeMainLay = new QVBoxLayout(m_routeGroup);
    routeMainLay->setContentsMargins(8, 12, 8, 8);
    routeMainLay->setSpacing(6);

    auto *routeBadgeRow = new QHBoxLayout;
    routeBadgeRow->addStretch();
    m_routeBadge = new QLabel;
    m_routeBadge->setStyleSheet("border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;");
    m_routeBadge->setVisible(false);
    routeBadgeRow->addWidget(m_routeBadge);
    routeMainLay->addLayout(routeBadgeRow);

    m_routeEmptyHint = new QLabel("点击地图上的线条\n查看路线信息");
    m_routeEmptyHint->setAlignment(Qt::AlignCenter);
    m_routeEmptyHint->setStyleSheet("color:#556677; font-size:12px; padding:14px 0;");
    routeMainLay->addWidget(m_routeEmptyHint);

    m_routeContentWidget = new QWidget;
    m_routeContentWidget->setStyleSheet("background:transparent; border:none;");
    auto *rtLay = new QVBoxLayout(m_routeContentWidget);
    rtLay->setContentsMargins(0, 0, 0, 0);
    rtLay->setSpacing(4);

    m_routeTypeVal       = mkInfoLabel();
    m_routeIdVal         = mkInfoLabel();
    m_routeNameVal       = mkInfoLabel();
    m_routeStartVal      = mkInfoLabel();
    m_routeEndVal        = mkInfoLabel();
    m_routeDirectVal     = mkInfoLabel();
    m_routeMaxSpeedVal   = mkInfoLabel();
    m_routeMaxAccVal     = mkInfoLabel();
    m_routeMaxDecVal     = mkInfoLabel();
    m_routeMaxRotVal     = mkInfoLabel();
    m_routeMaxRotAccVal  = mkInfoLabel();
    m_routeReachDistVal  = mkInfoLabel();
    m_routeReachAngleVal = mkInfoLabel();
    m_routeWidthVal      = mkInfoLabel();
    m_routeObsRangeVal   = mkInfoLabel();

    rtLay->addWidget(mkReadOnlyCheckableRow(m_chkRouteType,       "线路类型:",     m_routeTypeVal));
    rtLay->addWidget(mkReadOnlyCheckableRow(m_chkRouteId,         "线路编号:",     m_routeIdVal));
    rtLay->addWidget(mkReadOnlyCheckableRow(m_chkRouteName,       "线路名称:",     m_routeNameVal));
    rtLay->addWidget(mkReadOnlyCheckableRow(m_chkRouteStart,      "起点:",         m_routeStartVal));
    rtLay->addWidget(mkReadOnlyCheckableRow(m_chkRouteEnd,        "终点:",         m_routeEndVal));
    rtLay->addWidget(mkReadOnlyCheckableRow(m_chkRouteDirect,     "运动方向:",     m_routeDirectVal));
    rtLay->addWidget(mkReadOnlyCheckableRow(m_chkRouteMaxSpeed,   "最大速度:",     m_routeMaxSpeedVal));
    rtLay->addWidget(mkReadOnlyCheckableRow(m_chkRouteMaxAcc,     "最大加速度:",   m_routeMaxAccVal));
    rtLay->addWidget(mkReadOnlyCheckableRow(m_chkRouteMaxRot,     "最大角速度:",   m_routeMaxRotVal));
    rtLay->addWidget(mkReadOnlyCheckableRow(m_chkRouteMaxRotAcc,  "最大角加减:",   m_routeMaxRotAccVal));
    rtLay->addWidget(mkReadOnlyCheckableRow(m_chkRouteReachDist,  "到达距离精度:", m_routeReachDistVal));
    rtLay->addWidget(mkReadOnlyCheckableRow(m_chkRouteReachAngle, "到达角度精度:", m_routeReachAngleVal));
    rtLay->addWidget(mkReadOnlyCheckableRow(m_chkRouteWidth,      "路线宽度:",     m_routeWidthVal));
    rtLay->addWidget(mkReadOnlyCheckableRow(m_chkRouteObsRange,   "避障范围:",     m_routeObsRangeVal));

    routeMainLay->addWidget(m_routeContentWidget);
    m_routeContentWidget->setVisible(false);
    lay->addWidget(m_routeGroup);

    // ── 3. Map info (点击地图空白处显示) ───────────────────────────────────────
    auto *infoGroup = mkGroup("地图属性");
    m_mapPropsGroup = infoGroup;
    auto *infoForm  = new QFormLayout(infoGroup);
    infoForm->setSpacing(6);
    infoForm->setLabelAlignment(Qt::AlignRight);

    m_infoNameLabel   = mkInfoLabel();
    m_mapPropIdVal    = mkInfoLabel();
    m_infoSizeLabel   = mkInfoLabel();
    m_infoResLabel    = mkInfoLabel();
    m_mapPropAreaVal  = mkInfoLabel();
    m_infoNameLabel->setWordWrap(true);

    infoForm->addRow("地图名称:", m_infoNameLabel);
    infoForm->addRow("地图编号:", m_mapPropIdVal);
    infoForm->addRow("尺寸:",     m_infoSizeLabel);
    infoForm->addRow("分辨率:",   m_infoResLabel);
    infoForm->addRow("面积:",     m_mapPropAreaVal);
    lay->addWidget(infoGroup);
    addMapGroup(infoGroup);

    // ── 3b. 图元数量 ──────────────────────────────────────────────────────────
    m_mapCountGroup = mkGroup("图元数量");
    auto *countForm = new QFormLayout(m_mapCountGroup);
    countForm->setSpacing(6);
    countForm->setLabelAlignment(Qt::AlignRight);
    m_mapCountPointVal  = mkInfoLabel("0");
    m_mapCountHpVal     = mkInfoLabel("0");
    m_mapCountChargeVal = mkInfoLabel("0");
    m_mapCountLineVal   = mkInfoLabel("0");
    m_mapCountCurveVal  = mkInfoLabel("0");
    countForm->addRow("普通点:",   m_mapCountPointVal);
    countForm->addRow("高精定位点:", m_mapCountHpVal);
    countForm->addRow("充电桩:",   m_mapCountChargeVal);
    countForm->addRow("路线:",     m_mapCountLineVal);
    countForm->addRow("曲线:",     m_mapCountCurveVal);
    lay->addWidget(m_mapCountGroup);
    addMapGroup(m_mapCountGroup);

    auto *hint = new QLabel(
        "<small style='color:#556677;'>"
        "左键点击点位/线条: 查看属性<br>"
        "双击点位/线条: 放大并居中<br>"
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
    propsLay->addWidget(scrollArea);

    // ── 图元页 (列表 + 搜索 + 点击定位) ────────────────────────────────────────
    auto *elemPage = new QWidget;
    auto *elemLay  = new QVBoxLayout(elemPage);
    elemLay->setContentsMargins(8, 8, 8, 8);
    elemLay->setSpacing(6);

    m_elemSearchEdit = new QLineEdit(elemPage);
    m_elemSearchEdit->setPlaceholderText("输入搜索关键字");
    m_elemSearchEdit->setClearButtonEnabled(true);
    m_elemSearchEdit->setStyleSheet(
        "QLineEdit{background:#12151a; border:1px solid #2d3340; border-radius:3px;"
        " padding:3px 6px; color:#c8d0d8; font-size:12px;}"
        "QLineEdit:focus{border:1px solid #2563eb;}");
    elemLay->addWidget(m_elemSearchEdit);

    m_elemList = new QListWidget(elemPage);
    m_elemList->setStyleSheet(
        "QListWidget{background:#12151a; border:1px solid #2d3340; border-radius:3px;"
        " color:#c8d0d8; font-size:12px; outline:none;}"
        "QListWidget::item{padding:3px 5px;}"
        "QListWidget::item:selected{background:#1e3a5f; color:#d8e4f0;}");
    elemLay->addWidget(m_elemList, 1);
    m_rightStack->addWidget(elemPage);

    // ── 图层页 (隐藏选中图元 / 隐藏所有站点 / 隐藏所有线路) ──────────────────────
    auto *layerPage = new QWidget;
    auto *layerLay  = new QVBoxLayout(layerPage);
    layerLay->setContentsMargins(8, 8, 8, 8);
    layerLay->setSpacing(6);

    auto mkLayerChk = [&](const QString &text) {
        auto *c = new QCheckBox(text, layerPage);
        c->setStyleSheet("QCheckBox{color:#c8d0d8; font-size:12px; padding:2px 0;}");
        layerLay->addWidget(c);
        return c;
    };
    m_hideSelectedChk = mkLayerChk("隐藏选中图元");
    m_hideStationsChk = mkLayerChk("隐藏所有站点");
    m_hideRoutesChk   = mkLayerChk("隐藏所有线路");

    m_showAllElemsBtn = new QPushButton("显示所有图元", layerPage);
    m_showAllElemsBtn->setCursor(Qt::PointingHandCursor);
    m_showAllElemsBtn->setStyleSheet(
        "QPushButton{background:#1e2530; color:#c8d0d8; border:1px solid #2d3340;"
        " border-radius:3px; padding:4px 8px; font-size:12px;}"
        "QPushButton:hover{background:#26303c;}");
    layerLay->addWidget(m_showAllElemsBtn);
    layerLay->addStretch();
    m_rightStack->addWidget(layerPage);

    // ── 信号连接 ─────────────────────────────────────────────────────────────
    connect(m_elemSearchEdit, &QLineEdit::textChanged, this, [this](const QString &kw) {
        for (int i = 0; i < m_elemList->count(); ++i) {
            QListWidgetItem *it = m_elemList->item(i);
            it->setHidden(!kw.isEmpty() && !it->text().contains(kw, Qt::CaseInsensitive));
        }
    });
    connect(m_elemList, &QListWidget::itemClicked, this, &MapViewWidget::onElementItemClicked);
    connect(m_hideSelectedChk, &QCheckBox::toggled, this, [this](bool on) {
        if (on) {
            m_layerHiddenItem = m_selItem;
            if (m_layerHiddenItem) m_layerHiddenItem->setVisible(false);
        } else if (m_layerHiddenItem) {
            m_layerHiddenItem->setVisible(true);
            m_layerHiddenItem = nullptr;
        }
    });
    connect(m_hideStationsChk, &QCheckBox::toggled, this, [this](bool) { applyLayerVisibility(); });
    connect(m_hideRoutesChk,   &QCheckBox::toggled, this, [this](bool) { applyLayerVisibility(); });
    connect(m_showAllElemsBtn, &QPushButton::clicked, this, &MapViewWidget::onShowAllElements);

    setRightTab(TabProps);
}

// ══ 右侧面板: 属性 / 图元 / 图层 ═════════════════════════════════════════════

void MapViewWidget::setRightTab(int tab)
{
    m_rightTab = tab;
    if (m_rightStack)     m_rightStack->setCurrentIndex(tab);
    if (m_tabPropsBtn)    m_tabPropsBtn->setChecked(tab == TabProps);
    if (m_tabElementsBtn) m_tabElementsBtn->setChecked(tab == TabElements);
    if (m_tabLayersBtn)   m_tabLayersBtn->setChecked(tab == TabLayers);

    // 只有"属性"页响应地图点击; 切到图元/图层时清除地图选中高亮
    if (tab == TabProps) {
        switchPropsView(m_propsView);
    } else {
        clearMapSelection();
    }
    if (tab == TabElements) refreshElementList();
}

void MapViewWidget::switchPropsView(int view)
{
    m_propsView = view;
    const bool onProps = (m_rightTab == TabProps);

    for (QWidget *g : m_deviceGroups) {
        if (!g) continue;
        g->setVisible(onProps && view == ViewDevice && m_deviceHasData.contains(g));
    }
    if (m_stationGroup) m_stationGroup->setVisible(onProps && view == ViewStation);
    if (m_routeGroup)   m_routeGroup->setVisible(onProps && view == ViewRoute);
    for (QWidget *g : m_mapGroups) {
        if (g) g->setVisible(onProps && view == ViewMap);
    }
    if (onProps && view == ViewMap) updateMapPropsView();
}

// ── 名称辅助 ─────────────────────────────────────────────────────────────────

QString MapViewWidget::stationNameById(const QString &id) const
{
    if (id.isEmpty()) return QString();
    const QJsonArray *arrs[] = { &m_points, &m_charges, &m_highPrecisionPoints };
    for (const QJsonArray *a : arrs) {
        for (const auto &v : *a) {
            QJsonObject o = v.toObject();
            if (o.value("id").toString() == id) {
                QString n = o.value("name").toString();
                return n.isEmpty() ? id : n;
            }
        }
    }
    return id;
}

QString MapViewWidget::routeFeatureName(const QJsonObject &obj) const
{
    QString n = obj.value("name").toString();
    if (!n.isEmpty()) return n;
    QString s = stationNameById(obj.value("startid").toString());
    QString e = stationNameById(obj.value("endid").toString());
    if (!s.isEmpty() && !e.isEmpty()) return QString("%1 ⟷ %2").arg(s, e);
    return obj.value("id").toString();
}

// ── 设备数据监视 (MQTT /rob/{id}/info, 无数据的分区整块隐藏) ──────────────────

void MapViewWidget::updateDeviceMonitor(const RobotInfo &info)
{
    // 控制屏: 电量 / 实际速度 / 接收速度
    if (m_ctlGroup) {
        auto velText = [](const VelocityInfo &v) {
            return QString("%1 m/s  %2 m/s  %3 °/s")
                .arg(v.vx, 0, 'f', 3).arg(v.vy, 0, 'f', 3)
                .arg(v.wz * 180.0 / M_PI, 0, 'f', 3);
        };
        bool has = (info.battery >= 0.0) || info.actual_vel.valid || info.cmd_vel.valid;
        if (m_ctlBatteryVal) m_ctlBatteryVal->setText(info.battery >= 0.0 ? QString("%1%").arg(info.batteryPercent()) : "--");
        if (m_ctlActualVal)  m_ctlActualVal->setText(info.actual_vel.valid ? velText(info.actual_vel) : "--");
        if (m_ctlCmdVal)     m_ctlCmdVal->setText(info.cmd_vel.valid ? velText(info.cmd_vel) : "--");
        if (has) m_deviceHasData.insert(m_ctlGroup); else m_deviceHasData.remove(m_ctlGroup);
    }

    // 里程
    if (m_mileageGroup) {
        if (info.mileage.valid) {
            if (m_mileageTotalVal) m_mileageTotalVal->setText(QString("%1 m").arg(info.mileage.total_distance_m, 0, 'f', 2));
            if (m_mileageTodayVal) m_mileageTodayVal->setText(QString("%1 m").arg(info.mileage.today_distance_m, 0, 'f', 2));
            m_deviceHasData.insert(m_mileageGroup);
        } else {
            m_deviceHasData.remove(m_mileageGroup);
        }
    }

    // 运行时间
    if (m_runtimeGroup) {
        if (info.runtime.valid) {
            if (m_runtimeCurVal)   m_runtimeCurVal->setText(RobotInfoManager::formatDuration(info.runtime.current_runtime_sec));
            if (m_runtimeTotalVal) m_runtimeTotalVal->setText(RobotInfoManager::formatDuration(info.runtime.total_runtime_sec));
            m_deviceHasData.insert(m_runtimeGroup);
        } else {
            m_deviceHasData.remove(m_runtimeGroup);
        }
    }

    // 障碍 / 阻挡
    if (m_obstacleGroup) {
        if (info.obstacle.valid) {
            if (m_obstacleTypeVal) m_obstacleTypeVal->setText(info.obstacle.blocked ? "已检测到障碍" : "无障碍");
            if (m_obstaclePointVal) {
                m_obstaclePointVal->setText(info.obstacle.hasPoint
                    ? QString("(%1, %2) m").arg(info.obstacle.point.x, 0, 'f', 3)
                                           .arg(info.obstacle.point.y, 0, 'f', 3)
                    : "无");
            }
            m_deviceHasData.insert(m_obstacleGroup);
        } else {
            m_deviceHasData.remove(m_obstacleGroup);
        }
    }
    if (m_blockedGroup) {
        if (info.obstacle.valid) {
            if (m_blockedStateVal) m_blockedStateVal->setText(info.obstacle.blocked ? "已阻挡" : "未阻挡");
            m_deviceHasData.insert(m_blockedGroup);
        } else {
            m_deviceHasData.remove(m_blockedGroup);
        }
    }

    // 位姿与机台
    if (m_poseGroup) {
        if (info.hasPose && m_posePosVal) {
            m_posePosVal->setText(QString("(%1, %2, %3) m")
                .arg(info.car_x, 0, 'f', 3).arg(info.car_y, 0, 'f', 3).arg(info.car_z, 0, 'f', 3));
        } else if (m_posePosVal) {
            m_posePosVal->setText("--");
        }
        if (m_poseYawVal) {
            m_poseYawVal->setText(info.hasPose
                ? QString("%1°").arg(info.car_yaw * 180.0 / M_PI, 0, 'f', 1)
                : "--");
        }
        if (m_poseThetaVal) m_poseThetaVal->setText(info.hasTheta ? QString("%1°").arg(info.theta, 0, 'f', 1) : "--");
        if (m_poseEpcVal) {
            m_poseEpcVal->setText(info.hasEpc
                ? QString("(%1, %2) m, %3°")
                      .arg(info.epc_x, 0, 'f', 3).arg(info.epc_y, 0, 'f', 3).arg(info.epc_theta * 180.0 / M_PI, 0, 'f', 1)
                : "--");
        }
        if (info.hasPose || info.hasTheta || info.hasEpc) m_deviceHasData.insert(m_poseGroup);
        else                                             m_deviceHasData.remove(m_poseGroup);
    }

    // 机器人信息
    if (m_robotInfoGroup) {
        bool has = (info.car_id != 0) || !info.status.isEmpty() || (info.map_id != 0) || (info.task_id >= 0);
        if (m_robotCarIdVal)  m_robotCarIdVal->setText(info.car_id != 0 ? QString::number(info.car_id) : "--");
        if (m_robotMapIdVal)  m_robotMapIdVal->setText(info.map_id != 0 ? lookupMapName(info.map_id) : "--");
        if (m_robotTaskIdVal) m_robotTaskIdVal->setText(info.task_id >= 0 ? QString::number(info.task_id) : "无");
        if (m_robotStatusVal) {
            m_robotStatusVal->setText(info.statusText());
            m_robotStatusVal->setStyleSheet(QString("color:%1; font-size:12px; font-weight:600;").arg(info.statusColor()));
        }
        if (has) m_deviceHasData.insert(m_robotInfoGroup); else m_deviceHasData.remove(m_robotInfoGroup);
    }

    if (m_rightTab == TabProps && m_propsView == ViewDevice) switchPropsView(ViewDevice);
}

// ── 地图属性 (点击地图空白) ───────────────────────────────────────────────────

void MapViewWidget::updateMapPropsView()
{
    if (m_infoNameLabel) m_infoNameLabel->setText(m_mapName.isEmpty() ? "--" : m_mapName);
    if (m_mapPropIdVal)  m_mapPropIdVal->setText(m_mapId.isEmpty() ? "--" : m_mapId);
    if (m_infoSizeLabel) m_infoSizeLabel->setText(QString("%1 × %2 px").arg((int)m_imgW).arg((int)m_imgH));
    if (m_infoResLabel) {
        double resX = (m_imgW > 0) ? (m_worldR - m_worldL) / m_imgW : 0.0;
        m_infoResLabel->setText(QString("%1 m/px").arg(resX, 0, 'f', 4));
    }
    if (m_mapPropAreaVal) {
        double w = m_worldR - m_worldL, h = m_worldT - m_worldB;
        m_mapPropAreaVal->setText((w > 0 && h > 0)
            ? QString("%1 × %2 m  (%3 m²)").arg(w, 0, 'f', 2).arg(h, 0, 'f', 2).arg(w * h, 0, 'f', 2)
            : "--");
    }
    if (m_mapCountPointVal)  m_mapCountPointVal->setText(QString::number(m_points.size()));
    if (m_mapCountHpVal)     m_mapCountHpVal->setText(QString::number(m_highPrecisionPoints.size()));
    if (m_mapCountChargeVal) m_mapCountChargeVal->setText(QString::number(m_charges.size()));
    if (m_mapCountLineVal)   m_mapCountLineVal->setText(QString::number(m_lines.size()));
    if (m_mapCountCurveVal)  m_mapCountCurveVal->setText(QString::number(m_curves.size()));
}

// ── 图元列表 ─────────────────────────────────────────────────────────────────

void MapViewWidget::refreshElementList()
{
    if (!m_elemList) return;
    m_elemList->clear();

    int n = 0;
    auto add = [&](const QJsonObject &o, const QString &type, const QString &disp) {
        ++n;
        auto *it = new QListWidgetItem(QString("%1  %2").arg(n).arg(disp));
        it->setData(Qt::UserRole,     type);
        it->setData(Qt::UserRole + 1, o.value("id").toString());
        m_elemList->addItem(it);
    };
    auto label = [](const QJsonObject &o) {
        QString name = o.value("name").toString();
        return name.isEmpty() ? o.value("id").toString() : name;
    };

    for (const auto &v : m_points)              { QJsonObject o = v.toObject(); add(o, "point",         label(o)); }
    for (const auto &v : m_highPrecisionPoints) { QJsonObject o = v.toObject(); add(o, "highPrecision", label(o)); }
    for (const auto &v : m_charges)             { QJsonObject o = v.toObject(); add(o, "charge",        label(o)); }
    for (const auto &v : m_lines)               { QJsonObject o = v.toObject(); add(o, "line",  routeFeatureName(o)); }
    for (const auto &v : m_curves)              { QJsonObject o = v.toObject(); add(o, "curve", routeFeatureName(o)); }

    // 保持搜索过滤状态
    const QString kw = m_elemSearchEdit ? m_elemSearchEdit->text() : QString();
    for (int i = 0; i < m_elemList->count(); ++i) {
        QListWidgetItem *it = m_elemList->item(i);
        it->setHidden(!kw.isEmpty() && !it->text().contains(kw, Qt::CaseInsensitive));
    }
}

void MapViewWidget::onElementItemClicked(QListWidgetItem *item)
{
    if (!item) return;
    selectFeatureOnMap(item->data(Qt::UserRole).toString(),
                       item->data(Qt::UserRole + 1).toString(), true);
}

void MapViewWidget::syncElementListSelection(const QString &type, const QString &id)
{
    if (!m_elemList) return;
    for (int i = 0; i < m_elemList->count(); ++i) {
        QListWidgetItem *it = m_elemList->item(i);
        if (it->data(Qt::UserRole).toString() == type
            && it->data(Qt::UserRole + 1).toString() == id) {
            m_elemList->setCurrentItem(it);
            return;
        }
    }
}

void MapViewWidget::selectFeatureOnMap(const QString &type, const QString &id, bool center)
{
    if (!m_scene) return;

    QGraphicsItem *found = nullptr;
    for (QGraphicsItem *it : m_routeTopLevelItems) {
        if (!it) continue;
        if (it->data(0).toString() == type && it->data(1).toString() == id) { found = it; break; }
    }
    if (!found) return;

    if (found != m_selItem) {
        RouteFeatureSelect::clearSelectColor(m_selItem);
        m_selItem = found;
    }
    found->setVisible(true);
    RouteFeatureSelect::applySelectColor(found);

    // 属性页内容跟随选中图元切换
    if (type == "line" || type == "curve") {
        const QJsonArray &arr = (type == "line") ? m_lines : m_curves;
        QJsonObject obj;
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            if (o.value("id").toString() == id) { obj = o; break; }
        }
        if (obj.isEmpty()) obj["id"] = id;
        showRouteDetails(obj, type);
        if (m_rightTab != TabProps) setRightTab(TabProps);
        switchPropsView(ViewRoute);
    } else {
        const QJsonArray &arr = (type == "charge")        ? m_charges
                              : (type == "highPrecision") ? m_highPrecisionPoints : m_points;
        QJsonObject obj;
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            if (o.value("id").toString() == id) { obj = o; break; }
        }
        if (!obj.isEmpty()) showStationDetails(obj, type);
        if (m_rightTab != TabProps) setRightTab(TabProps);
        switchPropsView(ViewStation);
    }

    syncElementListSelection(type, id);

    if (center && m_canvas) {
        m_canvas->centerOn(found->sceneBoundingRect().center());
        updateRulers();
    }
}

void MapViewWidget::clearMapSelection()
{
    RouteFeatureSelect::clearSelectColor(m_selItem);
    m_selItem = nullptr;
}

// ── 图层可见性 ───────────────────────────────────────────────────────────────

void MapViewWidget::applyLayerVisibility()
{
    const bool hideStations = m_hideStationsChk && m_hideStationsChk->isChecked();
    const bool hideRoutes   = m_hideRoutesChk   && m_hideRoutesChk->isChecked();

    for (QGraphicsItem *it : m_routeTopLevelItems) {
        if (!it || it == m_layerHiddenItem) continue;   // "隐藏选中图元"单独管理
        const QString t = it->data(0).toString();
        const bool isRoute = (t == "line" || t == "curve");
        it->setVisible(isRoute ? !hideRoutes : !hideStations);
    }
}

void MapViewWidget::onShowAllElements()
{
    if (m_hideSelectedChk) m_hideSelectedChk->setChecked(false);
    if (m_hideStationsChk) m_hideStationsChk->setChecked(false);
    if (m_hideRoutesChk)   m_hideRoutesChk->setChecked(false);
    if (m_layerHiddenItem) { m_layerHiddenItem->setVisible(true); m_layerHiddenItem = nullptr; }
    applyLayerVisibility();
}

// ── 双击站点/线路: 放大并居中 ────────────────────────────────────────────────

void MapViewWidget::onSceneDoubleClicked(const QPointF &scenePos, Qt::MouseButton btn)
{
    if (btn != Qt::LeftButton || m_rightTab != TabProps || !m_scene) return;

    QGraphicsItem *picked = RouteFeatureSelect::pickItem(m_scene, scenePos, hitTolScene(12.0));
    if (!picked) return;
    const QString t = picked->data(0).toString();
    if (t.isEmpty()) return;   // 只处理站点与线路

    if (m_canvas) {
        m_canvas->scale(1.4, 1.4);
        m_canvas->centerOn(picked->sceneBoundingRect().center());
    }
    updateRulers();
    selectFeatureOnMap(t, picked->data(1).toString());
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

    QSet<QString> chkSet;
    QJsonArray cpArr = pt.value("checked_props").toArray();
    for (const auto &v : cpArr) chkSet.insert(v.toString());

    if (m_chkStationType)   m_chkStationType->setChecked(chkSet.contains("stationType"));
    if (m_stationTypeVal)   m_stationTypeVal->setText(typeText);

    if (m_chkStationId)     m_chkStationId->setChecked(chkSet.contains("id"));
    if (m_stationIdVal)     m_stationIdVal->setText(id);

    if (m_chkStationName)   m_chkStationName->setChecked(chkSet.contains("name"));
    if (m_stationNameVal)   m_stationNameVal->setText(name);

    double x = pt.value("x").toDouble();
    double y = pt.value("y").toDouble();
    if (m_chkStationPos)    m_chkStationPos->setChecked(chkSet.contains("x") || chkSet.contains("y"));
    if (m_stationPosVal) {
        m_stationPosVal->setText(QString("(%1, %2) m").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3));
    }

    bool hasYaw = false;
    double yawVal = 0.0;
    if (pt.contains("yaw") && !pt.value("yaw").isNull()) {
        yawVal = pt.value("yaw").toVariant().toDouble();
        hasYaw = true;
    } else if (pt.contains("theta") && !pt.value("theta").isNull()) {
        yawVal = pt.value("theta").toVariant().toDouble() * M_PI / 180.0;
        hasYaw = true;
    } else if (pt.contains("robotYaw") && !pt.value("robotYaw").isNull()) {
        yawVal = pt.value("robotYaw").toVariant().toDouble();
        hasYaw = true;
    }
    if (m_chkStationYaw)    m_chkStationYaw->setChecked(chkSet.contains("yaw"));
    if (m_stationYawVal) {
        m_stationYawVal->setText(QString("%1°").arg(yawVal * 180.0 / M_PI, 0, 'f', 1));
    }

    if (m_chkStationDock)   m_chkStationDock->setChecked(chkSet.contains("rdsDockable"));
    if (m_stationDockVal) {
        m_stationDockVal->setText(pt.value("rdsDockable").toBool(true) ? "允许停靠" : "禁止停靠");
    }

    if (m_chkStationRotate) m_chkStationRotate->setChecked(chkSet.contains("allowRotate"));
    if (m_stationRotateVal) {
        m_stationRotateVal->setText(pt.value("allowRotate").toBool(true) ? "允许转身" : "禁止转身");
    }

    if (m_chkStationPgv)    m_chkStationPgv->setChecked(chkSet.contains("useDownPGV"));
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
            m_stationEpcYawVal->setText(QString("%1°").arg(epcTheta * 180.0 / M_PI, 0, 'f', 1));
        }
        if (m_stationEpcGroup) m_stationEpcGroup->setVisible(true);
    } else {
        if (m_stationEpcGroup) m_stationEpcGroup->setVisible(false);
    }

    if (m_stationEmptyHint)     m_stationEmptyHint->setVisible(false);
    if (m_stationContentWidget) m_stationContentWidget->setVisible(true);
}

void MapViewWidget::clearStationDetails()
{
    m_selectedStationId.clear();
    if (m_stationBadge) {
        m_stationBadge->setText("");
        m_stationBadge->setVisible(false);
    }
    if (m_chkStationType)   m_chkStationType->setChecked(false);
    if (m_chkStationId)     m_chkStationId->setChecked(false);
    if (m_chkStationName)   m_chkStationName->setChecked(false);
    if (m_chkStationPos)    m_chkStationPos->setChecked(false);
    if (m_chkStationYaw)    m_chkStationYaw->setChecked(false);
    if (m_chkStationDock)   m_chkStationDock->setChecked(false);
    if (m_chkStationRotate) m_chkStationRotate->setChecked(false);
    if (m_chkStationPgv)    m_chkStationPgv->setChecked(false);
    if (m_stationContentWidget) m_stationContentWidget->setVisible(false);
    if (m_stationEmptyHint)     m_stationEmptyHint->setVisible(true);
}

// ── 命中容差 (与"启用编辑"完全一致) ──────────────────────────────────────────

double MapViewWidget::hitTolScene(double screenPx) const
{
    // 场景坐标与屏幕像素并非 1:1，固定场景容差在缩放后会越来越难点中；
    // 这里除以当前缩放比例，使命中范围始终保持约 screenPx 个屏幕像素。
    double s = m_canvas ? m_canvas->transform().m11() : 1.0;
    return s > 1e-6 ? screenPx / s : screenPx;
}

// ── 路线属性面板 (对齐"启用编辑"线路属性) ─────────────────────────────────────

void MapViewWidget::showRouteDetails(const QJsonObject &obj, const QString &type)
{
    bool isLine = (type == "line");

    if (m_routeBadge) {
        m_routeBadge->setText(isLine ? "直线" : "贝塞尔曲线");
        m_routeBadge->setStyleSheet("background:#17344f; color:#38bdf8; border:1px solid #0284c7; border-radius:3px; padding:1px 6px; font-size:10px; font-weight:bold;");
        m_routeBadge->setVisible(true);
    }

    QSet<QString> chkSet;
    QJsonArray cpArr = obj.value("checked_props").toArray();
    for (const auto &v : cpArr) chkSet.insert(v.toString());

    if (m_chkRouteType)       m_chkRouteType->setChecked(chkSet.contains("routeType"));
    if (m_routeTypeVal)       m_routeTypeVal->setText(isLine ? "直线" : "贝塞尔曲线");

    if (m_chkRouteId)         m_chkRouteId->setChecked(chkSet.contains("id"));
    if (m_routeIdVal)         m_routeIdVal->setText(obj.value("id").toString());

    bool hasName = obj.contains("name") && !obj.value("name").toString().isEmpty();
    if (m_chkRouteName)       m_chkRouteName->setChecked(chkSet.contains("name"));
    if (m_routeNameVal)       m_routeNameVal->setText(obj.value("name").toString("--"));

    QString sId = obj.value("startid").toString();
    QString eId = obj.value("endid").toString();
    if (m_chkRouteStart)      m_chkRouteStart->setChecked(chkSet.contains("startid"));
    if (m_routeStartVal)      m_routeStartVal->setText(sId.isEmpty() ? "--" : stationNameById(sId));

    if (m_chkRouteEnd)        m_chkRouteEnd->setChecked(chkSet.contains("endid"));
    if (m_routeEndVal)        m_routeEndVal->setText(eId.isEmpty() ? "--" : stationNameById(eId));

    if (m_chkRouteDirect)     m_chkRouteDirect->setChecked(chkSet.contains("direct"));
    int direct = obj.value("direct").toInt(10);
    QString directText = (direct == 10) ? "双向 ↔" : ((direct == 0) ? "单向前进 →" : "单向后退 ←");
    if (m_routeDirectVal)     m_routeDirectVal->setText(directText);

    const auto &mc = MotionConfigManager::instance();
    if (m_chkRouteMaxSpeed)   m_chkRouteMaxSpeed->setChecked(chkSet.contains("maxspeed"));
    if (m_routeMaxSpeedVal)
        m_routeMaxSpeedVal->setText(QString("%1 m/s").arg(mc.clampLinearSpeed(obj.value("maxspeed").toDouble(mc.maxSpeed())), 0, 'f', 2));

    bool hasAcc = chkSet.contains("maxacc") || chkSet.contains("max_acceleration");
    if (m_chkRouteMaxAcc)     m_chkRouteMaxAcc->setChecked(hasAcc);
    if (m_routeMaxAccVal) {
        double acc = obj.contains("max_acceleration") ? obj.value("max_acceleration").toDouble() : obj.value("maxacc").toDouble(mc.maxAcceleration());
        m_routeMaxAccVal->setText(QString("%1 m/s²").arg(qMin(acc, mc.maxAcceleration()), 0, 'f', 2));
    }
    if (m_routeMaxDecVal) {
        double dec = obj.contains("max_deceleration") ? obj.value("max_deceleration").toDouble() : obj.value("maxdec").toDouble(mc.maxDeceleration());
        m_routeMaxDecVal->setText(QString("%1 m/s²").arg(qMin(dec, mc.maxDeceleration()), 0, 'f', 2));
    }

    bool hasRot = chkSet.contains("maxrot") || chkSet.contains("max_angular_speed");
    if (m_chkRouteMaxRot)     m_chkRouteMaxRot->setChecked(hasRot);
    if (m_routeMaxRotVal) {
        double rot = obj.contains("max_angular_speed") ? obj.value("max_angular_speed").toDouble() : obj.value("maxrot").toDouble(mc.maxAngularSpeedDeg());
        m_routeMaxRotVal->setText(QString("%1 °/s").arg(qMin(rot, mc.maxAngularSpeedDeg()), 0, 'f', 1));
    }

    bool hasRotAcc = chkSet.contains("maxrotacc") || chkSet.contains("max_angular_acceleration");
    if (m_chkRouteMaxRotAcc)  m_chkRouteMaxRotAcc->setChecked(hasRotAcc);
    if (m_routeMaxRotAccVal) {
        double rotAcc = obj.contains("max_angular_acceleration") ? obj.value("max_angular_acceleration").toDouble() : obj.value("maxrotacc").toDouble(mc.maxAngularAccelerationDeg());
        m_routeMaxRotAccVal->setText(QString("%1 °/s²").arg(qMin(rotAcc, mc.maxAngularAccelerationDeg()), 0, 'f', 1));
    }

    if (m_chkRouteReachDist)  m_chkRouteReachDist->setChecked(chkSet.contains("reachdist"));
    if (m_routeReachDistVal)
        m_routeReachDistVal->setText(QString("%1 m").arg(obj.value("reachdist").toDouble(mc.arrivalDistanceTolerance()), 0, 'f', 2));

    if (m_chkRouteReachAngle) m_chkRouteReachAngle->setChecked(chkSet.contains("reachangle"));
    if (m_routeReachAngleVal)
        m_routeReachAngleVal->setText(QString("%1 °").arg(obj.value("reachangle").toDouble(mc.arrivalAngleToleranceDeg()), 0, 'f', 1));

    if (m_chkRouteWidth)      m_chkRouteWidth->setChecked(chkSet.contains("width"));
    if (m_routeWidthVal)
        m_routeWidthVal->setText(QString("%1 m").arg(obj.value("width").toDouble(mc.routeWidth()), 0, 'f', 2));

    bool hasObs = chkSet.contains("intrusion_stop_length") || chkSet.contains("intrusion_stop_width") ||
                  chkSet.contains("intrusion_deceleration_length") || chkSet.contains("intrusion_deceleration_width");
    if (m_chkRouteObsRange)   m_chkRouteObsRange->setChecked(hasObs);
    if (m_routeObsRangeVal) {
        double sl = obj.value("intrusion_stop_length").toDouble(0.0);
        double sw = obj.value("intrusion_stop_width").toDouble(0.0);
        double dl = obj.value("intrusion_deceleration_length").toDouble(0.0);
        double dw = obj.value("intrusion_deceleration_width").toDouble(0.0);
        auto fmtVal = [](double v) -> QString {
            if (qFuzzyIsNull(v)) return "0";
            QString s = QString::number(v, 'f', 2);
            while (s.contains('.') && (s.endsWith('0') || s.endsWith('.'))) {
                if (s.endsWith('.')) { s.chop(1); break; }
                s.chop(1);
            }
            return s;
        };
        m_routeObsRangeVal->setText(QString("[%1,%2,%3,%4]")
                                    .arg(fmtVal(sl))
                                    .arg(fmtVal(sw))
                                    .arg(fmtVal(dl))
                                    .arg(fmtVal(dw)));
        m_routeObsRangeVal->setToolTip(QString("正向停止: %1m, 侧向停止: %2m\n正向减速: %3m, 侧向减速: %4m")
                                       .arg(sl).arg(sw).arg(dl).arg(dw));
    }

    if (m_routeEmptyHint)     m_routeEmptyHint->setVisible(false);
    if (m_routeContentWidget) m_routeContentWidget->setVisible(true);
}

void MapViewWidget::clearRouteDetails()
{
    if (m_routeBadge) {
        m_routeBadge->setText("");
        m_routeBadge->setVisible(false);
    }
    if (m_chkRouteType)       m_chkRouteType->setChecked(false);
    if (m_chkRouteId)         m_chkRouteId->setChecked(false);
    if (m_chkRouteName)       m_chkRouteName->setChecked(false);
    if (m_chkRouteStart)      m_chkRouteStart->setChecked(false);
    if (m_chkRouteEnd)        m_chkRouteEnd->setChecked(false);
    if (m_chkRouteDirect)     m_chkRouteDirect->setChecked(false);
    if (m_chkRouteMaxSpeed)   m_chkRouteMaxSpeed->setChecked(false);
    if (m_chkRouteMaxAcc)     m_chkRouteMaxAcc->setChecked(false);
    if (m_chkRouteMaxRot)     m_chkRouteMaxRot->setChecked(false);
    if (m_chkRouteMaxRotAcc)  m_chkRouteMaxRotAcc->setChecked(false);
    if (m_chkRouteReachDist)  m_chkRouteReachDist->setChecked(false);
    if (m_chkRouteReachAngle) m_chkRouteReachAngle->setChecked(false);
    if (m_chkRouteWidth)      m_chkRouteWidth->setChecked(false);
    if (m_chkRouteObsRange)   m_chkRouteObsRange->setChecked(false);
    if (m_routeContentWidget) m_routeContentWidget->setVisible(false);
    if (m_routeEmptyHint)     m_routeEmptyHint->setVisible(true);
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

    // 每次进入页面回到默认视图: 属性页 → 设备数据监视
    m_propsView       = ViewDevice;
    m_layerHiddenItem = nullptr;
    setRightTab(TabProps);

    // 每次页面展示（菜单切换/从下级编辑页返回/首次打开）全量刷新设备数据与地图/路网
    fetchMapNameList();
    loadCurrentCarMap();
    if (m_mapReady && !m_mapId.isEmpty()) {
        loadRoute();
    }
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

// ── 地图名称解析与缓存 ─────────────────────────────────────────────────────────

void MapViewWidget::fetchMapNameList(std::function<void()> onDone)
{
    if (m_isFetchingMapList) {
        if (onDone) onDone();
        return;
    }
    m_isFetchingMapList = true;
    ApiService::instance().listMapAll(QJsonObject{{"pageNum", 1}, {"pageSize", 1000}},
        [this, onDone](int code, const QJsonObject &data) {
            m_isFetchingMapList = false;
            if (code == 200) {
                const QJsonArray rows = data.value("rows").toArray();
                for (const auto &v : rows) {
                    QJsonObject m = v.toObject();
                    int mid = jsonIdStr(m.value("mapId")).toInt();
                    if (mid == 0 && m.contains("mapId")) mid = m.value("mapId").toInt();
                    QString mname = m.value("mapName").toString().trimmed();
                    if (mid != 0 && !mname.isEmpty()) {
                        m_mapIdToName[mid] = mname;
                    }
                }
                // 若当前已有 robot info 显示，立即刷新地图名称
                RobotInfo cur = RobotInfoManager::instance().currentInfo();
                if (cur.map_id != 0 && m_robotMapIdVal) {
                    QString name = m_mapIdToName.value(cur.map_id);
                    if (name.isEmpty() && !m_mapId.isEmpty() && m_mapId.toInt() == cur.map_id && !m_mapName.isEmpty()) {
                        name = m_mapName;
                    }
                    m_robotMapIdVal->setText(!name.isEmpty() ? name : QString::number(cur.map_id));
                }
            }
            if (onDone) onDone();
        });
}

QString MapViewWidget::lookupMapName(int mapId)
{
    if (mapId == 0) return QStringLiteral("--");
    if (m_mapIdToName.contains(mapId)) {
        return m_mapIdToName.value(mapId);
    }
    if (!m_mapId.isEmpty() && m_mapId.toInt() == mapId && !m_mapName.isEmpty()) {
        m_mapIdToName[mapId] = m_mapName;
        return m_mapName;
    }
    // 纯内存查询：未命中时仅返回数字ID，坚决杜绝在此触发异步网络请求，彻底消除死循环
    return QString::number(mapId);
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
        m_mapIdToName[mapId.toInt()] = mapName;
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

void MapViewWidget::clearScene()
{
    m_mapReady = false;
    emit mapStatusChanged();
    if (m_scene) {
        m_scene->clear();
    }
    m_mapBg = nullptr;
    m_robotItem = nullptr;
    m_routeLayer = nullptr;
    m_axesLayer = nullptr;
    m_selItem = nullptr;
    m_poseDotItem = nullptr;
    m_poseArrowItem = nullptr;
    m_routeTopLevelItems.clear();
    m_gridItems.clear();
}

void MapViewWidget::clearRouteItems()
{
    m_selItem = nullptr;
    for (auto *item : m_routeTopLevelItems) {
        if (item) {
            if (m_scene && item->scene() == m_scene) {
                m_scene->removeItem(item);
            }
            delete item;
        }
    }
    m_routeTopLevelItems.clear();
}

void MapViewWidget::clearGridItems()
{
    for (auto *item : m_gridItems) {
        if (item) {
            if (m_scene && item->scene() == m_scene) {
                m_scene->removeItem(item);
            }
            delete item;
        }
    }
    m_gridItems.clear();
}

void MapViewWidget::installMapPixmap(const QPixmap &pix)
{
    m_imgW = pix.width();
    m_imgH = pix.height();

    clearScene();

    auto *bgItem = m_scene->addPixmap(pix);
    bgItem->setTransformationMode(Qt::SmoothTransformation);
    bgItem->setZValue(-100);
    m_mapBg = bgItem;

    m_scene->setSceneRect(0, 0, m_imgW, m_imgH);
    // 最短边铺满、长边居中裁切 ("cover" 而非 "contain")
    m_canvas->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatioByExpanding);

    m_mapReady = true;
    m_mapLoadFailed = false;

    renderAll();
    updateRulers();
    updateMapPropsView();
    emit mapStatusChanged();
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
                        QJsonObject pt = props;
                        pt["id"]   = id;
                        pt["x"]    = coords[0].toDouble();
                        pt["y"]    = coords[1].toDouble();
                        if (pt.value("name").toString().isEmpty()) {
                            pt["name"] = props.value("name").toString();
                        }
                        if (id.startsWith("c_") || props.value("typeName").toString() == "ChargePoint")
                            m_charges.append(pt);
                        else if (id.startsWith("hp_") || props.value("typeName").toString() == "ActionPoint")
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
                        // 保留全部属性(name/startid/endid/direct/maxspeed 等)以便属性面板完整展示
                        for (auto it = props.begin(); it != props.end(); ++it)
                            ln[it.key()] = it.value();
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
                        // 保留全部属性(startid/endid/direct/maxspeed 等)
                        for (auto it = props.begin(); it != props.end(); ++it)
                            cv[it.key()] = it.value();
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
    clearGridItems();

    if (!m_mapReady || m_imgW <= 0 || m_imgH <= 0) return;
    if (!m_showGrid) return;

    // 网格/坐标轴不参与路网命中 (data(0) 仅用于标记路网图元, 与"启用编辑"一致)
    auto tag = [this](QGraphicsItem *item, int z) {
        item->setZValue(z);
        m_gridItems.append(item);
        return item;
    };

    double wLeft   = m_worldL;
    double wRight  = m_worldR;
    double wBottom = m_worldB;
    double wTop    = m_worldT;

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
    clearRouteItems();

    if (m_points.isEmpty() && m_lines.isEmpty() && m_curves.isEmpty()
        && m_charges.isEmpty() && m_highPrecisionPoints.isEmpty()) {
        m_selItem = nullptr;
        m_layerHiddenItem = nullptr;
        refreshElementList();
        updateMapPropsView();
        return;
    }

    // 图元 data 约定与"启用编辑"完全一致: data(0)=类型串, data(1)=ID, data(2)=原始配色
    auto addRoute = [this](QGraphicsItem *item) {
        item->setZValue(20);
        m_routeTopLevelItems.append(item);
    };

    QPen linePen(QColor("#1296DB"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    for (const auto &v : m_lines) {
        QJsonObject ln = v.toObject();
        QPointF a = worldToScene(ln.value("x1").toDouble(), ln.value("y1").toDouble());
        QPointF b = worldToScene(ln.value("x2").toDouble(), ln.value("y2").toDouble());
        int direct = ln.value("direct").toInt(10);
        auto *lineItem = m_scene->addLine(a.x(), a.y(), b.x(), b.y(), linePen);
        addRoute(lineItem);
        lineItem->setData(0, QString("line"));
        lineItem->setData(1, ln.value("id").toString());
        lineItem->setData(2, linePen.color());
        lineItem->setData(3, routeFeatureName(ln));

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
        pathItem->setData(0, QString("curve"));
        pathItem->setData(1, cv.value("id").toString());
        pathItem->setData(2, curvePen.color());
        pathItem->setData(3, routeFeatureName(cv));

        if (pts.size() >= 2) {
            double cLen = std::hypot(pts.last().x() - pts.first().x(),
                                     pts.last().y() - pts.first().y());
            QList<QPolygonF> arrows = MapPgmUtil::createBezierDirectionArrows(pts, direct, cLen);
            for (const auto &poly : arrows) {
                auto *arrItem = m_scene->addPolygon(poly, QPen(Qt::NoPen), QBrush(curvePen.color()));
                arrItem->setParentItem(pathItem);
                arrItem->setZValue(6);
                arrItem->setAcceptedMouseButtons(Qt::NoButton);
            }
        }
    }

    double pxPerMeterX = (m_imgW > 0 && (m_worldR > m_worldL)) ? (m_imgW / (m_worldR - m_worldL)) : 20.0;
    double pxPerMeterY = (m_imgH > 0 && (m_worldT > m_worldB)) ? (m_imgH / (m_worldT - m_worldB)) : 20.0;

    auto addPointData = [this](QGraphicsItem *item, const QString &id, const QString &type,
                              const QString &name, const QPointF &wPos, const QColor &fillCol) {
        item->setData(0, type);                   // Point type ("point", "charge", "highPrecision")
        item->setData(1, id);                     // Point ID
        item->setData(2, fillCol);                // 原始填充配色，取消选中时据此还原
        item->setData(3, name);                   // Point Name
        item->setData(4, wPos);                   // World position
        item->setZValue(20);
        m_routeTopLevelItems.append(item);
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
        addPointData(item, id, type, name, wPos, fillCol);

        // 内部红色实线精确定位小十字 (居中位于 (0,0)，与路线精准交汇，不遮挡文字)
        QPen crossPen(QColor("#E74C3C"), 0.8, Qt::SolidLine);
        crossPen.setCapStyle(Qt::FlatCap);
        auto *crossItem = m_scene->addPath(crossPath, crossPen);
        crossItem->setParentItem(item);
        crossItem->setPos(0, 0);
        crossItem->setAcceptedMouseButtons(Qt::NoButton);
        crossItem->setFlag(QGraphicsItem::ItemIsSelectable, false);

        // 获取朝向角 (弧度制)
        double yawVal = 0.0;
        if (pt.contains("yaw") && !pt.value("yaw").isNull()) {
            yawVal = pt.value("yaw").toVariant().toDouble();
        } else if (pt.contains("theta") && !pt.value("theta").isNull()) {
            yawVal = pt.value("theta").toVariant().toDouble() * M_PI / 180.0;
        } else if (pt.contains("robotYaw") && !pt.value("robotYaw").isNull()) {
            yawVal = pt.value("robotYaw").toVariant().toDouble();
        }

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

        // 站点名称：水平正向居中显示，反向旋转抵消父级旋转以保持永远水平易读
        auto *lbl = m_scene->addText(name, f);
        lbl->document()->setDocumentMargin(0);
        lbl->setDefaultTextColor(QColor("#1A1A1A"));
        lbl->setParentItem(item);
        lbl->setTransformOriginPoint(tw / 2.0, fm.height() / 2.0);
        lbl->setRotation(-rotDeg);
        double textY = (rectH >= 12.0) ? (rectH * 0.16) : (rectH / 2.0 + 1.0);
        lbl->setPos(-tw / 2.0, textY);
        lbl->setAcceptedMouseButtons(Qt::NoButton);
        lbl->setFlag(QGraphicsItem::ItemIsSelectable, false);
    };

    for (const auto &v : m_points)              addStationNode(v.toObject(), QColor(210, 230, 250, 235), QColor(41, 128, 185, 240), "point");
    for (const auto &v : m_charges)             addStationNode(v.toObject(), QColor(254, 243, 199, 235), QColor(217, 119, 6, 240), "charge");
    for (const auto &v : m_highPrecisionPoints) addStationNode(v.toObject(), QColor(254, 226, 226, 235), QColor(220, 38, 38, 240), "highPrecision");

    // 图元重建后同步右侧"图元"页与"图层"页状态
    m_selItem = nullptr;
    m_layerHiddenItem = nullptr;
    refreshElementList();
    updateMapPropsView();
    applyLayerVisibility();
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
            pixItem->setCursor(Qt::PointingHandCursor);
        } else {
            if (m_robotItem) {
                if (m_scene && m_robotItem->scene() == m_scene) {
                    m_scene->removeItem(m_robotItem);
                }
                delete m_robotItem;
                m_robotItem = nullptr;
            }
            auto *robotItem = m_scene->addPixmap(m_robotPixmap);
            robotItem->setScale(scale);
            robotItem->setTransformOriginPoint(pivotX, pivotY);
            robotItem->setPos(sp.x() - pivotX, sp.y() - pivotY);
            robotItem->setRotation(screenAngle);
            robotItem->setZValue(120);
            robotItem->setCursor(Qt::PointingHandCursor);
            m_robotItem = robotItem;
        }
    } else {
        // Fallback: draw 110*60 rectangle with light-blue head
        if (m_robotItem) {
            if (m_scene && m_robotItem->scene() == m_scene) {
                m_scene->removeItem(m_robotItem);
            }
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
        marker->setZValue(120);
        marker->setCursor(Qt::PointingHandCursor);
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
    if (info.latency >= 0) {
        m_robot.latency = info.latency;
    }

    if (m_deviceStatusLabel) {
        m_deviceStatusLabel->setText(info.statusText());
        m_deviceStatusLabel->setStyleSheet(QString("color:%1; font-size:12px; font-weight:600;").arg(info.statusColor()));
    }

    renderRobot();
    updateInfoPanel();
    updateDeviceMonitor(info);

    const QString newStatus = info.status;
    const bool isRobotNavigating = (newStatus == "1" || newStatus == "2" || newStatus == "3" || newStatus == "5");
    if (m_navActive) {
        if (isRobotNavigating) {
            m_navEnteredRunning = true;
            if (newStatus == "2" || newStatus == "5") {
                m_navPaused = true;
            } else if (newStatus == "1") {
                m_navPaused = false;
            }
        } else {
            // 当曾经进入过导航运行状态，现在变为非导航状态（如 0 就绪），且距离发起已超过 1.5 秒，判定为导航结束
            qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_navStartTime;
            if (m_navEnteredRunning && elapsed > 1500) {
                m_navActive = false;
                m_navEnteredRunning = false;
                m_navPaused = false;
            } else if (!m_navEnteredRunning && elapsed > 15000) {
                // 若下发超过 15 秒仍未进入且未移动，超时重置
                m_navActive = false;
                m_navPaused = false;
            }
        }
    } else {
        if (isRobotNavigating) {
            m_navActive = true;
            m_navEnteredRunning = true;
            if (newStatus == "2" || newStatus == "5") {
                m_navPaused = true;
            } else if (newStatus == "1") {
                m_navPaused = false;
            }
        }
    }

    // --- 参考 /rob/navigate/status MQTT 状态更新菜单左下角/地图左上角状态栏 ---
    if (m_lastRobotNavStatus != newStatus) {
        const QString oldStatus = m_lastRobotNavStatus;
        m_lastRobotNavStatus = newStatus;

        if (newStatus == "0") {
            // 从运行态/暂停态转为就绪态 -> 判定为导航或移动已完成
            if (oldStatus == "1" || oldStatus == "2" || oldStatus == "3" || oldStatus == "5") {
                if (m_currentOpType == OpNavigation) {
                    setStatus(m_currentNavTargetName.isEmpty() ? "导航已完成，设备就绪" : QString("已到达点位 \"%1\"，导航完成").arg(m_currentNavTargetName), "#00e676");
                } else if (m_currentOpType == OpMotion) {
                    setStatus(m_currentMotionDesc.isEmpty() ? "移动已完成，设备就绪" : QString("%1 已完成，设备就绪").arg(m_currentMotionDesc), "#00e676");
                } else {
                    setStatus("设备移动/导航已完成，就绪", "#00e676");
                }
                m_currentOpType = OpNone;
                m_currentNavTargetName.clear();
                m_currentMotionDesc.clear();
            }
        } else if (newStatus == "1" || newStatus == "3") {
            // 进入运行态 (导航中 / 移动中)
            if (oldStatus == "0" || oldStatus.isEmpty()) {
                if (m_currentOpType == OpNavigation) {
                    setStatus(QString("正在导航至点位 \"%1\"...").arg(m_currentNavTargetName.isEmpty() ? "目标点" : m_currentNavTargetName), "#00e676");
                } else if (m_currentOpType == OpMotion) {
                    setStatus(QString("正在移动中 (%1)...").arg(m_currentMotionDesc.isEmpty() ? "移动" : m_currentMotionDesc), "#00e676");
                } else {
                    setStatus("设备正在导航/移动中...", "#00e676");
                }
            } else if (oldStatus == "2" || oldStatus == "5") {
                setStatus(m_currentOpType == OpNavigation ? "已继续导航" : (m_currentOpType == OpMotion ? "已恢复移动" : "已继续运行"), "#00e676");
            }
        } else if (newStatus == "2" || newStatus == "5") {
            // 暂停态
            setStatus(m_currentOpType == OpNavigation ? "导航已暂停" : (m_currentOpType == OpMotion ? "移动已暂停" : "设备运行已暂停"), "#f59e0b");
        } else if (newStatus == "4") {
            // 停止导航，作业中
            setStatus("停止导航，作业中", "#06b6d4");
        } else if (newStatus == "6") {
            // 故障
            setStatus("设备故障，请检查设备状态", "#ef4444");
        }
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

    if (btn == Qt::RightButton) {
        bool onRobot = false;
        if (m_robotItem && m_robotItem->isVisible() && m_scene) {
            for (QGraphicsItem *it : m_scene->items(scenePos)) {
                if (it == m_robotItem || it->parentItem() == m_robotItem) { onRobot = true; break; }
            }
            if (!onRobot) {
                const double tol = hitTolScene(16.0);
                for (QGraphicsItem *it : m_scene->items(QRectF(scenePos.x() - tol, scenePos.y() - tol, tol * 2.0, tol * 2.0))) {
                    if (it == m_robotItem || it->parentItem() == m_robotItem) { onRobot = true; break; }
                }
            }
        }
        if (onRobot) {
            showRobotContextMenu(QCursor::pos());
        }
        return;
    }

    if (btn != Qt::LeftButton) return;

    // 只有"属性"页响应地图点击; 图元/图层页点击地图不做任何反应
    if (m_rightTab != TabProps) return;

    // ── 优先检测设备图标命中 ──────────────────────────────────────────────────
    // 当设备与站点/路线重合时，由于小车在视觉最顶层 (Z=120)，
    // 在常规查看模式下应优先选中设备，避免站点和路线遮蔽设备的点击事件。
    bool onRobot = false;
    if (m_robotItem && m_robotItem->isVisible() && m_scene) {
        for (QGraphicsItem *it : m_scene->items(scenePos)) {
            if (it == m_robotItem || it->parentItem() == m_robotItem) { onRobot = true; break; }
        }
        if (!onRobot) {
            const double tol = hitTolScene(12.0);
            for (QGraphicsItem *it : m_scene->items(QRectF(scenePos.x() - tol, scenePos.y() - tol, tol * 2.0, tol * 2.0))) {
                if (it == m_robotItem || it->parentItem() == m_robotItem) { onRobot = true; break; }
            }
        }
    }

    // 常规模式下：如果点击命中设备，且当前尚未聚焦设备属性，优先激活设备视图
    // （如果已处于设备视图且下方有重合图元，则允许继续向下穿透选择重合的站点/线路）
    if (onRobot && !m_navMode && !m_moveMode && !m_relocMode && m_propsView != ViewDevice) {
        RouteFeatureSelect::clearSelectColor(m_selItem);
        m_selItem = nullptr;
        clearStationDetails();
        clearRouteDetails();
        switchPropsView(ViewDevice);
        QString cName = m_robot.carName.isEmpty() ? m_robot.carId : m_robot.carName;
        setStatus(QString("已选中设备: %1").arg(cName));
        return;
    }

    // ── 命中与选中效果: 直接复用"启用编辑"的同一份实现 (RouteFeatureSelect) ─────
    QGraphicsItem *picked = RouteFeatureSelect::pickItem(m_scene, scenePos, hitTolScene(12.0));
    QString fType = picked ? picked->data(0).toString() : QString();
    QString fId   = picked ? picked->data(1).toString() : QString();

    auto findIn = [](const QJsonArray &arr, const QString &id) -> QJsonObject {
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            if (o.value("id").toString() == id) return o;
        }
        return QJsonObject();
    };

    QGraphicsItem *stationItem = nullptr;
    QGraphicsItem *routeItem   = nullptr;
    if (fType == "line" || fType == "curve")  routeItem   = picked;
    else if (!fType.isEmpty())                stationItem = picked;

    if (picked != m_selItem) {
        RouteFeatureSelect::clearSelectColor(m_selItem);
        m_selItem = picked;
    }

    if (stationItem) {
        QJsonObject ptObj;
        if (fType == "charge")             ptObj = findIn(m_charges, fId);
        else if (fType == "highPrecision") ptObj = findIn(m_highPrecisionPoints, fId);
        else                               ptObj = findIn(m_points, fId);

        if (!ptObj.isEmpty()) {
            RouteFeatureSelect::applySelectColor(stationItem);
            showStationDetails(ptObj, fType);
            switchPropsView(ViewStation);
            syncElementListSelection(fType, fId);
        }
    } else if (routeItem) {
        QJsonObject rObj = (fType == "line") ? findIn(m_lines, fId) : findIn(m_curves, fId);
        if (rObj.isEmpty()) rObj["id"] = fId;
        RouteFeatureSelect::applySelectColor(routeItem);
        showRouteDetails(rObj, fType);
        switchPropsView(ViewRoute);
        syncElementListSelection(fType, fId);
    } else {
        if (!m_navMode && !m_moveMode && !m_relocMode) {
            clearStationDetails();
            clearRouteDetails();
            // 点击设备图标 → 设备数据监视; 点击空白 → 地图属性
            switchPropsView(onRobot ? ViewDevice : ViewMap);
            if (onRobot) {
                QString cName = m_robot.carName.isEmpty() ? m_robot.carId : m_robot.carName;
                setStatus(QString("已选中设备: %1").arg(cName));
            }
        }
    }

    if (m_navMode) {
        QGraphicsItem *targetItem = stationItem;
        if (!targetItem) {
            return;
        }

        QString ptId   = targetItem->data(1).toString();
        QString ptType = targetItem->data(0).toString();
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

        if (m_canvas) m_canvas->resetPanState();
        navBox.exec();
        if (m_canvas) m_canvas->resetPanState();
        if (navBox.clickedButton() == startBtn) {
            QString carId = AppConfig::instance().currentDevice().carId;
            if (carId.isEmpty()) carId = AppConfig::instance().agvId();
            if (carId.isEmpty()) carId = "1";

            int actionId = static_cast<int>(QDateTime::currentMSecsSinceEpoch() % 1000000 + 1);
            double speed = TeleopController::instance().baseLinearSpeed();
            if (speed <= 0.01) speed = MotionConfigManager::instance().maxSpeed();
            speed = MotionConfigManager::instance().clampLinearSpeed(speed);

            // POST 8048 /robot/navigation/navigate/to?locationId=ptId
            setStatus(QString("正在下发导航指令至点位 \"%1\"...").arg(ptName), "#00bcd4");
            m_navActive = true;
            m_navStartTime = QDateTime::currentMSecsSinceEpoch();
            m_currentOpType = OpNavigation;
            m_currentNavTargetName = ptName;
            updateNavButtonsState();
            ApiService::instance().navigateTo(ptId, [this, ptName](int code, const QJsonObject &resp) {
                QString msg = resp.value("msg").toString();
                // 收到 http 返回的 ok 后应该亮起"取消导航"和"暂停导航"
                if (code == 200 || msg.compare("ok", Qt::CaseInsensitive) == 0 || resp.value("code").toInt() == 200) {
                    m_navActive = true;
                    m_navPaused = false;
                    m_navEnteredRunning = false;
                    m_navStartTime = QDateTime::currentMSecsSinceEpoch();
                    m_currentOpType = OpNavigation;
                    m_currentNavTargetName = ptName;
                    updateNavButtonsState();
                    setStatus(QString("正在导航至点位 \"%1\"...").arg(ptName), "#00e676");
                } else {
                    QString errMsg = msg.isEmpty() ? "请求失败" : msg;
                    m_navActive = false;
                    m_currentOpType = OpNone;
                    m_currentNavTargetName.clear();
                    updateNavButtonsState();
                    setStatus(QString("导航下发失败: %1").arg(errMsg), "#ef4444");
                }
            });
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
        if (m_canvas) m_canvas->resetPanState();
        moveBox.exec();
        if (m_canvas) m_canvas->resetPanState();

        if (moveBox.clickedButton() == startBtn) {
            int actionId = static_cast<int>(QDateTime::currentMSecsSinceEpoch() % 1000000 + 1);
            double speed = TeleopController::instance().baseLinearSpeed();
            if (speed <= 0.01) speed = MotionConfigManager::instance().maxSpeed();
            speed = MotionConfigManager::instance().clampLinearSpeed(speed);

            // 控制器 8061 端口三方固定路径点导航 (Table 4 set_third_fix_path)
            // 仅此一路下发, 不再额外走 MQTT, 避免同一指令被执行两次
            const QString targetDesc = QString("自由移动至 (%1, %2)").arg(world.x(), 0, 'f', 2).arg(world.y(), 0, 'f', 2);
            setStatus(QString("正在下发移动指令至 (%1, %2)...").arg(world.x(), 0, 'f', 2).arg(world.y(), 0, 'f', 2), "#00bcd4");
            m_navActive = true;
            m_navStartTime = QDateTime::currentMSecsSinceEpoch();
            m_currentOpType = OpMotion;
            m_currentMotionDesc = targetDesc;
            updateNavButtonsState();
            ApiService::instance().navigateToPose(m_robot.wx, m_robot.wy, world.x(), world.y(), speed, actionId, [this, world, targetDesc](int code, const QJsonObject &resp) {
                if (code == 200) {
                    QString msg = resp.value("data").toObject().value("msg").toString("ok");
                    if (msg == "failed") {
                        QString err = resp.value("data").toObject().value("error_info").toString();
                        m_navActive = false;
                        m_currentOpType = OpNone;
                        m_currentMotionDesc.clear();
                        updateNavButtonsState();
                        setStatus(QString("控制器拒绝移动: %1").arg(err), "#ef4444");
                    } else {
                        m_navActive = true;
                        m_navPaused = false;
                        m_navEnteredRunning = false;
                        m_navStartTime = QDateTime::currentMSecsSinceEpoch();
                        m_currentOpType = OpMotion;
                        m_currentMotionDesc = targetDesc;
                        updateNavButtonsState();
                        setStatus(QString("正在移动至 (%1, %2)...").arg(world.x(), 0, 'f', 2).arg(world.y(), 0, 'f', 2), "#00e676");
                    }
                } else {
                    QString msg = resp.value("msg").toString("控制器未响应");
                    m_navActive = false;
                    m_currentOpType = OpNone;
                    m_currentMotionDesc.clear();
                    updateNavButtonsState();
                    setStatus(QString("移动请求失败: %1").arg(msg), "#ef4444");
                }
            });
        }
        return;
    }

    if (m_relocMode) {
        if (m_robot.carId.isEmpty()) { setStatus("无设备ID，无法重定位", "#e05555"); return; }
        // Start drag: record press position, create dot preview
        m_relocDragging   = true;
        m_relocStartWorld = world;

        // Remove stale preview items
        if (m_poseDotItem)   { if (m_scene && m_poseDotItem->scene() == m_scene) m_scene->removeItem(m_poseDotItem); delete m_poseDotItem; m_poseDotItem = nullptr; }
        if (m_poseArrowItem) { if (m_scene && m_poseArrowItem->scene() == m_scene) m_scene->removeItem(m_poseArrowItem); delete m_poseArrowItem; m_poseArrowItem = nullptr; }

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
    if (m_poseArrowItem) { if (m_scene && m_poseArrowItem->scene() == m_scene) m_scene->removeItem(m_poseArrowItem); delete m_poseArrowItem; m_poseArrowItem = nullptr; }

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
    if (m_poseDotItem)   { if (m_scene && m_poseDotItem->scene() == m_scene) m_scene->removeItem(m_poseDotItem); delete m_poseDotItem; m_poseDotItem = nullptr; }
    if (m_poseArrowItem) { if (m_scene && m_poseArrowItem->scene() == m_scene) m_scene->removeItem(m_poseArrowItem); delete m_poseArrowItem; m_poseArrowItem = nullptr; }

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
            Toast::success(this, "重定位消息已发送，请稍等...");
            setStatus("重定位消息已发送，请稍等...");
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
    QString switchSuccessMsg;
    QVector<QFrame*> allCards;
    QJsonArray mapList;

    // 点击确认切换逻辑：订阅 /rob/1/crud 并下发切换接口，收到设备成功确认后关闭窗口并刷新地图
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

        // 阻塞式进度等待框 (设置 40 秒安全熔断超时)
        auto *loading = new LoadingDialog("正在切换地图，等待设备确认...", &dlg, Qt::ApplicationModal, 40000);
        loading->show();

        QString carId = AppConfig::instance().agvId();
        if (carId.isEmpty()) carId = "1";
        QStringList crudTopics;
        crudTopics << QString("/rob/%1/crud").arg(carId);
        if (carId != "1") {
            crudTopics << QStringLiteral("/rob/1/crud");
        }

        auto handled = std::make_shared<bool>(false);
        auto *timeoutTimer = new QTimer(&dlg);
        timeoutTimer->setSingleShot(true);

        auto cleanup = [handled, loading, crudTopics, timeoutTimer]() {
            if (*handled) return;
            *handled = true;
            if (timeoutTimer) {
                timeoutTimer->stop();
            }
            for (const QString &t : crudTopics) {
                MqttService::instance().unsubscribeTopic(t);
            }
            if (loading) {
                loading->finish();
            }
        };

        // 对话框关闭/销毁时兜底反注册与清理
        QObject::connect(&dlg, &QDialog::finished, &dlg, [cleanup](int) {
            cleanup();
        });

        // 订阅 MQTT /rob/1/crud (及 /rob/<carId>/crud)
        auto onCrudMessage = [this, &dlg, selectedMapId, okBtn, cancelBtn, selTipLabel, &switchSuccessMsg, handled, cleanup](const QString &/*topic*/, const QJsonObject &obj) {
            if (*handled) return;

            QString sender = obj.value("sender").toString().trimmed();
            if (sender.compare("Robot", Qt::CaseInsensitive) != 0) {
                return;
            }

            int msgMapId = obj.value("map_id").toInt();
            if (msgMapId > 0 && msgMapId != selectedMapId.toInt()) {
                return;
            }

            QString status = obj.value("status").toString().trimmed();
            QString info = obj.value("info").toString().trimmed();

            cleanup();

            if (status.compare("success", Qt::CaseInsensitive) == 0) {
                qDebug().noquote() << QString("[SwitchMap] Robot reported map switch success: %1").arg(info);
                switchSuccessMsg = info;
                dlg.accept();
            } else {
                qWarning().noquote() << QString("[SwitchMap] Robot reported map switch failed: %1 (status: %2)").arg(info, status);
                okBtn->setEnabled(true);
                cancelBtn->setEnabled(true);
                okBtn->setText("确认切换");
                QString errMsg = info.isEmpty() ? QString("设备切换地图失败 (状态: %1)").arg(status) : info;
                selTipLabel->setText(QString("<font color='#f87171'>切换失败: %1</font>").arg(errMsg));
                QMessageBox::warning(&dlg, "切换地图失败", errMsg);
            }
        };

        for (const QString &t : crudTopics) {
            MqttService::instance().subscribeJsonTopic(t, 0, onCrudMessage);
        }

        // 超时处理 (30秒)
        QObject::connect(timeoutTimer, &QTimer::timeout, &dlg, [&dlg, okBtn, cancelBtn, selTipLabel, handled, cleanup]() {
            if (*handled) return;
            cleanup();
            okBtn->setEnabled(true);
            cancelBtn->setEnabled(true);
            okBtn->setText("确认切换");
            selTipLabel->setText("<font color='#f87171'>切换失败: 等待设备确认超时</font>");
            QMessageBox::warning(&dlg, "切换超时", "等待设备切换地图响应超时 (30秒)，请确认设备在线且网络畅通。");
        });
        timeoutTimer->start(30000);

        // 访问后端切换地图接口 "/robot/car/edit/map/{mapid}"
        ApiService::instance().editCarMap(selectedMapId, [&dlg, okBtn, cancelBtn, selTipLabel, loading, handled, cleanup](int code, const QJsonObject &data) {
            if (*handled) return;
            if (code == 200) {
                if (loading) {
                    loading->setMessage("指令已下发，正在等待设备切换与导航重启...");
                }
            } else {
                cleanup();
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
            if (mid.toInt() != 0 && !name.isEmpty()) {
                m_mapIdToName[mid.toInt()] = name;
            }

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
        Toast::success(this, switchSuccessMsg.isEmpty() ? "地图切换成功" : switchSuccessMsg);
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
    if (m_poseDotItem)   { if (m_scene && m_poseDotItem->scene() == m_scene) m_scene->removeItem(m_poseDotItem); delete m_poseDotItem; m_poseDotItem = nullptr; }
    if (m_poseArrowItem) { if (m_scene && m_poseArrowItem->scene() == m_scene) m_scene->removeItem(m_poseArrowItem); delete m_poseArrowItem; m_poseArrowItem = nullptr; }
    if (m_canvas) {
        m_canvas->setPanWithLeftButton(!m_navMode && !m_moveMode);
    }
}

void MapViewWidget::onToggleMoveMode()
{
    if (!m_motionDialog) {
        m_motionDialog = new MotionDialog(this);
        connect(m_motionDialog, &MotionDialog::statusHintRequested, this, [this](const QString &msg, const QString &color) {
            setStatus(msg, color);
        });
        connect(m_motionDialog, &MotionDialog::motionStarted, this, [this](int, double, const QString &desc) {
            m_currentOpType = OpMotion;
            m_currentMotionDesc = desc;
            m_navActive = true;
            m_navStartTime = QDateTime::currentMSecsSinceEpoch();
            updateNavButtonsState();
        });
        connect(m_motionDialog, &MotionDialog::motionCompleted, this, [this]() {
            setStatus(m_currentMotionDesc.isEmpty() ? "移动已完成，设备就绪" : QString("%1 已完成，设备就绪").arg(m_currentMotionDesc), "#00e676");
            m_currentOpType = OpNone;
            m_currentMotionDesc.clear();
            m_navActive = false;
            updateNavButtonsState();
        });
        connect(m_motionDialog, &MotionDialog::motionCanceled, this, [this]() {
            setStatus("移动已取消", "#f59e0b");
            m_currentOpType = OpNone;
            m_currentMotionDesc.clear();
            m_navActive = false;
            updateNavButtonsState();
        });
        connect(m_motionDialog, &MotionDialog::motionPaused, this, [this](bool paused) {
            m_navPaused = paused;
            setStatus(paused ? "移动已暂停" : "已恢复移动", paused ? "#f59e0b" : "#00e676");
            updateNavButtonsState();
        });
    }
    m_motionDialog->show();
    m_motionDialog->raise();
    m_motionDialog->activateWindow();
}

void MapViewWidget::onCancelNavigation()
{
    // 统一使用 /robot/navigation/control?command=stop navigation 结束/取消导航
    ApiService::instance().controlNavigation(QStringLiteral("stop navigation"), [](int code, const QJsonObject &resp) {
        qDebug() << "[MapViewWidget] controlNavigation stop navigation code:" << code << resp;
    });

    m_navActive = false;
    m_navPaused = false;
    m_navEnteredRunning = false;
    updateNavButtonsState();
    if (m_currentOpType == OpMotion) {
        setStatus("移动已取消", "#f59e0b");
    } else {
        setStatus("已取消导航", "#f59e0b");
    }
    m_currentOpType = OpNone;
    m_currentNavTargetName.clear();
    m_currentMotionDesc.clear();
}

void MapViewWidget::onPauseNavigation()
{
    const RobotInfo &info = RobotInfoManager::instance().currentInfo();
    const bool isPaused = (m_navPaused || info.status == "2" || info.status == "5");

    if (isPaused) {
        // 恢复导航统一使用 /robot/navigation/control?command=continue navigation
        ApiService::instance().controlNavigation(QStringLiteral("continue navigation"), [](int code, const QJsonObject &resp) {
            qDebug() << "[MapViewWidget] controlNavigation continue navigation code:" << code << resp;
        });

        m_navPaused = false;
        updateNavButtonsState();
        if (m_currentOpType == OpMotion) {
            setStatus("已恢复移动", "#00e676");
        } else {
            setStatus("已继续导航", "#00e676");
        }
    } else {
        // 暂停导航统一使用 /robot/navigation/control?command=pause navigation
        ApiService::instance().controlNavigation(QStringLiteral("pause navigation"), [](int code, const QJsonObject &resp) {
            qDebug() << "[MapViewWidget] controlNavigation pause navigation code:" << code << resp;
        });

        m_navPaused = true;
        updateNavButtonsState();
        if (m_currentOpType == OpMotion) {
            setStatus("移动已暂停", "#f59e0b");
        } else {
            setStatus("导航已暂停", "#f59e0b");
        }
    }
}

void MapViewWidget::updateNavButtonsState()
{
    const RobotInfo &info = RobotInfoManager::instance().currentInfo();
    // 严格根据 /rob/navigate/status 控制: status == "1" 导航中, status == "2" 暂停导航，两者均为导航进行态；status == "0" 就绪
    const bool isRobotNav = (info.status == "1" || info.status == "2" || info.status == "5" || info.status == "3");
    const bool isNav = (isRobotNav || m_navActive);
    // 用户标记：当用户点击抢占控制权时，不要让取消导航和暂停导航按钮可点击
    const bool isUserPreempted = TeleopController::instance().isUserPreempted() || TeleopController::instance().isActive();
    const bool canControlNav = isNav && !isUserPreempted;

    if (m_cancelNavBtn) {
        m_cancelNavBtn->setEnabled(canControlNav);
    }
    if (m_pauseNavBtn) {
        m_pauseNavBtn->setEnabled(canControlNav);
        const bool isPaused = (m_navPaused || info.status == "2" || info.status == "5");
        if (isPaused) {
            m_pauseNavBtn->setText("继续导航");
            m_pauseNavBtn->setIcon(makeMapToolIcon("resume"));
        } else {
            m_pauseNavBtn->setText("暂停导航");
            m_pauseNavBtn->setIcon(makeMapToolIcon("pause"));
        }
    }

    // 路径导航按钮状态：导航过程中变暗且不可被再次点击，直到导航完成
    if (m_navModeBtn) {
        if (isNav) {
            if (m_navMode || m_navModeBtn->isChecked()) {
                m_navMode = false;
                m_navModeBtn->setChecked(false);
                if (m_canvas) {
                    m_canvas->setPanWithLeftButton(!m_relocMode && !m_moveMode);
                    m_canvas->setCursor(Qt::CrossCursor);
                }
            }
            m_navModeBtn->setEnabled(false);
            m_navModeBtn->setStyleSheet(kToolBtnStyle);
        } else {
            m_navModeBtn->setEnabled(true);
            if (m_navModeBtn->isChecked()) {
                m_navModeBtn->setStyleSheet(kToolBtnPrimary);
            } else {
                m_navModeBtn->setStyleSheet(kToolBtnStyle);
            }
        }
    }
}

void MapViewWidget::onTeleopClicked()
{
    const bool nextActive = !TeleopController::instance().isActive();
    TeleopController::instance().setUserPreempted(nextActive);
    TeleopController::instance().setActive(nextActive);
}

void MapViewWidget::onTeleopActiveChanged(bool active)
{
    if (m_teleopBtn) {
        // 保持按钮文本恒定为“抢占控制权”，仅改变视觉高亮
        m_teleopBtn->setText("抢占控制权");
        m_teleopBtn->setStyleSheet(active ? kToolBtnDanger : kToolBtnStyle);
        if (active) {
            Toast::info(this, "已抢占控制权：WASD 移动 | Space 刹车 | Ctrl 加速 | P/L 升降");
        }
    }
    // 抢占控制权状态改变时立即联动取消导航与暂停导航按钮可用性
    updateNavButtonsState();
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

void MapViewWidget::onShowConfigDialog()
{
    if (!m_configDialog) {
        m_configDialog = new QDialog(this);
        m_configDialog->setWindowTitle("参数配置");
        m_configDialog->resize(1100, 700);
        m_configDialog->setMinimumSize(850, 520);
        m_configDialog->setStyleSheet("QDialog { background-color: #1e2125; color: #c8d0d8; }");
        auto *dialogLayout = new QVBoxLayout(m_configDialog);
        dialogLayout->setContentsMargins(10, 10, 10, 10);
        dialogLayout->setSpacing(6);

        m_editConfigWidget = new EditConfigWidget(m_configDialog);
        dialogLayout->addWidget(m_editConfigWidget);

        // 原有信号联动：路网参数更新后刷新地图视图
        connect(m_editConfigWidget, &EditConfigWidget::routeNetworkUpdated,
                this, &MapViewWidget::refreshData);
    }

    if (m_editConfigWidget) {
        m_editConfigWidget->refreshData();
    }

    m_configDialog->show();
    m_configDialog->raise();
    m_configDialog->activateWindow();
}

void MapViewWidget::onShowMoreMenu()
{
    if (!m_moreMenu || !m_moreBtn) return;
    // 在按钮下方弹出工具列表
    QPoint pos = m_moreBtn->mapToGlobal(QPoint(0, m_moreBtn->height()));
    m_moreMenu->exec(pos);
}

void MapViewWidget::onShowAudioManager()
{
    AudioManagerDialog dlg(this);
    dlg.exec();
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
    QString localDir;
    if (!m_mapId.isEmpty()) {
        QString dirById = QCoreApplication::applicationDirPath() + "/maps/" + m_mapId;
        QString dirByName = QCoreApplication::applicationDirPath() + "/maps/" + m_mapName + "_" + m_mapId;
        if (QDir(dirById).exists()) {
            localDir = dirById;
        } else if (QDir(dirByName).exists()) {
            localDir = dirByName;
        } else {
            localDir = dirById;
            QDir().mkpath(localDir);
        }
    } else {
        localDir = QCoreApplication::applicationDirPath() + "/maps";
        QDir().mkpath(localDir);
    }

    QUrl url = QUrl::fromLocalFile(localDir);
    if (!QDesktopServices::openUrl(url)) {
        Toast::error(this, QString("无法打开缓存文件夹: %1").arg(QDir::toNativeSeparators(localDir)));
    } else {
        Toast::info(this, QString("已打开地图缓存目录: %1").arg(QDir::toNativeSeparators(localDir)));
    }
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
            clearScene();
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

// ── 设备右键标记与路网保存 ───────────────────────────────────────────────────

static int nextIdNum(const QJsonArray &arr, const QString &prefix)
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

void MapViewWidget::showRobotContextMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #22262e; border: 1px solid #3a3f4a; padding: 4px; border-radius: 4px; }"
        "QMenu::item { color: #c8d0d8; padding: 6px 20px; font-size: 12px; border-radius: 3px; }"
        "QMenu::item:selected { background-color: #1e4080; color: #ffffff; }"
    );

    QAction *actStation = menu.addAction("标记站点");
    QAction *actWorkStation = menu.addAction("标记工作站");
    QAction *actSpecialWorkStation = menu.addAction("标记特殊工作站");

    QAction *sel = menu.exec(globalPos);
    if (sel == actStation) {
        onMarkStation();
    } else if (sel == actWorkStation) {
        onMarkDevicePoint("highPrecision");
    } else if (sel == actSpecialWorkStation) {
        onMarkDevicePoint("charge");
    }
}

void MapViewWidget::saveRouteToServer(std::function<void(bool ok)> onDone)
{
    if (m_mapId.isEmpty()) {
        Toast::info(this, "当前未加载地图，无法保存站点");
        if (onDone) onDone(false);
        return;
    }

    auto makePointFeat = [](const QJsonObject &pt) -> QJsonObject {
        QJsonArray coords;
        coords.append(pt.value("x").toDouble());
        coords.append(pt.value("y").toDouble());
        QJsonObject geom;
        geom["type"] = "Point";
        geom["coordinates"] = coords;
        QJsonObject props = pt;
        props.remove("x");
        props.remove("y");
        QJsonObject f;
        f["type"] = "Feature";
        f["geometry"] = geom;
        f["properties"] = props;
        return f;
    };

    QJsonArray features;
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

    QPointer<MapViewWidget> self = this;
    ApiService::instance().saveEditedMap(geojson, [self, geojson, onDone](int code, const QJsonObject &resp) {
        if (!self) return;
        if (code == 200) {
            QString localDir = QCoreApplication::applicationDirPath() + "/maps/" + self->m_mapName + "_" + self->m_mapId;
            QDir().mkpath(localDir);
            QFile rf(localDir + "/route.json");
            if (rf.open(QIODevice::WriteOnly)) {
                rf.write(QJsonDocument(geojson).toJson());
                rf.close();
            }
            self->renderRoute();
            self->refreshElementList();
            if (onDone) onDone(true);
        } else {
            QString err = resp.value("msg").toString();
            if (err.isEmpty()) err = resp.value("message").toString();
            if (err.isEmpty()) err = QString("HTTP %1").arg(code);
            Toast::error(self, QString("保存路网失败: %1").arg(err));
            if (onDone) onDone(false);
        }
    });
}

void MapViewWidget::onMarkStation()
{
    if (m_mapId.isEmpty()) {
        Toast::info(this, "当前未加载地图，无法打点");
        return;
    }
    if (!m_robot.posValid) {
        Toast::info(this, "未收到设备位置信息，无法打点");
        return;
    }

    double carTheta = m_robot.yawRad * 180.0 / M_PI;
    int n = nextIdNum(m_points, "p_");
    QJsonObject pt;
    pt["id"] = QString("p_%1").arg(n);
    pt["name"] = QString("LM%1").arg(n);
    pt["typeName"] = "LandMark";
    pt["x"] = m_robot.wx;
    pt["y"] = m_robot.wy;
    pt["yaw"] = m_robot.yawRad;
    pt["theta"] = carTheta;
    pt["robotYaw"] = 0.0;
    pt["roll"] = 0.0;
    pt["pitch"] = 0.0;
    pt["offset"] = 0.0;
    pt["isyawfix"] = false;
    pt["rdsDockable"] = true;
    pt["allowRotate"] = true;
    pt["useDownPGV"] = false;
    m_points.append(pt);

    QPointer<MapViewWidget> self = this;
    saveRouteToServer([self, pt, carTheta](bool ok) {
        if (!self) return;
        if (ok) {
            Toast::success(self, QString("已在设备位置(%1, %2)添加普通站点 %3\n朝向角: %4° 并已保存路网")
                .arg(pt.value("x").toDouble(), 0, 'f', 2).arg(pt.value("y").toDouble(), 0, 'f', 2)
                .arg(pt.value("name").toString()).arg(carTheta, 0, 'f', 1));
        }
    });
}

void MapViewWidget::onMarkDevicePoint(const QString &type)
{
    if (m_mapId.isEmpty()) {
        Toast::info(this, "当前未加载地图，无法打点");
        return;
    }
    if (m_devicePointPending) {
        Toast::info(this, "上一次设备打点尚未完成，请稍候");
        return;
    }
    if (!m_robot.posValid) {
        Toast::info(this, "未收到设备位置信息，无法打点");
        return;
    }
    QString carId = m_robot.carId;
    if (carId.isEmpty()) {
        Toast::info(this, "未选择设备，无法打点");
        return;
    }

    m_devicePointPending = true;

    // 1. 发布 /rob/{id}/pointType 命令让设备记录当前相对位置
    QJsonObject data;
    data["point_type"] = (type == "charge") ? "charge" : "highPrecision";
    data["sender"] = "Web";
    MqttService::instance().publish(
        QString("/rob/%1/pointType").arg(carId),
        QJsonDocument(data).toJson(QJsonDocument::Compact));

    // 2. 阻塞式进度条：等待 3s 让设备更新 epc 坐标
    const int kWaitMs = 3000;
    const int kStepMs = 100;
    auto *prog = new QProgressDialog(
        QString("正在获取设备相对定位 (epc)...\n剩余 %1 秒").arg(kWaitMs / 1000),
        QString(), 0, kWaitMs / kStepMs, this);
    prog->setWindowTitle("设备打点");
    prog->setWindowModality(Qt::WindowModal);
    prog->setMinimumDuration(0);
    prog->setAutoClose(false);
    prog->setAutoReset(false);
    prog->setValue(0);
    prog->setCancelButton(nullptr);

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
            if (!m_robot.posValid) {
                Toast::error(this, "设备位置信息丢失，打点失败");
                return;
            }

            const RobotInfo &info = RobotInfoManager::instance().currentInfo();
            double epcX = info.hasEpc ? info.epc_x : 0.0;
            double epcY = info.hasEpc ? info.epc_y : 0.0;
            double epcTheta = info.hasEpc ? info.epc_theta : 0.0;
            double carYaw = m_robot.yawRad;
            double carTheta = m_robot.yawRad * 180.0 / M_PI;

            if (type == "charge") {
                int n = nextIdNum(m_charges, "c_");
                QJsonObject ch;
                ch["id"] = QString("c_%1").arg(n);
                ch["name"] = QString("CP_%1").arg(n);
                ch["typeName"] = "ChargePoint";
                ch["x"] = m_robot.wx;
                ch["y"] = m_robot.wy;
                ch["epc_x"] = epcX;
                ch["epc_y"] = epcY;
                ch["epc_theta"] = epcTheta;
                ch["epcX"] = epcX;
                ch["epcY"] = epcY;
                ch["epcTheta"] = epcTheta;
                ch["epc_Yaw"] = epcTheta;
                ch["epc_yaw"] = epcTheta;
                ch["yaw"] = carYaw;
                ch["theta"] = carTheta;
                ch["roll"] = 0.0;
                ch["pitch"] = 0.0;
                ch["offset"] = 0.0;
                ch["isyawfix"] = false;
                ch["robotYaw"] = 0.0;
                ch["rdsDockable"] = true;
                ch["allowRotate"] = true;
                ch["useDownPGV"] = false;
                m_charges.append(ch);

                saveRouteToServer([this, epcX, epcY, ch](bool ok) {
                    if (ok) {
                        Toast::success(this, QString("已在设备位置(%1, %2)添加特殊工作站 %3\nepc(%4, %5) 并已保存路网")
                            .arg(m_robot.wx, 0, 'f', 2).arg(m_robot.wy, 0, 'f', 2)
                            .arg(ch.value("name").toString())
                            .arg(epcX, 0, 'f', 2).arg(epcY, 0, 'f', 2));
                    }
                });
            } else if (type == "highPrecision") {
                int n = nextIdNum(m_highPrecisionPoints, "hp_");
                QJsonObject hp;
                hp["id"] = QString("hp_%1").arg(n);
                hp["name"] = QString("AP%1").arg(n);
                hp["typeName"] = "ActionPoint";
                hp["x"] = m_robot.wx;
                hp["y"] = m_robot.wy;
                hp["epc_x"] = epcX;
                hp["epc_y"] = epcY;
                hp["epc_theta"] = epcTheta;
                hp["epcX"] = epcX;
                hp["epcY"] = epcY;
                hp["epcTheta"] = epcTheta;
                hp["epc_Yaw"] = epcTheta;
                hp["epc_yaw"] = epcTheta;
                hp["yaw"] = carYaw;
                hp["theta"] = carTheta;
                hp["roll"] = 0.0;
                hp["pitch"] = 0.0;
                hp["offset"] = 0.0;
                hp["isyawfix"] = false;
                hp["robotYaw"] = 0.0;
                hp["rdsDockable"] = true;
                hp["allowRotate"] = true;
                hp["useDownPGV"] = false;
                m_highPrecisionPoints.append(hp);

                saveRouteToServer([this, epcX, epcY, hp](bool ok) {
                    if (ok) {
                        Toast::success(this, QString("已在设备位置(%1, %2)添加工作站 %3\nepc(%4, %5) 并已保存路网")
                            .arg(m_robot.wx, 0, 'f', 2).arg(m_robot.wy, 0, 'f', 2)
                            .arg(hp.value("name").toString())
                            .arg(epcX, 0, 'f', 2).arg(epcY, 0, 'f', 2));
                    }
                });
            }
        }
    });
    tick->start();
    prog->exec();
    prog->deleteLater();
}
