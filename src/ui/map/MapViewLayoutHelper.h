/**
 * @file    MapViewLayoutHelper.h
 * @layer   GUI
 * @brief   Factory for building the MapView canvas area, rulers, overlays, and splitter hierarchy.
 * @deps    QWidget, MapCanvas.h, RulerWidget.h, DirectionPadWidget.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>

class MapCanvas;
class RulerWidget;
class DirectionPadWidget;
class QGraphicsScene;

struct MapViewCanvasArea {
    QWidget            *mapArea      = nullptr;
    MapCanvas          *canvas       = nullptr;
    RulerWidget        *topRuler     = nullptr;
    RulerWidget        *leftRuler    = nullptr;
    QLabel             *coordLabel   = nullptr;
    QPushButton        *gridBtn      = nullptr;
    DirectionPadWidget *directionPad = nullptr;
};

class MapViewLayoutHelper
{
public:
    static MapViewCanvasArea setupCanvasArea(QWidget *parent, QGraphicsScene *scene, QObject *receiver);
    static QSplitter* setupSplitter(QWidget *parent, QWidget *leftArea, QWidget *rightArea);
};
