/**
 * @file    DirectionPadGeometry.h
 * @layer   GUILayer
 * @brief   Vector geometry and path builder for virtual D-pad widget.
 * @deps    QPainterPath
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QPainterPath>

namespace DirectionPadGeometry {

void buildPaths(QPainterPath &up,
                QPainterPath &down,
                QPainterPath &left,
                QPainterPath &right,
                QPainterPath &center,
                QPainterPath &settings);

} // namespace DirectionPadGeometry
