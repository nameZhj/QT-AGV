/**
 * @file    MapPaintCanvas.h
 * @module  GUILayer
 * @brief   Freehand drawing canvas for occupancy grid obstacle erasure and painting.
 * @deps    QWidget, QPainter, QImage
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QImage>
#include <QList>

// 栅格地图(PGM/PNG)画笔编辑画布：支持画笔/橡皮涂抹、撤销重做、滚轮缩放与右键拖拽平移
class MapPaintCanvas : public QWidget
{
    Q_OBJECT
public:
    enum class Tool { Brush, Eraser, Move };

    explicit MapPaintCanvas(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    const QImage &image() const { return m_image; }
    bool isModified() const { return m_modified; }

    void setTool(Tool tool) { m_tool = tool; setCursor(tool == Tool::Move ? Qt::OpenHandCursor : Qt::ArrowCursor); update(); }
    void setBrushSize(int size) { m_brushSize = size; update(); }
    int brushSize() const { return m_brushSize; }

    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }
    void undo();
    void redo();

    void zoomIn();
    void zoomOut();
    void resetView();

signals:
    void historyChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QPointF widgetToImage(const QPoint &pos) const;
    void applyZoom(double factor, const QPoint &center);
    void paintStroke(const QPointF &from, const QPointF &to, bool isPoint);
    void pushUndo();
    double fitScale() const;

    QImage m_image;
    QImage m_originalImage;   // 原始地图数据(用于橡皮擦恢复)
    double m_scale = 1.0;
    double m_panX = 0;
    double m_panY = 0;
    bool m_scaleInited = false;

    Tool m_tool = Tool::Brush;
    int m_brushSize = 2;

    bool m_drawing = false;
    bool m_panning = false;
    QPoint m_lastMousePos;
    QPointF m_lastImagePos;

    QPoint m_cursorPos;
    bool m_cursorInside = false;

    QList<QImage> m_undoStack;
    QList<QImage> m_redoStack;
    bool m_modified = false;

    static constexpr int kMaxStack = 30;
    static constexpr double kMaxScale = 8.0;
};
