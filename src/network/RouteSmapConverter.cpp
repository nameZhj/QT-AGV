/**
 * @file    RouteSmapConverter.cpp
 * @layer   DataComm
 * @brief   Implementation of industrial .smap map format import and export.
 * @deps    RouteSmapConverter.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "RouteSmapConverter.h"

bool RouteSmapConverter::parseSmap(const QJsonObject &smapObj, RouteNetwork &outNetwork)
{
    outNetwork.clear();
    outNetwork.mapName = smapObj.value(QStringLiteral("header")).toObject().value(QStringLiteral("mapName")).toString();

    const QJsonArray advPoints = smapObj.value(QStringLiteral("advancedPointList")).toArray();
    QMap<QString, QString> instToId;
    int pointCount = 0;
    for (const auto &pointVal : advPoints) {
        const QJsonObject pointObj = pointVal.toObject();
        const QString inst = pointObj.value(QStringLiteral("instanceName")).toString().trimmed();
        ++pointCount;
        const QString pid = QStringLiteral("p_%1").arg(pointCount);
        instToId[inst] = pid;

        RoutePoint pt;
        pt.id = pid;
        pt.name = inst.isEmpty() ? QStringLiteral("P%1").arg(pointCount) : inst;
        const QJsonObject posObj = pointObj.value(QStringLiteral("pos")).toObject();
        pt.x = posObj.value(QStringLiteral("x")).toDouble(0.0);
        pt.y = posObj.value(QStringLiteral("y")).toDouble(0.0);
        pt.yaw = posObj.value(QStringLiteral("dir")).toDouble(0.0);

        const QString className = pointObj.value(QStringLiteral("className")).toString();
        if (className.contains(QStringLiteral("Charge"), Qt::CaseInsensitive) || inst.startsWith(QStringLiteral("CP"), Qt::CaseInsensitive)) {
            pt.type = QStringLiteral("charge");
            outNetwork.charges.append(pt);
        } else if (className.contains(QStringLiteral("Precision"), Qt::CaseInsensitive) || inst.startsWith(QStringLiteral("TL"), Qt::CaseInsensitive)) {
            pt.type = QStringLiteral("highPrecision");
            outNetwork.highPrecisionPoints.append(pt);
        } else {
            pt.type = QStringLiteral("point");
            outNetwork.points.append(pt);
        }
    }

    const QJsonArray advCurves = smapObj.value(QStringLiteral("advancedCurveList")).toArray();
    int curveCount = 0;
    for (const auto &curveVal : advCurves) {
        const QJsonObject curveObj = curveVal.toObject();
        ++curveCount;
        RouteSegment seg;
        seg.id = QStringLiteral("b_%1").arg(curveCount);
        seg.name = curveObj.value(QStringLiteral("instanceName")).toString();
        const QString sInst = curveObj.value(QStringLiteral("startPos")).toObject().value(QStringLiteral("instanceName")).toString();
        const QString eInst = curveObj.value(QStringLiteral("endPos")).toObject().value(QStringLiteral("instanceName")).toString();
        seg.startId = instToId.value(sInst, sInst);
        seg.endId   = instToId.value(eInst, eInst);
        seg.direct  = static_cast<int>(RouteDirection::Bidirectional);

        const QJsonObject p0Obj = curveObj.value(QStringLiteral("startPos")).toObject().value(QStringLiteral("pos")).toObject();
        const QJsonObject p1Obj = curveObj.value(QStringLiteral("controlPos1")).toObject();
        const QJsonObject p2Obj = curveObj.value(QStringLiteral("controlPos2")).toObject();
        const QJsonObject p3Obj = curveObj.value(QStringLiteral("endPos")).toObject().value(QStringLiteral("pos")).toObject();

        seg.startPos = QPointF(p0Obj.value(QStringLiteral("x")).toDouble(), p0Obj.value(QStringLiteral("y")).toDouble());
        seg.endPos   = QPointF(p3Obj.value(QStringLiteral("x")).toDouble(), p3Obj.value(QStringLiteral("y")).toDouble());
        seg.controlPts = {
            seg.startPos,
            QPointF(p1Obj.value(QStringLiteral("x")).toDouble(), p1Obj.value(QStringLiteral("y")).toDouble()),
            QPointF(p2Obj.value(QStringLiteral("x")).toDouble(), p2Obj.value(QStringLiteral("y")).toDouble()),
            seg.endPos
        };
        seg.type = QStringLiteral("curve");
        outNetwork.curves.append(seg);
    }
    return true;
}

QJsonObject RouteSmapConverter::exportSmap(const RouteNetwork &network)
{
    constexpr double kOneThirdControlPoint  = 0.333;
    constexpr double kTwoThirdsControlPoint = 0.667;

    QJsonObject smap;
    QJsonObject header;
    header[QStringLiteral("mapName")] = network.mapName.isEmpty() ? QStringLiteral("exported_map") : network.mapName;
    header[QStringLiteral("mapType")] = QStringLiteral("2D-Map");
    header[QStringLiteral("version")] = QStringLiteral("1.0.0");
    smap[QStringLiteral("header")]    = header;

    // 1. Points
    QJsonArray advPoints;
    auto addPoint = [&](const RoutePoint &pt, const QString &cls) {
        QJsonObject p;
        p[QStringLiteral("instanceName")] = pt.name;
        p[QStringLiteral("className")]    = cls;
        QJsonObject pos;
        pos[QStringLiteral("x")]   = pt.x;
        pos[QStringLiteral("y")]   = pt.y;
        pos[QStringLiteral("dir")] = pt.yaw;
        p[QStringLiteral("pos")]   = pos;
        advPoints.append(p);
    };

    for (const auto &pt : network.points)              addPoint(pt, QStringLiteral("LandMark"));
    for (const auto &pt : network.charges)             addPoint(pt, QStringLiteral("ChargePoint"));
    for (const auto &pt : network.highPrecisionPoints) addPoint(pt, QStringLiteral("PrecisionPoint"));
    smap[QStringLiteral("advancedPointList")] = advPoints;

    // 2. Curves (将双向线/曲线自动分解为成对工业贝塞尔)
    QJsonArray advCurves;
    auto addCurve = [&](const QString &name, const QString &sName, const QString &eName,
                        const QPointF &p0, const QPointF &p1, const QPointF &p2, const QPointF &p3) {
        QJsonObject c;
        c[QStringLiteral("instanceName")] = name;
        c[QStringLiteral("className")]    = QStringLiteral("DegenerateBezier");

        QJsonObject sp;
        sp[QStringLiteral("instanceName")] = sName;
        QJsonObject spp;
        spp[QStringLiteral("x")] = p0.x();
        spp[QStringLiteral("y")] = p0.y();
        sp[QStringLiteral("pos")] = spp;
        c[QStringLiteral("startPos")] = sp;

        QJsonObject cp1;
        cp1[QStringLiteral("x")] = p1.x();
        cp1[QStringLiteral("y")] = p1.y();
        c[QStringLiteral("controlPos1")] = cp1;

        QJsonObject cp2;
        cp2[QStringLiteral("x")] = p2.x();
        cp2[QStringLiteral("y")] = p2.y();
        c[QStringLiteral("controlPos2")] = cp2;

        QJsonObject ep;
        ep[QStringLiteral("instanceName")] = eName;
        QJsonObject epp;
        epp[QStringLiteral("x")] = p3.x();
        epp[QStringLiteral("y")] = p3.y();
        ep[QStringLiteral("pos")] = epp;
        c[QStringLiteral("endPos")] = ep;
        advCurves.append(c);
    };

    for (const auto &ln : network.lines) {
        const QString sName = ln.startId;
        const QString eName = ln.endId;
        const QPointF p0 = ln.startPos;
        const QPointF p3 = ln.endPos;
        const QPointF p1 = p0 + (p3 - p0) * kOneThirdControlPoint;
        const QPointF p2 = p0 + (p3 - p0) * kTwoThirdsControlPoint;

        const bool hasFwd = (ln.direct == 0 || ln.direct == 10 || ln.direct == 2);
        const bool hasRev = (ln.direct == 1 || ln.direct == 10 || ln.direct == 2);

        if (hasFwd) {
            addCurve(QStringLiteral("%1-%2").arg(sName, eName), sName, eName, p0, p1, p2, p3);
        }
        if (hasRev) {
            addCurve(QStringLiteral("%1-%2").arg(eName, sName), eName, sName, p3, p2, p1, p0);
        }
    }

    for (const auto &cv : network.curves) {
        const QString sName = cv.startId;
        const QString eName = cv.endId;
        const QPointF p0 = cv.startPos;
        const QPointF p3 = cv.endPos;
        const QPointF p1 = (cv.controlPts.size() >= 4) ? cv.controlPts[1] : (p0 + (p3 - p0) * kOneThirdControlPoint);
        const QPointF p2 = (cv.controlPts.size() >= 4) ? cv.controlPts[2] : (p0 + (p3 - p0) * kTwoThirdsControlPoint);

        const bool hasFwd = (cv.direct == 0 || cv.direct == 10 || cv.direct == 2);
        const bool hasRev = (cv.direct == 1 || cv.direct == 10 || cv.direct == 2);

        if (hasFwd) {
            addCurve(QStringLiteral("%1-%2").arg(sName, eName), sName, eName, p0, p1, p2, p3);
        }
        if (hasRev) {
            addCurve(QStringLiteral("%1-%2").arg(eName, sName), eName, sName, p3, p2, p1, p0);
        }
    }
    smap[QStringLiteral("advancedCurveList")] = advCurves;

    return smap;
}
