#ifndef CREATEMAPWIDGET_H
#define CREATEMAPWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QPolygonF>
#include <QKeyEvent>
#include <QSvgRenderer>

// 建图画布 - 渲染 MQTT 接收的栅格地图数据
class MapCanvasWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapCanvasWidget(QWidget *parent = nullptr) : QWidget(parent) {}

    void setMapData(const QJsonObject &data) { m_mapData = data; update(); }
    void setRobotPose(const QJsonObject &pose) { m_robotPose = pose; update(); }
    void setPoles(const QJsonArray &poles) { m_poles = poles; update(); }
    void setHighMappingActive(bool active) { m_highMappingActive = active; update(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), Qt::white);

        if (m_mapData.isEmpty()) {
            p.setPen(Qt::gray);
            p.drawText(rect(), Qt::AlignCenter, "等待地图数据...");
            return;
        }

        // 解析地图数据
        QJsonObject info = m_mapData.value("info").toObject();
        int cols = m_mapData.value("width").toInt(info.value("width").toInt());
        int rows = m_mapData.value("height").toInt(info.value("height").toInt());
        double resolution = m_mapData.value("resolution").toDouble(info.value("resolution").toDouble(1.0));
        if (cols <= 0 || rows <= 0 || resolution <= 0) return;

        // 获取栅格数据
        QJsonArray dataArray = m_mapData.value("data").toArray(info.value("data").toArray());
        if (dataArray.isEmpty()) return;

        // 获取原点
        double originX = m_mapData.value("originX").toDouble(0);
        double originY = m_mapData.value("originY").toDouble(0);
        QJsonObject origin = info.value("origin").toObject();
        if (origin.contains("position")) {
            QJsonObject pos = origin.value("position").toObject();
            if (m_mapData.value("originX").isUndefined()) originX = pos.value("x").toDouble(0);
            if (m_mapData.value("originY").isUndefined()) originY = pos.value("y").toDouble(0);
        }

        // 计算缩放
        double mapW = cols * resolution;
        double mapH = rows * resolution;
        double scale = qMin((double)width() / mapW, (double)height() / mapH);
        double resPx = resolution * scale;
        double actualW = cols * resPx;
        double actualH = rows * resPx;
        double xOff = (width() - actualW) / 2.0;
        double yOff = (height() - actualH) / 2.0;

        // 绘制栅格
        p.setPen(Qt::NoPen);
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                int idx = y * cols + x;
                if (idx >= dataArray.size()) break;
                int v = dataArray[idx].toInt(0) & 0xff;
                if (v >= 1 && v <= 100) {
                    int g = qRound(255 - (v / 100.0) * 255);
                    p.setBrush(QColor(g, g, g));
                    double px0 = xOff + x * resPx;
                    double py0 = height() - yOff - (y + 1) * resPx;
                    p.drawRect(QRectF(px0, py0, qMax(resPx, 1.0), qMax(resPx, 1.0)));
                }
            }
        }

        // 绘制反光柱
        if (m_highMappingActive) {
            for (const auto &item : m_poles) {
                QJsonObject pole = item.toObject();
                QJsonObject pos = pole.value("position").toObject();
                double px = xOff + (pos.value("x").toDouble() - originX) * scale;
                double py = height() - yOff - (pos.value("y").toDouble() - originY) * scale;
                QString ns = pole.value("ns").toString();
                if (ns == "matched") {
                    p.setBrush(QColor(0, 200, 0, 180));
                } else {
                    p.setBrush(QColor(255, 165, 0, 180));
                }
                p.setPen(Qt::black);
                p.drawEllipse(QPointF(px, py), 6, 6);
            }
        }

        // 绘制机器人位置 (SVG icon, crisp at any zoom)
        if (m_robotPose.contains("x") && m_robotPose.contains("y")) {
            double rx = m_robotPose.value("x").toDouble();
            double ry = m_robotPose.value("y").toDouble();
            double px = xOff + (rx - originX) * scale;
            double py = height() - yOff - (ry - originY) * scale;
            double angle = m_robotPose.value("angle").toDouble(0);
            if (qAbs(angle) > M_PI * 2) angle = angle * M_PI / 180.0;

            static QPixmap s_robotPix;
            if (s_robotPix.isNull()) {
                QSvgRenderer renderer(QString(":/ic-robot1.svg"));
                if (renderer.isValid()) {
                    s_robotPix = QPixmap(180, 330);
                    s_robotPix.fill(Qt::transparent);
                    QPainter rp(&s_robotPix);
                    rp.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
                    renderer.render(&rp);
                }
            }

            // scale 为当前屏幕像素/米 (ppm)。真实尺寸: 长 1.10m, 宽 0.60m
            const double kRobotLength = 1.10;
            const double kRobotWidth  = 0.60;
            double sceneW = kRobotWidth * scale;
            double sceneH = kRobotLength * scale;

            p.save();
            p.translate(px, py);
            p.rotate(90.0 - qRadiansToDegrees(angle));
            if (!s_robotPix.isNull()) {
                double sx = sceneW / (double)s_robotPix.width();
                double sy = sceneH / (double)s_robotPix.height();
                p.scale(sx, sy);
                p.setRenderHint(QPainter::SmoothPixmapTransform);
                p.drawPixmap(-s_robotPix.width() / 2, -s_robotPix.height() / 2, s_robotPix);
            } else {
                p.setBrush(QColor(30, 41, 59, 220));
                p.setPen(QPen(QColor(56, 189, 248), 1.5));
                p.drawRect(QRectF(-sceneW / 2.0, -sceneH / 2.0, sceneW, sceneH));
            }
            p.restore();
        }
    }

private:
    QJsonObject m_mapData;
    QJsonObject m_robotPose;
    QJsonArray m_poles;
    bool m_highMappingActive = false;
};

// 对应 create-map/index.vue - 建图功能
// 包含自适应建图、高阶建图（反光柱）、暂停/恢复巡航、MQTT 实时地图数据、遥控
//
// 注：键盘遥控已提取为全局 TeleopController 单例，本页面只保留：
//   - 鼠标方向/刹车按钮（走 /rob/{agvId}/cmd_vel 与 /brake 主题）
//   - "启用遥控"开关（委托给 TeleopController::setActive，与主窗口底部"开始遥控"按钮同步）
//   - HUD（线/角速度来自 /carto_pose 实时反馈，遥控/刹车状态来自 TeleopController 信号）
class CreateMapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CreateMapWidget(QWidget *parent = nullptr);

    // 建图中途退出时弹出保存提示，API 返回成功后才允许离开
    // 返回 true 表示可以离开，false 表示用户取消或 API 调用中
    bool tryExitMapping();

signals:
    void goBack();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onStartAdaptiveClick();
    void onStopAdaptiveClick();
    void onStartHighClick();
    void onStopHighClick();
    void onResetReflectorAll();
    void onResetReflectorLocal();
    void onToggleCruiseClick();
    void onMqttMessage(const QString &topic, const QByteArray &payload);
    void onTeleopActiveChanged(bool active);
    void onTeleopBrakeChanged(bool brake);

private:
    void setupUI();
    void initMqtt();
    void disconnectMqtt();
    void updateToolbar();

    // 工具栏按钮
    QPushButton *m_adaptiveBtn = nullptr;
    QPushButton *m_highBtn = nullptr;
    QPushButton *m_stopAdaptiveBtn = nullptr;
    QPushButton *m_resetAllBtn = nullptr;
    QPushButton *m_resetLocalBtn = nullptr;
    QPushButton *m_stopHighBtn = nullptr;
    QPushButton *m_cruiseBtn = nullptr;

    // HUD
    QLabel *m_linearLabel = nullptr;
    QLabel *m_angularLabel = nullptr;
    QLabel *m_teleopLabel = nullptr;
    QLabel *m_brakeLabel = nullptr;

    // 地图画布区域
    MapCanvasWidget *m_mapCanvas = nullptr;

    // 遥控面板
    QWidget *m_teleopPanel = nullptr;
    QPushButton *m_teleopEnableBtn = nullptr;
    QPushButton *m_forwardBtn = nullptr;
    QPushButton *m_backwardBtn = nullptr;
    QPushButton *m_leftBtn = nullptr;
    QPushButton *m_rightBtn = nullptr;
    QPushButton *m_brakeBtn = nullptr;
    bool m_brakeActive = false;

    // 状态
    bool m_mappingActive = false;
    bool m_highMappingActive = false;
    bool m_cruiseSuspended = false;

    // MQTT 数据
    QJsonObject m_mapData;
    QJsonObject m_robotPose;
    QJsonArray m_poles;
    int m_cellSize = 4;
};

#endif // CREATEMAPWIDGET_H
