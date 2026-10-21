/**
 * @file    EditConfigRouteSync.h
 * @layer   GUILayer
 * @brief   Synchronizes global obstacle avoidance and motion parameters to route network GeoJSON.
 * @deps    EditConfigWidget.h, functional
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <functional>

class EditConfigWidget;

class EditConfigRouteSync
{
public:
    static void syncObstacleParamsToRouteNetwork(EditConfigWidget *w,
                                                 double stopDx, double stopDy,
                                                 double decelDx, double decelDy,
                                                 std::function<void(bool ok)> onDone = nullptr);
};
