/**
 * @file    ConfigDefaultCompleter.h
 * @layer   GUILayer
 * @brief   Completion helpers for motion control, align, and obstacle default configs.
 * @deps    none
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QJsonArray>

class ConfigDefaultCompleter
{
public:
    static void ensureMotionControlConfig(QJsonArray &configs);
    static void ensureAlignConfig(QJsonArray &configs);
    static void ensureObstacleConfig(QJsonArray &configs);
};
