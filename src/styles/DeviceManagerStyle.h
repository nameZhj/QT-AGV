/**
 * @file    DeviceManagerStyle.h
 * @layer   GUILayer
 * @brief   Dark theme stylesheet constants for multi-device management dialog and tool buttons.
 * @deps    none
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

namespace DeviceManagerStyle {

inline const char *kDlgStyle = R"(
QDialog { background:#1a1d23; color:#c8d0d8; font-family:"Microsoft YaHei UI","Segoe UI",sans-serif; }
QLineEdit {
    background:#22262e; border:1px solid #3a3f4a; border-radius:4px;
    color:#c8d0d8; padding:6px 10px; font-size:13px;
}
QLineEdit:focus { border-color:#00bcd4; }
QPushButton {
    border-radius:4px; padding:6px 14px; font-size:12px; font-weight:600;
    border:1px solid #3a3f4a; background:#252a35; color:#c8d0d8;
}
QPushButton:hover { background:#2d3340; border-color:#4a9eff; }
QPushButton:pressed { background:#1e4080; }
QPushButton:disabled { color:#445566; border-color:#2d3340; }
QScrollArea { border:none; background:transparent; }
QScrollBar:vertical { background:#1a1d23; width:6px; border-radius:3px; }
QScrollBar::handle:vertical { background:#3a3f4a; border-radius:3px; min-height:20px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
)";

inline const char *kBtnPrimary =
    "QPushButton{background:#00bcd4;color:#fff;border:none;"
    "border-radius:4px;padding:0 14px;font-size:12px;font-weight:700;}"
    "QPushButton:hover{background:#00acc1;}";

inline const char *kBtnOutline =
    "QPushButton{background:transparent;color:#00bcd4;"
    "border:1px solid #00bcd4;border-radius:4px;"
    "padding:0 12px;font-size:12px;}"
    "QPushButton:hover{background:rgba(0,188,212,0.1);}";

inline const char *kBtnEnterHome =
    "QPushButton{background:#1e3a5f;color:#38bdf8;border:1px solid #0284c7;"
    "border-radius:4px;padding:0 14px;font-size:12px;font-weight:700;}"
    "QPushButton:hover{background:#0284c7;color:#fff;}";

} // namespace DeviceManagerStyle
