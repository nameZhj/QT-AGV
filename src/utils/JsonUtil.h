/**
 * @file    JsonUtil.h
 * @module  DataComm
 * @brief   Defensive JSON file and string reader/writer with dot-path safe querying.
 * @deps    QJsonObject, QJsonDocument, QFile
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

/**
 * @brief JSON 序列化、反序列化与安全字段读取工具类
 */
class JsonUtil
{
public:
    JsonUtil() = delete;

    // ==============================================================================
    // === [Section: File & Byte Array I/O] ===
    // ==============================================================================

    /**
     * @brief 从本地文件读取 QJsonObject
     */
    static QJsonObject readJsonObjectFromFile(const QString &filePath, bool *ok = nullptr);

    /**
     * @brief 将 QJsonObject 写入本地文件
     */
    static bool writeJsonObjectToFile(const QString &filePath, const QJsonObject &obj, bool compact = false);

    /**
     * @brief 解析 UTF-8 字节数组为 QJsonObject
     */
    static QJsonObject parseJsonObject(const QByteArray &data, bool *ok = nullptr);

    /**
     * @brief 解析 UTF-8 字节数组为 QJsonArray
     */
    static QJsonArray parseJsonArray(const QByteArray &data, bool *ok = nullptr);

    /**
     * @brief 解析字符串为 QJsonObject
     */
    static QJsonObject parseJsonObject(const QString &str, bool *ok = nullptr);

    /**
     * @brief 将 QJsonObject 序列化为 UTF-8 字节流
     */
    static QByteArray toByteArray(const QJsonObject &obj, bool compact = false);

    /**
     * @brief 将 QJsonArray 序列化为 UTF-8 字节流
     */
    static QByteArray toByteArray(const QJsonArray &arr, bool compact = false);

    // ==============================================================================
    // === [Section: Defensive Field Accessors] ===
    // ==============================================================================

    /**
     * @brief 安全获取字符串，支持默认值
     */
    static QString getString(const QJsonObject &obj, const QString &key, const QString &defaultVal = QString());

    /**
     * @brief 安全获取浮点数，支持字符串容错转换
     */
    static double getDouble(const QJsonObject &obj, const QString &key, double defaultVal = 0.0);

    /**
     * @brief 安全获取整数，支持字符串容错转换
     */
    static int getInt(const QJsonObject &obj, const QString &key, int defaultVal = 0);

    /**
     * @brief 安全获取布尔值，支持字符串容错转换
     */
    static bool getBool(const QJsonObject &obj, const QString &key, bool defaultVal = false);

    /**
     * @brief 安全获取子 QJsonObject
     */
    static QJsonObject getObject(const QJsonObject &obj, const QString &key);

    /**
     * @brief 安全获取子 QJsonArray
     */
    static QJsonArray getArray(const QJsonObject &obj, const QString &key);

    // ==============================================================================
    // === [Section: Nested Path Query] ===
    // ==============================================================================

    /**
     * @brief 按点分隔路径获取嵌套值 (如 "a.b.c")
     */
    static QJsonValue getValueByPath(const QJsonObject &root, const QString &dottedPath);
};

