/**
 * @file    MapWidgetDialogs.h
 * @layer   GUILayer
 * @brief   Dialog helpers for adding and renaming maps in MapWidget.
 * @deps    QWidget, QJsonObject
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QJsonObject>
#include <functional>

namespace MapWidgetDialogs {

void showAddDialog(QWidget *parent, std::function<void()> onSuccess);
void showRenameDialog(QWidget *parent, const QJsonObject &mapData, std::function<void()> onSuccess);

} // namespace MapWidgetDialogs
