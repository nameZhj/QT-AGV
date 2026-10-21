/**
 * @file    MapRasterPaintController.cpp
 * @layer   GUI
 * @brief   Implementation of raster map painting and undo/redo history.
 * @deps    MapRasterPaintController.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapRasterPaintController.h"
#include <QPainter>
#include <QGraphicsPixmapItem>
#include <QPixmap>

MapRasterPaintController::MapRasterPaintController(QObject *parent)
    : QObject(parent)
{
}

void MapRasterPaintController::pushUndo(const QImage &img)
{
    if (img.isNull()) return;
    m_undoStack.append(img);
    if (m_undoStack.size() > kMaxHistory) m_undoStack.removeFirst();
    m_redoStack.clear();
    emit historyChanged();
}

QImage MapRasterPaintController::undo(const QImage &currentImg)
{
    if (m_undoStack.isEmpty()) return currentImg;
    m_redoStack.append(currentImg);
    if (m_redoStack.size() > kMaxHistory) m_redoStack.removeFirst();
    QImage prev = m_undoStack.takeLast();
    emit historyChanged();
    return prev;
}

QImage MapRasterPaintController::redo(const QImage &currentImg)
{
    if (m_redoStack.isEmpty()) return currentImg;
    m_undoStack.append(currentImg);
    if (m_undoStack.size() > kMaxHistory) m_undoStack.removeFirst();
    QImage next = m_redoStack.takeLast();
    emit historyChanged();
    return next;
}

void MapRasterPaintController::clearHistory()
{
    m_undoStack.clear();
    m_redoStack.clear();
    emit historyChanged();
}

void MapRasterPaintController::paintStroke(QImage &mapImage,
                                           QGraphicsPixmapItem *bgItem,
                                           const QPointF &from,
                                           const QPointF &to,
                                           int brushSize,
                                           bool isBrush,
                                           bool isPoint)
{
    if (mapImage.isNull()) return;

    if (mapImage.format() != QImage::Format_RGB32 &&
        mapImage.format() != QImage::Format_ARGB32_Premultiplied) {
        mapImage = mapImage.convertToFormat(QImage::Format_RGB32);
    }

    QPainter painter(&mapImage);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QColor color = isBrush ? QColor(0, 0, 0) : QColor(255, 255, 255);
    QPen pen(color, brushSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    if (isPoint) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(color));
        double r = brushSize / 2.0;
        painter.drawEllipse(to, r, r);
    } else {
        painter.setPen(pen);
        painter.setBrush(QBrush(color));
        painter.drawLine(from, to);
    }
    painter.end();

    if (bgItem) {
        bgItem->setPixmap(QPixmap::fromImage(mapImage));
        bgItem->setTransformationMode(Qt::SmoothTransformation);
    }
}
