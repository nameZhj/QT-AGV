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

// ── 内部常量定义 ──
constexpr double kArrowLengthScale           = 3.5;
constexpr double kArrowWingScale             = 2.2;
constexpr double kArrowNotchScale            = 0.8;
constexpr double kMinTangentMagnitudeSquared = 1e-12;
constexpr int    kDefaultPgmMaxVal           = 255;

// 跳过空白与 '#' 注释，返回下一个 token（PGM header 采用标准空白字符分隔）
QByteArray nextToken(const QByteArray &data, int &pos)
{
    const int n = data.size();
    while (pos < n) {
        const char c = data[pos];
        if (c == '#') {
            while (pos < n && data[pos] != '\n') {
                pos++;
            }
        } else if (std::isspace(static_cast<unsigned char>(c))) {
            pos++;
        } else {
            break;
        }
    }
    const int start = pos;
    while (pos < n && !std::isspace(static_cast<unsigned char>(data[pos])) && data[pos] != '#') {
        pos++;
    }
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

    // maxval 之后的单个空白字符即为二进制栅格数据起始点
    if (pos < data.size() && std::isspace(static_cast<unsigned char>(data[pos]))) {
        pos++;
    }
    h.dataOffset = pos;
    h.ok = true;
    return h;
}

/**
 * @brief 构建流线型燕尾内凹指示箭头多边形
 */
QPolygonF buildArrowPolygon(const QPointF &center, const QPointF &dirUnit, double scale)
{
    const QPointF norm(-dirUnit.y(), dirUnit.x());
    QPolygonF poly;
    poly << center + dirUnit * (kArrowLengthScale * scale)
         << center - dirUnit * (kArrowWingScale * scale) + norm * (kArrowWingScale * scale)
         << center - dirUnit * (kArrowNotchScale * scale)
         << center - dirUnit * (kArrowWingScale * scale) - norm * (kArrowWingScale * scale);
    return poly;
}

/**
 * @brief 判定是否为双向通行模式
 */
inline bool isBidirectional(int direct)
{
    return (direct == static_cast<int>(MapPgmUtil::Direction::Bidirectional) ||
            direct == static_cast<int>(MapPgmUtil::Direction::BidirectionalAlt));
}

} // anonymous namespace

// ── 1. PGM 图像编解码 ──

QImage MapPgmUtil::pgmToImage(const QByteArray &pgmData, bool negate)
{
    const PgmHeader h = parsePgmHeader(pgmData);
    if (!h.ok) return QImage();

    QImage image(h.width, h.height, QImage::Format_Grayscale8);
    if (image.isNull()) return QImage();

    if (h.magic == "P5") {
        const int bytesPerPixel = (h.maxval < 256) ? 1 : 2;
        const qint64 neededBytes = static_cast<qint64>(h.width) * h.height * bytesPerPixel;
        if (pgmData.size() - h.dataOffset < neededBytes) return QImage();

        const uchar *src = reinterpret_cast<const uchar*>(pgmData.constData()) + h.dataOffset;
        for (int y = 0; y < h.height; ++y) {
            uchar *line = image.scanLine(y);
            for (int x = 0; x < h.width; ++x) {
                int val = 0;
                if (bytesPerPixel == 1) {
                    val = src[y * h.width + x];
                } else {
                    const int idx = (y * h.width + x) * 2;
                    val = (src[idx] << 8) | src[idx + 1];
                }
                val = (val * 255) / h.maxval;
                line[x] = static_cast<uchar>(negate ? (255 - val) : val);
            }
        }
    } else { // P2 文本模式
        int pos = h.dataOffset;
        for (int y = 0; y < h.height; ++y) {
            uchar *line = image.scanLine(y);
            for (int x = 0; x < h.width; ++x) {
                const int val = (nextToken(pgmData, pos).toInt() * 255) / h.maxval;
                line[x] = static_cast<uchar>(negate ? (255 - val) : val);
            }
        }
    }

    return image;
}

QByteArray MapPgmUtil::imageToPgm(const QImage &image, bool negate)
{
    if (image.isNull()) return QByteArray();

    const QImage gray = (image.format() == QImage::Format_Grayscale8)
                            ? image
                            : image.convertToFormat(QImage::Format_Grayscale8);

    const int w = gray.width();
    const int h = gray.height();

    QByteArray out = QString("P5\n%1 %2\n%3\n").arg(w).arg(h).arg(kDefaultPgmMaxVal).toLatin1();

    QByteArray pixels;
    pixels.resize(w * h);
    char *dst = pixels.data();

    for (int y = 0; y < h; ++y) {
        const uchar *line = gray.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            const uchar v = line[x];
            dst[y * w + x] = static_cast<char>(negate ? (255 - v) : v);
        }
    }
    out.append(pixels);
    return out;
}

// ── 2. ROS map_server YAML 解析 ──

MapYamlInfo MapPgmUtil::parseMapYaml(const QString &yamlText)
{
    MapYamlInfo info;
    info.resolution = kDefaultResolution;

    const QStringList lines = yamlText.split('\n');
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.startsWith("image:")) {
            info.imageFile = line.section(':', 1).trimmed();
            info.imageFile.remove('\'').remove('"');
        } else if (line.startsWith("resolution:")) {
            info.resolution = line.section(':', 1).trimmed().toDouble();
            info.isValid = true;
        } else if (line.startsWith("origin:")) {
            QString val = line.section(':', 1).trimmed();
            val.remove('[').remove(']');
            const QStringList parts = val.split(',', Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                info.originX = parts[0].trimmed().toDouble();
                info.originY = parts[1].trimmed().toDouble();
                if (parts.size() >= 3) {
                    info.originYaw = parts[2].trimmed().toDouble();
                }
            }
        } else if (line.startsWith("negate:")) {
            info.negate = (line.section(':', 1).trimmed().toInt() != 0);
        }
    }

    return info;
}

bool MapPgmUtil::parseMapYaml(const QString &yamlText,
                             double &resolution,
                             double &originX,
                             double &originY,
                             bool &negate,
                             QString *imageFile)
{
    const MapYamlInfo info = parseMapYaml(yamlText);
    resolution = info.resolution;
    originX    = info.originX;
    originY    = info.originY;
    negate     = info.negate;
    if (imageFile) {
        *imageFile = info.imageFile;
    }
    return info.isValid;
}

// ── 3. 曲线与几何计算 ──

QPointF MapPgmUtil::evaluateBezier(const QList<QPointF> &controlPoints, double t)
{
    const int n = controlPoints.size() - 1;
    if (n < 0) return QPointF();
    if (n == 0) return controlPoints[0];

    // 特化常见 3 次贝塞尔曲线，避免频繁临时堆分配
    if (n == 3) {
        const double u = 1.0 - t;
        const double tt = t * t;
        const double uu = u * u;
        const double uuu = uu * u;
        const double ttt = tt * t;

        return uuu * controlPoints[0]
             + 3.0 * uu * t * controlPoints[1]
             + 3.0 * u * tt * controlPoints[2]
             + ttt * controlPoints[3];
    }

    // 任意阶 De Casteljau 通用算法
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

    constexpr int steps = kDefaultBezierSampleSteps;
    for (int i = 1; i <= steps; ++i) {
        const double t = static_cast<double>(i) / steps;
        path.lineTo(evaluateBezier(controlPoints, t));
    }
    return path;
}

QList<QPolygonF> MapPgmUtil::createDirectionArrows(const QPointF &mid,
                                                  const QPointF &dirUnit,
                                                  int direct,
                                                  double lineLen)
{
    QList<QPolygonF> result;
    const double lenSq = dirUnit.x() * dirUnit.x() + dirUnit.y() * dirUnit.y();
    if (lenSq < kMinTangentMagnitudeSquared) return result;

    const double len = std::sqrt(lenSq);
    const QPointF d = dirUnit / len;

    double scale = 1.0;
    if (lineLen < 15.0) {
        scale = 0.65;
    } else if (lineLen < 25.0) {
        scale = 0.85;
    }

    if (isBidirectional(direct)) {
        // 双向通行 (↔)：在线段中点两侧对称绘制反向箭头对
        const double gap = qBound(3.5, lineLen * 0.12, 7.5);
        result.append(buildArrowPolygon(mid + d * gap, d, scale));   // 正向 →
        result.append(buildArrowPolygon(mid - d * gap, -d, scale));  // 反向 ←
    } else if (direct == static_cast<int>(MapPgmUtil::Direction::Backward)) {
        // 单向后退 ←
        result.append(buildArrowPolygon(mid, -d, scale));
    } else {
        // 单向前进 →
        result.append(buildArrowPolygon(mid, d, scale));
    }
    return result;
}

QList<QPolygonF> MapPgmUtil::createBezierDirectionArrows(const QList<QPointF> &controlPoints,
                                                        int direct,
                                                        double curveLen)
{
    QList<QPolygonF> result;
    if (controlPoints.size() < 2) return result;

    double scale = 1.0;
    if (curveLen < 15.0) {
        scale = 0.65;
    } else if (curveLen < 25.0) {
        scale = 0.85;
    }

    auto safeMakeArrow = [scale](const QPointF &center, const QPointF &forwardDir) -> QPolygonF {
        const double lenSq = forwardDir.x() * forwardDir.x() + forwardDir.y() * forwardDir.y();
        if (lenSq < kMinTangentMagnitudeSquared) return QPolygonF();
        const QPointF d = forwardDir / std::sqrt(lenSq);
        return buildArrowPolygon(center, d, scale);
    };

    if (isBidirectional(direct)) {
        // 双向曲线 (↔)：沿贝塞尔弧线曲率在偏前与偏后两个参数点分别取切线生成贴合箭头
        const double dt = (curveLen < 25.0) ? 0.03 : 0.05;
        const double t1 = 0.50 + dt;
        const double t2 = 0.50 - dt;

        const QPointF p1 = evaluateBezier(controlPoints, t1);
        const QPointF dir1 = evaluateBezier(controlPoints, t1 + 0.02) - evaluateBezier(controlPoints, t1 - 0.02);
        const QPolygonF arrow1 = safeMakeArrow(p1, dir1);
        if (!arrow1.isEmpty()) result.append(arrow1);

        const QPointF p2 = evaluateBezier(controlPoints, t2);
        const QPointF dir2 = evaluateBezier(controlPoints, t2 + 0.02) - evaluateBezier(controlPoints, t2 - 0.02);
        const QPolygonF arrow2 = safeMakeArrow(p2, -dir2);
        if (!arrow2.isEmpty()) result.append(arrow2);
    } else if (direct == static_cast<int>(MapPgmUtil::Direction::Backward)) {
        // 单向后退 ←
        const QPointF mid = evaluateBezier(controlPoints, 0.50);
        const QPointF dir = evaluateBezier(controlPoints, 0.52) - evaluateBezier(controlPoints, 0.48);
        const QPolygonF arrow = safeMakeArrow(mid, -dir);
        if (!arrow.isEmpty()) result.append(arrow);
    } else {
        // 单向前进 →
        const QPointF mid = evaluateBezier(controlPoints, 0.50);
        const QPointF dir = evaluateBezier(controlPoints, 0.52) - evaluateBezier(controlPoints, 0.48);
        const QPolygonF arrow = safeMakeArrow(mid, dir);
        if (!arrow.isEmpty()) result.append(arrow);
    }
    return result;
}

QPainterPath MapPgmUtil::createPointPath(double pxPerMeterX,
                                        double pxPerMeterY,
                                        double *outW,
                                        double *outH,
                                        QPainterPath *outCrossPath,
                                        double minWidth)
{
    const double scaleX = (pxPerMeterX > 0.0) ? pxPerMeterX : 20.0;
    const double scaleY = (pxPerMeterY > 0.0) ? pxPerMeterY : 20.0;

    // 真实车辆物理尺寸比例
    double rectW = kRobotPhysicalLengthMeters * scaleX;
    const double rectH = kRobotPhysicalWidthMeters * scaleY;
    if (minWidth > 0.0 && rectW < minWidth && minWidth <= rectW * 1.25) {
        rectW = minWidth;
    }

    if (outW) *outW = rectW;
    if (outH) *outH = rectH;

    // 锚点 (0, 0) 为车体中心
    QPainterPath rectPath;
    rectPath.addRect(QRectF(-rectW / 2.0, -rectH / 2.0, rectW, rectH));

    if (outCrossPath) {
        QPainterPath crossPath;
        const double cw = qBound(1.0, 0.06 * scaleX, rectW * 0.22);
        const double ch = qBound(1.0, 0.06 * scaleY, rectH * 0.22);
        crossPath.moveTo(-cw, 0.0); crossPath.lineTo(cw, 0.0);
        crossPath.moveTo(0.0, -ch); crossPath.lineTo(0.0, ch);
        *outCrossPath = crossPath;
    }

    return rectPath;
}

// ── 4. .smap 路网文件解析与转换 ──

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

    // 1. 解析 Header
    const QJsonObject header = smapObj.value("header").toObject();
    outMapName = header.value("mapName").toString();
    if (outHeader) {
        *outHeader = header;
    }

    // 2. 解析 advancedPointList (站点、充电桩、精确定位点)
    const QJsonArray advPoints = smapObj.value("advancedPointList").toArray();
    QMap<QString, QString> instToIdMap;   // "LM1" -> "p_1"
    QMap<QString, QPointF> instToPosMap;  // "LM1" -> QPointF(x, y)

    int pointCount = 0;
    for (const auto &pv : advPoints) {
        const QJsonObject pt = pv.toObject();
        QString instName = pt.value("instanceName").toString().trimmed();
        if (instName.isEmpty()) {
            instName = QString("P%1").arg(pointCount + 1);
        }
        pointCount++;
        const QString pid = QString("p_%1").arg(pointCount);
        instToIdMap[instName] = pid;

        const QJsonObject posObj = pt.value("pos").toObject();
        const double x = posObj.value("x").toDouble(0.0);
        const double y = posObj.value("y").toDouble(0.0);
        instToPosMap[instName] = QPointF(x, y);

        const QString className = pt.value("className").toString();
        QString type = "point";
        if (className.contains("Charge", Qt::CaseInsensitive) || instName.startsWith("CP", Qt::CaseInsensitive)) {
            type = "charge";
        } else if (className.contains("Precision", Qt::CaseInsensitive) ||
                   className.contains("Transfer", Qt::CaseInsensitive) ||
                   instName.startsWith("TL", Qt::CaseInsensitive)) {
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

    // 3. 解析 advancedCurveList (贝塞尔曲线与路径段)
    const QJsonArray advCurves = smapObj.value("advancedCurveList").toArray();
    struct ParsedCurve {
        QString name;
        QString startInst;
        QString endInst;
        QList<QPointF> ctrlPts;
    };
    QList<ParsedCurve> rawCurves;

    for (const auto &cv : advCurves) {
        const QJsonObject cObj = cv.toObject();
        const QString instName = cObj.value("instanceName").toString();
        const QJsonObject startPos = cObj.value("startPos").toObject();
        const QJsonObject endPos   = cObj.value("endPos").toObject();
        const QString startInst    = startPos.value("instanceName").toString().trimmed();
        const QString endInst      = endPos.value("instanceName").toString().trimmed();

        const QJsonObject p0Obj = startPos.value("pos").toObject();
        const QJsonObject p1Obj = cObj.value("controlPos1").toObject();
        const QJsonObject p2Obj = cObj.value("controlPos2").toObject();
        const QJsonObject p3Obj = endPos.value("pos").toObject();

        QPointF p0(p0Obj.value("x").toDouble(0.0), p0Obj.value("y").toDouble(0.0));
        const QPointF p1(p1Obj.value("x").toDouble(0.0), p1Obj.value("y").toDouble(0.0));
        const QPointF p2(p2Obj.value("x").toDouble(0.0), p2Obj.value("y").toDouble(0.0));
        QPointF p3(p3Obj.value("x").toDouble(0.0), p3Obj.value("y").toDouble(0.0));

        // 若站点名在节点表中，校准贝塞尔端点严格对齐站点真实坐标
        if (instToPosMap.contains(startInst)) p0 = instToPosMap[startInst];
        if (instToPosMap.contains(endInst))   p3 = instToPosMap[endInst];

        ParsedCurve pc;
        pc.name      = instName;
        pc.startInst = startInst;
        pc.endInst   = endInst;
        pc.ctrlPts   = {p0, p1, p2, p3};
        rawCurves.append(pc);
    }

    // 智能归并对称反向曲线为单条双向曲线
    QList<ParsedCurve> mergedCurves;
    for (const auto &rc : rawCurves) {
        bool isReverseOfExisting = false;
        for (const auto &mc : mergedCurves) {
            if (rc.startInst == mc.endInst && rc.endInst == mc.startInst) {
                if (rc.ctrlPts.size() == 4 && mc.ctrlPts.size() == 4) {
                    constexpr double tol = kCurveMergeToleranceMeters;
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
        const QString cid = QString("b_%1").arg(curveCount);
        const QString startId = instToIdMap.value(mc.startInst);
        const QString endId   = instToIdMap.value(mc.endInst);

        QJsonObject curveObj;
        curveObj["id"]       = cid;
        curveObj["name"]     = mc.name.isEmpty() ? QString("曲%1").arg(curveCount) : mc.name;
        curveObj["startid"]  = startId;
        curveObj["endid"]    = endId;
        curveObj["direct"]   = static_cast<int>(MapPgmUtil::Direction::Bidirectional);
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
