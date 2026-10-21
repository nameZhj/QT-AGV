/**
 * @file    CreateMapCanvasWidget.cpp
 * @layer   GUI
 * @brief   SLAM grid map and robot pose canvas rendering implementation.
 * @deps    CreateMapCanvasWidget.h, QSvgRenderer, QPainter
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "CreateMapCanvasWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QSvgRenderer>
#include <QtMath>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

CreateMapCanvasWidget::CreateMapCanvasWidget(QWidget *parent)
    : QWidget(parent)
{
}

void CreateMapCanvasWidget::setMapData(const QJsonObject &data)
{
    m_mapData = data;
    update();
}

void CreateMapCanvasWidget::setRobotPose(const QJsonObject &pose)
{
    m_robotPose = pose;
    update();
}

void CreateMapCanvasWidget::setPoles(const QJsonArray &poles)
{
    m_poles = poles;
    update();
}

void CreateMapCanvasWidget::setHighMappingActive(bool active)
{
    m_highMappingActive = active;
    update();
}

void CreateMapCanvasWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Qt::white);

    if (m_mapData.isEmpty()) {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter, "等待地图数据...");
        return;
    }

    QJsonObject info = m_mapData.value("info").toObject();
    int cols = m_mapData.value("width").toInt(info.value("width").toInt());
    int rows = m_mapData.value("height").toInt(info.value("height").toInt());
    double resolution = m_mapData.value("resolution").toDouble(info.value("resolution").toDouble(1.0));
    if (cols <= 0 || rows <= 0 || resolution <= 0) return;

    QJsonArray dataArray = m_mapData.value("data").toArray(info.value("data").toArray());
    if (dataArray.isEmpty()) return;

    double originX = m_mapData.value("originX").toDouble(0);
    double originY = m_mapData.value("originY").toDouble(0);
    QJsonObject origin = info.value("origin").toObject();
    if (origin.contains("position")) {
        QJsonObject pos = origin.value("position").toObject();
        if (m_mapData.value("originX").isUndefined()) originX = pos.value("x").toDouble(0);
        if (m_mapData.value("originY").isUndefined()) originY = pos.value("y").toDouble(0);
    }

    double mapW = cols * resolution;
    double mapH = rows * resolution;
    double scale = qMin((double)width() / mapW, (double)height() / mapH);
    double resPx = resolution * scale;
    double actualW = cols * resPx;
    double actualH = rows * resPx;
    double xOff = (width() - actualW) / 2.0;
    double yOff = (height() - actualH) / 2.0;

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

    if (m_highMappingActive) {
        for (const auto &item : m_poles) {
            QJsonObject pole = item.toObject();
            QJsonObject pos = pole.value("position").toObject();
            double px = xOff + (pos.value("x").toDouble() - originX) * scale;
            double py = height() - yOff - (pos.value("y").toDouble() - originY) * scale;
            QString ns = pole.value("ns").toString();
            p.setBrush((ns == "matched") ? QColor(0, 200, 0, 180) : QColor(255, 165, 0, 180));
            p.setPen(Qt::black);
            p.drawEllipse(QPointF(px, py), 6, 6);
        }
    }

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

        const double kRobotLength = 1.0;
        const double kRobotWidth  = 0.5;
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
