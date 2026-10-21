/**
 * @file    CreateMapWorkflowController.h
 * @layer   GUI
 * @brief   SLAM and high-order reflector mapping lifecycle workflow controller.
 * @deps    QObject, QWidget
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QWidget>
#include <functional>

class CreateMapWorkflowController : public QObject
{
    Q_OBJECT
public:
    explicit CreateMapWorkflowController(QWidget *parent = nullptr);
    ~CreateMapWorkflowController() override = default;

    void startAdaptiveMapping(std::function<void(bool success)> onDone);
    void stopAdaptiveMapping(std::function<void(bool finished, bool saved)> onDone);

    void startHighOrderMapping(std::function<void(bool success)> onDone);
    void stopHighOrderMapping(std::function<void(bool finished, bool saved)> onDone);

    void resetReflectorAll();
    void resetReflectorLocal();

    void toggleCruise(bool currentSuspended, std::function<void(bool newSuspended)> onDone);

    bool tryExitMapping(bool mappingActive, bool highMappingActive, std::function<void()> onExitAllowed);

private:
    QWidget *m_parentWidget = nullptr;
};
