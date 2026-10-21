/**
 * @file    MapEditDialogs.h
 * @layer   GUI
 * @brief   Dialog helpers for unsaved change prompts and map attribute edits.
 * @deps    QWidget, QString, functional
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QString>
#include <functional>

class QLabel;

namespace MapEditDialogs {

// Returns true if caller can leave immediately; false if staying or saving asynchronously
bool checkUnsavedChanges(QWidget *parent, bool dirty,
                         std::function<void()> onSave,
                         std::function<void()> onDiscard);

void showEditMapDialog(const QString &mapId, QString &mapName,
                       QLabel *mapInfoLabel, QWidget *parent);

} // namespace MapEditDialogs
