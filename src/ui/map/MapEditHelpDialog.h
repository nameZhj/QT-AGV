/**
 * @file    MapEditHelpDialog.h
 * @layer   GUI
 * @brief   Help and keyboard shortcut cheat sheet dialog for map editor.
 * @deps    QWidget
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>

namespace MapEditHelpDialog {

void showHelp(QWidget *parent = nullptr);

} // namespace MapEditHelpDialog
