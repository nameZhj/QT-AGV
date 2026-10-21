/**
 * @file    MotionDialogStyle.h
 * @layer   GUILayer
 * @brief   Dark theme QSS style constants for MotionDialog widgets.
 * @deps    none
 * @note    Refactored for AI readability; visual styling invariant.
 */

#pragma once

namespace MotionDialogStyle {

inline const char *kDialogStyle = R"(
QDialog {
  background-color: #24272c;
  border: 1px solid #3d434d;
  border-radius: 4px;
}
QLabel {
  color: #c5cbd5;
  font-size: 13px;
}
QTabWidget::pane {
  border: none;
  background: transparent;
}
QTabBar::tab {
  background: #2a2e35;
  color: #9aa1ad;
  border: 1px solid #3a404b;
  border-bottom: none;
  padding: 5px 16px;
  margin-right: 2px;
  border-top-left-radius: 3px;
  border-top-right-radius: 3px;
  font-size: 13px;
}
QTabBar::tab:selected {
  background: #24272c;
  color: #ffffff;
  border-color: #4b5360;
}
QDoubleSpinBox {
  background-color: #1a1d21;
  color: #e2e8f0;
  border: 1px solid #3d434d;
  border-radius: 3px;
  padding: 3px 6px;
  font-size: 13px;
}
QDoubleSpinBox:focus {
  border: 1px solid #3b82f6;
}
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
  background-color: #2a2e35;
  border-left: 1px solid #3d434d;
  width: 16px;
}
QRadioButton {
  color: #c5cbd5;
  font-size: 13px;
  spacing: 6px;
}
QRadioButton::indicator {
  width: 14px;
  height: 14px;
}
QRadioButton::indicator:unchecked {
  border: 2px solid #64748b;
  border-radius: 7px;
  background: transparent;
}
QRadioButton::indicator:checked {
  border: 2px solid #0ea5e9;
  border-radius: 7px;
  background: #0ea5e9;
}
QPushButton {
  background-color: #2b2f36;
  color: #e2e8f0;
  border: 1px solid #454d59;
  border-radius: 3px;
  padding: 5px 14px;
  font-size: 13px;
}
QPushButton:hover:!disabled {
  background-color: #373d47;
  border-color: #586373;
}
QPushButton:pressed:!disabled {
  background-color: #1f2328;
}
QPushButton:disabled {
  background-color: #22252a;
  color: #555c68;
  border-color: #323740;
}
)";

inline const char *kTitleBarStyle =
    "background-color: #1e2125; border-top-left-radius: 4px; border-top-right-radius: 4px;";

inline const char *kCloseBtnStyle = R"(
QPushButton {
  background: transparent;
  color: #9aa1ad;
  border: none;
  font-size: 13px;
  padding: 0;
}
QPushButton:hover {
  background: #dc2626;
  color: #ffffff;
  border-radius: 2px;
}
)";

inline const char *kPanelStyle =
    "QFrame { border: 1px solid #383e47; border-radius: 4px; background-color: #212428; }";

} // namespace MotionDialogStyle
