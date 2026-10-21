/**
 * @file    MapViewToolbarHelper.h
 * @layer   GUI
 * @brief   Factory and state management for MapView top toolbar buttons and action menus.
 * @deps    QWidget, QToolButton, QMenu
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QToolButton>
#include <QMenu>

struct MapViewToolbarButtons {
    QToolButton *editMapBtn     = nullptr;
    QToolButton *uploadSmapBtn  = nullptr;
    QToolButton *createMapBtn   = nullptr;
    QToolButton *relocBtn       = nullptr;
    QToolButton *navModeBtn     = nullptr;
    QToolButton *moveBtn        = nullptr;
    QToolButton *cancelNavBtn   = nullptr;
    QToolButton *pauseNavBtn    = nullptr;
    QToolButton *uploadMapBtn   = nullptr;
    QToolButton *downloadMapBtn = nullptr;
    QToolButton *switchMapBtn   = nullptr;
    QToolButton *teleopBtn      = nullptr;
    QToolButton *configBtn      = nullptr;
    QToolButton *statusBtn      = nullptr;
    QToolButton *moreBtn        = nullptr;
    QMenu       *moreMenu       = nullptr;
    QToolButton *resetViewBtn   = nullptr;
    QToolButton *refreshBtn     = nullptr;
};

class MapViewToolbarHelper
{
public:
    static const QString kToolBtnStyle;
    static const QString kToolBtnPrimary;
    static const QString kToolBtnDanger;

    static MapViewToolbarButtons createToolbar(QWidget *parent, QObject *receiver);
    static void updateNavButtonsState(MapViewToolbarButtons &btns,
                                      bool isNav, bool isPaused,
                                      bool isUserPreempted, bool navModeActive);
};
