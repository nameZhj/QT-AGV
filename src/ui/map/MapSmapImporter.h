/**
 * @file    MapSmapImporter.h
 * @layer   GUI
 * @brief   SMAP vector map importer and coordinate bounding validator.
 * @deps    QString, QRectF, MapEditDataModel
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QRectF>

class MapEditDataModel;

struct SmapImportResult {
    bool ok = false;
    QString mapName;
    QString errorMessage;
    int pointCount = 0;
    int chargeCount = 0;
    int hpCount = 0;
    int curveCount = 0;
    int lineCount = 0;
    bool outOfBounds = false;
    QRectF boundingBox;
};

class MapSmapImporter
{
public:
    MapSmapImporter() = delete;

    static SmapImportResult importFromFile(const QString &filePath,
                                          MapEditDataModel *model,
                                          double worldL = 0, double worldB = 0,
                                          double worldR = 0, double worldT = 0);
};
