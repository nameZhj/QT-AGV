/**
 * @file    MapImageEditIcons.h
 * @layer   GUILayer
 * @brief   Vector icon generators for raster map image editing tools.
 * @deps    QIcon
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QIcon>

namespace MapImageEditIcons {

QIcon drawBrushToolIcon(bool eraser);
QIcon drawMoveToolIcon();

} // namespace MapImageEditIcons
