#include "MapPaintCanvas.h"
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>

MapPaintCanvas::MapPaintCanvas(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(400, 300);
    setMouseTracking(true);
}

double MapPaintCanvas::fitScale() const
{
    if (m_image.isNull() || width() <= 0 || height() <= 0) return 1.0;
    return qMin((double)width() / m_image.width(), (double)height() / m_image.height());
}

void MapPaintCanvas::setImage(const QImage &image)
{
    m_image = image.convertToFormat(QImage::Format_Grayscale8);
    m_originalImage = m_image;   // 保存原始地图数据，橡皮擦时恢复
    m_undoStack.clear();
    m_redoStack.clear();
    m_modified = false;
    m_panX = 0;
    m_panY = 0;
    m_scaleInited = false;
    if (width() > 0 && height() > 0 && !m_image.isNull()) {
        m_scale = fitScale();
        m_scaleInited = true;
    }
    emit historyChanged();
    update();
}

void MapPaintCanvas::pushUndo()
{
    m_undoStack.append(m_image);
    if (m_undoStack.size() > kMaxStack) m_undoStack.removeFirst();
    m_redoStack.clear();
    m_modified = true;
    emit historyChanged();
}

void MapPaintCanvas::undo()
{
    if (m_undoStack.isEmpty()) return;
    m_redoStack.append(m_image);
    if (m_redoStack.size() > kMaxStack) m_redoStack.removeFirst();
    m_image = m_undoStack.takeLast();
    emit historyChanged();
    update();
}

void MapPaintCanvas::redo()
{
    if (m_redoStack.isEmpty()) return;
    m_undoStack.append(m_image);
    if (m_undoStack.size() > kMaxStack) m_undoStack.removeFirst();
    m_image = m_redoStack.takeLast();
    emit historyChanged();
    update();
}

void MapPaintCanvas::resetView()
{
    if (m_image.isNull()) return;
    m_scale = fitScale();
    m_panX = 0;
    m_panY = 0;
    update();
}

void MapPaintCanvas::zoomIn() { applyZoom(1.1, QPoint(width() / 2, height() / 2)); }
void MapPaintCanvas::zoomOut() { applyZoom(0.9, QPoint(width() / 2, height() / 2)); }

void MapPaintCanvas::applyZoom(double factor, const QPoint &center)
{
    if (m_image.isNull()) return;
    double minScale = fitScale() * 0.2;
    double newScale = qBound(minScale, m_scale * factor, kMaxScale);
    if (qFuzzyCompare(newScale, m_scale)) return;

    // 缩放以 center 为锚点，保持其在图像上对应的位置不变
    double imgX = (center.x() - width() / 2.0 - m_panX) / m_scale;
    double imgY = (center.y() - height() / 2.0 - m_panY) / m_scale;
    m_scale = newScale;
    m_panX = center.x() - width() / 2.0 - imgX * m_scale;
    m_panY = center.y() - height() / 2.0 - imgY * m_scale;
    update();
}

QPointF MapPaintCanvas::widgetToImage(const QPoint &pos) const
{
    double x = (pos.x() - width() / 2.0 - m_panX) / m_scale + m_image.width() / 2.0;
    double y = (pos.y() - height() / 2.0 - m_panY) / m_scale + m_image.height() / 2.0;
    return QPointF(x, y);
}

void MapPaintCanvas::paintStroke(const QPointF &from, const QPointF &to, bool isPoint)
{
    if (m_image.isNull()) return;
    if (m_tool == Tool::Eraser && !m_originalImage.isNull()) {
        // 橡皮擦：从原始地图恢复像素数据，而不是涂白
        QPainter painter(&m_image);
        painter.setClipRegion(QRegion(QRect(0, 0, m_image.width(), m_image.height())));
        QPen pen(Qt::NoPen);
        painter.setPen(pen);
        double r = m_brushSize / 2.0;
        if (isPoint) {
            QRectF srcRect(to.x() - r, to.y() - r, m_brushSize, m_brushSize);
            QRectF dstRect = srcRect.intersected(QRectF(0, 0, m_image.width(), m_image.height()));
            if (!dstRect.isEmpty() && m_originalImage.rect().contains(dstRect.toRect())) {
                painter.drawImage(dstRect, m_originalImage, dstRect);
            }
        } else {
            // 沿线段恢复原始像素：用 clip path 限制绘制区域
            QPainterPath clipPath;
            clipPath.moveTo(from);
            QPainterPathStroker stroker;
            stroker.setWidth(m_brushSize);
            stroker.setCapStyle(Qt::RoundCap);
            stroker.setJoinStyle(Qt::RoundJoin);
            QPainterPath linePath;
            linePath.moveTo(from);
            linePath.lineTo(to);
            clipPath = stroker.createStroke(linePath);
            painter.setClipPath(clipPath);
            painter.drawImage(QPointF(0, 0), m_originalImage);
        }
        return;
    }
    // 画笔：涂黑（标记障碍/墙体）
    QPainter painter(&m_image);
    QColor color = (m_tool == Tool::Brush) ? QColor(0, 0, 0) : QColor(255, 255, 255);
    QPen pen(color, m_brushSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(color);
    if (isPoint) {
        double r = m_brushSize / 2.0;
        painter.drawEllipse(to, r, r);
    } else {
        painter.drawLine(from, to);
    }
}

void MapPaintCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0));

    if (!m_image.isNull()) {
        painter.save();
        painter.translate(width() / 2.0 + m_panX, height() / 2.0 + m_panY);
        painter.scale(m_scale, m_scale);
        painter.drawImage(QPointF(-m_image.width() / 2.0, -m_image.height() / 2.0), m_image);
        painter.restore();
    }

    if (m_cursorInside && !m_image.isNull() && m_tool != Tool::Move) {
        double r = m_brushSize * m_scale / 2.0;
        painter.setPen(QPen(QColor(24, 144, 255), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(m_cursorPos), r, r);
    }
}

void MapPaintCanvas::wheelEvent(QWheelEvent *event)
{
    applyZoom(event->angleDelta().y() > 0 ? 1.1 : 0.9, event->position().toPoint());
}

void MapPaintCanvas::mousePressEvent(QMouseEvent *event)
{
    // "移动"工具或右键：平移画布，不绘制——用来取消画笔/橡皮，安全地浏览地图
    if (event->button() == Qt::RightButton ||
        (event->button() == Qt::LeftButton && m_tool == Tool::Move)) {
        m_panning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (event->button() == Qt::LeftButton && !m_image.isNull()) {
        pushUndo();
        m_drawing = true;
        QPointF imgPos = widgetToImage(event->pos());
        paintStroke(imgPos, imgPos, true);
        m_lastImagePos = imgPos;
        update();
    }
}

void MapPaintCanvas::mouseMoveEvent(QMouseEvent *event)
{
    m_cursorPos = event->pos();
    m_cursorInside = true;

    if (m_drawing && !m_image.isNull()) {
        QPointF imgPos = widgetToImage(event->pos());
        paintStroke(m_lastImagePos, imgPos, false);
        m_lastImagePos = imgPos;
    } else if (m_panning) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_panX += delta.x();
        m_panY += delta.y();
        m_lastMousePos = event->pos();
    }
    update();
}

void MapPaintCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) m_drawing = false;
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        if (m_panning) {
            m_panning = false;
            setCursor(m_tool == Tool::Move ? Qt::OpenHandCursor : Qt::ArrowCursor);
        }
    }
}

void MapPaintCanvas::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_cursorInside = false;
    update();
}

void MapPaintCanvas::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_scaleInited && !m_image.isNull() && width() > 0 && height() > 0) {
        m_scale = fitScale();
        m_scaleInited = true;
        update();
    }
}
