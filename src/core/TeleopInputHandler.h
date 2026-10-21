/**
 * @file    TeleopInputHandler.h
 * @layer   AGVModel
 * @brief   Keyboard event filter, focus detector, and virtual key dispatcher for TeleopController.
 * @deps    TeleopController.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

class QKeyEvent;
class TeleopController;

class TeleopInputHandler
{
public:
    static bool isMovementKey(int key);
    static bool isTeleopKey(int key);
    static bool focusIsTextInput();

    static void handleWindowDeactivation(TeleopController *ctrl);
    static bool handleFilteredKeyEvent(TeleopController *ctrl, QKeyEvent *event);
    static bool handleKeyEvent(TeleopController *ctrl, QKeyEvent *event, bool pressed);
    static bool updateMotionKeyState(TeleopController *ctrl, int key, bool pressed);
    static void setVirtualKey(TeleopController *ctrl, int key, bool pressed);
    static bool isKeyPressed(const TeleopController *ctrl, int key);
};
