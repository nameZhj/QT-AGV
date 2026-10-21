/**
 * @file    MapViewStyle.h
 * @layer   GUILayer
 * @brief   Dark theme stylesheet constants for MapView toolbar buttons, status labels, and menus.
 * @deps    QString
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>

namespace MapViewStyle {

inline const char *kToolBtnStyle =
    "QToolButton{background:#252a35;color:#c8d0d8;border:1px solid #3a3f4a;"
    "border-radius:4px;padding:3px 8px;font-size:11px;font-weight:600;}"
    "QToolButton:hover{background:#2d3340;border-color:#4a9eff;}"
    "QToolButton:pressed{background:#1e4080;}"
    "QToolButton:disabled{background:#1c1f26;color:#556070;border-color:#2a2e38;}";

inline const char *kToolBtnPrimary =
    "QToolButton{background:#1e6fd4;color:#fff;border:none;"
    "border-radius:4px;padding:3px 8px;font-size:11px;font-weight:600;}"
    "QToolButton:hover{background:#2980e8;}"
    "QToolButton:pressed{background:#155db8;}"
    "QToolButton:disabled{background:#1c1f26;color:#556070;border:none;}";

inline const char *kToolBtnDanger =
    "QToolButton{background:#c0392b;color:#fff;border:none;"
    "border-radius:4px;padding:3px 8px;font-size:11px;font-weight:600;}"
    "QToolButton:hover{background:#e74c3c;}"
    "QToolButton:pressed{background:#a93226;}";

inline const char *kMoreMenuStyle =
    "QMenu {"
    "  background: #1e2125;"
    "  color: #e2e8f0;"
    "  border: 1px solid #2d3340;"
    "  padding: 4px;"
    "}"
    "QMenu::item {"
    "  padding: 6px 22px;"
    "  border-radius: 3px;"
    "  font-size: 12px;"
    "}"
    "QMenu::item:selected {"
    "  background: #0ea5e9;"
    "  color: #ffffff;"
    "}"
    "QMenu::separator {"
    "  height: 1px;"
    "  background: #2d3340;"
    "  margin: 4px 6px;"
    "}";

inline const char *kToolbarWidgetStyle = "background:#12151a; border-bottom:1px solid #2d3340;";

inline const char *kStatusLabelStyle = "background:#0d1117; color:#8898a8; font-size:11px; padding:0 10px; border-bottom:1px solid #2d3340;";

} // namespace MapViewStyle
