/**
 * @file    NetworkConfigStyle.h
 * @layer   GUILayer
 * @brief   QSS dark theme styles for NetworkConfigDialog.
 * @deps    none
 * @note    Refactored for AI readability; runtime behavior & visual styling invariant.
 */

#pragma once

namespace NetworkConfigStyle {

inline const char *kDialogStyle = R"(
QDialog {
    background-color: #1a1d23;
    color: #c8d0d8;
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
    font-size: 13px;
}
QGroupBox {
    color: #00bcd4;
    font-weight: bold;
    border: 1px solid #2d3340;
    border-radius: 6px;
    margin-top: 10px;
    padding-top: 15px;
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
QLineEdit, QComboBox {
    background-color: #14171c;
    border: 1px solid #363d4d;
    border-radius: 4px;
    color: #ffffff;
    padding: 5px 8px;
    font-family: "Consolas", monospace;
    font-size: 13px;
}
QLineEdit:focus, QComboBox:focus {
    border: 1px solid #00bcd4;
}
QLineEdit:disabled, QComboBox:disabled {
    background-color: #181b20;
    color: #606875;
    border: 1px solid #282c35;
}
QPushButton {
    background-color: #252a35;
    color: #c8d0d8;
    border: 1px solid #3a3f4a;
    border-radius: 4px;
    padding: 6px 14px;
    font-weight: 600;
}
QPushButton:hover {
    background-color: #2e3544;
    border-color: #00bcd4;
    color: #ffffff;
}
QPushButton:pressed {
    background-color: #1a1d23;
}
QPushButton:disabled {
    background-color: #1e222a;
    color: #555d6b;
    border-color: #2d3340;
}
QPushButton#applyBtn {
    background-color: #1e6fd4;
    color: #ffffff;
    border: none;
    font-weight: 700;
}
QPushButton#applyBtn:hover {
    background-color: #2980e8;
}
QPushButton#applyBtn:disabled {
    background-color: #253346;
    color: #6a7f98;
}
QTextEdit {
    background-color: #111317;
    color: #48bb78;
    font-family: "Consolas", monospace;
    font-size: 12px;
    border: 1px solid #2a2f3b;
    border-radius: 4px;
    padding: 6px;
}
)";

} // namespace NetworkConfigStyle
