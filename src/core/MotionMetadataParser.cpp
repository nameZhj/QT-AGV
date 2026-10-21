/**
 * @file    MotionMetadataParser.cpp
 * @layer   CoreScheduling
 * @brief   Implementation of metadata array parser and default range definitions.
 * @deps    MotionMetadataParser.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MotionMetadataParser.h"

#include <QJsonDocument>

void MotionMetadataParser::parseMetadataJson(const QByteArray &rawJson, QJsonArray &outList)
{
    outList = QJsonArray();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(rawJson, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }
    const QJsonObject root = doc.object();
    if (!root.contains("data")) return;

    const QJsonValue dv = root.value("data");
    if (dv.isArray()) {
        outList = dv.toArray();
    } else if (dv.isObject()) {
        const QJsonObject dataObj = dv.toObject();
        if (dataObj.contains("data") && dataObj.value("data").isArray()) {
            outList = dataObj.value("data").toArray();
        } else {
            for (const auto &k : dataObj.keys()) {
                if (dataObj.value(k).isArray()) {
                    outList = dataObj.value(k).toArray();
                    break;
                }
            }
        }
    }
}

bool MotionMetadataParser::parseMetadataArray(const QJsonArray &metaList,
                                             QMap<QString, MotionParamMeta> &metas,
                                             double &stopLen,
                                             double &stopWid,
                                             double &decelLen,
                                             double &decelWid)
{
    bool metaUpdated = false;
    for (const auto &item : metaList) {
        const QJsonObject obj = item.toObject();
        const QJsonObject params = obj.value("params").toObject();
        for (auto it = params.begin(); it != params.end(); ++it) {
            const QString paramName = it.key();
            const QJsonObject pObj = it.value().toObject();
            MotionParamMeta m;
            if (pObj.contains("default") && !pObj.value("default").isNull()) {
                m.defaultValue = pObj.value("default").toDouble();
                m.hasDefault = true;
            }
            if (pObj.contains("min") && !pObj.value("min").isNull()) {
                m.minVal = pObj.value("min").toDouble();
                m.hasMin = true;
            }
            if (pObj.contains("max") && !pObj.value("max").isNull()) {
                m.maxVal = pObj.value("max").toDouble();
                m.hasMax = true;
            }
            metas[paramName] = m;
            metaUpdated = true;
        }

        if (params.contains(QStringLiteral("id_stop_range_threshold"))) {
            const QJsonValue stopVal = params.value(QStringLiteral("id_stop_range_threshold"));
            if (stopVal.isObject()) {
                const QJsonObject stopObj = stopVal.toObject();
                if (stopObj.contains("default") && stopObj.value("default").isArray()) {
                    const QJsonArray defArr = stopObj.value("default").toArray();
                    if (defArr.size() >= 2) {
                        stopLen = defArr.at(0).toDouble();
                        stopWid = defArr.at(1).toDouble();
                    }
                }
            } else if (stopVal.isArray()) {
                const QJsonArray defArr = stopVal.toArray();
                if (defArr.size() >= 2) {
                    stopLen = defArr.at(0).toDouble();
                    stopWid = defArr.at(1).toDouble();
                }
            }
        }
        if (params.contains(QStringLiteral("id_deceleration_range_threshold"))) {
            const QJsonValue decelVal = params.value(QStringLiteral("id_deceleration_range_threshold"));
            if (decelVal.isObject()) {
                const QJsonObject decelObj = decelVal.toObject();
                if (decelObj.contains("default") && decelObj.value("default").isArray()) {
                    const QJsonArray defArr = decelObj.value("default").toArray();
                    if (defArr.size() >= 2) {
                        decelLen = defArr.at(0).toDouble();
                        decelWid = defArr.at(1).toDouble();
                    }
                }
            } else if (decelVal.isArray()) {
                const QJsonArray defArr = decelVal.toArray();
                if (defArr.size() >= 2) {
                    decelLen = defArr.at(0).toDouble();
                    decelWid = defArr.at(1).toDouble();
                }
            }
        }
    }
    return metaUpdated;
}

QString MotionMetadataParser::canonicalKey(const QString &paramName,
                                          const QMap<QString, MotionParamMeta> &metas)
{
    QString k = paramName;
    if (!metas.contains(k)) {
        if (k == "maxspeed" && metas.contains("max_speed")) k = "max_speed";
        else if (k == "max_speed" && metas.contains("maxspeed")) k = "maxspeed";
    }
    return k;
}

double MotionMetadataParser::getParamMin(const QString &paramName,
                                        const QMap<QString, MotionParamMeta> &metas,
                                        double fallback)
{
    const QString k = canonicalKey(paramName, metas);
    if (metas.contains(k) && metas[k].hasMin) {
        return metas[k].minVal;
    }
    return fallback;
}

double MotionMetadataParser::getParamMax(const QString &paramName,
                                        const QMap<QString, MotionParamMeta> &metas,
                                        double fallback)
{
    const QString k = canonicalKey(paramName, metas);
    if (metas.contains(k) && metas[k].hasMax) {
        return metas[k].maxVal;
    }
    return fallback;
}

double MotionMetadataParser::getParamDefault(const QString &paramName,
                                            const QMap<QString, MotionParamMeta> &metas,
                                            double fallback)
{
    const QString k = canonicalKey(paramName, metas);
    if (metas.contains(k) && metas[k].hasDefault) {
        return metas[k].defaultValue;
    }
    return fallback;
}

MotionParamRange MotionMetadataParser::getParamRange(const QString &paramName,
                                                    const QMap<QString, MotionParamMeta> &metas)
{
    MotionParamRange r;
    const QString k = paramName.toLower();

    if (k == QStringLiteral("max_speed") || k == QStringLiteral("maxspeed") || k == QStringLiteral("speed")) {
        r.minVal   = 0.01;
        r.maxVal   = 5.0;
        r.defVal   = 0.5;
        r.step     = 0.05;
        r.decimals = 2;
        r.unit     = QStringLiteral("m/s");
    } else if (k == QStringLiteral("max_acceleration") || k == QStringLiteral("maxacc") || k == QStringLiteral("acceleration")) {
        r.minVal   = 0.01;
        r.maxVal   = 3.0;
        r.defVal   = 0.4;
        r.step     = 0.05;
        r.decimals = 2;
        r.unit     = QStringLiteral("m/s²");
    } else if (k == QStringLiteral("max_deceleration") || k == QStringLiteral("maxdec") || k == QStringLiteral("deceleration")) {
        r.minVal   = 0.01;
        r.maxVal   = 3.0;
        r.defVal   = 0.4;
        r.step     = 0.05;
        r.decimals = 2;
        r.unit     = QStringLiteral("m/s²");
    } else if (k == QStringLiteral("max_angular_speed") || k == QStringLiteral("rotate_speed") || k == QStringLiteral("angular_speed")) {
        r.minVal   = 1.0;
        r.maxVal   = 180.0;
        r.defVal   = 50.0;
        r.step     = 1.0;
        r.decimals = 1;
        r.unit     = QStringLiteral("°/s");
    } else if (k == QStringLiteral("max_angular_acceleration") || k == QStringLiteral("rotate_acc") || k == QStringLiteral("angular_acceleration")) {
        r.minVal   = 1.0;
        r.maxVal   = 90.0;
        r.defVal   = 20.0;
        r.step     = 1.0;
        r.decimals = 1;
        r.unit     = QStringLiteral("°/s²");
    } else if (k == QStringLiteral("arrival_distance_tolerance") || k == QStringLiteral("dist_tol")) {
        r.minVal   = 0.01;
        r.maxVal   = 1.0;
        r.defVal   = 0.1;
        r.step     = 0.01;
        r.decimals = 2;
        r.unit     = QStringLiteral("m");
    } else if (k == QStringLiteral("arrival_angle_tolerance") || k == QStringLiteral("angle_tol")) {
        r.minVal   = 0.1;
        r.maxVal   = 30.0;
        r.defVal   = 0.5;
        r.step     = 0.1;
        r.decimals = 1;
        r.unit     = QStringLiteral("°");
    } else if (k == QStringLiteral("route_width")) {
        r.minVal   = 0.5;
        r.maxVal   = 10.0;
        r.defVal   = 4.0;
        r.step     = 0.1;
        r.decimals = 1;
        r.unit     = QStringLiteral("m");
    } else if (k.contains(QStringLiteral("stop_length")) || k.contains(QStringLiteral("stop_width"))) {
        r.minVal   = 0.0;
        r.maxVal   = 2.0;
        r.defVal   = 0.1;
        r.step     = 0.05;
        r.decimals = 2;
        r.unit     = QStringLiteral("m");
    } else if (k.contains(QStringLiteral("deceleration_length")) || k.contains(QStringLiteral("deceleration_width"))) {
        r.minVal   = 0.0;
        r.maxVal   = 3.0;
        r.defVal   = 0.5;
        r.step     = 0.05;
        r.decimals = 2;
        r.unit     = QStringLiteral("m");
    }

    const QString ck = canonicalKey(paramName, metas);
    if (metas.contains(ck)) {
        const auto &m = metas[ck];
        if (m.hasMin) r.minVal = m.minVal;
        if (m.hasMax) r.maxVal = m.maxVal;
        if (m.hasDefault) r.defVal = m.defaultValue;
    }
    return r;
}
