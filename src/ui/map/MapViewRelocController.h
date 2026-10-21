/**
 * @file    MapViewRelocController.h
 * @layer   GUI
 * @brief   Controller for robot 2D pose relocalization interaction and preview items.
 * @deps    QObject, QPointF, QGraphicsItem
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QPointF>
#include <functional>

class QGraphicsScene;
class QGraphicsItem;
class QWidget;

class MapViewRelocController : public QObject
{
    Q_OBJECT
public:
    explicit MapViewRelocController(QObject *parent = nullptr);
    ~MapViewRelocController() override;

    bool isActive() const { return m_relocMode; }
    bool isDragging() const { return m_relocDragging; }

    void enterRelocMode();
    void exitRelocMode(QGraphicsScene *scene = nullptr);

    bool handlePress(const QPointF &worldPos, const QPointF &scenePos, QGraphicsScene *scene,
                     const QString &carId, std::function<void(const QString &, const QString &)> setStatus);
    void handleMove(const QPointF &scenePos, QGraphicsScene *scene,
                    std::function<QPointF(double, double)> worldToScene);
    void handleRelease(const QPointF &worldPos, const QPointF &scenePos, QGraphicsScene *scene,
                       const QString &carId, const QString &mapId, double fallbackYaw,
                       std::function<void(const QString &, const QString &)> setStatus,
                       std::function<void()> onFinished, QWidget *parent);

    void clearPreview(QGraphicsScene *scene);

signals:
    void relocModeChanged(bool active);

private:
    bool           m_relocMode      = false;
    bool           m_relocDragging  = false;
    QPointF        m_relocStartWorld;
    QGraphicsItem *m_poseArrowItem  = nullptr;
    QGraphicsItem *m_poseDotItem    = nullptr;
};
