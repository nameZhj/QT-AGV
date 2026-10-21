/**
 * @file    StyleLoader.cpp
 * @module  GUILayer
 * @brief   Implementation of modular QSS stylesheet loader and dynamic styler.
 * @deps    StyleLoader.h, QFile, QTextStream
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "StyleLoader.h"

#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>

QString StyleLoader::loadAllStylesheets()
{
    static const QStringList qssFiles = {
        QStringLiteral(":/qss/base.qss"),
        QStringLiteral(":/qss/mainwindow.qss"),
        QStringLiteral(":/qss/dialogs.qss"),
        QStringLiteral(":/qss/map.qss"),
        QStringLiteral(":/qss/task.qss"),
        QStringLiteral(":/qss/control.qss")
    };

    QString mergedStyles;
    for (const QString &filePath : qssFiles) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            mergedStyles += stream.readAll() + QStringLiteral("\n");
            file.close();
        } else {
            qWarning() << "[StyleLoader] Failed to open QSS file:" << filePath;
        }
    }

    return mergedStyles;
}

void StyleLoader::applyApplicationStyle()
{
    const QString merged = loadAllStylesheets();
    if (!merged.isEmpty() && qApp) {
        qApp->setStyleSheet(merged);
    }
}
