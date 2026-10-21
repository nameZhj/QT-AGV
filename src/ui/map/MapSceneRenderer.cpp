/**
 * @file    MapSceneRenderer.cpp
 * @layer   GUI
 * @brief   Implementation of map graphics scene rendering for nodes, lines, and areas.
 * @deps    MapSceneRenderer.h, MapEditDataModel.h, MapCurveHandleManager.h, MapPgmUtil.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapSceneRenderer.h"
#include "MapEditDataModel.h"
#include "MapCurveHandleManager.h"
#include "MapEditTypes.h"
#include "utils/MapPgmUtil.h"
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsPathItem>
#include <QGraphicsLineItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsSimpleTextItem>
#include <QTextDocument>
#include <QFontMetrics>
#include <QPen>
#include <QBrush>
#include <cmath>

MapSceneRenderer::MapSceneRenderer(QGraphicsScene *scene, MapEditDataModel *model,
                                   CoordToSceneFn worldToScene, QObject *parent)
    : QObject(parent), m_scene(scene), m_model(model), m_worldToScene(std::move(worldToScene))
{
}

void MapSceneRenderer::setBackgroundPixmap(const QPixmap &pixmap)
{
    if (!m_scene) return;
    if (!m_mapBgItem) {
        m_mapBgItem = m_scene->addPixmap(pixmap);
        m_mapBgItem->setZValue(-100);
    } else {
        m_mapBgItem->setPixmap(pixmap);
    }
}

void MapSceneRenderer::renderMap(const QImage &mapImage, double imgW, double imgH,
                                const QString &selType, const QString &selId,
                                bool showReflectors, bool isDragStatus)
{
    Q_UNUSED(mapImage);
    if (!m_scene || !m_model) return;
    for (auto *it : m_featureItems) {
        if (it && it->scene() == m_scene) m_scene->removeItem(it);
        delete it;
    }
    m_featureItems.clear();

    double pxPerMeterX = (imgW > 0) ? imgW / 20.0 : 20.0;
    double pxPerMeterY = (imgH > 0) ? imgH / 20.0 : 20.0;

    renderHighLevelAreas(selType, selId, 6.0);
    renderStationNodes(pxPerMeterX, pxPerMeterY);
    renderRouteLines();
    renderRouteCurves(isDragStatus);
    if (showReflectors) renderReflectorPoles();
}

void MapSceneRenderer::renderHighLevelAreas(const QString &selType, const QString &selId, double hitTolSize)
{
    if (!m_scene || !m_model) return;
    QJsonArray areas = m_model->highLevelAreas();

    for (int idx = 0; idx < areas.size(); ++idx) {
        QJsonObject areaObj = areas[idx].toObject();
        QString areaId = areaObj.value("id").toString();
        if (areaId.isEmpty()) {
            areaId = QString("highLevelArea_%1").arg(idx + 1);
            areaObj["id"] = areaId;
            areas[idx] = areaObj;
        }

        double minX = 0, maxX = 0, minY = 0, maxY = 0;
        MapPgmUtil::extractAreaBounds(areaObj, minX, maxX, minY, maxY);
        QRectF rect = QRectF(m_worldToScene(minX, maxY), m_worldToScene(maxX, minY)).normalized();

        bool isSelected = (selType == "highLevelArea" && selId == areaId);
        QColor borderCol = isSelected ? QColor("#00e676") : QColor("#8b5cf6");
        double penWidth = isSelected ? 0.8 : 0.5;
        QColor fillCol = isSelected ? QColor(0, 230, 118, 30) : QColor(139, 92, 246, 20);

        auto *rectItem = m_scene->addRect(rect, QPen(borderCol, penWidth), QBrush(fillCol));
        rectItem->setZValue(-10);
        rectItem->setData(0, "highLevelArea"); rectItem->setData(1, areaId); rectItem->setData(2, QColor("#8b5cf6"));
        m_featureItems["highLevelArea_" + areaId] = rectItem;

        auto *textItem = m_scene->addSimpleText(areaObj.value("name").toString("AdvancedArea"));
        textItem->setBrush(QBrush(isSelected ? QColor("#00e676") : QColor("#8b5cf6")));
        textItem->setFont(QFont("Segoe UI", 11, QFont::Bold));
        textItem->setScale(0.25);
        textItem->setZValue(-9);
        QRectF tb = textItem->boundingRect();
        textItem->setPos(rect.center().x() - (tb.width() * 0.25) / 2.0, rect.center().y() - (tb.height() * 0.25) / 2.0);

        if (!isSelected) continue;

        double hSize = (hitTolSize > 0) ? hitTolSize : 6.0;
        QPointF c = rect.center();
        const QPointF hPts[] = { rect.topLeft(), rect.topRight(), rect.bottomLeft(), rect.bottomRight(),
                                 {c.x(), rect.top()}, {c.x(), rect.bottom()}, {rect.left(), c.y()}, {rect.right(), c.y()} };
        for (const auto &hp : hPts) {
            auto *h = m_scene->addRect(QRectF(hp.x() - hSize / 2.0, hp.y() - hSize / 2.0, hSize, hSize),
                                       QPen(QColor("#00e676"), 0.8), QBrush(Qt::white));
            h->setZValue(-8); h->setAcceptedMouseButtons(Qt::NoButton);
        }
    }
}

void MapSceneRenderer::renderStationNodes(double pxPerMeterX, double pxPerMeterY)
{
    if (!m_scene || !m_model) return;

    auto addNode = [&](const QJsonObject &pt, const QColor &fillCol, const QColor &borderCol, const QString &type, bool visible = true) {
        QPointF sp = m_worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble());
        QString id = pt.value("id").toString();
        QString nameText = pt.value("name").toString(id);

        double rectW = 0, rectH = 0;
        QPainterPath crossPath;
        QPainterPath path = MapPgmUtil::createPointPath(pxPerMeterX, pxPerMeterY, &rectW, &rectH, &crossPath, 0.0);

        QFont f("Segoe UI");
        f.setPointSizeF(qBound(4.0, rectH * 0.35, 7.0));
        f.setBold(true);
        QFontMetrics fm(f);
        int tw = fm.horizontalAdvance(nameText);

        auto *item = m_scene->addPath(path, QPen(borderCol, 0.8), QBrush(fillCol));
        item->setPos(sp); item->setZValue(10);
        item->setData(0, type); item->setData(1, id); item->setData(2, fillCol); item->setVisible(visible);

        auto *crossItem = m_scene->addPath(crossPath, QPen(QColor("#E74C3C"), 0.8, Qt::SolidLine, Qt::FlatCap));
        crossItem->setParentItem(item); crossItem->setPos(0, 0); crossItem->setAcceptedMouseButtons(Qt::NoButton);

        double yawVal = pt.contains("yaw") ? pt.value("yaw").toDouble()
                      : (pt.contains("robotYaw") ? pt.value("robotYaw").toDouble()
                      : pt.value("theta").toDouble() * MapEdit::kDegToRad);
        double rotDeg = -yawVal * MapEdit::kRadToDeg;
        item->setRotation(rotDeg);

        double arrowLen = qBound(3.5, rectW * 0.26, 6.5), wingLen = qBound(1.8, arrowLen * 0.40, 2.6), wingHalf = qBound(1.2, wingLen * 0.65, 1.8);
        QPainterPath arrPath;
        arrPath.moveTo(0.0, 0.0); arrPath.lineTo(arrowLen, 0.0);
        arrPath.moveTo(arrowLen - wingLen, -wingHalf); arrPath.lineTo(arrowLen, 0.0); arrPath.lineTo(arrowLen - wingLen, wingHalf);
        auto *arrowItem = m_scene->addPath(arrPath, QPen(QColor("#E74C3C"), 0.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        arrowItem->setParentItem(item); arrowItem->setAcceptedMouseButtons(Qt::NoButton);

        m_featureItems[type + "_" + id] = item;

        auto *lbl = m_scene->addText(nameText, f);
        lbl->document()->setDocumentMargin(0); lbl->setDefaultTextColor(QColor("#1A1A1A"));
        lbl->setParentItem(item); lbl->setTransformOriginPoint(tw / 2.0, fm.height() / 2.0); lbl->setRotation(-rotDeg);
        lbl->setPos(-tw / 2.0, (rectH >= 12.0) ? (rectH * 0.16) : (rectH / 2.0 + 1.0));
        lbl->setAcceptedMouseButtons(Qt::NoButton); lbl->setVisible(visible);
    };

    for (const auto &v : m_model->points())              addNode(v.toObject(), QColor(210, 230, 250, 235), QColor(41, 128, 185, 240), "point");
    for (const auto &v : m_model->charges())             addNode(v.toObject(), QColor(254, 243, 199, 235), QColor(217, 119, 6, 240), "charge");
    for (const auto &v : m_model->highPrecisionPoints()) addNode(v.toObject(), QColor(254, 226, 226, 235), QColor(220, 38, 38, 240), "highPrecision", true);
}

void MapSceneRenderer::renderRouteLines()
{
    if (!m_scene || !m_model) return;
    for (const auto &v : m_model->lines()) {
        QJsonObject ln = v.toObject();
        QPointF a = m_worldToScene(ln.value("x1").toDouble(), ln.value("y1").toDouble());
        QPointF b = m_worldToScene(ln.value("x2").toDouble(), ln.value("y2").toDouble());
        QString id = ln.value("id").toString();
        int direct = ln.value("direct").toInt(10);
        QPen pen(QColor("#1296DB"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        auto *li = m_scene->addLine(a.x(), a.y(), b.x(), b.y(), pen);
        li->setZValue(5); li->setData(0, "line"); li->setData(1, id); li->setData(2, pen.color());
        m_featureItems["line_" + id] = li;

        QPointF dir = b - a;
        double len = std::hypot(dir.x(), dir.y());
        if (len > 1e-3) {
            dir /= len;
            QList<QPolygonF> arrows = MapPgmUtil::createDirectionArrows((a + b) / 2.0, dir, direct, len);
            for (const auto &arr : arrows) {
                auto *arrItem = m_scene->addPolygon(arr, QPen(Qt::NoPen), QBrush(pen.color()));
                arrItem->setParentItem(li); arrItem->setZValue(6); arrItem->setAcceptedMouseButtons(Qt::NoButton);
            }
        }
    }
}

void MapSceneRenderer::renderRouteCurves(bool isDragStatus)
{
    if (!m_scene || !m_model) return;
    for (const auto &v : m_model->curves()) {
        QJsonObject cv = v.toObject();
        QString id = cv.value("id").toString();
        int direct = cv.value("direct").toInt(10);
        QPen pen(QColor("#9B59B6"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

        QJsonArray ctrlArr = cv.value("controlPoints").toArray();
        QPainterPath path;
        QList<QPointF> curveScenePts;
        if (!ctrlArr.isEmpty()) {
            for (const auto &pv : ctrlArr)
                curveScenePts.append(m_worldToScene(pv.toObject().value("x").toDouble(), pv.toObject().value("y").toDouble()));
            path = MapPgmUtil::getBezierPath(curveScenePts);
        } else {
            QPointF p1 = m_worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble());
            QPointF pc = m_worldToScene(cv.value("cx").toDouble(), cv.value("cy").toDouble());
            QPointF p2 = m_worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble());
            curveScenePts << p1 << pc << p2;
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
                arrItem->setParentItem(pi); arrItem->setZValue(6); arrItem->setAcceptedMouseButtons(Qt::NoButton);
            }
        }
        if (m_curveHandleMgr) m_curveHandleMgr->rebuildCurveHandles(id, isDragStatus);
    }
}

void MapSceneRenderer::renderReflectorPoles()
{
    if (!m_scene || !m_model) return;
    for (const auto &v : m_model->reflectorPoles()) {
        QJsonObject f = v.toObject();
        QJsonArray coords = f.value("geometry").toObject().value("coordinates").toArray();
        if (coords.size() < 2) continue;
        double x = coords[0].isString() ? coords[0].toString().toDouble() : coords[0].toDouble();
        double y = coords[1].isString() ? coords[1].toString().toDouble() : coords[1].toDouble();
        auto *e = m_scene->addEllipse(-5, -5, 10, 10, QPen(QColor("#caa020"), 1.5), QBrush(QColor("#f0c040")));
        e->setPos(m_worldToScene(x, y)); e->setZValue(8);
    }
}

void MapSceneRenderer::updateGrid(bool showGrid, double worldL, double worldB, double worldR, double worldT,
                                  double imgW, double imgH)
{
    for (auto *l : m_gridLines) {
        if (l && m_scene) m_scene->removeItem(l);
        delete l;
    }
    m_gridLines.clear();

    if (!showGrid || !m_scene || imgW <= 0 || imgH <= 0 || worldR <= worldL) return;

    const double gridStep = 1.0;
    QPen gridPen (QColor(80, 110, 160,  80), 0.5);
    QPen axisGPen(QColor(120, 160, 240, 130), 0.8);

    for (double wx = std::ceil(worldL / gridStep) * gridStep; wx <= worldR + 1e-9; wx += gridStep) {
        QPointF a = m_worldToScene(wx, worldB), b = m_worldToScene(wx, worldT);
        auto *l = m_scene->addLine(a.x(), a.y(), b.x(), b.y(), std::abs(wx) < 1e-6 ? axisGPen : gridPen);
        l->setZValue(-50); m_gridLines.append(l);
    }
    for (double wy = std::ceil(worldB / gridStep) * gridStep; wy <= worldT + 1e-9; wy += gridStep) {
        QPointF a = m_worldToScene(worldL, wy), b = m_worldToScene(worldR, wy);
        auto *l = m_scene->addLine(a.x(), a.y(), b.x(), b.y(), std::abs(wy) < 1e-6 ? axisGPen : gridPen);
        l->setZValue(-50); m_gridLines.append(l);
    }
}
