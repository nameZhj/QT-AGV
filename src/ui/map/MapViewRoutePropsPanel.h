/**
 * @file    MapViewRoutePropsPanel.h
 * @layer   GUI
 * @brief   Property panel for inspecting line, curve, and advanced area topologies and speed limits.
 * @deps    QWidget, QJsonObject, QLabel, QCheckBox, QGroupBox
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QGroupBox>
#include <QJsonObject>
#include <functional>

class MapViewRoutePropsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MapViewRoutePropsPanel(QWidget *parent = nullptr);
    ~MapViewRoutePropsPanel() override = default;

    void showRouteDetails(const QJsonObject &obj, const QString &type,
                          const std::function<QString(const QString &)> &lookup);
    void clearRouteDetails();

    void showAreaDetails(const QJsonObject &obj);
    void clearAreaDetails();

    void showRouteView();
    void showAreaView();
    void clearAll();

private:
    void setupUI();
    static QWidget* mkCheckRow(QCheckBox *&chk, const QString &text, QLabel *valLbl);

    // Route
    QGroupBox *m_routeGroup         = nullptr;
    QLabel    *m_routeBadge         = nullptr;
    QLabel    *m_routeEmptyHint     = nullptr;
    QWidget   *m_routeContentWidget = nullptr;
    QCheckBox *m_chkRouteType       = nullptr;
    QLabel    *m_routeTypeVal       = nullptr;
    QCheckBox *m_chkRouteId         = nullptr;
    QLabel    *m_routeIdVal         = nullptr;
    QCheckBox *m_chkRouteName       = nullptr;
    QLabel    *m_routeNameVal       = nullptr;
    QCheckBox *m_chkRouteStart      = nullptr;
    QLabel    *m_routeStartVal      = nullptr;
    QCheckBox *m_chkRouteEnd        = nullptr;
    QLabel    *m_routeEndVal        = nullptr;
    QCheckBox *m_chkRouteDirect     = nullptr;
    QLabel    *m_routeDirectVal     = nullptr;
    QCheckBox *m_chkRouteMaxSpeed   = nullptr;
    QLabel    *m_routeMaxSpeedVal   = nullptr;
    QCheckBox *m_chkRouteMaxAcc     = nullptr;
    QLabel    *m_routeMaxAccVal     = nullptr;
    QLabel    *m_routeMaxDecVal     = nullptr;
    QCheckBox *m_chkRouteMaxRot     = nullptr;
    QLabel    *m_routeMaxRotVal     = nullptr;
    QCheckBox *m_chkRouteMaxRotAcc  = nullptr;
    QLabel    *m_routeMaxRotAccVal  = nullptr;
    QCheckBox *m_chkRouteReachDist  = nullptr;
    QLabel    *m_routeReachDistVal  = nullptr;
    QCheckBox *m_chkRouteReachAngle = nullptr;
    QLabel    *m_routeReachAngleVal = nullptr;
    QCheckBox *m_chkRouteWidth      = nullptr;
    QLabel    *m_routeWidthVal      = nullptr;
    QCheckBox *m_chkRouteObsRange   = nullptr;
    QLabel    *m_routeObsRangeVal   = nullptr;

    // Area
    QGroupBox *m_areaGroup         = nullptr;
    QLabel    *m_areaBadge         = nullptr;
    QLabel    *m_areaEmptyHint     = nullptr;
    QWidget   *m_areaContentWidget = nullptr;
    QCheckBox *m_chkAreaName       = nullptr;
    QLabel    *m_areaNameVal       = nullptr;
    QLabel    *m_areaBoundsVal     = nullptr;
    QCheckBox *m_chkAreaMaxSpeed   = nullptr;
    QLabel    *m_areaMaxSpeedVal   = nullptr;
    QCheckBox *m_chkAreaMaxAcc     = nullptr;
    QLabel    *m_areaMaxAccVal     = nullptr;
    QCheckBox *m_chkAreaMaxRot     = nullptr;
    QLabel    *m_areaMaxRotVal     = nullptr;
    QCheckBox *m_chkAreaMaxRotAcc  = nullptr;
    QLabel    *m_areaMaxRotAccVal  = nullptr;
    QCheckBox *m_chkAreaReachDist  = nullptr;
    QLabel    *m_areaReachDistVal  = nullptr;
    QCheckBox *m_chkAreaReachAngle = nullptr;
    QLabel    *m_areaReachAngleVal = nullptr;
    QCheckBox *m_chkAreaWidth      = nullptr;
    QLabel    *m_areaWidthVal      = nullptr;
    QCheckBox *m_chkAreaObsRange   = nullptr;
    QLabel    *m_areaObsRangeVal   = nullptr;
};
