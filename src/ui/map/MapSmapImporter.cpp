/**
 * @file    MapSmapImporter.cpp
 * @layer   GUI
 * @brief   Implementation of SMAP format file parser and model populator.
 * @deps    MapSmapImporter.h, MapEditDataModel.h, MapPgmUtil.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapSmapImporter.h"
#include "MapEditDataModel.h"
#include "utils/MapPgmUtil.h"
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>

SmapImportResult MapSmapImporter::importFromFile(const QString &filePath,
                                                 MapEditDataModel *model,
                                                 double worldL, double worldB,
                                                 double worldR, double worldT)
{
    SmapImportResult res;
    if (!model) {
        res.errorMessage = "Data model pointer is null.";
        return res;
    }

    QJsonArray newPoints, newLines, newCurves;
    QJsonObject smapHeader;
    bool ok = MapPgmUtil::parseSmapFile(filePath, newPoints, newLines, newCurves,
                                        res.mapName, &res.errorMessage, &smapHeader);
    if (!ok) return res;

    QJsonArray pts, chgs, hps;
    for (const auto &pv : newPoints) {
        QJsonObject p = pv.toObject();
        QString type = p.value("type").toString("point");
        if (type == "charge") {
            chgs.append(p);
        } else if (type == "highPrecision") {
            hps.append(p);
        } else {
            pts.append(p);
        }
    }

    res.pointCount = pts.size();
    res.chargeCount = chgs.size();
    res.hpCount = hps.size();
    res.lineCount = newLines.size();
    res.curveCount = newCurves.size();

    model->setPoints(pts);
    model->setCharges(chgs);
    model->setHighPrecisionPoints(hps);
    model->setLines(newLines);
    model->setCurves(newCurves);
    model->setDirty(true);

    // 计算导入边界
    if (!newPoints.isEmpty() || !newLines.isEmpty() || !newCurves.isEmpty()) {
        double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
        auto updateBounds = [&](double x, double y) {
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        };
        for (const auto &v : newPoints) {
            QJsonObject o = v.toObject();
            updateBounds(o.value("x").toDouble(), o.value("y").toDouble());
        }
        for (const auto &v : newLines) {
            QJsonObject o = v.toObject();
            updateBounds(o.value("x1").toDouble(), o.value("y1").toDouble());
            updateBounds(o.value("x2").toDouble(), o.value("y2").toDouble());
        }

        res.boundingBox = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
        if (worldR > worldL && worldT > worldB) {
            if (minX < worldL || maxX > worldR || minY < worldB || maxY > worldT) {
                res.outOfBounds = true;
            }
        }
    }

    res.ok = true;
    return res;
}
