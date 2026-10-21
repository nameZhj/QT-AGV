/**
 * @file    TaskActionParamDialog.h
 * @layer   GUI
 * @brief   Dialog for configuring action parameters in task chain
 * @deps    TaskChainActionDefs.h, QDialog
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QStringList>
#include <QList>
#include <QMap>
#include "TaskChainActionDefs.h"

struct TaskChainStationItem {
    QString id;
    QString name;
};

class TaskActionParamDialog : public QDialog {
    Q_OBJECT
public:
    explicit TaskActionParamDialog(const ActionDef &def,
                                  const QJsonObject &existingParams,
                                  const QList<TaskChainStationItem> &stations,
                                  QStringList *cachedAudioFiles = nullptr,
                                  QWidget *parent = nullptr);

    static QString editActionParams(const ActionDef &def,
                                    const QJsonObject &existingParams,
                                    const QList<TaskChainStationItem> &stations,
                                    QStringList *cachedAudioFiles = nullptr,
                                    QWidget *parent = nullptr);

    static QString audioDisplayName(const QString &fullName) {
        return TaskChainActionDefs::audioDisplayName(fullName);
    }
    static QStringList fetchAudioFiles(int timeoutMs = 2500) {
        return TaskChainActionDefs::fetchAudioFiles(timeoutMs);
    }

    QJsonObject getResultParams() const { return m_resultParams; }

private:
    void setupUI(const ActionDef &def, const QJsonObject &existingParams);
    void collectParams(const ActionDef &def);
    QWidget *createParamInput(const ActionDef::ParamDef &param, const QJsonObject &existingParams);

    const QList<TaskChainStationItem> &m_stations;
    QStringList *m_cachedAudioFiles = nullptr;
    QMap<QString, QWidget*> m_inputWidgets;
    QJsonObject m_resultParams;
};
