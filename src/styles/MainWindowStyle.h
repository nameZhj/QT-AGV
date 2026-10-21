/**
 * @file    MainWindowStyle.h
 * @layer   GUILayer
 * @brief   Application dark palette initializers and status bar telemetry styles for MainWindow.
 * @deps    QPalette, QColor
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QPalette>
#include <QColor>

namespace MainWindowStyle {

inline const char *kMqttConnected    = "color:#5cb85c; background:transparent; border-right:1px solid #2d3340;";
inline const char *kMqttConnecting   = "color:#ecc94b; background:transparent; border-right:1px solid #2d3340;";
inline const char *kMqttDisconnected = "color:#e05555; background:transparent; border-right:1px solid #2d3340;";

inline void applyDarkPalette(QPalette &darkPalette)
{
    darkPalette.setColor(QPalette::Window, QColor(18, 21, 26));          // #12151a
    darkPalette.setColor(QPalette::WindowText, QColor(200, 208, 216));   // #c8d0d8
    darkPalette.setColor(QPalette::Base, QColor(30, 34, 40));            // #1e2228
    darkPalette.setColor(QPalette::AlternateBase, QColor(34, 38, 46));   // #22262e
    darkPalette.setColor(QPalette::Text, QColor(200, 208, 216));
    darkPalette.setColor(QPalette::Button, QColor(37, 42, 53));          // #252a35
    darkPalette.setColor(QPalette::ButtonText, QColor(200, 208, 216));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Highlight, QColor(30, 64, 128));      // #1e4080
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    darkPalette.setColor(QPalette::ToolTipBase, QColor(18, 21, 26));
    darkPalette.setColor(QPalette::ToolTipText, QColor(200, 208, 216));
    darkPalette.setColor(QPalette::PlaceholderText, QColor(122, 138, 153));
    darkPalette.setColor(QPalette::Active, QPalette::Window, QColor(18, 21, 26));
    darkPalette.setColor(QPalette::Inactive, QPalette::Window, QColor(18, 21, 26));
}

} // namespace MainWindowStyle
