/**
 * @file    MotionMetadataParser.h
 * @layer   CoreScheduling
 * @brief   Parses ROS config metadata array and provides parameter range definitions.
 * @deps    QJsonArray, QJsonObject, QMap
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

struct MotionParamMeta {
    double defaultValue = 0.0;
    double minVal       = 0.0;
    double maxVal       = 0.0;
    bool   hasMin       = false;
    bool   hasMax       = false;
    bool   hasDefault   = false;
};

struct MotionParamRange {
    double  minVal   = 0.0;
    double  maxVal   = 10.0;
    double  defVal   = 0.5;
    double  step     = 0.05;
    int     decimals = 2;
    QString unit;
};

class MotionMetadataParser final
{
public:
    MotionMetadataParser() = delete;
    ~MotionMetadataParser() = delete;

    static void parseMetadataJson(const QByteArray &rawJson, QJsonArray &outList);

    static bool parseMetadataArray(const QJsonArray &metaList,
                                   QMap<QString, MotionParamMeta> &metas,
                                   double &stopLen,
                                   double &stopWid,
                                   double &decelLen,
                                   double &decelWid);

    static QString canonicalKey(const QString &paramName,
                                const QMap<QString, MotionParamMeta> &metas);

    static double getParamMin(const QString &paramName,
                              const QMap<QString, MotionParamMeta> &metas,
                              double fallback);

    static double getParamMax(const QString &paramName,
                              const QMap<QString, MotionParamMeta> &metas,
                              double fallback);

    static double getParamDefault(const QString &paramName,
                                  const QMap<QString, MotionParamMeta> &metas,
                                  double fallback);

    static MotionParamRange getParamRange(const QString &paramName,
                                          const QMap<QString, MotionParamMeta> &metas);
};
