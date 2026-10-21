/**
 * @file    RulerWidget.cpp
 * @layer   GUI
 * @brief   Coordinate scale ruler widget implementation.
 * @deps    RulerWidget.h, QPainter, QGraphicsView
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "RulerWidget.h"
#include <QPainter>
#include <QGraphicsView>
#include <cmath>

RulerWidget::RulerWidget(Orientation ori, QWidget *parent)
    : QWidget(parent), m_ori(ori)
{
    if (ori == Horizontal) setFixedHeight(22);
    else                   setFixedWidth(44);
    setStyleSheet("background:#22262e;");
}

void RulerWidget::syncWithView(QGraphicsView *view,
                               double worldL, double worldB, double worldR, double worldT,
                               double imgW,   double imgH)
{
    m_view  = view;
    m_worldL = worldL; m_worldB = worldB;
    m_worldR = worldR; m_worldT = worldT;
    m_imgW   = imgW;   m_imgH   = imgH;
    update();
}

void RulerWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x1a, 0x1d, 0x23));
    p.setPen(QColor(0x55, 0x66, 0x77));
    p.setFont(QFont("Consolas", 7));

    double mapW = m_worldR - m_worldL;
    double mapH = m_worldT - m_worldB;
    if (!m_view || mapW <= 0 || mapH <= 0 || m_imgW <= 0 || m_imgH <= 0) {
        p.setPen(QColor(0x44, 0x55, 0x66));
        p.drawText(rect(), Qt::AlignCenter, m_ori == Horizontal ? "X (m)" : "Y (m)");
        return;
    }

    // Per-axis metres-per-scene-pixel (handles non-square maps correctly)
    double resX = mapW / m_imgW;
    double resY = mapH / m_imgH;

    // Scale from view transform
    double scale = m_view->transform().m11();

    // Tick steps in world metres
    auto pickStep = [](double pxPerM) -> double {
        double rawStep = 80.0 / pxPerM;
        static const double steps[] = {0.01,0.02,0.05,0.1,0.2,0.5,1,2,5,10,20,50,100,200,500};
        double step = steps[0];
        for (double s : steps) { step = s; if (s >= rawStep) break; }
        return step;
    };

    auto fmtLabel = [](double v, double st) -> QString {
        if (st < 0.1)  return QString::number(v, 'f', 2);
        if (st < 1.0)  return QString::number(v, 'f', 1);
        return QString::number((int)std::round(v));
    };

    if (m_ori == Horizontal) {
        double pxPerM = scale / resX;
        double step   = pickStep(pxPerM);
        QRectF vis    = m_view->mapToScene(m_view->viewport()->rect()).boundingRect();
        double wLeft  = vis.left()  * resX + m_worldL;
        double wRight = vis.right() * resX + m_worldL;
        double first  = std::ceil(wLeft / step) * step;

        int H = height();
        for (double wx = first; wx <= wRight + 1e-9; wx += step) {
            double sceneX = (wx - m_worldL) / resX;
            QPointF vp    = m_view->mapFromScene(sceneX, 0);
            int sx        = (int)vp.x();
            bool major    = qFuzzyIsNull(std::fmod(wx / step + 0.5, 5.0) - 0.5);
            p.setPen(major ? QColor(0x88,0x99,0xaa) : QColor(0x44,0x55,0x66));
            p.drawLine(sx, major ? H-10 : H-5, sx, H);
            if (major) {
                p.setPen(QColor(0x99,0xaa,0xbb));
                p.drawText(sx-22, 0, 44, H-11, Qt::AlignCenter, fmtLabel(wx, step));
            }
        }
        p.setPen(QColor(0x44,0x55,0x66));
        p.drawLine(0, H-1, width(), H-1);
    } else {
        double pxPerM = scale / resY;
        double step   = pickStep(pxPerM);
        QRectF vis    = m_view->mapToScene(m_view->viewport()->rect()).boundingRect();
        // scene Y increases downward; world Y increases upward
        double wTop    = m_worldT - vis.top()    * resY;
        double wBottom = m_worldT - vis.bottom() * resY;
        double first   = std::ceil(wBottom / step) * step;

        int W = width();
        for (double wy = first; wy <= wTop + 1e-9; wy += step) {
            double sceneY = (m_worldT - wy) / resY;
            QPointF vp    = m_view->mapFromScene(0, sceneY);
            int sy        = (int)vp.y();
            bool major    = qFuzzyIsNull(std::fmod(wy / step + 0.5, 5.0) - 0.5);
            p.setPen(major ? QColor(0x88,0x99,0xaa) : QColor(0x44,0x55,0x66));
            p.drawLine(W, sy, major ? 5 : W-5, sy);
            if (major) {
                p.setPen(QColor(0x99,0xaa,0xbb));
                p.save();
                p.translate(W-14, sy);
                p.rotate(-90);
                p.drawText(-22, -6, 44, 12, Qt::AlignCenter, fmtLabel(wy, step));
                p.restore();
            }
        }
        p.setPen(QColor(0x44,0x55,0x66));
        p.drawLine(W-1, 0, W-1, height());
    }
}
