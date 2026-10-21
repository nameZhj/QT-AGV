/**
 * @file    MapSmapParser.cpp
 * @layer   Utils
 * @brief   Implementation of SMAP format JSON/file parser and symmetric curve merger.
 * @deps    MapSmapParser.h, MapPgmUtil.h, QFile, QJsonDocument
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapSmapParser.h"
#include "MapPgmUtil.h"

#include <QFile>
#include <QJsonDocument>
#include <cmath>

bool MapSmapParser::parseSmapFile(const QString &filePath,
                                  QJsonArray &outPoints,
                                  QJsonArray &outLines,
                                  QJsonArray &outCurves,
                                  QString &outMapName,
                                  QString *outError,
                                  QJsonObject *outHeader)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (outError) *outError = QString("无法打开 SMAP 文件: %1").arg(file.errorString());
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (outError) *outError = QString("SMAP JSON 格式解析失败: %1").arg(parseErr.errorString());
        return false;
    }

    return parseSmapJson(doc.object(), outPoints, outLines, outCurves, outMapName, outError, outHeader);
}

void MapSmapParser::parseSmapPoints(const QJsonArray &advPoints,
                                    QJsonArray &outPoints,
                                    QMap<QString, QString> &instToIdMap,
                                    QMap<QString, QPointF> &instToPosMap)
{
    int pointCount = 0;
    for (const auto &pv : advPoints) {
        const QJsonObject pt = pv.toObject();
        QString instName = pt.value(QStringLiteral("instanceName")).toString().trimmed();
        if (instName.isEmpty()) {
            instName = QStringLiteral("P%1").arg(pointCount + 1);
        }
        ++pointCount;
        const QString pid = QStringLiteral("p_%1").arg(pointCount);
        instToIdMap[instName] = pid;

        const QJsonObject posObj = pt.value(QStringLiteral("pos")).toObject();
        const double x = posObj.value(QStringLiteral("x")).toDouble(0.0);
        const double y = posObj.value(QStringLiteral("y")).toDouble(0.0);
        instToPosMap[instName] = QPointF(x, y);

        const QString className = pt.value(QStringLiteral("className")).toString();
        QString type = QStringLiteral("point");
        if (className.contains(QStringLiteral("Charge"), Qt::CaseInsensitive) ||
            instName.startsWith(QStringLiteral("CP"), Qt::CaseInsensitive)) {
            type = QStringLiteral("charge");
        } else if (className.contains(QStringLiteral("Precision"), Qt::CaseInsensitive) ||
                   className.contains(QStringLiteral("Transfer"), Qt::CaseInsensitive) ||
                   instName.startsWith(QStringLiteral("TL"), Qt::CaseInsensitive)) {
            type = QStringLiteral("highPrecision");
        }

        double yaw = 0.0;
        if (pt.contains(QStringLiteral("dir"))) {
            yaw = pt.value(QStringLiteral("dir")).toDouble();
        } else if (posObj.contains(QStringLiteral("dir"))) {
            yaw = posObj.value(QStringLiteral("dir")).toDouble();
        } else if (pt.contains(QStringLiteral("yaw"))) {
            yaw = pt.value(QStringLiteral("yaw")).toDouble();
        }

        QJsonObject pObj;
        pObj[QStringLiteral("id")]       = pid;
        pObj[QStringLiteral("name")]     = instName;
        pObj[QStringLiteral("type")]     = type;
        pObj[QStringLiteral("x")]        = x;
        pObj[QStringLiteral("y")]        = y;
        pObj[QStringLiteral("yaw")]      = yaw;
        pObj[QStringLiteral("isyawfix")] = false;
        pObj[QStringLiteral("taskid")]   = 0;
        outPoints.append(pObj);
    }
}

QList<MapSmapParser::ParsedCurve> MapSmapParser::parseSmapCurves(const QJsonArray &advCurves,
                                                                const QMap<QString, QPointF> &instToPosMap)
{
    QList<ParsedCurve> rawCurves;
    for (const auto &cv : advCurves) {
        const QJsonObject cObj = cv.toObject();
        const QString instName = cObj.value(QStringLiteral("instanceName")).toString();
        const QJsonObject startPos = cObj.value(QStringLiteral("startPos")).toObject();
        const QJsonObject endPos   = cObj.value(QStringLiteral("endPos")).toObject();
        const QString startInst    = startPos.value(QStringLiteral("instanceName")).toString().trimmed();
        const QString endInst      = endPos.value(QStringLiteral("instanceName")).toString().trimmed();

        const QJsonObject p0Obj = startPos.value(QStringLiteral("pos")).toObject();
        const QJsonObject p1Obj = cObj.value(QStringLiteral("controlPos1")).toObject();
        const QJsonObject p2Obj = cObj.value(QStringLiteral("controlPos2")).toObject();
        const QJsonObject p3Obj = endPos.value(QStringLiteral("pos")).toObject();

        QPointF p0(p0Obj.value(QStringLiteral("x")).toDouble(0.0), p0Obj.value(QStringLiteral("y")).toDouble(0.0));
        const QPointF p1(p1Obj.value(QStringLiteral("x")).toDouble(0.0), p1Obj.value(QStringLiteral("y")).toDouble(0.0));
        const QPointF p2(p2Obj.value(QStringLiteral("x")).toDouble(0.0), p2Obj.value(QStringLiteral("y")).toDouble(0.0));
        QPointF p3(p3Obj.value(QStringLiteral("x")).toDouble(0.0), p3Obj.value(QStringLiteral("y")).toDouble(0.0));

        if (instToPosMap.contains(startInst)) p0 = instToPosMap[startInst];
        if (instToPosMap.contains(endInst))   p3 = instToPosMap[endInst];

        ParsedCurve pc;
        pc.name      = instName;
        pc.startInst = startInst;
        pc.endInst   = endInst;
        pc.ctrlPts   = {p0, p1, p2, p3};
        rawCurves.append(pc);
    }
    return rawCurves;
}

bool MapSmapParser::isSymmetricReverse(const ParsedCurve &a, const ParsedCurve &b, double tolerance)
{
    if (a.startInst != b.endInst || a.endInst != b.startInst) return false;
    if (a.ctrlPts.size() != 4 || b.ctrlPts.size() != 4) return false;

    return (std::hypot(a.ctrlPts[0].x() - b.ctrlPts[3].x(), a.ctrlPts[0].y() - b.ctrlPts[3].y()) < tolerance &&
            std::hypot(a.ctrlPts[1].x() - b.ctrlPts[2].x(), a.ctrlPts[1].y() - b.ctrlPts[2].y()) < tolerance &&
            std::hypot(a.ctrlPts[2].x() - b.ctrlPts[1].x(), a.ctrlPts[2].y() - b.ctrlPts[1].y()) < tolerance &&
            std::hypot(a.ctrlPts[3].x() - b.ctrlPts[0].x(), a.ctrlPts[3].y() - b.ctrlPts[0].y()) < tolerance);
}

QList<MapSmapParser::ParsedCurve> MapSmapParser::mergeSymmetricCurves(const QList<ParsedCurve> &rawCurves)
{
    QList<ParsedCurve> mergedCurves;
    for (const auto &rawCurve : rawCurves) {
        bool isReverseOfExisting = false;
        for (const auto &existingCurve : mergedCurves) {
            if (isSymmetricReverse(rawCurve, existingCurve, kCurveMergeToleranceMeters)) {
                isReverseOfExisting = true;
                break;
            }
        }
        if (!isReverseOfExisting) {
            mergedCurves.append(rawCurve);
        }
    }
    return mergedCurves;
}

QJsonObject MapSmapParser::buildCurveJsonObject(const ParsedCurve &curve,
                                               int curveIndex,
                                               const QMap<QString, QString> &instToIdMap)
{
    const QString cid = QStringLiteral("b_%1").arg(curveIndex);
    const QString startId = instToIdMap.value(curve.startInst);
    const QString endId   = instToIdMap.value(curve.endInst);

    QJsonObject curveObj;
    curveObj[QStringLiteral("id")]                        = cid;
    curveObj[QStringLiteral("name")]                      = curve.name.isEmpty() ? QStringLiteral("曲%1").arg(curveIndex) : curve.name;
    curveObj[QStringLiteral("startid")]                   = startId;
    curveObj[QStringLiteral("endid")]                     = endId;
    curveObj[QStringLiteral("direct")]                    = static_cast<int>(MapPgmUtil::Direction::Bidirectional);
    curveObj[QStringLiteral("maxspeed")]                  = kDefaultCurveMaxSpeed;
    curveObj[QStringLiteral("minspeed")]                  = kDefaultCurveMinSpeed;
    curveObj[QStringLiteral("max_acceleration")]          = kDefaultCurveMaxAcc;
    curveObj[QStringLiteral("maxacc")]                    = kDefaultCurveMaxAcc;
    curveObj[QStringLiteral("max_angular_speed")]         = kDefaultCurveMaxRotSpeed;
    curveObj[QStringLiteral("maxrot")]                    = kDefaultCurveMaxRotSpeed;
    curveObj[QStringLiteral("max_angular_acceleration")]  = kDefaultCurveMaxRotAcc;
    curveObj[QStringLiteral("maxrotacc")]                 = kDefaultCurveMaxRotAcc;
    curveObj[QStringLiteral("slowdis")]                   = kDefaultCurveSlowDistance;

    QJsonArray ctrlArr;
    for (const auto &pt : curve.ctrlPts) {
        QJsonObject p;
        p[QStringLiteral("x")] = pt.x();
        p[QStringLiteral("y")] = pt.y();
        ctrlArr.append(p);
    }
    curveObj[QStringLiteral("controlPoints")] = ctrlArr;

    if (!curve.ctrlPts.isEmpty()) {
        curveObj[QStringLiteral("x1")] = curve.ctrlPts.first().x();
        curveObj[QStringLiteral("y1")] = curve.ctrlPts.first().y();
        curveObj[QStringLiteral("x2")] = curve.ctrlPts.last().x();
        curveObj[QStringLiteral("y2")] = curve.ctrlPts.last().y();
        if (curve.ctrlPts.size() == 3) {
            curveObj[QStringLiteral("cx")] = curve.ctrlPts[1].x();
            curveObj[QStringLiteral("cy")] = curve.ctrlPts[1].y();
        }
    }
    return curveObj;
}

bool MapSmapParser::parseSmapJson(const QJsonObject &smapObj,
                                  QJsonArray &outPoints,
                                  QJsonArray &outLines,
                                  QJsonArray &outCurves,
                                  QString &outMapName,
                                  QString *outError,
                                  QJsonObject *outHeader)
{
    Q_UNUSED(outError);
    outPoints = QJsonArray();
    outLines  = QJsonArray();
    outCurves = QJsonArray();

    const QJsonObject header = smapObj.value(QStringLiteral("header")).toObject();
    outMapName = header.value(QStringLiteral("mapName")).toString();
    if (outHeader) {
        *outHeader = header;
    }

    QMap<QString, QString> instToIdMap;
    QMap<QString, QPointF> instToPosMap;
    parseSmapPoints(smapObj.value(QStringLiteral("advancedPointList")).toArray(),
                    outPoints, instToIdMap, instToPosMap);

    const QList<ParsedCurve> rawCurves = parseSmapCurves(
        smapObj.value(QStringLiteral("advancedCurveList")).toArray(), instToPosMap);

    const QList<ParsedCurve> mergedCurves = mergeSymmetricCurves(rawCurves);

    int curveIndex = 0;
    for (const auto &curve : mergedCurves) {
        ++curveIndex;
        outCurves.append(buildCurveJsonObject(curve, curveIndex, instToIdMap));
    }

    return true;
}
