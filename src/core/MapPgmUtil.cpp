#include "MapPgmUtil.h"
#include <QStringList>
#include <QFile>
#include <QJsonDocument>
#include <QMap>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <QtGlobal>

namespace {

// 跳过空白与 '#' 注释，返回下一个 token（PGM header 用空白分隔），并将 pos 移到 token 之后
QByteArray nextToken(const QByteArray &data, int &pos)
{
    const int n = data.size();
    while (pos < n) {
        char c = data[pos];
        if (c == '#') {
            while (pos < n && data[pos] != '\n') pos++;
        } else if (std::isspace(static_cast<unsigned char>(c))) {
            pos++;
        } else {
            break;
        }
    }
    int start = pos;
    while (pos < n && !std::isspace(static_cast<unsigned char>(data[pos])) && data[pos] != '#') pos++;
    return data.mid(start, pos - start);
}

struct PgmHeader {
    bool ok = false;
    QByteArray magic;
    int width = 0;
    int height = 0;
    int maxval = 1;
    int dataOffset = 0;
};

PgmHeader parsePgmHeader(const QByteArray &data)
{
    PgmHeader h;
    int pos = 0;
    h.magic = nextToken(data, pos);
    if (h.magic != "P5" && h.magic != "P2") return h;
    h.width = nextToken(data, pos).toInt();
    h.height = nextToken(data, pos).toInt();
    h.maxval = nextToken(data, pos).toInt();
    if (h.width <= 0 || h.height <= 0 || h.maxval <= 0) return h;
    // maxval 之后的单个空白字符即为二进制栅格数据起始位置
    if (pos < data.size() && std::isspace(static_cast<unsigned char>(data[pos]))) pos++;
    h.dataOffset = pos;
    h.ok = true;
    return h;
}

} // namespace

QImage MapPgmUtil::pgmToImage(const QByteArray &pgmData, bool negate)
{
    PgmHeader h = parsePgmHeader(pgmData);
    if (!h.ok) return QImage();

    QImage image(h.width, h.height, QImage::Format_Grayscale8);

    if (h.magic == "P5") {
        const int bytesPerPixel = h.maxval < 256 ? 1 : 2;
        const qint64 needed = qint64(h.width) * h.height * bytesPerPixel;
        if (pgmData.size() - h.dataOffset < needed) return QImage();
        const uchar *src = reinterpret_cast<const uchar*>(pgmData.constData()) + h.dataOffset;
        for (int y = 0; y < h.height; y++) {
            uchar *line = image.scanLine(y);
            for (int x = 0; x < h.width; x++) {
                int v;
                if (bytesPerPixel == 1) {
                    v = src[y * h.width + x];
                } else {
                    int idx = (y * h.width + x) * 2;
                    v = (src[idx] << 8) | src[idx + 1];
                }
                v = v * 255 / h.maxval;
                line[x] = static_cast<uchar>(negate ? 255 - v : v);
            }
        }
    } else { // P2: 文本格式，逐 token 读取灰度值
        int pos = h.dataOffset;
        for (int y = 0; y < h.height; y++) {
            uchar *line = image.scanLine(y);
            for (int x = 0; x < h.width; x++) {
                int v = nextToken(pgmData, pos).toInt() * 255 / h.maxval;
                line[x] = static_cast<uchar>(negate ? 255 - v : v);
            }
        }
    }

    return image;
}

QByteArray MapPgmUtil::imageToPgm(const QImage &image, bool negate)
{
    if (image.isNull()) return QByteArray();
    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);

    QByteArray out = QString("P5\n%1 %2\n255\n").arg(gray.width()).arg(gray.height()).toLatin1();

    QByteArray pixels;
    pixels.resize(gray.width() * gray.height());
    char *dst = pixels.data();
    for (int y = 0; y < gray.height(); y++) {
        const uchar *line = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); x++) {
            uchar v = line[x];
            dst[y * gray.width() + x] = static_cast<char>(negate ? 255 - v : v);
        }
    }
    out.append(pixels);
    return out;
}

bool MapPgmUtil::parseMapYaml(const QString &yamlText, double &resolution,
                               double &originX, double &originY, bool &negate)
{
    resolution = 0.05;
    originX = 0;
    originY = 0;
    negate = false;
    bool sawResolution = false;

    const QStringList lines = yamlText.split('\n');
    for (const QString &raw : lines) {
        QString line = raw.trimmed();
        if (line.startsWith("resolution:")) {
            resolution = line.section(':', 1).trimmed().toDouble();
            sawResolution = true;
        } else if (line.startsWith("origin:")) {
            QString val = line.section(':', 1).trimmed();
            val.remove('[').remove(']');
            QStringList parts = val.split(',', Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                originX = parts[0].trimmed().toDouble();
                originY = parts[1].trimmed().toDouble();
            }
        } else if (line.startsWith("negate:")) {
            negate = line.section(':', 1).trimmed().toInt() != 0;
        }
    }
    return sawResolution;
}

QPointF MapPgmUtil::evaluateBezier(const QList<QPointF> &controlPoints, double t)
{
    int n = controlPoints.size() - 1;
    if (n < 0) return QPointF();
    if (n == 0) return controlPoints[0];

    QList<QPointF> temp = controlPoints;
    for (int r = 1; r <= n; ++r) {
        for (int i = 0; i <= n - r; ++i) {
            temp[i] = (1.0 - t) * temp[i] + t * temp[i + 1];
        }
    }
    return temp[0];
}

QPainterPath MapPgmUtil::getBezierPath(const QList<QPointF> &controlPoints)
{
    QPainterPath path;
    if (controlPoints.isEmpty()) return path;
    path.moveTo(controlPoints.first());
    if (controlPoints.size() < 2) return path;

    int steps = 100;
    for (int i = 1; i <= steps; ++i) {
        double t = double(i) / steps;
        path.lineTo(evaluateBezier(controlPoints, t));
    }
    return path;
}

QList<QPolygonF> MapPgmUtil::createDirectionArrows(const QPointF &mid, const QPointF &dirUnit, int direct, double lineLen)
{
    QList<QPolygonF> result;
    double len = std::hypot(dirUnit.x(), dirUnit.y());
    if (len < 1e-6) return result;
    QPointF d = dirUnit / len;

    double scale = 1.0;
    if (lineLen < 15.0) scale = 0.65;
    else if (lineLen < 25.0) scale = 0.85;

    auto makeArrow = [&](const QPointF &center, const QPointF &forwardDir) {
        QPointF norm(-forwardDir.y(), forwardDir.x());
        QPolygonF poly;
        poly << center + forwardDir * (3.5 * scale)
             << center - forwardDir * (2.2 * scale) + norm * (2.2 * scale)
             << center - forwardDir * (0.8 * scale)
             << center - forwardDir * (2.2 * scale) - norm * (2.2 * scale);
        return poly;
    };

    if (direct == 10 || direct == 2) {
        // 双向通行 (↔)：参考工业级标准，在线段中点两侧对称绘制朝向各自端点的反向箭头对
        double gap = qBound(3.5, lineLen * 0.12, 7.5);
        result.append(makeArrow(mid + d * gap, d));   // 正向前进箭头 →
        result.append(makeArrow(mid - d * gap, -d));  // 反向前进箭头 ←
    } else if (direct == 1) {
        // 单向后退 ←
        result.append(makeArrow(mid, -d));
    } else {
        // 单向前进 →
        result.append(makeArrow(mid, d));
    }
    return result;
}

QList<QPolygonF> MapPgmUtil::createBezierDirectionArrows(const QList<QPointF> &controlPoints, int direct, double curveLen)
{
    QList<QPolygonF> result;
    if (controlPoints.size() < 2) return result;

    double scale = 1.0;
    if (curveLen < 15.0) scale = 0.65;
    else if (curveLen < 25.0) scale = 0.85;

    auto makeArrow = [&](const QPointF &center, const QPointF &forwardDir) {
        double len = std::hypot(forwardDir.x(), forwardDir.y());
        if (len < 1e-6) return QPolygonF();
        QPointF d = forwardDir / len;
        QPointF norm(-d.y(), d.x());
        QPolygonF poly;
        poly << center + d * (3.5 * scale)
             << center - d * (2.2 * scale) + norm * (2.2 * scale)
             << center - d * (0.8 * scale)
             << center - d * (2.2 * scale) - norm * (2.2 * scale);
        return poly;
    };

    if (direct == 10 || direct == 2) {
        // 双向曲线 (↔)：严格沿着贝塞尔曲线的弧线走向，在中部偏前与偏后两个参数点分别取切线，绘制贴合曲线曲率的双向箭头对
        double dt = (curveLen < 25.0) ? 0.03 : 0.05;
        double t1 = 0.50 + dt;
        double t2 = 0.50 - dt;

        QPointF p1 = evaluateBezier(controlPoints, t1);
        QPointF dir1 = evaluateBezier(controlPoints, t1 + 0.02) - evaluateBezier(controlPoints, t1 - 0.02);
        QPolygonF arrow1 = makeArrow(p1, dir1);
        if (!arrow1.isEmpty()) result.append(arrow1);

        QPointF p2 = evaluateBezier(controlPoints, t2);
        QPointF dir2 = evaluateBezier(controlPoints, t2 + 0.02) - evaluateBezier(controlPoints, t2 - 0.02);
        QPolygonF arrow2 = makeArrow(p2, -dir2); // 沿曲线反向朝向起点
        if (!arrow2.isEmpty()) result.append(arrow2);
    } else if (direct == 1) {
        // 单向后退 ← (曲线中点沿切线反向)
        QPointF mid = evaluateBezier(controlPoints, 0.50);
        QPointF dir = evaluateBezier(controlPoints, 0.52) - evaluateBezier(controlPoints, 0.48);
        QPolygonF arrow = makeArrow(mid, -dir);
        if (!arrow.isEmpty()) result.append(arrow);
    } else {
        // 单向前进 → (曲线中点沿切线正向)
        QPointF mid = evaluateBezier(controlPoints, 0.50);
        QPointF dir = evaluateBezier(controlPoints, 0.52) - evaluateBezier(controlPoints, 0.48);
        QPolygonF arrow = makeArrow(mid, dir);
        if (!arrow.isEmpty()) result.append(arrow);
    }
    return result;
}

QPainterPath MapPgmUtil::createPointPath(double pxPerMeterX, double pxPerMeterY,
                                          double *outW, double *outH,
                                          QPainterPath *outCrossPath,
                                          double minWidth)
{
    double scaleX = (pxPerMeterX > 0) ? pxPerMeterX : 20.0;
    double scaleY = (pxPerMeterY > 0) ? pxPerMeterY : 20.0;

    // 真实车辆物理尺寸: 长 0.50m, 宽 0.30m
    double rectW = 0.50 * scaleX;
    double rectH = 0.30 * scaleY;
    if (minWidth > 0 && rectW < minWidth && minWidth <= rectW * 1.25) {
        rectW = minWidth;
    }

    if (outW) *outW = rectW;
    if (outH) *outH = rectH;

    // 锚点 (0, 0) 为十字中心与车辆中心
    QPainterPath rectPath;
    rectPath.addRect(QRectF(-rectW / 2.0, -rectH / 2.0, rectW, rectH));

    if (outCrossPath) {
        QPainterPath crossPath;
        double cw = qBound(1.0, 0.06 * scaleX, rectW * 0.22);
        double ch = qBound(1.0, 0.06 * scaleY, rectH * 0.22);
        crossPath.moveTo(-cw, 0); crossPath.lineTo(cw, 0);
        crossPath.moveTo(0, -ch); crossPath.lineTo(0, ch);
        *outCrossPath = crossPath;
    }

    return rectPath;
}

bool MapPgmUtil::parseSmapFile(const QString &filePath,
                              QJsonArray &outPoints,
                              QJsonArray &outLines,
                              QJsonArray &outCurves,
                              QString &outMapName,
                              QString *outError,
                              QJsonObject *outHeader)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (outError) *outError = QString("无法打开文件: %1").arg(file.errorString());
        return false;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (outError) *outError = QString("JSON 解析失败: %1").arg(parseErr.errorString());
        return false;
    }

    return parseSmapJson(doc.object(), outPoints, outLines, outCurves, outMapName, outError, outHeader);
}

bool MapPgmUtil::parseSmapJson(const QJsonObject &smapObj,
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

    // 1. Header
    QJsonObject header = smapObj.value("header").toObject();
    outMapName = header.value("mapName").toString();
    if (outHeader) {
        *outHeader = header;
    }

    // 2. advancedPointList (LocationMark / Stations / Charges / Precision)
    QJsonArray advPoints = smapObj.value("advancedPointList").toArray();
    QMap<QString, QString> instToIdMap;  // "LM1" -> "p_1"
    QMap<QString, QPointF> instToPosMap; // "LM1" -> QPointF(x, y)

    int pointCount = 0;
    for (const auto &pv : advPoints) {
        QJsonObject pt = pv.toObject();
        QString instName = pt.value("instanceName").toString().trimmed();
        if (instName.isEmpty()) {
            instName = QString("P%1").arg(pointCount + 1);
        }
        pointCount++;
        QString pid = QString("p_%1").arg(pointCount);
        instToIdMap[instName] = pid;

        QJsonObject posObj = pt.value("pos").toObject();
        double x = posObj.value("x").toDouble(0.0);
        double y = posObj.value("y").toDouble(0.0);
        instToPosMap[instName] = QPointF(x, y);

        QString className = pt.value("className").toString();
        QString type = "point";
        if (className.contains("Charge", Qt::CaseInsensitive) || instName.startsWith("CP", Qt::CaseInsensitive)) {
            type = "charge";
        } else if (className.contains("Precision", Qt::CaseInsensitive) || className.contains("Transfer", Qt::CaseInsensitive) || instName.startsWith("TL", Qt::CaseInsensitive)) {
            type = "highPrecision";
        }

        double yaw = 0.0;
        if (pt.contains("dir")) yaw = pt.value("dir").toDouble();
        else if (posObj.contains("dir")) yaw = posObj.value("dir").toDouble();
        else if (pt.contains("yaw")) yaw = pt.value("yaw").toDouble();

        QJsonObject pObj;
        pObj["id"]       = pid;
        pObj["name"]     = instName;
        pObj["type"]     = type;
        pObj["x"]        = x;
        pObj["y"]        = y;
        pObj["yaw"]      = yaw;
        pObj["isyawfix"] = false;
        pObj["taskid"]   = 0;
        outPoints.append(pObj);
    }

    // 3. advancedCurveList (DegenerateBezier / Paths)
    // 提取并自动识别对称的反向曲线合并为单条双向曲线 (direct = 10)
    QJsonArray advCurves = smapObj.value("advancedCurveList").toArray();
    struct ParsedCurve {
        QString name;
        QString startInst;
        QString endInst;
        QList<QPointF> ctrlPts; // 4 points for cubic bezier
    };
    QList<ParsedCurve> rawCurves;

    for (const auto &cv : advCurves) {
        QJsonObject cObj = cv.toObject();
        QString instName = cObj.value("instanceName").toString();
        QJsonObject startPos = cObj.value("startPos").toObject();
        QJsonObject endPos   = cObj.value("endPos").toObject();
        QString startInst    = startPos.value("instanceName").toString().trimmed();
        QString endInst      = endPos.value("instanceName").toString().trimmed();

        QJsonObject p0Obj = startPos.value("pos").toObject();
        QJsonObject p1Obj = cObj.value("controlPos1").toObject();
        QJsonObject p2Obj = cObj.value("controlPos2").toObject();
        QJsonObject p3Obj = endPos.value("pos").toObject();

        QPointF p0(p0Obj.value("x").toDouble(0.0), p0Obj.value("y").toDouble(0.0));
        QPointF p1(p1Obj.value("x").toDouble(0.0), p1Obj.value("y").toDouble(0.0));
        QPointF p2(p2Obj.value("x").toDouble(0.0), p2Obj.value("y").toDouble(0.0));
        QPointF p3(p3Obj.value("x").toDouble(0.0), p3Obj.value("y").toDouble(0.0));

        // 如果节点名在 instToPosMap 中，校准端点坐标为节点坐标
        if (instToPosMap.contains(startInst)) p0 = instToPosMap[startInst];
        if (instToPosMap.contains(endInst))   p3 = instToPosMap[endInst];

        ParsedCurve pc;
        pc.name      = instName;
        pc.startInst = startInst;
        pc.endInst   = endInst;
        pc.ctrlPts   = {p0, p1, p2, p3};
        rawCurves.append(pc);
    }

    // 智能去重/合并对称反向曲线
    QList<ParsedCurve> mergedCurves;
    for (const auto &rc : rawCurves) {
        bool isReverseOfExisting = false;
        for (const auto &mc : mergedCurves) {
            if (rc.startInst == mc.endInst && rc.endInst == mc.startInst) {
                const double tol = 0.05; // 5cm
                if (rc.ctrlPts.size() == 4 && mc.ctrlPts.size() == 4) {
                    if (std::hypot(rc.ctrlPts[0].x() - mc.ctrlPts[3].x(), rc.ctrlPts[0].y() - mc.ctrlPts[3].y()) < tol &&
                        std::hypot(rc.ctrlPts[1].x() - mc.ctrlPts[2].x(), rc.ctrlPts[1].y() - mc.ctrlPts[2].y()) < tol &&
                        std::hypot(rc.ctrlPts[2].x() - mc.ctrlPts[1].x(), rc.ctrlPts[2].y() - mc.ctrlPts[1].y()) < tol &&
                        std::hypot(rc.ctrlPts[3].x() - mc.ctrlPts[0].x(), rc.ctrlPts[3].y() - mc.ctrlPts[0].y()) < tol) {
                        isReverseOfExisting = true;
                        break;
                    }
                }
            }
        }
        if (!isReverseOfExisting) {
            mergedCurves.append(rc);
        }
    }

    int curveCount = 0;
    for (const auto &mc : mergedCurves) {
        curveCount++;
        QString cid = QString("b_%1").arg(curveCount);
        QString startId = instToIdMap.value(mc.startInst);
        QString endId   = instToIdMap.value(mc.endInst);

        QJsonObject curveObj;
        curveObj["id"]       = cid;
        curveObj["name"]     = mc.name.isEmpty() ? QString("曲%1").arg(curveCount) : mc.name;
        curveObj["startid"]  = startId;
        curveObj["endid"]    = endId;
        curveObj["direct"]   = 10; // 双向 ↔ (根据需求：任何直线和曲线都是双向的)
        curveObj["maxspeed"] = 8.0;
        curveObj["minspeed"] = 0.0;
        curveObj["slowdis"]  = 2.0;

        QJsonArray ctrlArr;
        for (const auto &pt : mc.ctrlPts) {
            QJsonObject p;
            p["x"] = pt.x();
            p["y"] = pt.y();
            ctrlArr.append(p);
        }
        curveObj["controlPoints"] = ctrlArr;

        if (!mc.ctrlPts.isEmpty()) {
            curveObj["x1"] = mc.ctrlPts.first().x();
            curveObj["y1"] = mc.ctrlPts.first().y();
            curveObj["x2"] = mc.ctrlPts.last().x();
            curveObj["y2"] = mc.ctrlPts.last().y();
            if (mc.ctrlPts.size() == 3) {
                curveObj["cx"] = mc.ctrlPts[1].x();
                curveObj["cy"] = mc.ctrlPts[1].y();
            }
        }
        outCurves.append(curveObj);
    }

    return true;
}



