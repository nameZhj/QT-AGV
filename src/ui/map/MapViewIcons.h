/**
 * @file    MapViewIcons.h
 * @layer   GUI
 * @brief   Custom procedural painter icons for MapView toolbar buttons.
 * @deps    QIcon, QString
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QIcon>
#include <QString>

namespace MapViewIcons {

QIcon makeMapToolIcon(const QString &name);

} // namespace MapViewIcons
