/**
 * @file    EditConfigParamEditor.h
 * @layer   GUILayer
 * @brief   Parameter modification validation, range clamping, and manager updates for EditConfigWidget.
 * @deps    EditConfigWidget.h, QJsonValue, QStringList
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QJsonValue>
#include <QStringList>

class EditConfigWidget;

class EditConfigParamEditor
{
public:
    static QJsonValue jsonGet(const QJsonValue &root, const QStringList &path);
    static QJsonValue jsonSet(const QJsonValue &root, const QStringList &path, const QJsonValue &newVal);
    static void onParamCellChanged(EditConfigWidget *w, int row, int col);
};
