/**
 * @file    MapEditToolBarHelper.h
 * @layer   GUI
 * @brief   Helper constructing top navigation, sub-toolbars, and left drawing mode toolbars.
 * @deps    QWidget, QPushButton, QComboBox, QLabel, QMap
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QMap>

class QVBoxLayout;
class QHBoxLayout;
class ToggleSwitch;

namespace MapEditToolBarHelper {

struct TopBarWidgets {
    QWidget      *container = nullptr;
    QPushButton  *backBtn = nullptr;
    QLabel       *mapInfoLabel = nullptr;
    QPushButton  *undoBtn = nullptr;
    QPushButton  *redoBtn = nullptr;
    QComboBox    *brushSizeCombo = nullptr;
    QPushButton  *resetViewBtn = nullptr;
    QPushButton  *gridBtn = nullptr;
    ToggleSwitch *robotBtn = nullptr;
    ToggleSwitch *reflectorBtn = nullptr;
    QPushButton  *editMapBtn = nullptr;
    QPushButton  *importSmapBtn = nullptr;
    QPushButton  *saveBtn = nullptr;
    QPushButton  *teleopBtn = nullptr;
    QPushButton  *helpBtn = nullptr;
};

struct SubToolBarWidgets {
    QWidget      *stationBar = nullptr;
    QMap<int, QPushButton*> stationBtns;
    QWidget      *areaBar = nullptr;
    QPushButton  *areaAdvBtn = nullptr;
};

struct LeftToolBarWidgets {
    QWidget      *container = nullptr;
    QMap<int, QPushButton*> toolBtns;
    QPushButton  *tableModeBtn = nullptr;
};

TopBarWidgets setupTopBar(QWidget *parent, QVBoxLayout *root);
SubToolBarWidgets setupSubToolBars(QWidget *parent, QVBoxLayout *root);
LeftToolBarWidgets setupLeftToolbar(QWidget *parent, QHBoxLayout *contentLayout);

} // namespace MapEditToolBarHelper
