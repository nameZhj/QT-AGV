/**
 * @file    MapCurveHandleManager.cpp
 * @layer   GUI
 * @brief   Implementation of Bezier curve handle synchronization and interactive updates.
 * @deps    MapCurveHandleManager.h, MapEditDataModel.h, MapPgmUtil.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapCurveHandleManager.h"
#include "MapEditDataModel.h"
#include "utils/MapPgmUtil.h"
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsLineItem>
#include <QGraphicsPolygonItem>
#include <QPen>
#include <QBrush>
#include <cmath>

MapCurveHandleManager::MapCurveHandleManager(QGraphicsScene *scene,
                                             MapEditDataModel *model,
                                             CoordToSceneFn worldToScene,
                                             CoordToWorldFn sceneToWorld,
                                             FeatureItemLookupFn getFeatureItem,
                                             QObject *parent)
    : QObject(parent)
    , m_scene(scene)
    , m_model(model)
    , m_worldToScene(std::move(worldToScene))
    , m_sceneToWorld(std::move(sceneToWorld))
    , m_getFeatureItem(std::move(getFeatureItem))
{
}

MapCurveHandleManager::~MapCurveHandleManager()
{
    removeAllHandles();
}

void MapCurveHandleManager::removeHandlesForCurve(const QString &id)
{
    if (!m_curveHandles.contains(id)) return;
    for (auto *h : m_curveHandles[id]) {
        if (!h) continue;
        if (m_scene) m_scene->removeItem(h);
        delete h;
    }
    m_curveHandles.remove(id);
}

void MapCurveHandleManager::removeAllHandles()
{
    for (auto *line : m_armLines) {
        if (line && m_scene) m_scene->removeItem(line);
        delete line;
    }
    m_armLines.clear();

    for (const auto &lst : m_curveHandles.values()) {
        for (auto *h : lst) {
            if (h && m_scene) m_scene->removeItem(h);
            delete h;
        }
    }
    m_curveHandles.clear();
}

void MapCurveHandleManager::setHandlesVisible(bool visible)
{
    for (const auto &lst : m_curveHandles.values()) {
        for (auto *h : lst) {
            if (h) h->setVisible(visible);
        }
    }
    for (auto *line : m_armLines) {
        if (line) line->setVisible(visible);
    }
}

void MapCurveHandleManager::rebuildCurveHandles(const QString &id, bool isDragStatus)
{
    if (!m_scene || !m_model) return;
    removeHandlesForCurve(id);

    QJsonObject cv = m_model->findFeature("curve", id);
    if (cv.isEmpty()) return;

    QJsonArray ctrlArr = cv.value("controlPoints").toArray();
    QList<QPointF> scenePts;
    if (!ctrlArr.isEmpty()) {
        for (const auto &pv : ctrlArr) {
            scenePts.append(m_worldToScene(pv.toObject().value("x").toDouble(),
                                          pv.toObject().value("y").toDouble()));
        }
    } else {
        scenePts.append(m_worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble()));
        scenePts.append(m_worldToScene(cv.value("cx").toDouble(), cv.value("cy").toDouble()));
        scenePts.append(m_worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble()));
    }

    QString startId = cv.value("startid").toString();
    QString endId   = cv.value("endid").toString();
    QList<QGraphicsEllipseItem*> handles;

    for (int i = 0; i < scenePts.size(); ++i) {
        bool isEndpoint = (i == 0 || i == scenePts.size() - 1);
        bool hasNode    = isEndpoint && ((i == 0) ? !startId.isEmpty() : !endId.isEmpty());
        if (hasNode) {
            handles.append(nullptr);
            continue;
        }
        auto *h = m_scene->addEllipse(-4, -4, 8, 8,
                                       QPen(QColor("#2980B9"), 1.2),
                                       QBrush(isEndpoint ? QColor("#D4E6F1") : Qt::white));
        h->setPos(scenePts[i]);
        h->setZValue(50);
        h->setData(Qt::UserRole,   id);
        h->setData(Qt::UserRole+1, i);
        h->setVisible(isDragStatus);
        handles.append(h);
    }

    auto arm = QPen(QColor(170, 185, 215, 180), 1, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
    for (int i = 0; i + 1 < scenePts.size(); ++i) {
        auto *line = m_scene->addLine(scenePts[i].x(), scenePts[i].y(),
                                      scenePts[i+1].x(), scenePts[i+1].y(), arm);
        line->setZValue(49);
        line->setVisible(isDragStatus);
        m_armLines.append(line);
    }
    m_curveHandles[id] = handles;
}

void MapCurveHandleManager::updateCurveShape(const QString &id)
{
    if (!m_scene || !m_model) return;
    auto it = m_curveHandles.find(id);
    if (it == m_curveHandles.end() || it.value().size() < 2) return;

    QJsonObject cv = m_model->findFeature("curve", id);
    if (cv.isEmpty()) return;

    QPointF startScene = m_worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble());
    QPointF endScene   = m_worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble());

    QList<QPointF> pts;
    const auto &lst = it.value();
    for (int i = 0; i < lst.size(); ++i) {
        if (lst[i]) pts.append(lst[i]->pos());
        else if (i == 0)            pts.append(startScene);
        else if (i == lst.size()-1) pts.append(endScene);
        else                        pts.append(QPointF());
    }

    QPainterPath path = MapPgmUtil::getBezierPath(pts);
    auto *pi = qgraphicsitem_cast<QGraphicsPathItem*>(m_getFeatureItem("curve_" + id));
    if (!pi) return;

    pi->setPath(path);
    for (auto *child : pi->childItems()) delete child;

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

void MapCurveHandleManager::flushCurveHandlesToJson()
{
    if (!m_model) return;
    QJsonArray curves = m_model->curves();

    for (auto it = m_curveHandles.begin(); it != m_curveHandles.end(); ++it) {
        auto &lst = it.value();
        if (lst.size() < 2) continue;
        const QString &id = it.key();

        for (int i = 0; i < curves.size(); ++i) {
            QJsonObject cv = curves[i].toObject();
            if (cv.value("id").toString() != id) continue;

            auto handlePos = [&](int idx) -> QPointF {
                if (idx >= 0 && idx < lst.size() && lst[idx]) return lst[idx]->pos();
                if (idx == 0)
                    return m_worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble());
                return m_worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble());
            };

            QJsonArray ctrlArr = cv.value("controlPoints").toArray();
            if (ctrlArr.isEmpty() && lst.size() == 3) {
                QList<QPointF> wPts;
                for (int k = 0; k < lst.size(); ++k)
                    wPts.append(m_sceneToWorld(handlePos(k).x(), handlePos(k).y()));
                cv["x1"] = wPts[0].x(); cv["y1"] = wPts[0].y();
                cv["cx"] = wPts[1].x(); cv["cy"] = wPts[1].y();
                cv["x2"] = wPts[2].x(); cv["y2"] = wPts[2].y();
            } else {
                for (int j = 0; j < lst.size() && j < ctrlArr.size(); ++j) {
                    QPointF wp = m_sceneToWorld(handlePos(j).x(), handlePos(j).y());
                    QJsonObject pt = ctrlArr[j].toObject();
                    pt["x"] = wp.x(); pt["y"] = wp.y();
                    ctrlArr[j] = pt;
                }
                cv["controlPoints"] = ctrlArr;
                if (!ctrlArr.isEmpty()) {
                    QPointF wFirst = m_sceneToWorld(handlePos(0).x(), handlePos(0).y());
                    QPointF wLast  = m_sceneToWorld(handlePos(lst.size()-1).x(), handlePos(lst.size()-1).y());
                    cv["x1"] = wFirst.x(); cv["y1"] = wFirst.y();
                    cv["x2"] = wLast.x();  cv["y2"] = wLast.y();
                }
            }
            curves[i] = cv;
            break;
        }
    }
    m_model->setCurves(curves);
}
