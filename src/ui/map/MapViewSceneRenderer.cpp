/**
 * @file    MapViewSceneRenderer.cpp
 * @layer   GUI
 * @brief   Topological map scene elements rendering implementation.
 * @deps    MapViewSceneRenderer.h, MapPgmUtil.h, RouteFeatureSelect.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewSceneRenderer.h"
#include "utils/MapPgmUtil.h"
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsTextItem>
#include <QTextDocument>
#include <QFontMetrics>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MapViewSceneRenderer::MapViewSceneRenderer(QObject *parent)
    : QObject(parent)
{
}

void MapViewSceneRenderer::setExtent(double wL, double wB, double wR, double wT, double imgW, double imgH)
{
    m_worldL = wL; m_worldB = wB; m_worldR = wR; m_worldT = wT;
    m_imgW = imgW; m_imgH = imgH;
}

QPointF MapViewSceneRenderer::worldToScene(double wx, double wy) const
{
    double mapW = m_worldR - m_worldL, mapH = m_worldT - m_worldB;
    if (m_imgW <= 0 || m_imgH <= 0 || mapW <= 0 || mapH <= 0) return {wx, wy};
    return { (wx - m_worldL) / mapW * m_imgW, (m_worldT - wy) / mapH * m_imgH };
}

QPointF MapViewSceneRenderer::sceneToWorld(double sx, double sy) const
{
    double mapW = m_worldR - m_worldL, mapH = m_worldT - m_worldB;
    if (m_imgW <= 0 || m_imgH <= 0 || mapW <= 0 || mapH <= 0) return {sx, sy};
    return { sx / m_imgW * mapW + m_worldL, m_worldT - sy / m_imgH * mapH };
}

double MapViewSceneRenderer::hitTolScene(double screenPx, double scale) const
{
    return scale > 1e-6 ? screenPx / scale : screenPx;
}

void MapViewSceneRenderer::extractAreaBounds(const QJsonObject &obj, double &minX, double &maxX, double &minY, double &maxY)
{
    MapPgmUtil::extractAreaBounds(obj, minX, maxX, minY, maxY);
}

void MapViewSceneRenderer::clearGridItems(QGraphicsScene *scene, QList<QGraphicsItem*> &gridItems)
{
    for (auto *it : gridItems) {
        if (it) {
            if (scene && it->scene() == scene) scene->removeItem(it);
            delete it;
        }
    }
    gridItems.clear();
}

void MapViewSceneRenderer::clearRouteItems(QGraphicsScene *scene, QList<QGraphicsItem*> &routeItems, QGraphicsItem *&selItem)
{
    selItem = nullptr;
    for (auto *it : routeItems) {
        if (it) {
            if (scene && it->scene() == scene) scene->removeItem(it);
            delete it;
        }
    }
    routeItems.clear();
}

void MapViewSceneRenderer::renderAxesAndGrid(QGraphicsScene *scene, bool showGrid, QList<QGraphicsItem*> &gridItems)
{
    clearGridItems(scene, gridItems);
    if (!scene || !showGrid || m_imgW <= 0 || m_imgH <= 0) return;

    auto tag = [&gridItems](QGraphicsItem *item, int z) {
        item->setZValue(z); gridItems.append(item); return item;
    };
    const double gridStep = 1.0;
    QPen gridPen(QColor(80, 110, 160, 80), 0.5);
    QPen axisGPen(QColor(120, 160, 240, 130), 0.8);

    for (double wx = std::ceil(m_worldL / gridStep) * gridStep; wx <= m_worldR + 1e-9; wx += gridStep) {
        QPointF a = worldToScene(wx, m_worldB), b = worldToScene(wx, m_worldT);
        tag(scene->addLine(a.x(), a.y(), b.x(), b.y(), std::abs(wx) < 1e-6 ? axisGPen : gridPen), 9);
    }
    for (double wy = std::ceil(m_worldB / gridStep) * gridStep; wy <= m_worldT + 1e-9; wy += gridStep) {
        QPointF a = worldToScene(m_worldL, wy), b = worldToScene(m_worldR, wy);
        tag(scene->addLine(a.x(), a.y(), b.x(), b.y(), std::abs(wy) < 1e-6 ? axisGPen : gridPen), 9);
    }
}

void MapViewSceneRenderer::renderRoute(QGraphicsScene *scene,
                                      const QJsonArray &points,
                                      const QJsonArray &lines,
                                      const QJsonArray &curves,
                                      const QJsonArray &charges,
                                      const QJsonArray &highPrecisionPoints,
                                      const QJsonArray &highLevelAreas,
                                      QList<QGraphicsItem*> &routeItems,
                                      QGraphicsItem *&selItem)
{
    clearRouteItems(scene, routeItems, selItem);
    if (!scene) return;

    // 1. High-level areas
    for (int i = 0; i < highLevelAreas.size(); ++i) {
        QJsonObject ao = highLevelAreas[i].toObject();
        QString aid = ao.value("id").toString(QString("highLevelArea_%1").arg(i + 1));
        double minX = 0, maxX = 0, minY = 0, maxY = 0;
        extractAreaBounds(ao, minX, maxX, minY, maxY);
        QRectF rect = QRectF(worldToScene(minX, maxY), worldToScene(maxX, minY)).normalized();

        auto *rectItem = scene->addRect(rect, QPen(QColor("#8b5cf6"), 0.5), QBrush(QColor(139, 92, 246, 20)));
        rectItem->setZValue(5);
        rectItem->setData(0, "highLevelArea"); rectItem->setData(1, aid); rectItem->setData(2, QColor("#8b5cf6"));
        routeItems.append(rectItem);

        auto *textItem = scene->addSimpleText(ao.value("name").toString("AdvancedArea"));
        textItem->setBrush(QBrush(QColor("#8b5cf6")));
        textItem->setFont(QFont("Segoe UI", 11, QFont::Bold));
        textItem->setScale(0.25);
        textItem->setParentItem(rectItem);
        textItem->setZValue(1);
        QRectF tb = textItem->boundingRect();
        QPointF c = rect.center();
        textItem->setPos(c.x() - (tb.width() * 0.25) / 2.0, c.y() - (tb.height() * 0.25) / 2.0);
    }

    // 2. Lines
    QPen linePen(QColor("#1296DB"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    for (const auto &v : lines) {
        QJsonObject ln = v.toObject();
        QPointF a = worldToScene(ln.value("x1").toDouble(), ln.value("y1").toDouble());
        QPointF b = worldToScene(ln.value("x2").toDouble(), ln.value("y2").toDouble());
        int direct = ln.value("direct").toInt(10);
        auto *lineItem = scene->addLine(a.x(), a.y(), b.x(), b.y(), linePen);
        lineItem->setZValue(20);
        lineItem->setData(0, "line"); lineItem->setData(1, ln.value("id").toString());
        lineItem->setData(2, linePen.color()); lineItem->setData(3, ln.value("name").toString(ln.value("id").toString()));
        routeItems.append(lineItem);

        QPointF dir = b - a;
        double len = std::hypot(dir.x(), dir.y());
        if (len > 1e-3) {
            dir /= len;
            for (const auto &poly : MapPgmUtil::createDirectionArrows((a + b) / 2.0, dir, direct, len)) {
                auto *arr = scene->addPolygon(poly, QPen(Qt::NoPen), QBrush(linePen.color()));
                arr->setParentItem(lineItem); arr->setZValue(6); arr->setAcceptedMouseButtons(Qt::NoButton);
            }
        }
    }

    // 3. Curves
    QPen curvePen(QColor("#9B59B6"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    for (const auto &v : curves) {
        QJsonObject cv = v.toObject();
        int direct = cv.value("direct").toInt(10);
        QList<QPointF> pts;
        QJsonArray ctrlArr = cv.value("controlPoints").toArray();
        QPainterPath path;
        if (ctrlArr.size() >= 2) {
            for (const auto &pv : ctrlArr) pts.append(worldToScene(pv.toObject().value("x").toDouble(), pv.toObject().value("y").toDouble()));
            path = MapPgmUtil::getBezierPath(pts);
        } else {
            QPointF p1 = worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble());
            QPointF pc = worldToScene(cv.value("cx").toDouble(), cv.value("cy").toDouble());
            QPointF p2 = worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble());
            pts << p1 << pc << p2; path.moveTo(p1); path.quadTo(pc, p2);
        }
        auto *pathItem = scene->addPath(path, curvePen);
        pathItem->setZValue(20);
        pathItem->setData(0, "curve"); pathItem->setData(1, cv.value("id").toString());
        pathItem->setData(2, curvePen.color()); pathItem->setData(3, cv.value("name").toString(cv.value("id").toString()));
        routeItems.append(pathItem);

        if (pts.size() >= 2) {
            double cLen = std::hypot(pts.last().x() - pts.first().x(), pts.last().y() - pts.first().y());
            for (const auto &poly : MapPgmUtil::createBezierDirectionArrows(pts, direct, cLen)) {
                auto *arr = scene->addPolygon(poly, QPen(Qt::NoPen), QBrush(curvePen.color()));
                arr->setParentItem(pathItem); arr->setZValue(6); arr->setAcceptedMouseButtons(Qt::NoButton);
            }
        }
    }

    // 4. Stations
    double ppmX = (m_imgW > 0 && (m_worldR > m_worldL)) ? (m_imgW / (m_worldR - m_worldL)) : 20.0;
    double ppmY = (m_imgH > 0 && (m_worldT > m_worldB)) ? (m_imgH / (m_worldT - m_worldB)) : 20.0;

    auto renderStation = [&](const QJsonObject &pt, const QColor &fillCol, const QColor &borderCol, const QString &type) {
        QPointF sp = worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble());
        QString id = pt.value("id").toString();
        QString name = pt.value("name").toString(id);

        double rectW = 0, rectH = 0;
        QPainterPath crossPath;
        QPainterPath path = MapPgmUtil::createPointPath(ppmX, ppmY, &rectW, &rectH, &crossPath, 0.0);

        QFont f("Segoe UI");
        f.setPointSizeF(qBound(4.0, rectH * 0.35, 7.0)); f.setBold(true);
        int tw = QFontMetrics(f).horizontalAdvance(name);

        auto *item = scene->addPath(path, QPen(borderCol, 0.8), QBrush(fillCol));
        item->setPos(sp);
        item->setData(0, type); item->setData(1, id); item->setData(2, fillCol);
        item->setData(3, name); item->setData(4, QPointF(pt.value("x").toDouble(), pt.value("y").toDouble()));
        item->setZValue(20);
        routeItems.append(item);

        auto *crossItem = scene->addPath(crossPath, QPen(QColor("#E74C3C"), 0.8));
        crossItem->setParentItem(item); crossItem->setAcceptedMouseButtons(Qt::NoButton);

        double yawVal = pt.contains("yaw") ? pt.value("yaw").toVariant().toDouble() : (pt.contains("theta") ? pt.value("theta").toVariant().toDouble() * M_PI / 180.0 : 0.0);
        double rotDeg = -yawVal * 180.0 / M_PI;
        item->setRotation(rotDeg);

        double aLen = qBound(3.5, rectW * 0.26, 6.5), wLen = qBound(1.8, aLen * 0.40, 2.6), wHalf = qBound(1.2, wLen * 0.65, 1.8);
        QPainterPath arrP;
        arrP.moveTo(0, 0); arrP.lineTo(aLen, 0);
        arrP.moveTo(aLen - wLen, -wHalf); arrP.lineTo(aLen, 0); arrP.lineTo(aLen - wLen, wHalf);
        auto *arrowItem = scene->addPath(arrP, QPen(QColor("#E74C3C"), 0.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        arrowItem->setParentItem(item); arrowItem->setZValue(12); arrowItem->setAcceptedMouseButtons(Qt::NoButton);

        auto *lbl = scene->addText(name, f);
        lbl->document()->setDocumentMargin(0); lbl->setDefaultTextColor(QColor("#1A1A1A"));
        lbl->setParentItem(item); lbl->setTransformOriginPoint(tw / 2.0, QFontMetrics(f).height() / 2.0);
        lbl->setRotation(-rotDeg);
        lbl->setPos(-tw / 2.0, rectH >= 12.0 ? rectH * 0.16 : rectH / 2.0 + 1.0);
        lbl->setAcceptedMouseButtons(Qt::NoButton);
    };

    for (const auto &v : points)              renderStation(v.toObject(), QColor(210, 230, 250, 235), QColor(41, 128, 185, 240), "point");
    for (const auto &v : charges)             renderStation(v.toObject(), QColor(254, 243, 199, 235), QColor(217, 119, 6, 240), "charge");
    for (const auto &v : highPrecisionPoints) renderStation(v.toObject(), QColor(254, 226, 226, 235), QColor(220, 38, 38, 240), "highPrecision");
}
