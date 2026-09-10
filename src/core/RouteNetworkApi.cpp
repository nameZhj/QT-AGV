#include "RouteNetworkApi.h"
#include "ApiService.h"
#include "MapPgmUtil.h"
#include <cmath>
#include <QDebug>

// ============================================================================
//  RoutePoint 序列化与反序列化
// ============================================================================

QJsonObject RoutePoint::toJson() const
{
    QJsonObject obj;
    obj["id"]          = id;
    obj["name"]        = name;
    obj["type"]        = type;
    obj["x"]           = x;
    obj["y"]           = y;
    obj["yaw"]         = yaw;
    obj["rdsDockable"] = rdsDockable;
    obj["allowRotate"] = allowRotate;
    obj["useDownPGV"]  = useDownPGV;
    obj["epc"]         = epc;
    obj["isyawfix"]    = isyawfix;
    obj["taskid"]      = taskid;

    // 工业级扩展字段
    if (ignoreDir) obj["ignoreDir"] = true;
    if (!label.isEmpty()) obj["label"] = label;
    if (waitTimeout > 0.0) obj["waitTimeout"] = waitTimeout;

    return obj;
}

RoutePoint RoutePoint::fromJson(const QJsonObject &obj)
{
    RoutePoint pt;
    pt.id          = obj.value("id").toString();
    pt.name        = obj.value("name").toString();
    pt.type        = obj.value("type").toString("point");
    pt.x           = obj.value("x").toDouble(0.0);
    pt.y           = obj.value("y").toDouble(0.0);
    pt.yaw         = obj.value("yaw").toDouble(0.0);
    pt.rdsDockable = obj.value("rdsDockable").toBool(false);
    pt.allowRotate = obj.value("allowRotate").toBool(false);
    pt.useDownPGV  = obj.value("useDownPGV").toBool(false);
    pt.epc         = obj.value("epc").toString();
    pt.isyawfix    = obj.value("isyawfix").toBool(false);
    pt.taskid      = obj.value("taskid").toInt(0);

    pt.ignoreDir   = obj.value("ignoreDir").toBool(false);
    pt.label       = obj.value("label").toString();
    pt.waitTimeout = obj.value("waitTimeout").toDouble(0.0);

    return pt;
}

// ============================================================================
//  RouteSegment 序列化与反序列化
// ============================================================================

QJsonObject RouteSegment::toJson() const
{
    QJsonObject obj;
    obj["id"]          = id;
    obj["name"]        = name;
    obj["type"]        = type;
    obj["sId"]         = startId;
    obj["eId"]         = endId;
    obj["startid"]     = startId; // 兼容旧接口别名
    obj["endid"]       = endId;
    obj["direct"]      = direct;
    obj["direction"]   = direct;  // 兼容工业命名

    obj["x1"]          = startPos.x();
    obj["y1"]          = startPos.y();
    obj["x2"]          = endPos.x();
    obj["y2"]          = endPos.y();

    if (type == "curve" || !controlPts.isEmpty()) {
        QJsonArray ctrlArr;
        for (const auto &p : controlPts) {
            QJsonObject cp;
            cp["x"] = p.x();
            cp["y"] = p.y();
            ctrlArr.append(cp);
        }
        obj["controlPoints"] = ctrlArr;
        if (controlPts.size() == 3) {
            obj["cx"] = controlPts[1].x();
            obj["cy"] = controlPts[1].y();
        }
    }

    // 动力学与控制参数
    obj["maxSpeed"]    = maxSpeed;
    obj["maxspeed"]    = maxSpeed;
    obj["minSpeed"]    = minSpeed;
    obj["minspeed"]    = minSpeed;
    obj["maxAcc"]      = maxAcc;
    obj["maxacc"]      = maxAcc;
    obj["maxDec"]      = maxDec;
    obj["maxdec"]      = maxDec;
    obj["maxRotSpeed"] = maxRotSpeed;
    obj["maxrot"]      = maxRotSpeed;
    obj["maxRotAcc"]   = maxRotAcc;
    obj["maxrotacc"]   = maxRotAcc;
    obj["reachDist"]   = reachDist;
    obj["reachAngle"]  = reachAngle;
    obj["roadWidth"]   = roadWidth;
    obj["width"]       = roadWidth;

    // 工业调度扩展字段
    obj["moveStyle"]     = moveStyle;
    obj["locked"]        = locked;
    obj["weight"]        = weight;
    obj["priority"]      = priority;
    obj["spinTurn"]      = spinTurn;
    obj["avoidObstacle"] = avoidObstacle;

    return obj;
}

RouteSegment RouteSegment::fromJson(const QJsonObject &obj)
{
    RouteSegment seg;
    seg.id      = obj.value("id").toString();
    seg.name    = obj.value("name").toString();
    seg.type    = obj.value("type").toString(seg.id.startsWith("b_") ? "curve" : "line");
    seg.startId = obj.contains("sId") ? obj.value("sId").toString() : obj.value("startid").toString();
    seg.endId   = obj.contains("eId") ? obj.value("eId").toString() : obj.value("endid").toString();
    seg.direct  = obj.value("direct").toInt(obj.value("direction").toInt(10));

    seg.startPos = QPointF(obj.value("x1").toDouble(0.0), obj.value("y1").toDouble(0.0));
    seg.endPos   = QPointF(obj.value("x2").toDouble(0.0), obj.value("y2").toDouble(0.0));

    if (obj.contains("controlPoints")) {
        QJsonArray arr = obj.value("controlPoints").toArray();
        for (const auto &v : arr) {
            QJsonObject p = v.toObject();
            seg.controlPts.append(QPointF(p.value("x").toDouble(), p.value("y").toDouble()));
        }
    } else if (obj.contains("cx") && obj.contains("cy")) {
        seg.controlPts.append(seg.startPos);
        seg.controlPts.append(QPointF(obj.value("cx").toDouble(), obj.value("cy").toDouble()));
        seg.controlPts.append(seg.endPos);
    }

    seg.maxSpeed    = obj.contains("maxSpeed") ? obj.value("maxSpeed").toDouble(0.5) : obj.value("maxspeed").toDouble(0.5);
    seg.minSpeed    = obj.contains("minSpeed") ? obj.value("minSpeed").toDouble(0.0) : obj.value("minspeed").toDouble(0.0);
    seg.maxAcc      = obj.contains("maxAcc") ? obj.value("maxAcc").toDouble(0.3) : obj.value("maxacc").toDouble(0.3);
    seg.maxDec      = obj.contains("maxDec") ? obj.value("maxDec").toDouble(0.3) : obj.value("maxdec").toDouble(0.3);
    seg.maxRotSpeed = obj.contains("maxRotSpeed") ? obj.value("maxRotSpeed").toDouble(60.0) : obj.value("maxrot").toDouble(60.0);
    seg.maxRotAcc   = obj.contains("maxRotAcc") ? obj.value("maxRotAcc").toDouble(45.0) : obj.value("maxrotacc").toDouble(45.0);
    seg.reachDist   = obj.value("reachDist").toDouble(0.05);
    seg.reachAngle  = obj.value("reachAngle").toDouble(2.0);
    seg.roadWidth   = obj.contains("roadWidth") ? obj.value("roadWidth").toDouble(1.0) : obj.value("width").toDouble(1.0);

    seg.moveStyle     = obj.value("moveStyle").toInt(obj.value("movestyle").toInt(0));
    seg.locked        = obj.value("locked").toBool(false);
    seg.weight        = obj.value("weight").toDouble(1.0);
    seg.priority       = obj.value("priority").toInt(0);
    seg.spinTurn      = obj.value("spinTurn").toBool(true);
    seg.avoidObstacle = obj.value("avoidObstacle").toBool(true);

    return seg;
}

// ============================================================================
//  RouteNetworkApi 数据解析与生成
// ============================================================================

RouteNetwork RouteNetworkApi::fromGeoJson(const QJsonObject &root)
{
    RouteNetwork net;
    net.mapId   = root.value("mapId").toString();
    net.mapName = root.value("mapName").toString();

    // 1. 兼容扁平格式
    if (!root.contains("features") && (root.contains("points") || root.contains("lines") || root.contains("curves"))) {
        for (const auto &v : root.value("points").toArray())              net.points.append(RoutePoint::fromJson(v.toObject()));
        for (const auto &v : root.value("charges").toArray())             net.charges.append(RoutePoint::fromJson(v.toObject()));
        for (const auto &v : root.value("highPrecisionPoints").toArray()) net.highPrecisionPoints.append(RoutePoint::fromJson(v.toObject()));
        for (const auto &v : root.value("lines").toArray())               net.lines.append(RouteSegment::fromJson(v.toObject()));
        for (const auto &v : root.value("curves").toArray())              net.curves.append(RouteSegment::fromJson(v.toObject()));
        return net;
    }

    // 2. 标准 GeoJSON FeatureCollection
    QJsonArray features = root.value("features").toArray();
    for (const auto &fv : features) {
        QJsonObject f     = fv.toObject();
        QJsonObject props = f.value("properties").toObject();
        QJsonObject geom  = f.value("geometry").toObject();
        QString gtype     = geom.value("type").toString();
        QString id        = props.value("id").toString();
        QJsonArray coords = geom.value("coordinates").toArray();

        if (gtype == "Point") {
            double x = (coords.size() > 0) ? coords[0].toDouble() : 0.0;
            double y = (coords.size() > 1) ? coords[1].toDouble() : 0.0;
            QJsonObject fullObj = props;
            fullObj["x"] = x; fullObj["y"] = y;
            RoutePoint pt = RoutePoint::fromJson(fullObj);

            if (id.startsWith("c_") || pt.type == "charge")
                net.charges.append(pt);
            else if (id.startsWith("hp_") || pt.type == "highPrecision")
                net.highPrecisionPoints.append(pt);
            else
                net.points.append(pt);
        } else if (gtype == "LineString") {
            QJsonObject fullObj = props;
            if (coords.size() >= 2) {
                QJsonArray c0 = coords.first().toArray();
                QJsonArray c1 = coords.last().toArray();
                fullObj["x1"] = c0[0].toDouble(); fullObj["y1"] = c0[1].toDouble();
                fullObj["x2"] = c1[0].toDouble(); fullObj["y2"] = c1[1].toDouble();
            }
            net.lines.append(RouteSegment::fromJson(fullObj));
        } else if (gtype == "MultiPoint") {
            QJsonObject fullObj = props;
            if (coords.size() >= 2) {
                QJsonArray ctrlArr;
                for (const auto &cv : coords) {
                    QJsonArray c = cv.toArray();
                    if (c.size() < 2) continue;
                    QJsonObject p;
                    p["x"] = c[0].toDouble(); p["y"] = c[1].toDouble();
                    ctrlArr.append(p);
                }
                fullObj["controlPoints"] = ctrlArr;
                QJsonArray c0 = coords.first().toArray();
                QJsonArray cN = coords.last().toArray();
                fullObj["x1"] = c0[0].toDouble(); fullObj["y1"] = c0[1].toDouble();
                fullObj["x2"] = cN[0].toDouble(); fullObj["y2"] = cN[1].toDouble();
            }
            net.curves.append(RouteSegment::fromJson(fullObj));
        }
    }
    return net;
}

QJsonObject RouteNetworkApi::toGeoJson(const RouteNetwork &network)
{
    QJsonArray features;

    auto makePointFeat = [](const RoutePoint &pt) {
        QJsonArray coords;
        coords.append(pt.x); coords.append(pt.y); coords.append(0.0);
        QJsonObject geom;
        geom["type"] = "Point";
        geom["coordinates"] = coords;

        QJsonObject props = pt.toJson();
        props.remove("x"); props.remove("y");

        QJsonObject f;
        f["type"] = "Feature";
        f["geometry"] = geom;
        f["properties"] = props;
        return f;
    };

    for (const auto &pt : network.points)              features.append(makePointFeat(pt));
    for (const auto &pt : network.charges)             features.append(makePointFeat(pt));
    for (const auto &pt : network.highPrecisionPoints) features.append(makePointFeat(pt));

    for (const auto &ln : network.lines) {
        QJsonArray ca, cb;
        ca.append(ln.startPos.x()); ca.append(ln.startPos.y()); ca.append(0.0);
        cb.append(ln.endPos.x());   cb.append(ln.endPos.y());   cb.append(0.0);
        QJsonArray coords; coords.append(ca); coords.append(cb);

        QJsonObject geom;
        geom["type"] = "LineString";
        geom["coordinates"] = coords;

        QJsonObject props = ln.toJson();
        props.remove("x1"); props.remove("y1"); props.remove("x2"); props.remove("y2");

        QJsonObject f;
        f["type"] = "Feature";
        f["geometry"] = geom;
        f["properties"] = props;
        features.append(f);
    }

    for (const auto &cv : network.curves) {
        QJsonArray coords;
        if (!cv.controlPts.isEmpty()) {
            for (const auto &p : cv.controlPts) {
                QJsonArray c;
                c.append(p.x()); c.append(p.y()); c.append(0.0);
                coords.append(c);
            }
        } else {
            QJsonArray c0, c1;
            c0.append(cv.startPos.x()); c0.append(cv.startPos.y()); c0.append(0.0);
            c1.append(cv.endPos.x());   c1.append(cv.endPos.y());   c1.append(0.0);
            coords.append(c0); coords.append(c1);
        }

        QJsonObject geom;
        geom["type"] = "MultiPoint";
        geom["coordinates"] = coords;

        QJsonObject props = cv.toJson();
        props.remove("x1"); props.remove("y1"); props.remove("x2"); props.remove("y2");
        props.remove("controlPoints");

        QJsonObject f;
        f["type"] = "Feature";
        f["geometry"] = geom;
        f["properties"] = props;
        features.append(f);
    }

    QJsonObject root;
    root["type"]     = "FeatureCollection";
    root["mapId"]    = network.mapId;
    root["mapName"]  = network.mapName;
    root["features"] = features;
    return root;
}

// ============================================================================
//  拓扑有向图展开算法 (双向自动展开为成对有向边)
// ============================================================================

QList<DirectedEdge> RouteNetworkApi::toDirectedGraph(const RouteNetwork &network)
{
    QList<DirectedEdge> graph;

    auto processSegment = [&](const RouteSegment &seg) {
        if (seg.startId.isEmpty() || seg.endId.isEmpty()) return;

        // 计算物理几何长度及插值采样点
        double len = 0.0;
        QList<QPointF> forwardPts;
        if (seg.type == "curve" && seg.controlPts.size() >= 2) {
            const int sampleCount = 30;
            QPointF prev = seg.controlPts.first();
            forwardPts.append(prev);
            for (int i = 1; i <= sampleCount; ++i) {
                double t = double(i) / sampleCount;
                QPointF curr = MapPgmUtil::evaluateBezier(seg.controlPts, t);
                len += std::hypot(curr.x() - prev.x(), curr.y() - prev.y());
                forwardPts.append(curr);
                prev = curr;
            }
        } else {
            len = std::hypot(seg.endPos.x() - seg.startPos.x(), seg.endPos.y() - seg.startPos.y());
            forwardPts = {seg.startPos, seg.endPos};
        }

        bool hasForward = (seg.direct == 0 || seg.direct == 10 || seg.direct == 2);
        bool hasBackward = (seg.direct == 1 || seg.direct == 10 || seg.direct == 2);

        // 正向有向边: startId -> endId
        if (hasForward) {
            DirectedEdge ef;
            ef.edgeId         = seg.id + "_fwd";
            ef.sourcePointId  = seg.startId;
            ef.targetPointId  = seg.endId;
            ef.lengthMeters   = len;
            ef.maxSpeed       = seg.maxSpeed;
            ef.weight         = seg.weight;
            ef.isReverse      = false;
            ef.pathPoints     = forwardPts;
            graph.append(ef);
        }

        // 反向有向边: endId -> startId
        if (hasBackward) {
            DirectedEdge eb;
            eb.edgeId         = seg.id + "_rev";
            eb.sourcePointId  = seg.endId;
            eb.targetPointId  = seg.startId;
            eb.lengthMeters   = len;
            eb.maxSpeed       = seg.maxSpeed;
            eb.weight         = seg.weight;
            eb.isReverse      = true;
            for (int i = forwardPts.size() - 1; i >= 0; --i) {
                eb.pathPoints.append(forwardPts[i]);
            }
            graph.append(eb);
        }
    };

    for (const auto &ln : network.lines)  processSegment(ln);
    for (const auto &cv : network.curves) processSegment(cv);

    return graph;
}

// ============================================================================
//  工业级 .smap 地图格式互转
// ============================================================================

bool RouteNetworkApi::parseSmap(const QJsonObject &smapObj, RouteNetwork &outNetwork)
{
    outNetwork.clear();
    outNetwork.mapName = smapObj.value("header").toObject().value("mapName").toString();

    QJsonArray outPoints, outLines, outCurves;
    QString mapName;
    // MapPgmUtil 中的核心 smap 提取
    // 写入临时文件或直接解析
    QJsonArray advPoints = smapObj.value("advancedPointList").toArray();
    QMap<QString, QString> instToId;
    int pCount = 0;
    for (const auto &pv : advPoints) {
        QJsonObject o = pv.toObject();
        QString inst = o.value("instanceName").toString().trimmed();
        pCount++;
        QString pid = QString("p_%1").arg(pCount);
        instToId[inst] = pid;

        RoutePoint pt;
        pt.id = pid;
        pt.name = inst.isEmpty() ? QString("P%1").arg(pCount) : inst;
        QJsonObject pos = o.value("pos").toObject();
        pt.x = pos.value("x").toDouble(0.0);
        pt.y = pos.value("y").toDouble(0.0);
        pt.yaw = pos.value("dir").toDouble(0.0);

        QString cls = o.value("className").toString();
        if (cls.contains("Charge", Qt::CaseInsensitive) || inst.startsWith("CP", Qt::CaseInsensitive)) {
            pt.type = "charge";
            outNetwork.charges.append(pt);
        } else if (cls.contains("Precision", Qt::CaseInsensitive) || inst.startsWith("TL", Qt::CaseInsensitive)) {
            pt.type = "highPrecision";
            outNetwork.highPrecisionPoints.append(pt);
        } else {
            pt.type = "point";
            outNetwork.points.append(pt);
        }
    }

    QJsonArray advCurves = smapObj.value("advancedCurveList").toArray();
    int cCount = 0;
    for (const auto &cv : advCurves) {
        QJsonObject o = cv.toObject();
        cCount++;
        RouteSegment seg;
        seg.id = QString("b_%1").arg(cCount);
        seg.name = o.value("instanceName").toString();
        QString sInst = o.value("startPos").toObject().value("instanceName").toString();
        QString eInst = o.value("endPos").toObject().value("instanceName").toString();
        seg.startId = instToId.value(sInst, sInst);
        seg.endId   = instToId.value(eInst, eInst);
        seg.direct  = 10; // 双向

        QJsonObject p0 = o.value("startPos").toObject().value("pos").toObject();
        QJsonObject p1 = o.value("controlPos1").toObject();
        QJsonObject p2 = o.value("controlPos2").toObject();
        QJsonObject p3 = o.value("endPos").toObject().value("pos").toObject();

        seg.startPos = QPointF(p0.value("x").toDouble(), p0.value("y").toDouble());
        seg.endPos   = QPointF(p3.value("x").toDouble(), p3.value("y").toDouble());
        seg.controlPts = {
            seg.startPos,
            QPointF(p1.value("x").toDouble(), p1.value("y").toDouble()),
            QPointF(p2.value("x").toDouble(), p2.value("y").toDouble()),
            seg.endPos
        };
        seg.type = "curve";
        outNetwork.curves.append(seg);
    }
    return true;
}

QJsonObject RouteNetworkApi::exportSmap(const RouteNetwork &network)
{
    QJsonObject smap;
    QJsonObject header;
    header["mapName"] = network.mapName.isEmpty() ? "exported_map" : network.mapName;
    header["mapType"] = "2D-Map";
    header["version"] = "1.0.0";
    smap["header"] = header;

    // 1. Points
    QJsonArray advPoints;
    auto addPoint = [&](const RoutePoint &pt, const QString &cls) {
        QJsonObject p;
        p["instanceName"] = pt.name;
        p["className"]    = cls;
        QJsonObject pos;
        pos["x"] = pt.x; pos["y"] = pt.y; pos["dir"] = pt.yaw;
        p["pos"] = pos;
        advPoints.append(p);
    };

    for (const auto &pt : network.points)              addPoint(pt, "LandMark");
    for (const auto &pt : network.charges)             addPoint(pt, "ChargePoint");
    for (const auto &pt : network.highPrecisionPoints) addPoint(pt, "PrecisionPoint");
    smap["advancedPointList"] = advPoints;

    // 2. Curves (将双向线/曲线自动分解为成对工业贝塞尔)
    QJsonArray advCurves;
    auto addCurve = [&](const QString &name, const QString &sName, const QString &eName,
                        const QPointF &p0, const QPointF &p1, const QPointF &p2, const QPointF &p3) {
        QJsonObject c;
        c["instanceName"] = name;
        c["className"]    = "DegenerateBezier";
        QJsonObject sp; sp["instanceName"] = sName;
        QJsonObject spp; spp["x"] = p0.x(); spp["y"] = p0.y(); sp["pos"] = spp;
        c["startPos"] = sp;

        QJsonObject cp1; cp1["x"] = p1.x(); cp1["y"] = p1.y(); c["controlPos1"] = cp1;
        QJsonObject cp2; cp2["x"] = p2.x(); cp2["y"] = p2.y(); c["controlPos2"] = cp2;

        QJsonObject ep; ep["instanceName"] = eName;
        QJsonObject epp; epp["x"] = p3.x(); epp["y"] = p3.y(); ep["pos"] = epp;
        c["endPos"] = ep;
        advCurves.append(c);
    };

    int cvIdx = 0;
    for (const auto &ln : network.lines) {
        cvIdx++;
        QString sName = ln.startId;
        QString eName = ln.endId;
        QPointF p0 = ln.startPos;
        QPointF p3 = ln.endPos;
        QPointF p1 = p0 + (p3 - p0) * 0.333;
        QPointF p2 = p0 + (p3 - p0) * 0.667;

        bool hasFwd = (ln.direct == 0 || ln.direct == 10 || ln.direct == 2);
        bool hasRev = (ln.direct == 1 || ln.direct == 10 || ln.direct == 2);

        if (hasFwd) {
            addCurve(QString("%1-%2").arg(sName, eName), sName, eName, p0, p1, p2, p3);
        }
        if (hasRev) {
            addCurve(QString("%1-%2").arg(eName, sName), eName, sName, p3, p2, p1, p0);
        }
    }

    for (const auto &cv : network.curves) {
        cvIdx++;
        QString sName = cv.startId;
        QString eName = cv.endId;
        QPointF p0 = cv.startPos;
        QPointF p3 = cv.endPos;
        QPointF p1 = (cv.controlPts.size() >= 4) ? cv.controlPts[1] : (p0 + (p3 - p0) * 0.333);
        QPointF p2 = (cv.controlPts.size() >= 4) ? cv.controlPts[2] : (p0 + (p3 - p0) * 0.667);

        bool hasFwd = (cv.direct == 0 || cv.direct == 10 || cv.direct == 2);
        bool hasRev = (cv.direct == 1 || cv.direct == 10 || cv.direct == 2);

        if (hasFwd) {
            addCurve(QString("%1-%2").arg(sName, eName), sName, eName, p0, p1, p2, p3);
        }
        if (hasRev) {
            addCurve(QString("%1-%2").arg(eName, sName), eName, sName, p3, p2, p1, p0);
        }
    }
    smap["advancedCurveList"] = advCurves;

    return smap;
}

// ============================================================================
//  路网拓扑校验
// ============================================================================

bool RouteNetworkApi::validate(const RouteNetwork &network, QStringList &warnings, QStringList &errors)
{
    QSet<QString> pointIds;
    for (const auto &p : network.points)              pointIds.insert(p.id);
    for (const auto &p : network.charges)             pointIds.insert(p.id);
    for (const auto &p : network.highPrecisionPoints) pointIds.insert(p.id);

    // 检查点集是否为空
    if (pointIds.isEmpty()) {
        errors.append("路网中无任何有效站点 (Point/Charge/HighPrecision)");
    }

    // 检查线路关联关系
    auto checkSeg = [&](const RouteSegment &seg) {
        if (seg.startId.isEmpty() || seg.endId.isEmpty()) {
            errors.append(QString("线路 %1 未指定起终点").arg(seg.id));
        } else {
            if (!pointIds.contains(seg.startId)) {
                warnings.append(QString("线路 %1 的起点 %2 不在站点列表中").arg(seg.id, seg.startId));
            }
            if (!pointIds.contains(seg.endId)) {
                warnings.append(QString("线路 %1 的终点 %2 不在站点列表中").arg(seg.id, seg.endId));
            }
        }
        if (seg.maxSpeed <= 0.0) {
            warnings.append(QString("线路 %1 最大速度为 0 或负数").arg(seg.id));
        }
    };

    for (const auto &ln : network.lines)  checkSeg(ln);
    for (const auto &cv : network.curves) checkSeg(cv);

    return errors.isEmpty();
}

// ============================================================================
//  异步网络服务 API
// ============================================================================

void RouteNetworkApi::loadNetwork(const QString &mapId, LoadCallback cb)
{
    QJsonObject req;
    req["mapId"] = mapId;

    ApiService::instance().loadEditedMap(req, [cb](int code, const QJsonObject &resp) {
        if (code != 200) {
            if (cb) cb(false, RouteNetwork(), "HTTP 请求失败或接口异常");
            return;
        }
        QString jsonStr = resp.value("json").toString();
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (!doc.isObject()) {
            if (cb) cb(false, RouteNetwork(), "解析返回路网 JSON 数据失败");
            return;
        }
        RouteNetwork net = fromGeoJson(doc.object());
        if (cb) cb(true, net, QString());
    });
}

void RouteNetworkApi::saveNetwork(const QString &mapId, const QString &mapName, const RouteNetwork &network, SaveCallback cb)
{
    RouteNetwork net = network;
    net.mapId = mapId;
    net.mapName = mapName;

    QJsonObject geojson = toGeoJson(net);
    ApiService::instance().saveEditedMap(geojson, [cb](int code, const QJsonObject &resp) {
        if (code == 200) {
            if (cb) cb(true, "路网保存成功");
        } else {
            QString msg = resp.value("msg").toString("保存路网失败");
            if (cb) cb(false, msg);
        }
    });
}
