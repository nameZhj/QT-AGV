/**
 * @file    CreateMapStyle.h
 * @layer   GUILayer
 * @brief   Dark theme stylesheet constants for SLAM online mapping widgets and teleoperation HUD.
 * @deps    none
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

namespace CreateMapStyle {

inline const char *kHeaderBar = "background:#1e2d3d; border-bottom:1px solid rgba(255,255,255,0.06);";

inline const char *kBackBtn =
    "QPushButton{color:white;border:none;border-radius:4px;padding:4px 12px;font-size:12px;background:transparent;} "
    "QPushButton:hover{background:rgba(255,255,255,18);}";

inline const char *kDivider = "color:rgba(255,255,255,0.08);";

inline const char *kTitleLabel = "color:#9ab8d8;font-weight:bold;font-size:13px;";

inline const char *kActionBtn =
    "QPushButton { padding: 6px 12px; background: #007bff; color: white; border: none; border-radius: 4px; } "
    "QPushButton:hover { background: #0056b3; } "
    "QPushButton:disabled { opacity: 0.5; }";

inline const char *kCanvasBackground = "background: #1a1a2e;";

inline const char *kBrakeBtn = "QPushButton { background: #f44336; color: white; padding: 8px; }";

} // namespace CreateMapStyle
