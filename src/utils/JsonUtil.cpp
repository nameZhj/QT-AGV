#include "JsonUtil.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

QJsonObject JsonUtil::readJsonObjectFromFile(const QString &filePath, bool *ok) {
    if (ok) *ok = false;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }
    return parseJsonObject(file.readAll(), ok);
}

bool JsonUtil::writeJsonObjectToFile(const QString &filePath, const QJsonObject &obj, bool compact) {
    QFileInfo info(filePath);
    QDir dir = info.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(toByteArray(obj, compact));
    file.close();
    return true;
}

QJsonObject JsonUtil::parseJsonObject(const QByteArray &data, bool *ok) {
    if (ok) *ok = false;
    if (data.isEmpty()) return QJsonObject();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        if (ok) *ok = true;
        return doc.object();
    }
    return QJsonObject();
}

QJsonArray JsonUtil::parseJsonArray(const QByteArray &data, bool *ok) {
    if (ok) *ok = false;
    if (data.isEmpty()) return QJsonArray();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError && doc.isArray()) {
        if (ok) *ok = true;
        return doc.array();
    }
    return QJsonArray();
}

QJsonObject JsonUtil::parseJsonObject(const QString &str, bool *ok) {
    return parseJsonObject(str.toUtf8(), ok);
}

QByteArray JsonUtil::toByteArray(const QJsonObject &obj, bool compact) {
    return QJsonDocument(obj).toJson(compact ? QJsonDocument::Compact : QJsonDocument::Indented);
}

QByteArray JsonUtil::toByteArray(const QJsonArray &arr, bool compact) {
    return QJsonDocument(arr).toJson(compact ? QJsonDocument::Compact : QJsonDocument::Indented);
}

QString JsonUtil::getString(const QJsonObject &obj, const QString &key, const QString &defaultVal) {
    if (!obj.contains(key)) return defaultVal;
    QJsonValue val = obj.value(key);
    if (val.isString()) return val.toString();
    if (val.isDouble()) return QString::number(val.toDouble());
    if (val.isBool()) return val.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return defaultVal;
}

double JsonUtil::getDouble(const QJsonObject &obj, const QString &key, double defaultVal) {
    if (!obj.contains(key)) return defaultVal;
    QJsonValue val = obj.value(key);
    if (val.isDouble()) return val.toDouble();
    if (val.isString()) {
        bool ok = false;
        double d = val.toString().trimmed().toDouble(&ok);
        if (ok) return d;
    }
    return defaultVal;
}

int JsonUtil::getInt(const QJsonObject &obj, const QString &key, int defaultVal) {
    if (!obj.contains(key)) return defaultVal;
    QJsonValue val = obj.value(key);
    if (val.isDouble()) return val.toInt();
    if (val.isString()) {
        bool ok = false;
        int i = val.toString().trimmed().toInt(&ok);
        if (ok) return i;
    }
    return defaultVal;
}

bool JsonUtil::getBool(const QJsonObject &obj, const QString &key, bool defaultVal) {
    if (!obj.contains(key)) return defaultVal;
    QJsonValue val = obj.value(key);
    if (val.isBool()) return val.toBool();
    if (val.isDouble()) return val.toInt() != 0;
    if (val.isString()) {
        QString s = val.toString().trimmed().toLower();
        if (s == "true" || s == "1") return true;
        if (s == "false" || s == "0") return false;
    }
    return defaultVal;
}

QJsonObject JsonUtil::getObject(const QJsonObject &obj, const QString &key) {
    return obj.value(key).toObject();
}

QJsonArray JsonUtil::getArray(const QJsonObject &obj, const QString &key) {
    return obj.value(key).toArray();
}

QJsonValue JsonUtil::getValueByPath(const QJsonObject &root, const QString &dottedPath) {
    if (dottedPath.isEmpty()) return QJsonValue();
    QStringList parts = dottedPath.split('.');
    QJsonObject cur = root;
    for (int i = 0; i < parts.size() - 1; ++i) {
        QJsonValue next = cur.value(parts[i]);
        if (!next.isObject()) return QJsonValue();
        cur = next.toObject();
    }
    return cur.value(parts.last());
}
