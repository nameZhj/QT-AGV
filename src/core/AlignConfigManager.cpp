#include "AlignConfigManager.h"
#include <QDebug>

AlignConfigManager &AlignConfigManager::instance()
{
    static AlignConfigManager s_inst;
    return s_inst;
}

AlignConfigManager::AlignConfigManager(QObject *parent)
    : QObject(parent)
{
    loadSettings();
}

void AlignConfigManager::loadSettings()
{
    QSettings settings;
    m_antiAlignDepth = settings.value("alignConfig/anti_align_depth", 0.0).toDouble();
    m_cylinderLength = settings.value("alignConfig/cylinder_length", 1.2).toDouble();
}

void AlignConfigManager::saveSettings()
{
    QSettings settings;
    settings.setValue("alignConfig/anti_align_depth", m_antiAlignDepth);
    settings.setValue("alignConfig/cylinder_length", m_cylinderLength);
}

void AlignConfigManager::setAntiAlignDepth(double val)
{
    if (qFuzzyCompare(m_antiAlignDepth, val)) return;
    m_antiAlignDepth = val;
    saveSettings();
    emit configChanged();
}

void AlignConfigManager::setCylinderLength(double val)
{
    if (qFuzzyCompare(m_cylinderLength, val)) return;
    m_cylinderLength = val;
    saveSettings();
    emit configChanged();
}

void AlignConfigManager::updateFromJsonObject(const QJsonObject &params)
{
    bool changed = false;
    if (params.contains("anti_align_depth")) {
        double d = params.value("anti_align_depth").toDouble(m_antiAlignDepth);
        if (!qFuzzyCompare(m_antiAlignDepth, d)) {
            m_antiAlignDepth = d;
            changed = true;
        }
    }
    if (params.contains("cylinder_length")) {
        double cl = params.value("cylinder_length").toDouble(m_cylinderLength);
        if (!qFuzzyCompare(m_cylinderLength, cl)) {
            m_cylinderLength = cl;
            changed = true;
        }
    }
    if (changed) {
        saveSettings();
        emit configChanged();
    }
}

void AlignConfigManager::updateFromConfigs(const QJsonArray &configs)
{
    for (const auto &v : configs) {
        QJsonObject entry = v.toObject();
        QJsonObject params = entry.value("params").toObject();
        if (params.contains("cylinder_length") || params.contains("anti_align_depth")) {
            updateFromJsonObject(params);
            break;
        }
    }
}
