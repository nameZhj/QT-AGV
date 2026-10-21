/**
 * @file    MapEditTypes.h
 * @layer   GUI
 * @brief   Common enumerations, data structures, and mathematical constants for map editing.
 * @deps    QString, QPointF
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QPointF>
#include <cmath>

namespace MapEdit {

// === [Section: Enums] ===

enum DrawStatus {
    Drag = 0,
    Point = 1,
    Line = 2,
    Curve = 3,
    Brush = 4,
    Eraser = 5,
    Charge = 6,
    HighPrecision = 7,
    Area = 8
};

enum AreaHitPart {
    AreaHit_None = 0,
    AreaHit_TopLeft,
    AreaHit_TopRight,
    AreaHit_BottomLeft,
    AreaHit_BottomRight,
    AreaHit_TopEdge,
    AreaHit_BottomEdge,
    AreaHit_LeftEdge,
    AreaHit_RightEdge,
    AreaHit_Body
};

// === [Section: Data Structures] ===

struct SnapResult {
    bool    valid = false;
    QString type;
    QString id;
    QPointF scenePos;
    QPointF worldPos;
};

// === [Section: Domain & Math Constants] ===

inline constexpr double kPi                    = 3.14159265358979323846;
inline constexpr double kDegToRad              = kPi / 180.0;
inline constexpr double kRadToDeg              = 180.0 / kPi;
inline constexpr int    kDirectionBidirectional= 10;
inline constexpr int    kDirectionForward      = 0;
inline constexpr int    kDirectionBackward     = 1;
inline constexpr int    kActionTypeObstacle    = 21;

} // namespace MapEdit
