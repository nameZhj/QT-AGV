/**
 * @file    DeviceCardWidgets.h
 * @layer   GUILayer
 * @brief   Auxiliary badges, custom icon painters, and map list caching for device cards.
 * @deps    QWidget, QPixmap, QColor, QString
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QPixmap>
#include <QColor>
#include <QString>
#include <QJsonObject>
#include <functional>

class BatteryBadge : public QWidget
{
    Q_OBJECT
public:
    explicit BatteryBadge(QWidget *parent = nullptr);
    void setBattery(int level);
    int  battery() const { return m_level; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_level = -1;
};

namespace DeviceCardWidgets {
    QPixmap makeSlidersIcon(int size = 18, const QColor &color = QColor("#ffffff"));
    QPixmap makeRobotHeadIcon(int size = 26, const QColor &color = QColor("#00e5ff"));
    QPixmap makeBaseStationIcon(int w = 26, int h = 20, const QColor &color = QColor("#00e5ff"));

    QString extractLinkStatus(const QJsonObject &obj, const QString &key = "linkStatus");
    void    fetchGlobalMapList(std::function<void()> onDone = nullptr);
    QString lookupMapName(const QString &mapId);
}
