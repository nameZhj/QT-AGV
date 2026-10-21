/**
 * @file    MapRasterPaintController.h
 * @layer   GUI
 * @brief   Raster map painting, brush/eraser strokes, and undo/redo history management.
 * @deps    QObject, QImage, QPointF, QGraphicsPixmapItem
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QImage>
#include <QList>
#include <QPointF>

class QGraphicsPixmapItem;

class MapRasterPaintController : public QObject
{
    Q_OBJECT

public:
    explicit MapRasterPaintController(QObject *parent = nullptr);
    ~MapRasterPaintController() override = default;

    void pushUndo(const QImage &img);
    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }
    QImage undo(const QImage &currentImg);
    QImage redo(const QImage &currentImg);
    void clearHistory();

    static void paintStroke(QImage &mapImage,
                            QGraphicsPixmapItem *bgItem,
                            const QPointF &from,
                            const QPointF &to,
                            int brushSize,
                            bool isBrush,
                            bool isPoint = false);

signals:
    void historyChanged();

private:
    static constexpr int kMaxHistory = 30;
    QList<QImage> m_undoStack;
    QList<QImage> m_redoStack;
};
