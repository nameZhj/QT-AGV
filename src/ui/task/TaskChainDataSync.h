/**
 * @file    TaskChainDataSync.h
 * @layer   GUILayer
 * @brief   Synchronizes task chain definitions and map stations with local storage and remote server.
 * @deps    TaskChainWidget.h, ApiService.h, TaskChainJsonSerializer.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>

class TaskChainWidget;

class TaskChainDataSync
{
public:
    static QString stationName(const TaskChainWidget *w, const QString &idOrName);
    static void loadChainsFromCacheAndServer(TaskChainWidget *w);
    static void scheduleCacheWrite(TaskChainWidget *w);
    static void writeCache(TaskChainWidget *w);
    static void loadMapStations(TaskChainWidget *w);
};
