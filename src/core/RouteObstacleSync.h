/**
 * @file    RouteObstacleSync.h
 * @layer   CoreScheduling
 * @brief   Synchronizes line and curve obstacle parameters to route endpoints.
 * @deps    QJsonArray
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#pragma once

#include <QJsonArray>

namespace RouteObstacleSync {

void syncLineObstaclesToEndpoints(QJsonArray &points,
                                  QJsonArray &lines,
                                  QJsonArray &curves,
                                  QJsonArray &charges,
                                  QJsonArray &highPrecisionPoints);

} // namespace RouteObstacleSync
