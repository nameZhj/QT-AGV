/**
 * @file    MapEditDataModel.h
 * @layer   GUI
 * @brief   In-memory data model storing points, lines, curves, areas, and reflectors for map editing.
 * @deps    QObject, QJsonObject, QJsonArray
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QPointF>

class MapEditDataModel : public QObject
{
    Q_OBJECT

public:
    explicit MapEditDataModel(QObject *parent = nullptr);
    ~MapEditDataModel() override = default;

    // === [Section: Core Data Accessors] ===
    const QJsonArray& points() const { return m_points; }
    const QJsonArray& lines() const { return m_lines; }
    const QJsonArray& curves() const { return m_curves; }
    const QJsonArray& charges() const { return m_charges; }
    const QJsonArray& highPrecisionPoints() const { return m_highPrecisionPoints; }
    const QJsonArray& highLevelAreas() const { return m_highLevelAreas; }
    const QJsonArray& reflectorPoles() const { return m_reflectorPoles; }

    void setPoints(const QJsonArray &arr);
    void setLines(const QJsonArray &arr);
    void setCurves(const QJsonArray &arr);
    void setCharges(const QJsonArray &arr);
    void setHighPrecisionPoints(const QJsonArray &arr);
    void setHighLevelAreas(const QJsonArray &arr);
    void setReflectorPoles(const QJsonArray &arr);

    // === [Section: Model Status & Dirty Flag] ===
    bool isDirty() const { return m_dirty; }
    void setDirty(bool dirty);
    bool isEmpty() const;
    void clearAllData();

    // === [Section: Feature CRUD Operations] ===
    int findFeatureIndex(const QString &type, const QString &id) const;
    QJsonObject findFeature(const QString &type, const QString &id) const;

    void addFeature(const QString &type, const QJsonObject &obj);
    void updateFeature(const QString &type, const QString &id, const QJsonObject &obj);
    bool removeFeature(const QString &type, const QString &id);

    // === [Section: Topology Helpers] ===
    bool hasRouteBetween(const QString &startId, const QString &endId,
                         const QPointF &sceneStart = QPointF(),
                         const QPointF &sceneEnd = QPointF()) const;
    void updateConnectedRoutesPos(const QString &nodeId, double newX, double newY,
                                  bool updateX = true, bool updateY = true);
    void syncLineObstaclesToEndpoints();

signals:
    void dataChanged();
    void dirtyChanged(bool dirty);
    void featureAdded(const QString &type, const QString &id);
    void featureUpdated(const QString &type, const QString &id);
    void featureRemoved(const QString &type, const QString &id);

private:
    QJsonArray* getArrayForType(const QString &type);
    const QJsonArray* getArrayForType(const QString &type) const;

    QJsonArray m_points;
    QJsonArray m_lines;
    QJsonArray m_curves;
    QJsonArray m_charges;
    QJsonArray m_highPrecisionPoints;
    QJsonArray m_highLevelAreas;
    QJsonArray m_reflectorPoles;

    bool m_dirty = false;
};
