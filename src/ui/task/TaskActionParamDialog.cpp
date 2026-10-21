/**
 * @file    TaskActionParamDialog.cpp
 * @layer   GUI
 * @brief   Implementation of TaskActionParamDialog
 * @deps    TaskActionParamDialog.h, ApiService.h, MotionConfigManager.h, TaskChainManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskActionParamDialog.h"
#include "MotionConfigManager.h"
#include "TaskChainManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>
#include "TaskChainStyle.h"

static constexpr double kMaxLiftLinearSpeed = TaskChainManager::kMaxLiftLinearSpeed;
static constexpr double kMaxLiftHeight = TaskChainManager::kMaxLiftHeight;

TaskActionParamDialog::TaskActionParamDialog(const ActionDef &def,
                                             const QJsonObject &existingParams,
                                             const QList<TaskChainStationItem> &stations,
                                             QStringList *cachedAudioFiles,
                                             QWidget *parent)
    : QDialog(parent), m_stations(stations), m_cachedAudioFiles(cachedAudioFiles)
{
    setWindowTitle(QStringLiteral("设置: %1").arg(def.name));
    setMinimumWidth(360);
    setupUI(def, existingParams);
}

void TaskActionParamDialog::setupUI(const ActionDef &def, const QJsonObject &existingParams)
{
    auto *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(16, 16, 16, 16);
    vbox->setSpacing(10);

    for (const auto &param : def.params) {
        auto *label = new QLabel(param.label);
        label->setStyleSheet("color:#c0c8d0; font-size:12px;");
        vbox->addWidget(label);

        QWidget *input = createParamInput(param, existingParams);
        if (!input) continue;

        if (!param.unit.isEmpty() && (param.type == "double" || param.type == "int")) {
            auto *rowWidget = new QWidget(this);
            auto *rowLayout = new QHBoxLayout(rowWidget);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(8);
            rowLayout->addWidget(input, 1);
            auto *unitLbl = new QLabel(param.unit, rowWidget);
            unitLbl->setStyleSheet("color: #a6b0be; font-size: 12px;");
            rowLayout->addWidget(unitLbl);
            vbox->addWidget(rowWidget);
        } else {
            vbox->addWidget(input);
        }
        m_inputWidgets[param.key] = input;
    }

    if (def.params.isEmpty()) {
        auto *hint = new QLabel(QStringLiteral("此动作无参数"));
        hint->setStyleSheet("color:#777; font-size:12px;");
        hint->setAlignment(Qt::AlignCenter);
        vbox->addWidget(hint);
    }

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto *okBtn = new QPushButton(QStringLiteral("确定"));
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"));
    okBtn->setStyleSheet(S_BTN_STYLE);
    cancelBtn->setStyleSheet(S_BTN_STYLE);
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    vbox->addLayout(btnRow);

    connect(okBtn, &QPushButton::clicked, this, [this, &def]() {
        collectParams(def);
        accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

QWidget *TaskActionParamDialog::createParamInput(const ActionDef::ParamDef &param, const QJsonObject &existingParams)
{
    const QJsonValue existingVal = existingParams.value(param.key);
    if (param.type == "text") {
        auto *edit = new QLineEdit;
        edit->setText(existingVal.toString());
        edit->setStyleSheet(kInputStyle);
        if (!param.unit.isEmpty()) edit->setPlaceholderText(param.unit);
        return edit;
    }
    if (param.type == "double") {
        auto *spin = new QDoubleSpinBox;
        const auto &mc = MotionConfigManager::instance();
        double minV = -99999.0, maxV = 99999.0, step = 1.0;
        double defaultVal = existingVal.toDouble(param.defaultValue);
        if (param.key == "lift_height") {
            minV = 0.0; maxV = kMaxLiftHeight; step = 0.01;
            defaultVal = qBound(0.0, defaultVal, kMaxLiftHeight);
            spin->setToolTip(QStringLiteral("顶升高度上限: %1 m").arg(kMaxLiftHeight));
        } else if (param.key == "lift_linear") {
            minV = 0.0; maxV = kMaxLiftLinearSpeed; step = 0.05;
            defaultVal = qBound(0.0, defaultVal, kMaxLiftLinearSpeed);
            spin->setToolTip(QStringLiteral("顶升线速度上限: %1 m/s").arg(kMaxLiftLinearSpeed));
        } else if (param.unit == "°/s") {
            maxV = mc.paramMax(QStringLiteral("max_angular_speed"), 60.0);
            minV = -maxV; step = 2.0;
            if (existingVal.isUndefined()) defaultVal = mc.maxAngularSpeedDeg();
            defaultVal = mc.clampSignedAngularSpeedDeg(defaultVal);
            spin->setToolTip(QStringLiteral("角速度范围: -%1 ~ %1 °/s (可以为负值，正数逆时针，负数顺时针)").arg(maxV, 0, 'f', 1));
        } else if (param.unit == "m/s") {
            maxV = mc.paramMax(QStringLiteral("max_speed"), 1.0);
            minV = -maxV; step = 0.05;
            if (existingVal.isUndefined()) defaultVal = (param.key == "vx" ? mc.maxSpeed() : 0.0);
            defaultVal = mc.clampSignedLinearSpeed(defaultVal);
            spin->setToolTip(QStringLiteral("线速度范围: -%1 ~ %1 m/s (可以为负值，正数前进，负数后退)").arg(maxV, 0, 'f', 2));
        } else if (param.key == "distance") {
            minV = 0.0; maxV = 100.0; step = 0.1;
            defaultVal = std::max(0.0, defaultVal);
            spin->setToolTip(QStringLiteral("平动距离范围: 0 ~ 100 m (不能为负数)"));
        } else if (param.key == "angle" || param.key == "rotate_angle" || param.unit == "°") {
            minV = 0.0; maxV = 360.0; step = 5.0;
            defaultVal = std::max(0.0, defaultVal);
            spin->setToolTip(QStringLiteral("旋转角度范围: 0 ~ 360 ° (不能为负数)"));
        }
        spin->setRange(minV, maxV);
        spin->setSingleStep(step);
        spin->setValue(defaultVal);
        spin->setDecimals(3);
        spin->setStyleSheet(kInputStyle);
        return spin;
    }
    if (param.type == "int") {
        auto *spin = new QSpinBox;
        spin->setRange(-99999, 99999);
        spin->setValue(existingVal.toInt(param.defaultValue));
        spin->setStyleSheet(kInputStyle);
        return spin;
    }
    if (param.type == "combo") {
        auto *combo = new QComboBox;
        if (param.key == "loop") {
            combo->addItem("否", "no");
            combo->addItem("是", "yes");
            QString cur = existingVal.toString();
            if (cur == "是") cur = "yes";
            else if (cur == "否") cur = "no";
            int idx = combo->findData(cur);
            combo->setCurrentIndex(idx >= 0 ? idx : 0);
        } else {
            combo->addItems(param.options);
            int idx = combo->findText(existingVal.toString());
            if (idx >= 0) combo->setCurrentIndex(idx);
        }
        combo->setStyleSheet(kInputStyle);
        return combo;
    }
    if (param.type == "audio_combo") {
        auto *combo = new QComboBox;
        const QStringList files = fetchAudioFiles();
        if (!files.isEmpty() && m_cachedAudioFiles) *m_cachedAudioFiles = files;
        const QStringList &audioList = m_cachedAudioFiles ? *m_cachedAudioFiles : files;
        for (const QString &full : audioList) {
            combo->addItem(audioDisplayName(full), full);
            combo->setItemData(combo->count() - 1, full, Qt::ToolTipRole);
        }
        const QString cur = existingVal.toString();
        int idx = combo->findData(cur);
        if (idx < 0 && !cur.isEmpty()) {
            combo->addItem(audioDisplayName(cur), cur);
            combo->setItemData(combo->count() - 1, cur, Qt::ToolTipRole);
            idx = combo->count() - 1;
        }
        if (idx < 0 && combo->count() > 0) idx = 0;
        if (idx >= 0) combo->setCurrentIndex(idx);
        combo->setEditable(combo->count() == 0);
        combo->setStyleSheet(kInputStyle);
        return combo;
    }
    if (param.type == "station_combo") {
        auto *combo = new QComboBox;
        combo->addItem(QStringLiteral("未知"), "");
        QString currentTarget = existingParams.value("target_station").toString();
        if (currentTarget.isEmpty()) currentTarget = existingParams.value("target").toString();
        int selectedIdx = 0;
        for (const auto &st : m_stations) {
            combo->addItem(st.name, st.id);
            if (!currentTarget.isEmpty() && (currentTarget == st.id || currentTarget == st.name)) {
                selectedIdx = combo->count() - 1;
            }
        }
        combo->setCurrentIndex(selectedIdx);
        combo->setStyleSheet(kInputStyle);
        return combo;
    }
    return nullptr;
}

void TaskActionParamDialog::collectParams(const ActionDef &def)
{
    m_resultParams = QJsonObject();
    for (const auto &param : def.params) {
        QWidget *w = m_inputWidgets.value(param.key);
        if (!w) continue;

        if (param.type == "text") {
            m_resultParams[param.key] = qobject_cast<QLineEdit*>(w)->text();
        } else if (param.type == "double") {
            double val = qobject_cast<QDoubleSpinBox*>(w)->value();
            const auto &mc = MotionConfigManager::instance();
            if (param.key == "lift_height") val = qBound(0.0, val, kMaxLiftHeight);
            else if (param.key == "lift_linear") val = qBound(0.0, val, kMaxLiftLinearSpeed);
            else if (param.unit == "m/s") val = mc.clampSignedLinearSpeed(val);
            else if (param.unit == "°/s") val = mc.clampSignedAngularSpeedDeg(val);
            else if (param.key == "distance") val = std::max(0.0, val);
            m_resultParams[param.key] = val;
        } else if (param.type == "int") {
            m_resultParams[param.key] = qobject_cast<QSpinBox*>(w)->value();
        } else if (param.type == "combo") {
            auto *cb = qobject_cast<QComboBox*>(w);
            m_resultParams[param.key] = (param.key == "loop" && !cb->currentData().toString().isEmpty())
                                            ? cb->currentData().toString() : cb->currentText();
        } else if (param.type == "audio_combo") {
            auto *cb = qobject_cast<QComboBox*>(w);
            QString full = cb->currentData().toString();
            m_resultParams[param.key] = full.isEmpty() ? cb->currentText().trimmed() : full;
        } else if (param.type == "station_combo") {
            m_resultParams["target_station"] = qobject_cast<QComboBox*>(w)->currentData().toString();
        }
    }
}

QString TaskActionParamDialog::editActionParams(const ActionDef &def,
                                               const QJsonObject &existingParams,
                                               const QList<TaskChainStationItem> &stations,
                                               QStringList *cachedAudioFiles,
                                               QWidget *parent)
{
    TaskActionParamDialog dlg(def, existingParams, stations, cachedAudioFiles, parent);
    if (dlg.exec() != QDialog::Accepted) return QString();
    return QString::fromUtf8(QJsonDocument(dlg.getResultParams()).toJson(QJsonDocument::Compact));
}
