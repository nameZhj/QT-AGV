/**
 * @file    MapEditKeyHandler.cpp
 * @layer   GUI
 * @brief   Key press event handling and tool cycling for map editor.
 * @deps    MapEditKeyHandler.h, MapEditTypes.h, TeleopController.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapEditKeyHandler.h"
#include "MapEditTypes.h"
#include "TeleopController.h"
#include <QApplication>
#include <QLineEdit>
#include <QAbstractSpinBox>
#include <QComboBox>

namespace MapEditKeyHandler {

using namespace MapEdit;

bool processKeyPress(QKeyEvent *event, const Handlers &h)
{
    QWidget *focused = QApplication::focusWidget();
    if (qobject_cast<QLineEdit*>(focused) || qobject_cast<QAbstractSpinBox*>(focused) || qobject_cast<QComboBox*>(focused)) {
        return false;
    }

    if (TeleopController::instance().isActive()) {
        int k = event->key();
        if (k == Qt::Key_W || k == Qt::Key_A || k == Qt::Key_S || k == Qt::Key_D ||
            k == Qt::Key_Space || k == Qt::Key_P || k == Qt::Key_L) {
            return true;
        }
    }

    int k = event->key();
    if (k == Qt::Key_Delete || k == Qt::Key_Backspace) {
        if (h.onDelete) h.onDelete();
        event->accept();
        return true;
    }
    if (k == Qt::Key_Escape) {
        if (h.onEscape) h.onEscape();
        event->accept();
        return true;
    }
    if (k == Qt::Key_M) { if (h.onSetMode) h.onSetMode(Point); event->accept(); return true; }
    if (k == Qt::Key_C) { if (h.onSetMode) h.onSetMode(Charge); event->accept(); return true; }
    if (k == Qt::Key_T) { if (h.onSetMode) h.onSetMode(HighPrecision); event->accept(); return true; }
    if (k == Qt::Key_P || k == Qt::Key_L) { if (h.onSetMode) h.onSetMode(Line); event->accept(); return true; }
    if (k == Qt::Key_B) { if (h.onSetMode) h.onSetMode(Curve); event->accept(); return true; }
    if (k == Qt::Key_A) { if (h.onSetMode) h.onSetMode(Area); event->accept(); return true; }
    if (k == Qt::Key_E) { if (h.onSetMode) h.onSetMode(Eraser); event->accept(); return true; }
    if (k == Qt::Key_F1) { if (h.onHelp) h.onHelp(); event->accept(); return true; }

    if (k == Qt::Key_Space && h.getCurrentMode && h.onSetMode) {
        int cur = h.getCurrentMode();
        int next = Drag;
        switch (cur) {
        case Drag:          next = Point; break;
        case Point:         next = Charge; break;
        case Charge:        next = HighPrecision; break;
        case HighPrecision: next = Line; break;
        case Line:          next = Curve; break;
        case Curve:         next = Area; break;
        case Area:          next = Brush; break;
        case Brush:         next = Eraser; break;
        case Eraser:        next = Drag; break;
        default:            next = Drag; break;
        }
        h.onSetMode(next);
        event->accept();
        return true;
    }

    return false;
}

} // namespace MapEditKeyHandler
