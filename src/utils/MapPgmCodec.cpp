/**
 * @file    MapPgmCodec.cpp
 * @layer   Utils
 * @brief   Implementation of PGM image encoding/decoding and ROS map_server YAML parser.
 * @deps    MapPgmCodec.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapPgmCodec.h"

#include <QStringList>
#include <cctype>

namespace {

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

    if (pos < data.size() && std::isspace(static_cast<unsigned char>(data[pos]))) {
        pos++;
    }
    h.dataOffset = pos;
    h.ok = true;
    return h;
}

} // anonymous namespace

QImage MapPgmCodec::pgmToImage(const QByteArray &pgmData, bool negate)
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
    } else {
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

QByteArray MapPgmCodec::imageToPgm(const QImage &image, bool negate)
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

MapYamlInfo MapPgmCodec::parseMapYaml(const QString &yamlText)
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

bool MapPgmCodec::parseMapYaml(const QString &yamlText,
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
