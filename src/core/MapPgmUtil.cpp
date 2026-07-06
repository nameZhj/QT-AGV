#include "MapPgmUtil.h"
#include <QStringList>
#include <cctype>

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

