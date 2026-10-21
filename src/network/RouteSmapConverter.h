/**
 * @file    RouteSmapConverter.h
 * @layer   DataComm
 * @brief   Industrial .smap map format import and export converter.
 * @deps    RouteNetworkApi.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include "RouteNetworkApi.h"

class RouteSmapConverter final
{
public:
    RouteSmapConverter() = delete;
    ~RouteSmapConverter() = delete;

    static bool parseSmap(const QJsonObject &smapObj, RouteNetwork &outNetwork);
    static QJsonObject exportSmap(const RouteNetwork &network);
};
