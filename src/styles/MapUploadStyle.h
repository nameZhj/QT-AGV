/**
 * @file    MapUploadStyle.h
 * @layer   GUILayer
 * @brief   Dark theme stylesheet constants for MapUploadDialog.
 * @deps    none
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

namespace MapUploadStyle {

inline const char *kDialogStyle = R"(
QDialog {
    background-color: #1a1d23;
    color: #c8d0d8;
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
    font-size: 13px;
}
QGroupBox {
    color: #4a9eff;
    font-weight: bold;
    border: 1px solid #2d3340;
    border-radius: 6px;
    margin-top: 10px;
    padding-top: 14px;
    background-color: #1f232b;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    padding: 0 4px;
    background-color: #1f232b;
}
QLabel {
    color: #a0aec0;
    font-size: 12px;
}
QLineEdit {
    background-color: #14171c;
    border: 1px solid #363d4d;
    border-radius: 4px;
    color: #ffffff;
    padding: 5px 8px;
    font-size: 12px;
}
QLineEdit:focus {
    border: 1px solid #4a9eff;
}
QLineEdit:read-only {
    background-color: #181b21;
    color: #8fa0b5;
}
QPushButton {
    background-color: #252a35;
    border: 1px solid #3a3f4a;
    border-radius: 4px;
    color: #c8d0d8;
    padding: 5px 12px;
    font-size: 12px;
}
QPushButton:hover {
    background-color: #2d3340;
    border-color: #4a9eff;
    color: #ffffff;
}
QPushButton#primaryBtn {
    background-color: #1e4080;
    border: 1px solid #4a9eff;
    color: #ffffff;
    font-weight: bold;
}
QPushButton#primaryBtn:hover {
    background-color: #2550a0;
}
)";

} // namespace MapUploadStyle
