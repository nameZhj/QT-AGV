/**
 * @file    MapEditLayoutHelper.h
 * @layer   GUI
 * @brief   Factory building map editor splitter, canvas area, rulers, and signal wiring.
 * @deps    QWidget, MapEditToolBarHelper.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include "MapEditToolBarHelper.h"

class MapCanvas;
class RulerWidget;
class DirectionPadWidget;
class QLabel;
class QGraphicsScene;
class MapPropsManager;
class MapFeatureTablePanel;
class QVBoxLayout;
class MapEditWidget;
class MapEditDataModel;
class MapDrawController;
class MapDragController;
class MapRasterPaintController;
class MapEditTelemetryController;

namespace MapEditLayoutHelper {

struct CentralUI {
    MapEditToolBarHelper::LeftToolBarWidgets leftWidgets;
    QGraphicsScene       *scene = nullptr;
    MapCanvas            *mapView = nullptr;
    RulerWidget          *topRuler = nullptr;
    RulerWidget          *leftRuler = nullptr;
    QLabel               *coordLabel = nullptr;
    DirectionPadWidget   *directionPad = nullptr;
    MapPropsManager      *propsMgr = nullptr;
    MapFeatureTablePanel *tablePanel = nullptr;
};

CentralUI buildCentralUI(QWidget *parent, QVBoxLayout *root);

void wireSignals(MapEditWidget *w,
                 const MapEditToolBarHelper::TopBarWidgets &top,
                 const MapEditToolBarHelper::SubToolBarWidgets &sub,
                 const CentralUI &ui,
                 MapEditDataModel *model,
                 MapDrawController *drawCtrl,
                 MapDragController *dragCtrl,
                 MapRasterPaintController *paintCtrl,
                 MapEditTelemetryController *telemetryCtrl);

} // namespace MapEditLayoutHelper
