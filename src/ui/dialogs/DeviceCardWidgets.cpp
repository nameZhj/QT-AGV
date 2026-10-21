/**
 * @file    DeviceCardWidgets.cpp
 * @layer   GUILayer
 * @brief   Implementation of BatteryBadge and device card icon rendering.
 * @deps    DeviceCardWidgets.h, ApiService.h, AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "DeviceCardWidgets.h"
#include "ApiService.h"
#include "AppConfig.h"
#include <QPainter>
#include <QPen>
#include <QFont>
#include <QJsonArray>
#include <QMap>

static QMap<QString, QString> s_mapIdToName;
static bool s_isFetchingMaps = false;

BatteryBadge::BatteryBadge(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(36, 16);
}

void BatteryBadge::setBattery(int level)
{
    m_level = level;
    update();
}

void BatteryBadge::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF body(1, 1, 28, 14);
    p.setPen(QPen(QColor(255, 255, 255, 220), 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(body, 2, 2);

    QRectF tip(29, 4.5, 2.5, 7);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 220));
    p.drawRoundedRect(tip, 1, 1);

    p.setPen(QColor("#ffffff"));
    QFont f = font();
    f.setPixelSize(10);
    f.setBold(true);
    p.setFont(f);
    QString txt = (m_level >= 0) ? QString::number(m_level) : "--";
    p.drawText(body, Qt::AlignCenter, txt);
}

namespace DeviceCardWidgets {

QPixmap makeSlidersIcon(int size, const QColor &color)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap));

    p.drawLine(QPointF(2, 4), QPointF(size - 2, 4));
    p.setBrush(color);
    p.drawEllipse(QPointF(6, 4), 2, 2);

    p.drawLine(QPointF(2, 9), QPointF(size - 2, 9));
    p.drawEllipse(QPointF(size - 6, 9), 2, 2);

    p.drawLine(QPointF(2, 14), QPointF(size - 2, 14));
    p.drawEllipse(QPointF(8, 14), 2, 2);

    p.end();
    return pm;
}

QPixmap makeRobotHeadIcon(int size, const QColor &color)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap));

    p.drawLine(QPointF(size / 2.0, 1.5), QPointF(size / 2.0, 6.0));
    p.setBrush(color);
    p.drawEllipse(QPointF(size / 2.0, 2.0), 1.5, 1.5);

    QRectF head(3.5, 6.0, size - 7.0, size - 10.0);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(head, 2.5, 2.5);

    p.setBrush(color);
    p.drawEllipse(QPointF(size / 2.0 - 4.5, 11.5), 1.5, 1.5);
    p.drawEllipse(QPointF(size / 2.0 + 4.5, 11.5), 1.5, 1.5);

    p.drawLine(QPointF(size / 2.0 - 3.0, 16.5), QPointF(size / 2.0 + 3.0, 16.5));

    p.end();
    return pm;
}

QPixmap makeBaseStationIcon(int w, int h, const QColor &color)
{
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(w / 2.0 - 8, 2, 16, 12), 45 * 16, 90 * 16);
    p.drawArc(QRectF(w / 2.0 - 4.5, 5, 9, 8), 45 * 16, 90 * 16);

    p.drawRoundedRect(QRectF(3.5, 13.5, w - 7, 5), 1.5, 1.5);

    p.end();
    return pm;
}

QString extractLinkStatus(const QJsonObject &obj, const QString &key)
{
    QJsonValue v = obj.value(key);
    if (v.isString()) return v.toString();
    if (v.isDouble()) return QString::number(v.toInt());
    if (v.isBool())   return v.toBool() ? "0" : "1";
    return QString();
}

void fetchGlobalMapList(std::function<void()> onDone)
{
    if (s_isFetchingMaps) {
        if (onDone) onDone();
        return;
    }
    s_isFetchingMaps = true;
    ApiService::instance().listMapAll(QJsonObject{{"pageNum", 1}, {"pageSize", 1000}}, [onDone](int code, const QJsonObject &data) {
        s_isFetchingMaps = false;
        if (code == 200) {
            const QJsonArray rows = data.value("rows").toArray();
            for (const auto &v : rows) {
                QJsonObject m = v.toObject();
                QString mid = jsonIdStr(m.value("mapId"));
                if (mid.isEmpty() && m.contains("mapId")) mid = QString::number(m.value("mapId").toInt());
                QString mname = m.value("mapName").toString().trimmed();
                if (!mid.isEmpty() && !mname.isEmpty()) {
                    s_mapIdToName[mid] = mname;
                }
            }
        }
        if (onDone) onDone();
    });
}

QString lookupMapName(const QString &mapId)
{
    if (mapId.isEmpty() || mapId == "0") return QString();
    if (s_mapIdToName.contains(mapId)) {
        return s_mapIdToName.value(mapId);
    }
    return QString();
}

} // namespace DeviceCardWidgets
