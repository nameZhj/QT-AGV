/**
 * @file    MapViewLayoutHelper.cpp
 * @layer   GUI
 * @brief   Canvas layout, coordinate overlay, ruler positioning, and splitter styling implementation.
 * @deps    MapViewLayoutHelper.h, MapCanvas.h, RulerWidget.h, DirectionPadWidget.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewLayoutHelper.h"
#include "MapCanvas.h"
#include "RulerWidget.h"
#include "DirectionPadWidget.h"
#include <QGridLayout>

MapViewCanvasArea MapViewLayoutHelper::setupCanvasArea(QWidget *parent, QGraphicsScene *scene, QObject *receiver)
{
    MapViewCanvasArea area;
    area.mapArea = new QWidget(parent);
    auto *lay = new QGridLayout(area.mapArea);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto *corner = new QLabel(area.mapArea);
    corner->setFixedSize(44, 22);
    corner->setStyleSheet("background:#1a1d23;");
    lay->addWidget(corner, 0, 0);

    area.topRuler = new RulerWidget(RulerWidget::Horizontal, area.mapArea);
    lay->addWidget(area.topRuler, 0, 1);

    area.leftRuler = new RulerWidget(RulerWidget::Vertical, area.mapArea);
    lay->addWidget(area.leftRuler, 1, 0);

    area.canvas = new MapCanvas(scene, area.mapArea);
    area.canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    lay->addWidget(area.canvas, 1, 1);
    lay->setRowStretch(1, 1);
    lay->setColumnStretch(1, 1);

    area.coordLabel = new QLabel("X: --  Y: --", area.canvas);
    area.coordLabel->setStyleSheet("background:rgba(18,21,26,200); color:#7ab0f0; font-size:11px; padding:3px 8px; border-radius:3px; font-family:Consolas,monospace;");
    area.coordLabel->setMinimumWidth(240);
    area.coordLabel->move(8, 8);
    area.coordLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    area.gridBtn = new QPushButton("⊞ 网格", area.canvas);
    area.gridBtn->setFixedSize(72, 26);
    area.gridBtn->setCheckable(true);
    area.gridBtn->setStyleSheet("QPushButton{background:rgba(30,34,44,200);color:#7ab0f0;border:1px solid #3a4050;border-radius:4px;font-size:11px;} QPushButton:checked{background:rgba(30,107,212,200);color:#fff;}");

    area.directionPad = new DirectionPadWidget(area.canvas);
    QObject::connect(area.canvas, &MapCanvas::canvasResized, area.canvas, [area](const QSize &sz) {
        area.gridBtn->move(sz.width() - area.gridBtn->width() - 8, sz.height() - area.gridBtn->height() - 8);
        if (area.directionPad) area.directionPad->move(16, sz.height() - area.directionPad->height() - 16);
    });

    if (receiver) {
        QObject::connect(area.canvas, SIGNAL(transformChanged()), receiver, SLOT(onViewTransformChanged()));
        QObject::connect(area.canvas, SIGNAL(sceneClicked(QPointF,Qt::MouseButton)), receiver, SLOT(onSceneClicked(QPointF,Qt::MouseButton)));
        QObject::connect(area.canvas, SIGNAL(sceneDoubleClicked(QPointF,Qt::MouseButton)), receiver, SLOT(onSceneDoubleClicked(QPointF,Qt::MouseButton)));
        QObject::connect(area.canvas, SIGNAL(sceneMoved(QPointF)), receiver, SLOT(onSceneMoved(QPointF)));
        QObject::connect(area.canvas, SIGNAL(sceneReleased(QPointF,Qt::MouseButton)), receiver, SLOT(onSceneReleased(QPointF,Qt::MouseButton)));
    }

    return area;
}

QSplitter* MapViewLayoutHelper::setupSplitter(QWidget *parent, QWidget *leftArea, QWidget *rightArea)
{
    auto *splitter = new QSplitter(Qt::Horizontal, parent);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(5);
    splitter->setStyleSheet("QSplitter::handle{background:#232733;} QSplitter::handle:hover{background:#3b82f6;}");
    splitter->addWidget(leftArea);
    splitter->addWidget(rightArea);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({900, 320});
    return splitter;
}
