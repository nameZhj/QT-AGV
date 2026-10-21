/**
 * @file    MqttTopicRouter.cpp
 * @layer   DataComm
 * @brief   Implementation of MQTT topic routing, wildcards, and variants expansion.
 * @deps    MqttTopicRouter.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MqttTopicRouter.h"

bool MqttTopicRouter::isGlobalTopic(const QString &topic)
{
    const QString t = topic.trimmed().toLower();
    if (t.contains(QStringLiteral("info"))) return true;
    if (t.contains(QStringLiteral("status"))) return true;
    if (t.contains(QStringLiteral("online"))) return true;
    if (t.contains(QStringLiteral("taskchain"))) return true;
    return false;
}

bool MqttTopicRouter::matchTopic(const QString &filter, const QString &topic)
{
    if (filter == topic) {
        return true;
    }

    auto clean = [](QString s) -> QString {
        s = s.trimmed();
        while (s.startsWith(':')) s = s.mid(1);
        if (!s.startsWith('/')) s = '/' + s;
        while (s.endsWith('/') && s.length() > 1) s.chop(1);
        return s;
    };

    const QString cleanFilter = clean(filter);
    const QString cleanTopic  = clean(topic);

    if (cleanFilter == cleanTopic) {
        return true;
    }

    const bool filterIsInfo = (cleanFilter == QStringLiteral("/info") || cleanFilter == QStringLiteral("info")
                            || cleanFilter == QStringLiteral("/rob/info") || cleanFilter == QStringLiteral("/rob/+/info")
                            || cleanFilter.endsWith(QStringLiteral("/info")));
    const bool topicIsInfo  = (cleanTopic == QStringLiteral("/info") || cleanTopic == QStringLiteral("info")
                            || cleanTopic == QStringLiteral("/rob/info") || cleanTopic.endsWith(QStringLiteral("/info")));

    if (filterIsInfo && topicIsInfo) {
        QString filterCar;
        QString topicCar;
        const QStringList fParts = cleanFilter.split('/', Qt::SkipEmptyParts);
        const QStringList tParts = cleanTopic.split('/', Qt::SkipEmptyParts);
        if (fParts.size() >= 3 && fParts[0] == QStringLiteral("rob")) {
            filterCar = fParts[1];
        }
        if (tParts.size() >= 3 && tParts[0] == QStringLiteral("rob")) {
            topicCar = tParts[1];
        }
        if (!filterCar.isEmpty() && filterCar != QStringLiteral("+") && !topicCar.isEmpty()) {
            if (filterCar != topicCar) {
                return false;
            }
        }
        return true;
    }

    const QStringList filterParts = cleanFilter.split('/', Qt::KeepEmptyParts);
    const QStringList topicParts  = cleanTopic.split('/', Qt::KeepEmptyParts);

    int f = 0;
    int t = 0;
    while (f < filterParts.size() && t < topicParts.size()) {
        const QString &fp = filterParts[f];
        const QString &tp = topicParts[t];

        if (fp == QStringLiteral("#")) {
            return true;
        } else if (fp == QStringLiteral("+") || fp == tp) {
            ++f;
            ++t;
        } else {
            return false;
        }
    }

    if (f == filterParts.size() - 1 && filterParts[f] == QStringLiteral("#")) {
        return true;
    }

    return (f == filterParts.size() && t == topicParts.size());
}

QStringList MqttTopicRouter::getTopicVariants(const QString &topic)
{
    QStringList variants;
    const QString trimmed = topic.trimmed();
    if (trimmed.isEmpty()) {
        return variants;
    }
    variants.append(trimmed);

    if (trimmed.startsWith(QStringLiteral(":/")) && trimmed.length() > 2) {
        variants.append(trimmed.mid(1));
        variants.append(trimmed.mid(2));
    } else if (trimmed.startsWith('/') && trimmed.length() > 1) {
        variants.append(trimmed.mid(1));
        variants.append(QStringLiteral(":") + trimmed);
    } else if (!trimmed.startsWith('/')) {
        variants.append(QStringLiteral("/") + trimmed);
        variants.append(QStringLiteral(":/") + trimmed);
    }

    if (trimmed.endsWith(QStringLiteral("info"))) {
        variants.append(QStringLiteral("/rob/1/info"));
        variants.append(QStringLiteral("rob/1/info"));
        variants.append(QStringLiteral("/rob/+/info"));
        variants.append(QStringLiteral("/info"));
        variants.append(QStringLiteral("info"));
        variants.append(QStringLiteral("/rob/info"));
    }

    variants.removeDuplicates();
    return variants;
}

QString MqttTopicRouter::clientErrorToString(QMqttClient::ClientError error)
{
    switch (error) {
    case QMqttClient::NoError:                return QStringLiteral("NoError");
    case QMqttClient::InvalidProtocolVersion: return QStringLiteral("InvalidProtocolVersion");
    case QMqttClient::IdRejected:             return QStringLiteral("IdRejected");
    case QMqttClient::ServerUnavailable:      return QStringLiteral("ServerUnavailable");
    case QMqttClient::BadUsernameOrPassword:  return QStringLiteral("BadUsernameOrPassword");
    case QMqttClient::NotAuthorized:          return QStringLiteral("NotAuthorized");
    case QMqttClient::TransportInvalid:       return QStringLiteral("TransportInvalid");
    case QMqttClient::ProtocolViolation:      return QStringLiteral("ProtocolViolation");
    case QMqttClient::UnknownError:           return QStringLiteral("UnknownError");
    case QMqttClient::Mqtt5SpecificError:     return QStringLiteral("Mqtt5SpecificError");
    default:                                  return QStringLiteral("Unknown(%1)").arg(static_cast<int>(error));
    }
}
