/**
 * @file    MapSmapParser.h
 * @layer   Utils
 * @brief   SMAP format JSON/file parser and symmetric curve merger.
 * @deps    QJsonObject, QJsonArray, QPointF
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QList>
#include <QMap>

/**
 * @brief SMAP 格式地图文件与 JSON 解析实用类
 */
class MapSmapParser final
{
public:
    MapSmapParser() = delete;
    ~MapSmapParser() = delete;
    MapSmapParser(const MapSmapParser &) = delete;
    MapSmapParser &operator=(const MapSmapParser &) = delete;

    static constexpr double kCurveMergeToleranceMeters   = 0.05;  ///< 对称曲线合并容差 (5cm)
    static constexpr double kDefaultCurveMaxSpeed        = 8.0;   ///< 默认曲线最大线速度 (m/s)
    static constexpr double kDefaultCurveMinSpeed        = 0.0;   ///< 默认曲线最小线速度 (m/s)
    static constexpr double kDefaultCurveMaxAcc          = 0.3;   ///< 默认曲线最大加速度 (m/s²)
    static constexpr double kDefaultCurveMaxRotSpeed     = 60.0;  ///< 默认曲线最大角速度 (°/s)
    static constexpr double kDefaultCurveMaxRotAcc       = 45.0;  ///< 默认曲线最大角加速度 (°/s²)
    static constexpr double kDefaultCurveSlowDistance    = 2.0;   ///< 默认曲线减速距离 (m)

    struct ParsedCurve {
        QString name;
        QString startInst;
        QString endInst;
        QList<QPointF> ctrlPts;
    };

    /**
     * @brief 解析 .smap 格式文件，提取路网点、线段、贝塞尔曲线并自动归并对称反向曲线
     */
    static bool parseSmapFile(const QString &filePath,
                              QJsonArray &outPoints,
                              QJsonArray &outLines,
                              QJsonArray &outCurves,
                              QString &outMapName,
                              QString *outError = nullptr,
                              QJsonObject *outHeader = nullptr);

    /**
     * @brief 解析 .smap JSON 数据对象
     */
    static bool parseSmapJson(const QJsonObject &smapObj,
                              QJsonArray &outPoints,
                              QJsonArray &outLines,
                              QJsonArray &outCurves,
                              QString &outMapName,
                              QString *outError = nullptr,
                              QJsonObject *outHeader = nullptr);

private:
    static void parseSmapPoints(const QJsonArray &advPoints,
                                QJsonArray &outPoints,
                                QMap<QString, QString> &instToIdMap,
                                QMap<QString, QPointF> &instToPosMap);

    static QList<ParsedCurve> parseSmapCurves(const QJsonArray &advCurves,
                                              const QMap<QString, QPointF> &instToPosMap);

    static QList<ParsedCurve> mergeSymmetricCurves(const QList<ParsedCurve> &rawCurves);

    static bool isSymmetricReverse(const ParsedCurve &a, const ParsedCurve &b, double tolerance);

    static QJsonObject buildCurveJsonObject(const ParsedCurve &curve,
                                            int curveIndex,
                                            const QMap<QString, QString> &instToIdMap);
};
