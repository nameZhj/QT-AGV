/**
 * @file    TeleopInputHandler.cpp
 * @layer   AGVModel
 * @brief   Implementation of keyboard event filtering and key state management.
 * @deps    TeleopInputHandler.h, TeleopController.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TeleopInputHandler.h"
#include "TeleopController.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QTimer>
#include <QDebug>

bool TeleopInputHandler::isMovementKey(int key)
{
    switch (key) {
    case Qt::Key_W:
    case Qt::Key_S:
    case Qt::Key_A:
    case Qt::Key_D:
    case Qt::Key_Space:
        return true;
    default:
        return false;
    }
}

bool TeleopInputHandler::isTeleopKey(int key)
{
    return isMovementKey(key) || key == Qt::Key_P || key == Qt::Key_L;
}

bool TeleopInputHandler::focusIsTextInput()
{
    QWidget *w = qApp ? qApp->focusWidget() : nullptr;
    if (!w) return false;
    if (qobject_cast<QLineEdit *>(w))       return true;
    if (qobject_cast<QTextEdit *>(w))       return true;
    if (qobject_cast<QPlainTextEdit *>(w))  return true;
    if (auto *cb = qobject_cast<QComboBox *>(w)) {
        if (cb->isEditable()) return true;
    }
    return false;
}

void TeleopInputHandler::handleWindowDeactivation(TeleopController *ctrl)
{
    if (!ctrl) return;
    if (ctrl->m_keyForward || ctrl->m_keyBack || ctrl->m_keyLeft || ctrl->m_keyRight || ctrl->m_keyHoistUp || ctrl->m_keyHoistDown) {
        qWarning().noquote() << QStringLiteral("[TeleopController Safety] 窗口失焦，触发安全急停与按键复位");
        ctrl->resetKeyStates();
        ctrl->emergencyStop();
        emit ctrl->motionChanged(0.0, 0.0);
        emit ctrl->hoistChanged(0);
    }
}

bool TeleopInputHandler::handleFilteredKeyEvent(TeleopController *ctrl, QKeyEvent *ke)
{
    if (!ctrl || !ke) return false;
    const int key = ke->key();
    const bool pressed = (ke->type() == QEvent::KeyPress);

    if (key == Qt::Key_Control) {
        ctrl->m_ctrlHeld = pressed;
        return false;
    }

    if (isMovementKey(key)) {
        if (!ke->isAutoRepeat()) {
            handleKeyEvent(ctrl, ke, pressed);
        }
        return true;
    }

    if (isTeleopKey(key) && !focusIsTextInput()) {
        if (ke->isAutoRepeat()) return true;
        return handleKeyEvent(ctrl, ke, pressed);
    }

    return false;
}

bool TeleopInputHandler::updateMotionKeyState(TeleopController *ctrl, int key, bool pressed)
{
    if (!ctrl) return false;
    bool changed = false;
    switch (key) {
    case Qt::Key_W:
        if (ctrl->m_keyForward != pressed) {
            ctrl->m_keyForward = pressed;
            changed = true;
        }
        break;
    case Qt::Key_S:
        if (ctrl->m_keyBack != pressed) {
            ctrl->m_keyBack = pressed;
            changed = true;
        }
        break;
    case Qt::Key_A:
        if (ctrl->m_keyLeft != pressed) {
            ctrl->m_keyLeft = pressed;
            changed = true;
        }
        break;
    case Qt::Key_D:
        if (ctrl->m_keyRight != pressed) {
            ctrl->m_keyRight = pressed;
            changed = true;
        }
        break;
    case Qt::Key_Space:
        if (ctrl->m_keyBrake != pressed) {
            ctrl->m_keyBrake = pressed;
            changed = true;
            emit ctrl->brakeChanged(ctrl->m_keyBrake);
        }
        break;
    default:
        return false;
    }

    if (changed) {
        emit ctrl->keyStateChanged(key, pressed);
    }
    return true;
}

bool TeleopInputHandler::handleKeyEvent(TeleopController *ctrl, QKeyEvent *event, bool pressed)
{
    if (!ctrl || !event) return false;
    const int key = event->key();
    if (updateMotionKeyState(ctrl, key, pressed)) {
        ctrl->updateMotion();
        return true;
    }

    if (key == Qt::Key_P || key == Qt::Key_L) {
        if (key == Qt::Key_P) ctrl->m_keyHoistUp = pressed;
        else if (key == Qt::Key_L) ctrl->m_keyHoistDown = pressed;

        int dir = 0;
        if (ctrl->m_keyHoistUp) dir = +1;
        else if (ctrl->m_keyHoistDown) dir = -1;
        emit ctrl->hoistChanged(dir);
        ctrl->m_wasHoisting = (ctrl->m_keyHoistUp || ctrl->m_keyHoistDown);

        ctrl->updateMotion();
        return true;
    }

    return false;
}

void TeleopInputHandler::setVirtualKey(TeleopController *ctrl, int key, bool pressed)
{
    if (!ctrl) return;
    if (!ctrl->m_active && key != Qt::Key_Space) return;
    if (!updateMotionKeyState(ctrl, key, pressed)) return;

    if (key == Qt::Key_Space && !ctrl->m_active) {
        if (pressed) ctrl->m_timer->start();
        else         ctrl->m_timer->stop();
    }

    ctrl->updateMotion();
}

bool TeleopInputHandler::isKeyPressed(const TeleopController *ctrl, int key)
{
    if (!ctrl) return false;
    switch (key) {
    case Qt::Key_W:     return ctrl->m_keyForward;
    case Qt::Key_S:     return ctrl->m_keyBack;
    case Qt::Key_A:     return ctrl->m_keyLeft;
    case Qt::Key_D:     return ctrl->m_keyRight;
    case Qt::Key_Space: return ctrl->m_keyBrake;
    case Qt::Key_P:     return ctrl->m_keyHoistUp;
    case Qt::Key_L:     return ctrl->m_keyHoistDown;
    default: return false;
    }
}
