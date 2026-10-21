/**
 * @file    CarDialogHelper.h
 * @layer   GUI
 * @brief   Dialog handlers for adding, editing, and linking AGV devices in CarWidget.
 * @deps    QJsonObject
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#pragma once

#include <QString>
#include <QJsonObject>

class CarWidget;

class CarDialogHelper
{
public:
    static void showAddDialog(CarWidget *owner);
    static void showEditDialog(CarWidget *owner, const QJsonObject &carData);
    static void showLinkDialog(CarWidget *owner, const QJsonObject &carData);
    static void checkCertification(CarWidget *owner, const QString &ckey);
};
