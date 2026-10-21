/**
 * @file    CreateMapApiHelper.h
 * @layer   GUI
 * @brief   SLAM mapping API response error checking helper.
 * @deps    QString, QJsonObject
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QJsonObject>

namespace CreateMapApiHelper {
    QString checkApiError(const QJsonObject &res);
}
