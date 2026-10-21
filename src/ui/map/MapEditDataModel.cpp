/**
 * @file    MapEditDataModel.cpp
 * @layer   GUI
 * @brief   Implementation of in-memory topological map data model and feature repository.
 * @deps    MapEditDataModel.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapEditDataModel.h"
#include "core/RouteNetworkValidator.h"
#include <cmath>

MapEditDataModel::MapEditDataModel(QObject *parent)
    : QObject(parent)
{
}

void MapEditDataModel::setPoints(const QJsonArray &arr)
{
    m_points = arr;
    emit dataChanged();
}

void MapEditDataModel::setLines(const QJsonArray &arr)
{
    m_lines = arr;
    emit dataChanged();
}

void MapEditDataModel::setCurves(const QJsonArray &arr)
{
    m_curves = arr;
    emit dataChanged();
}

void MapEditDataModel::setCharges(const QJsonArray &arr)
{
    m_charges = arr;
    emit dataChanged();
}

void MapEditDataModel::setHighPrecisionPoints(const QJsonArray &arr)
{
    m_highPrecisionPoints = arr;
    emit dataChanged();
}

void MapEditDataModel::setHighLevelAreas(const QJsonArray &arr)
{
    m_highLevelAreas = arr;
    emit dataChanged();
}

void MapEditDataModel::setReflectorPoles(const QJsonArray &arr)
{
    m_reflectorPoles = arr;
    emit dataChanged();
}

void MapEditDataModel::setDirty(bool dirty)
{
    if (m_dirty == dirty) return;
    m_dirty = dirty;
    emit dirtyChanged(m_dirty);
}

bool MapEditDataModel::isEmpty() const
{
    return m_points.isEmpty() && m_lines.isEmpty() && m_curves.isEmpty()
        && m_charges.isEmpty() && m_highPrecisionPoints.isEmpty()
        && m_highLevelAreas.isEmpty() && m_reflectorPoles.isEmpty();
}

void MapEditDataModel::clearAllData()
{
    m_points = QJsonArray();
    m_lines = QJsonArray();
    m_curves = QJsonArray();
    m_charges = QJsonArray();
    m_highPrecisionPoints = QJsonArray();
    m_highLevelAreas = QJsonArray();
    m_reflectorPoles = QJsonArray();
    setDirty(true);
    emit dataChanged();
}

QJsonArray* MapEditDataModel::getArrayForType(const QString &type)
{
    if (type == "point") return &m_points;
    if (type == "line") return &m_lines;
    if (type == "curve") return &m_curves;
    if (type == "charge") return &m_charges;
    if (type == "highPrecision") return &m_highPrecisionPoints;
    if (type == "highLevelArea" || type == "area") return &m_highLevelAreas;
    if (type == "reflector") return &m_reflectorPoles;
    return nullptr;
}

const QJsonArray* MapEditDataModel::getArrayForType(const QString &type) const
{
    if (type == "point") return &m_points;
    if (type == "line") return &m_lines;
    if (type == "curve") return &m_curves;
    if (type == "charge") return &m_charges;
    if (type == "highPrecision") return &m_highPrecisionPoints;
    if (type == "highLevelArea" || type == "area") return &m_highLevelAreas;
    if (type == "reflector") return &m_reflectorPoles;
    return nullptr;
}

int MapEditDataModel::findFeatureIndex(const QString &type, const QString &id) const
{
    const QJsonArray *arr = getArrayForType(type);
    if (!arr) return -1;
    for (int i = 0; i < arr->size(); ++i) {
        if ((*arr)[i].toObject().value("id").toString() == id) return i;
    }
    return -1;
}

QJsonObject MapEditDataModel::findFeature(const QString &type, const QString &id) const
{
    const QJsonArray *arr = getArrayForType(type);
    if (!arr) return QJsonObject();
    for (const auto &v : *arr) {
        QJsonObject obj = v.toObject();
        if (obj.value("id").toString() == id) return obj;
    }
    return QJsonObject();
}

void MapEditDataModel::addFeature(const QString &type, const QJsonObject &obj)
{
    QJsonArray *arr = getArrayForType(type);
    if (!arr) return;
    arr->append(obj);
    setDirty(true);
    emit featureAdded(type, obj.value("id").toString());
    emit dataChanged();
}

void MapEditDataModel::updateFeature(const QString &type, const QString &id, const QJsonObject &obj)
{
    QJsonArray *arr = getArrayForType(type);
    if (!arr) return;
    int idx = findFeatureIndex(type, id);
    if (idx < 0) return;
    (*arr)[idx] = obj;
    setDirty(true);
    emit featureUpdated(type, id);
    emit dataChanged();
}

bool MapEditDataModel::removeFeature(const QString &type, const QString &id)
{
    QJsonArray *arr = getArrayForType(type);
    if (!arr) return false;
    int idx = findFeatureIndex(type, id);
    if (idx < 0) return false;
    arr->removeAt(idx);
    setDirty(true);
    emit featureRemoved(type, id);
    emit dataChanged();
    return true;
}

bool MapEditDataModel::hasRouteBetween(const QString &startId, const QString &endId,
                                       const QPointF &sceneStart, const QPointF &sceneEnd) const
{
    if (startId.isEmpty() || endId.isEmpty() || startId == endId) return false;

    auto matchEndpoints = [&](const QJsonObject &route) -> bool {
        QString s = route.value("startid").toString();
        if (s.isEmpty()) s = route.value("startId").toString();
        if (s.isEmpty()) s = route.value("sId").toString();

        QString e = route.value("endid").toString();
        if (e.isEmpty()) e = route.value("endId").toString();
        if (e.isEmpty()) e = route.value("eId").toString();

        return (s == startId && e == endId) || (s == endId && e == startId);
    };

    for (const auto &v : m_lines) {
        if (matchEndpoints(v.toObject())) return true;
    }
    for (const auto &v : m_curves) {
        if (matchEndpoints(v.toObject())) return true;
    }
    return false;
}

void MapEditDataModel::updateConnectedRoutesPos(const QString &nodeId, double newX, double newY,
                                               bool updateX, bool updateY)
{
    if (!updateX && !updateY) return;

    for (int li = 0; li < m_lines.size(); ++li) {
        QJsonObject ln = m_lines[li].toObject();
        bool changed = false;
        if (ln.value("startid").toString() == nodeId) {
            if (updateX) ln["x1"] = newX;
            if (updateY) ln["y1"] = newY;
            changed = true;
        }
        if (ln.value("endid").toString() == nodeId) {
            if (updateX) ln["x2"] = newX;
            if (updateY) ln["y2"] = newY;
            changed = true;
        }
        if (changed) m_lines[li] = ln;
    }

    for (int ci = 0; ci < m_curves.size(); ++ci) {
        QJsonObject cv = m_curves[ci].toObject();
        bool changed = false;
        QJsonArray ctrlArr = cv.value("controlPoints").toArray();
        if (cv.value("startid").toString() == nodeId) {
            if (updateX) cv["x1"] = newX;
            if (updateY) cv["y1"] = newY;
            if (!ctrlArr.isEmpty()) {
                QJsonObject cp0 = ctrlArr[0].toObject();
                if (updateX) cp0["x"] = newX;
                if (updateY) cp0["y"] = newY;
                ctrlArr[0] = cp0;
                cv["controlPoints"] = ctrlArr;
            }
            changed = true;
        }
        if (cv.value("endid").toString() == nodeId) {
            if (updateX) cv["x2"] = newX;
            if (updateY) cv["y2"] = newY;
            if (!ctrlArr.isEmpty()) {
                int lastIdx = ctrlArr.size() - 1;
                QJsonObject cpLast = ctrlArr[lastIdx].toObject();
                if (updateX) cpLast["x"] = newX;
                if (updateY) cpLast["y"] = newY;
                ctrlArr[lastIdx] = cpLast;
                cv["controlPoints"] = ctrlArr;
            }
            changed = true;
        }
        if (changed) m_curves[ci] = cv;
    }
    setDirty(true);
    emit dataChanged();
}

void MapEditDataModel::syncLineObstaclesToEndpoints()
{
    RouteNetworkValidator::syncLineObstaclesToEndpoints(m_points, m_lines, m_curves, m_charges, m_highPrecisionPoints);
}
