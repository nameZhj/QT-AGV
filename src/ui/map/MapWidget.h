/**
 * @file    MapWidget.h
 * @module  GUILayer
 * @brief   Tabbed container hosting map monitoring and map editing view modes.
 * @deps    QWidget, MapViewWidget, MapEditWidget
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>

class QListWidget;
class QLabel;
class QPushButton;

// 对应 map/index.vue - 地图管理
// 包含地图卡片列表、新增/重命名/删除/下载/下发地图、导航、编辑路网
class MapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);

private slots:
    void onAdd();
    void onRefresh();

private:
    void setupUI();
    void getList();
    void showAddDialog();
    void showRenameDialog(const QJsonObject &mapData);
    void deleteMap(const QJsonObject &mapData);
    void downloadMap(const QJsonObject &mapData);
    void downloadSourceFiles(const QJsonObject &mapData);
    void navigateToMap(const QJsonObject &mapData);
    void editMapRoute(const QJsonObject &mapData);

signals:
    void navigateRequested(const QString &target, const QString &id);

private:
    // UI
    QListWidget *m_listWidget = nullptr;
    QPushButton *m_addBtn = nullptr;
    QPushButton *m_refreshBtn = nullptr;

    // 数据
    QJsonArray m_mapList;
    int m_pageNum = 1;
    int m_pageSize = 12;
    int m_total = 0;
};
