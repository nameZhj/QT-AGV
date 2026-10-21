/**
 * @file    RulerWidget.h
 * @layer   GUI
 * @brief   Coordinate scale ruler widget for QGraphicsView map viewports.
 * @deps    QWidget, QGraphicsView
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>

class QGraphicsView;

class RulerWidget : public QWidget
{
    Q_OBJECT
public:
    enum Orientation { Horizontal, Vertical };
    explicit RulerWidget(Orientation ori, QWidget *parent = nullptr);

    // worldL/B/R/T = world extent (metres); imgW/H = thumbnail pixel size
    void syncWithView(QGraphicsView *view,
                      double worldL, double worldB, double worldR, double worldT,
                      double imgW,   double imgH);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Orientation    m_ori;
    QGraphicsView *m_view  = nullptr;
    double m_worldL = 0, m_worldB = 0, m_worldR = 0, m_worldT = 0;
    double m_imgW   = 0, m_imgH   = 0;
};
