/**
 * @file    MapEditKeyHandler.h
 * @layer   GUI
 * @brief   Keyboard shortcut event processor for map editing tools.
 * @deps    QKeyEvent, functional
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QKeyEvent>
#include <functional>

namespace MapEditKeyHandler {

struct Handlers {
    std::function<void()> onDelete;
    std::function<void()> onEscape;
    std::function<void(int mode)> onSetMode;
    std::function<void()> onHelp;
    std::function<int()> getCurrentMode;
};

bool processKeyPress(QKeyEvent *event, const Handlers &h);

} // namespace MapEditKeyHandler
