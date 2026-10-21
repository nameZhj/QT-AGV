/**
 * @file    MqttTopicRouter.h
 * @layer   DataComm
 * @brief   MQTT topic routing utilities, wildcard pattern matching, and variants expansion.
 * @deps    none
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QStringList>

#include <QMqttClient>

class MqttTopicRouter
{
public:
    static bool matchTopic(const QString &filter, const QString &topic);
    static bool isGlobalTopic(const QString &topic);
    static QStringList getTopicVariants(const QString &topic);
    static QString clientErrorToString(QMqttClient::ClientError error);
};
