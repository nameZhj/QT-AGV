/**
 * @file    CreateMapCanvasWidget.h
 * @layer   GUI
 * @brief   Real-time SLAM occupancy grid and reflector marker canvas preview widget.
 * @deps    QWidget, QJsonObject, QJsonArray
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>

class CreateMapCanvasWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CreateMapCanvasWidget(QWidget *parent = nullptr);
    ~CreateMapCanvasWidget() override = default;

    void setMapData(const QJsonObject &data);
    void setRobotPose(const QJsonObject &pose);
    void setPoles(const QJsonArray &poles);
    void setHighMappingActive(bool active);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QJsonObject m_mapData;
    QJsonObject m_robotPose;
    QJsonArray  m_poles;
    bool        m_highMappingActive = false;
};
