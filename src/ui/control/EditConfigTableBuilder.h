/**
 * @file    EditConfigTableBuilder.h
 * @layer   GUILayer
 * @brief   Table widget rows population and filtering for EditConfigWidget.
 * @deps    EditConfigWidget.h, QJsonObject, QString
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QJsonObject>
#include <QString>

class EditConfigWidget;

class EditConfigTableBuilder
{
public:
    static void addParamRows(EditConfigWidget *w, const QJsonObject &params,
                             int configIdx, const QString &prefix, const QString &filter);
    static void refreshParams(EditConfigWidget *w, const QString &groupKey);
    static void refreshParamsForConfig(EditConfigWidget *w, int configIdx);
    static void updateDefaultValuesInTable(EditConfigWidget *w);
};
