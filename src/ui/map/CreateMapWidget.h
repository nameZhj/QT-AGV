/**
 * @file    CreateMapWidget.h
 * @layer   GUI
 * @brief   SLAM online mapping controller and laser scan preview widget.
 * @deps    QWidget, CreateMapCanvasWidget.h, CreateMapWorkflowController.h, CreateMapTeleopPanel.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include "CreateMapCanvasWidget.h"
#include "CreateMapWorkflowController.h"
#include "CreateMapTeleopPanel.h"

class QPushButton;
class DirectionPadWidget;
class QVBoxLayout;

class CreateMapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CreateMapWidget(QWidget *parent = nullptr);
    ~CreateMapWidget() override = default;

    bool tryExitMapping();

signals:
    void goBack();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onStartAdaptiveClick();
    void onStopAdaptiveClick();
    void onStartHighClick();
    void onStopHighClick();
    void onResetReflectorAll();
    void onResetReflectorLocal();
    void onToggleCruiseClick();
    void onMqttMessage(const QString &topic, const QByteArray &payload);
    void onTeleopActiveChanged(bool active);
    void onTeleopBrakeChanged(bool brake);

private:
    void setupUI();
    void setupHeaderBar(QVBoxLayout *root);
    void setupToolbar(QVBoxLayout *mainLayout);
    void setupContentSplitter(QVBoxLayout *mainLayout);
    void initMqtt();
    void disconnectMqtt();
    void updateToolbar();

    CreateMapWorkflowController *m_workflow    = nullptr;
    CreateMapCanvasWidget       *m_mapCanvas    = nullptr;
    CreateMapTeleopPanel        *m_teleopPanel  = nullptr;
    DirectionPadWidget          *m_directionPad = nullptr;

    // 工具栏按钮
    QPushButton *m_adaptiveBtn     = nullptr;
    QPushButton *m_highBtn         = nullptr;
    QPushButton *m_stopAdaptiveBtn = nullptr;
    QPushButton *m_resetAllBtn     = nullptr;
    QPushButton *m_resetLocalBtn   = nullptr;
    QPushButton *m_stopHighBtn     = nullptr;
    QPushButton *m_cruiseBtn       = nullptr;

    // 状态
    bool m_mappingActive     = false;
    bool m_highMappingActive = false;
    bool m_cruiseSuspended   = false;

    // MQTT 数据
    QJsonObject m_mapData;
    QJsonObject m_robotPose;
    QJsonArray  m_poles;
};
