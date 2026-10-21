/**
 * @file    TaskChainDataSync.cpp
 * @layer   GUILayer
 * @brief   Implementation of local and remote synchronization for task chains and stations.
 * @deps    TaskChainDataSync.h, TaskChainWidget.h, TaskChainJsonSerializer.h, ApiService.h, AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainDataSync.h"
#include "TaskChainWidget.h"
#include "TaskChainJsonSerializer.h"
#include "network/ApiService.h"
#include "app/AppConfig.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

QString TaskChainDataSync::stationName(const TaskChainWidget *w, const QString &idOrName)
{
    if (idOrName.isEmpty() || !w) return QString();
    for (const auto &st : w->m_stations) {
        if (st.id == idOrName || st.name == idOrName) return st.name;
    }
    return idOrName;
}

void TaskChainDataSync::loadChainsFromCacheAndServer(TaskChainWidget *w)
{
    if (!w || w->m_cacheLoaded) return;
    w->m_cacheLoaded = true;

    w->m_chains = TaskChainJsonSerializer::readLocalCache();
    const int localCount = w->m_chains.size();
    w->refreshChainList();
    if (!w->m_chains.isEmpty()) w->selectChain(0);

    // 2) 服务器列表: 解析并合并服务器任务链
    ApiService::instance().listTaskChain([w, localCount](int code, const QJsonObject &resp) {
        if (!w || code != 200) return;

        auto res = TaskChainJsonSerializer::mergeServerChains(resp, w->m_chains);
        if (res.added > 0 || res.updated > 0) {
            w->refreshChainList();
            if (w->m_currentChainIndex < 0 && !w->m_chains.isEmpty()) {
                w->selectChain(0);
            } else if (w->m_currentChainIndex >= 0 && w->m_currentChainIndex < w->m_chains.size()) {
                w->selectChain(w->m_currentChainIndex);
            }
        }
        w->m_cacheLoaded = true;
        w->updateStatus(QStringLiteral("已载入任务链: 本地 %1 条, 服务器获取 %2 条 (新增 %3, 同步 %4)")
            .arg(localCount).arg(res.added + res.updated).arg(res.added).arg(res.updated));
    });
}

void TaskChainDataSync::scheduleCacheWrite(TaskChainWidget *w)
{
    if (w && w->m_cacheTimer) {
        w->m_cacheTimer->start();
    }
}

void TaskChainDataSync::writeCache(TaskChainWidget *w)
{
    if (w) {
        TaskChainJsonSerializer::writeLocalCache(w->m_chains, w->m_stations);
    }
}

void TaskChainDataSync::loadMapStations(TaskChainWidget *w)
{
    if (!w) return;
    QString carId = AppConfig::instance().agvId();
    if (carId.isEmpty()) {
        carId = AppConfig::instance().currentDevice().carId;
    }

    auto doFetchRoute = [w](const QString &/*mapId*/) {
        ApiService::instance().loadEditedMap([w](int code, const QJsonObject &data) {
            if (!w || code != 200) return;
            w->m_stations.clear();

            QString jsonStr = data.value("json").toString();
            if (!jsonStr.isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
                if (doc.isObject()) {
                    QJsonArray features = doc.object().value("features").toArray();
                    for (const auto &fv : features) {
                        QJsonObject f = fv.toObject();
                        QJsonObject props = f.value("properties").toObject();
                        QJsonObject geom  = f.value("geometry").toObject();
                        if (geom.value("type").toString() == "Point") {
                            TaskChainStationItem item;
                            item.id   = props.value("id").toString();
                            item.name = props.value("name").toString();
                            if (item.name.isEmpty()) item.name = item.id;
                            if (!item.id.isEmpty()) {
                                w->m_stations.append(item);
                            }
                        }
                    }
                }
            }
            if (!w->m_listLocked && w->m_execStatus != TaskChainWidget::Running && !w->m_chainWorking) {
                w->refreshGroupList();
            }
        });
    };

    if (!carId.isEmpty()) {
        ApiService::instance().getCar(carId, [w, doFetchRoute](int code, const QJsonObject &data) {
            if (!w) return;
            QString mapId;
            if (code == 200) {
                mapId = data.value("mapId").toString();
            }
            doFetchRoute(mapId);
        });
    } else {
        doFetchRoute("");
    }
}
