/**
 * @file    MapViewMapPropsPanel.h
 * @layer   GUI
 * @brief   Property panel for map dimension metrics, layer element counts, and navigation shortcuts hint.
 * @deps    QWidget, QLabel, QGroupBox
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QLabel>
#include <QGroupBox>

class MapViewMapPropsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MapViewMapPropsPanel(QWidget *parent = nullptr);
    ~MapViewMapPropsPanel() override = default;

    void updateMapProps(const QString &mapId, const QString &mapName,
                        double imgW, double imgH,
                        double wL, double wB, double wR, double wT,
                        int ptCount, int hpCount, int chgCount,
                        int lineCount, int curveCount, int areaCount);

private:
    void setupUI();

    QGroupBox *m_mapPropsGroup    = nullptr;
    QLabel    *m_infoNameLabel    = nullptr;
    QLabel    *m_mapPropIdVal     = nullptr;
    QLabel    *m_infoSizeLabel    = nullptr;
    QLabel    *m_infoResLabel     = nullptr;
    QLabel    *m_mapPropAreaVal   = nullptr;

    QGroupBox *m_mapCountGroup    = nullptr;
    QLabel    *m_mapCountPointVal = nullptr;
    QLabel    *m_mapCountHpVal    = nullptr;
    QLabel    *m_mapCountChargeVal= nullptr;
    QLabel    *m_mapCountLineVal  = nullptr;
    QLabel    *m_mapCountCurveVal = nullptr;
    QLabel    *m_mapCountAreaVal  = nullptr;

    QLabel    *m_hintLabel        = nullptr;
};
