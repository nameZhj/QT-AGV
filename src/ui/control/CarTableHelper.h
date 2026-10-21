/**
 * @file    CarTableHelper.h
 * @layer   GUI
 * @brief   Table row populator, map name resolver, and row actions for CarWidget.
 * @deps    QJsonObject, QJsonArray
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>

class CarWidget;
class QTableWidget;

class CarTableHelper
{
public:
    static QString resolveMapNameById(const QJsonArray &mapOptions, int mapId);
    static void populateCarRow(CarWidget *owner, QTableWidget *table, int row,
                               const QJsonObject &car, const QJsonArray &mapOptions);
    static void handleDelete(CarWidget *owner);
    static void handleStatusChanged(CarWidget *owner, int row);
};
