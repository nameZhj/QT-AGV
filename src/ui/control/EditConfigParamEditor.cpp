/**
 * @file    EditConfigParamEditor.cpp
 * @layer   GUILayer
 * @brief   Parameter modification validation, range clamping, and manager updates for EditConfigWidget.
 * @deps    EditConfigParamEditor.h, EditConfigWidget.h, EditConfigRouteSync.h, ConfigMetadataHelper.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "EditConfigParamEditor.h"
#include "EditConfigWidget.h"
#include "EditConfigRouteSync.h"
#include "ConfigMetadataHelper.h"
#include "core/MotionConfigManager.h"
#include "core/AlignConfigManager.h"
#include "ui/common/Toast.h"

#include <QTableWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

QJsonValue EditConfigParamEditor::jsonGet(const QJsonValue &root, const QStringList &path) {
  if (path.isEmpty()) return root;
  if (!root.isObject()) return {};
  return jsonGet(root.toObject().value(path.first()), path.mid(1));
}

QJsonValue EditConfigParamEditor::jsonSet(const QJsonValue &root, const QStringList &path,
                                         const QJsonValue &newVal) {
  if (path.isEmpty()) return newVal;
  QJsonObject obj = root.isObject() ? root.toObject() : QJsonObject{};
  obj[path.first()] = jsonSet(obj.value(path.first()), path.mid(1), newVal);
  return obj;
}

static void applySpecialKeySideEffects(EditConfigWidget *w, const QJsonObject &entry,
                                      const QString &lastKey, const QJsonValue &newLeaf) {
  if (lastKey == "anti_align_depth") {
    AlignConfigManager::instance().setAntiAlignDepth(
        newLeaf.toDouble(AlignConfigManager::instance().antiAlignDepth()));
  } else if (lastKey == "cylinder_length") {
    AlignConfigManager::instance().setCylinderLength(
        newLeaf.toDouble(AlignConfigManager::instance().cylinderLength()));
  } else if (lastKey == "id_stop_range_threshold" || lastKey == "id_deceleration_range_threshold") {
    MotionConfigManager::instance().updateObstacleConfig(entry.value("params").toObject());
    const auto &mc = MotionConfigManager::instance();
    EditConfigRouteSync::syncObstacleParamsToRouteNetwork(
        w, mc.defaultObsStopLength(), mc.defaultObsStopWidth(),
        mc.defaultObsDecelLength(), mc.defaultObsDecelWidth());
  }

  QString cfgName = entry.value("name").toString().trimmed().toLower();
  QString cfgPath = entry.value("file_path").toString().trimmed().toLower();
  if (cfgName.contains("motion_control") || cfgPath.contains("motion_control")) {
    MotionConfigManager::instance().updateFromJsonObject(entry.value("params").toObject());
  }
}

void EditConfigParamEditor::onParamCellChanged(EditConfigWidget *w, int row, int col) {
  if (col != 1) return;
  auto *keyItem = w->m_paramTable->item(row, 0);
  auto *valItem = w->m_paramTable->item(row, 1);
  if (!keyItem || !valItem) return;

  QVariant idxVar = keyItem->data(Qt::UserRole);
  if (!idxVar.isValid()) return;
  int configIdx = idxVar.toInt();
  if (configIdx < 0 || configIdx >= w->m_configs.size()) return;

  QString fullPath = keyItem->data(Qt::UserRole + 1).toString();
  if (fullPath.isEmpty()) fullPath = keyItem->text();
  QStringList segments = fullPath.split(".");
  QString newStr = valItem->text().trimmed();

  QJsonObject entry = w->m_configs[configIdx].toObject();
  QJsonValue params = entry.value("params");
  QJsonValue origLeaf = jsonGet(params, segments);
  QJsonValue newLeaf;

  double minVal = 0.0, maxVal = 0.0;
  bool hasMin = false, hasMax = false;
  QJsonObject metaObj = ConfigMetadataHelper::findMetadataForConfig(w->m_metadataList, entry);
  ConfigMetadataHelper::getParamMinMax(metaObj, fullPath, minVal, maxVal, hasMin, hasMax);

  if (hasMin || hasMax) {
    bool ok = false;
    double d = newStr.toDouble(&ok);
    if (ok) {
      bool clamped = false;
      if (hasMin && d < minVal) { d = minVal; clamped = true; }
      if (hasMax && d > maxVal) { d = maxVal; clamped = true; }

      if (clamped) {
        QString rangeStr = (hasMin && hasMax) ? QString("[%1 ~ %2]").arg(minVal).arg(maxVal)
                                              : (hasMin ? QString("[>= %1]").arg(minVal)
                                                        : QString("[<= %1]").arg(maxVal));
        Toast::warning(w, QString("参数「%1」超出允许范围 %2，已限制为 %3")
                              .arg(fullPath).arg(rangeStr).arg(d));
      }

      newStr = (d == (long long)d) ? QString::number((long long)d)
                                   : QString::number(d, 'g', 10);
      w->m_paramTable->blockSignals(true);
      valItem->setText(newStr);
      w->m_paramTable->blockSignals(false);
      newLeaf = QJsonValue(d);
    } else {
      if (origLeaf.isDouble()) {
        double oldD = origLeaf.toDouble();
        newStr = (oldD == (long long)oldD) ? QString::number((long long)oldD)
                                           : QString::number(oldD, 'g', 10);
        w->m_paramTable->blockSignals(true);
        valItem->setText(newStr);
        w->m_paramTable->blockSignals(false);
        newLeaf = QJsonValue(oldD);
        Toast::warning(w, QString("参数「%1」必须为数值").arg(fullPath));
      } else {
        newLeaf = QJsonValue(newStr);
      }
    }
  } else {
    if (origLeaf.isDouble()) {
      bool ok;
      double d = newStr.toDouble(&ok);
      newLeaf = ok ? QJsonValue(d) : QJsonValue(newStr);
    } else if (origLeaf.isBool()) {
      QString lo = newStr.toLower();
      if (lo == "true" || lo == "1") newLeaf = QJsonValue(true);
      else if (lo == "false" || lo == "0") newLeaf = QJsonValue(false);
      else newLeaf = QJsonValue(newStr);
    } else if (origLeaf.isArray()) {
      QJsonParseError pe;
      QJsonDocument doc = QJsonDocument::fromJson(newStr.toUtf8(), &pe);
      newLeaf = (pe.error == QJsonParseError::NoError && doc.isArray())
                    ? QJsonValue(doc.array())
                    : QJsonValue(newStr);
    } else {
      newLeaf = QJsonValue(newStr);
    }
  }

  entry["params"] = jsonSet(params, segments, newLeaf).toObject();
  w->m_configs[configIdx] = entry;

  if (!segments.isEmpty()) {
    applySpecialKeySideEffects(w, entry, segments.last(), newLeaf);
  }
}
