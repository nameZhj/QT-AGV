/**
 * @file    CreateMapApiHelper.cpp
 * @layer   GUI
 * @brief   SLAM mapping API response error checking implementation.
 * @deps    CreateMapApiHelper.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "CreateMapApiHelper.h"
#include <QJsonValue>

namespace CreateMapApiHelper {

QString checkApiError(const QJsonObject &res)
{
    QJsonObject dataObj;
    if (res.value("data").isObject()) {
        dataObj = res.value("data").toObject();
    }

    auto isFailedVal = [](const QJsonValue &v) -> bool {
        if (v.isString()) {
            QString s = v.toString().trimmed();
            return (s.compare("failed", Qt::CaseInsensitive) == 0 ||
                    s.compare("error", Qt::CaseInsensitive) == 0 ||
                    s.compare("fail", Qt::CaseInsensitive) == 0);
        } else if (v.isBool()) {
            return !v.toBool();
        }
        return false;
    };

    bool isFailed = false;
    if (isFailedVal(res.value("msg")) || isFailedVal(dataObj.value("msg"))) {
        isFailed = true;
    }

    if (res.contains("success") && !res.value("success").toBool()) isFailed = true;
    if (dataObj.contains("success") && !dataObj.value("success").toBool()) isFailed = true;

    QString errInfoStr;
    if (dataObj.value("error_info").isString()) {
        errInfoStr = dataObj.value("error_info").toString().trimmed();
    } else if (res.value("error_info").isString()) {
        errInfoStr = res.value("error_info").toString().trimmed();
    }
    if (!errInfoStr.isEmpty() && errInfoStr.compare("none", Qt::CaseInsensitive) != 0) {
        isFailed = true;
    }

    if (!isFailed) return QString();

    QString infoStr;
    if (dataObj.value("info").isString()) {
        infoStr = dataObj.value("info").toString().trimmed();
    } else if (res.value("info").isString()) {
        infoStr = res.value("info").toString().trimmed();
    }

    if (!infoStr.isEmpty() && !errInfoStr.isEmpty() && infoStr != errInfoStr) {
        return QString("%1: %2").arg(infoStr, errInfoStr);
    }
    if (!errInfoStr.isEmpty()) return errInfoStr;
    if (!infoStr.isEmpty())    return infoStr;

    QString msgStr = res.value("message").toString().trimmed();
    if (msgStr.isEmpty()) msgStr = dataObj.value("message").toString().trimmed();
    if (!msgStr.isEmpty()) return msgStr;

    return QStringLiteral("操作失败");
}

} // namespace CreateMapApiHelper
