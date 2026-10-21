/**
 * @file    EditConfigTableBuilder.cpp
 * @layer   GUILayer
 * @brief   Table widget rows population and filtering for EditConfigWidget.
 * @deps    EditConfigTableBuilder.h, EditConfigWidget.h, ConfigMetadataHelper.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "EditConfigTableBuilder.h"
#include "EditConfigWidget.h"
#include "ConfigMetadataHelper.h"

#include <QTableWidget>
#include <QLineEdit>
#include <QJsonDocument>
#include <QColor>
#include <algorithm>

namespace {

QString formatJsonLeafValue(const QJsonValue &jv) {
  if (jv.isString()) return jv.toString();
  if (jv.isDouble()) {
    double d = jv.toDouble();
    return (d == (long long)d) ? QString::number((long long)d)
                               : QString::number(d, 'g', 10);
  }
  if (jv.isBool()) return jv.toBool() ? "true" : "false";
  if (jv.isArray()) {
    return QString::fromUtf8(QJsonDocument(jv.toArray()).toJson(QJsonDocument::Compact));
  }
  return "null";
}

} // namespace

void EditConfigTableBuilder::addParamRows(EditConfigWidget *w, const QJsonObject &params,
                                         int configIdx, const QString &prefix,
                                         const QString &filter) {
  QStringList keys = params.keys();
  static const QStringList kPreferredOrder = {
      "max_speed",
      "max_acceleration",
      "max_deceleration",
      "max_angular_speed",
      "max_angular_acceleration",
      "arrival_distance_tolerance",
      "arrival_angle_tolerance",
      "route_width"
  };
  std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
    int idxA = kPreferredOrder.indexOf(a);
    int idxB = kPreferredOrder.indexOf(b);
    if (idxA != -1 && idxB != -1) return idxA < idxB;
    if (idxA != -1) return true;
    if (idxB != -1) return false;
    return a < b;
  });

  for (const QString &key : keys) {
    const QJsonValue &jv = params.value(key);
    const QString fullPath = prefix.isEmpty() ? key : prefix + "." + key;

    // 隐藏 max_deceleration 相关的参数配置 UI
    if (key == "max_deceleration" || fullPath.endsWith(".max_deceleration")) {
      continue;
    }

    // Recurse into nested objects (expand them as dotted-path rows)
    if (jv.isObject() && !jv.toObject().isEmpty()) {
      addParamRows(w, jv.toObject(), configIdx, fullPath, filter);
      continue;
    }

    QString value = formatJsonLeafValue(jv);

    // Look up default value from metadata
    QJsonObject metaObj;
    if (configIdx >= 0 && configIdx < w->m_configs.size()) {
      metaObj = ConfigMetadataHelper::findMetadataForConfig(w->m_metadataList, w->m_configs[configIdx].toObject());
    }
    QString defaultStr = ConfigMetadataHelper::getDefaultValueString(metaObj, fullPath);

    // Filter
    auto fm = ConfigMetadataHelper::fieldMetaFor(key);
    if (!filter.isEmpty()) {
      bool hit = fullPath.toLower().contains(filter) ||
                 value.toLower().contains(filter) ||
                 defaultStr.toLower().contains(filter) ||
                 fm.first.contains(filter) || fm.second.contains(filter);
      if (!hit) continue;
    }

    int row = w->m_paramTable->rowCount();
    w->m_paramTable->insertRow(row);

    // Column 0: dotted key path (not editable)
    auto *keyItem = new QTableWidgetItem(fullPath);
    keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
    keyItem->setData(Qt::UserRole, configIdx);
    keyItem->setData(Qt::UserRole + 1, fullPath);
    w->m_paramTable->setItem(row, 0, keyItem);

    // Column 1: value (editable)
    w->m_paramTable->setItem(row, 1, new QTableWidgetItem(value));

    // Column 2: default value (not editable)
    auto *defaultItem = new QTableWidgetItem(defaultStr);
    defaultItem->setFlags(defaultItem->flags() & ~Qt::ItemIsEditable);
    defaultItem->setForeground(QColor(0x94, 0xa3, 0xb8));
    defaultItem->setData(Qt::UserRole, configIdx);
    defaultItem->setData(Qt::UserRole + 1, fullPath);
    w->m_paramTable->setItem(row, 2, defaultItem);

    // Column 3: Chinese name + remark + range (not editable)
    QString desc = fm.first;
    if (!fm.second.isEmpty()) desc += "  (" + fm.second + ")";

    double outMin = 0.0, outMax = 0.0;
    bool hasMin = false, hasMax = false;
    if (ConfigMetadataHelper::getParamMinMax(metaObj, fullPath, outMin, outMax, hasMin, hasMax)) {
      if (hasMin && hasMax) {
        desc += QString(" [范围: %1 ~ %2]").arg(outMin).arg(outMax);
      } else if (hasMin) {
        desc += QString(" [最小: %1]").arg(outMin);
      } else if (hasMax) {
        desc += QString(" [最大: %1]").arg(outMax);
      }
    }

    auto *descItem = new QTableWidgetItem(desc);
    descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
    descItem->setForeground(QColor(0x88, 0x99, 0xaa));
    w->m_paramTable->setItem(row, 3, descItem);
  }
}

void EditConfigTableBuilder::refreshParams(EditConfigWidget *w, const QString &groupKey) {
  w->m_paramTable->blockSignals(true);
  w->m_paramTable->setRowCount(0);

  QString filter = w->m_filterEdit->text().trimmed().toLower();

  for (int i = 0; i < w->m_configs.size(); ++i) {
    QJsonObject entry = w->m_configs[i].toObject();
    if (ConfigMetadataHelper::determineProfile(entry) != groupKey) continue;
    addParamRows(w, entry.value("params").toObject(), i, "", filter);
  }

  w->m_paramTable->blockSignals(false);
}

void EditConfigTableBuilder::refreshParamsForConfig(EditConfigWidget *w, int configIdx) {
  w->m_paramTable->blockSignals(true);
  w->m_paramTable->setRowCount(0);

  QString filter = w->m_filterEdit->text().trimmed().toLower();

  if (configIdx >= 0 && configIdx < w->m_configs.size()) {
    QJsonObject entry = w->m_configs[configIdx].toObject();
    addParamRows(w, entry.value("params").toObject(), configIdx, "", filter);
  }

  w->m_paramTable->blockSignals(false);
}

void EditConfigTableBuilder::updateDefaultValuesInTable(EditConfigWidget *w) {
  if (!w->m_paramTable || w->m_paramTable->rowCount() == 0) return;
  w->m_paramTable->blockSignals(true);
  for (int r = 0; r < w->m_paramTable->rowCount(); ++r) {
    auto *keyItem = w->m_paramTable->item(r, 0);
    auto *defItem = w->m_paramTable->item(r, 2);
    if (!keyItem) continue;

    int configIdx = keyItem->data(Qt::UserRole).toInt();
    QString fullPath = keyItem->data(Qt::UserRole + 1).toString();
    if (fullPath.isEmpty()) fullPath = keyItem->text();

    if (configIdx >= 0 && configIdx < w->m_configs.size()) {
      QJsonObject metaObj = ConfigMetadataHelper::findMetadataForConfig(w->m_metadataList, w->m_configs[configIdx].toObject());
      QString defaultStr = ConfigMetadataHelper::getDefaultValueString(metaObj, fullPath);
      if (!defItem) {
        defItem = new QTableWidgetItem(defaultStr);
        defItem->setFlags(defItem->flags() & ~Qt::ItemIsEditable);
        defItem->setForeground(QColor(0x94, 0xa3, 0xb8));
        defItem->setData(Qt::UserRole, configIdx);
        defItem->setData(Qt::UserRole + 1, fullPath);
        w->m_paramTable->setItem(r, 2, defItem);
      } else {
        defItem->setText(defaultStr);
      }
    }
  }
  w->m_paramTable->blockSignals(false);
}
