/**
 * @file    RobotStatusStyle.h
 * @layer   GUILayer
 * @brief   Dark theme QSS style constants for RobotStatusDialog tabs and frames.
 * @deps    none
 * @note    Refactored for AI readability; runtime behavior & visual styling invariant.
 */

#pragma once

namespace RobotStatusStyle {

inline const char *kDialogStyle = R"(
QDialog {
  background: #181b22;
  color: #e2e8f0;
}
QTabWidget::pane {
  border: none;
  background: #181b22;
}
QTabBar::tab {
  background: transparent;
  color: #94a3b8;
  padding: 8px 18px;
  font-size: 13px;
  font-weight: 500;
  border-bottom: 2px solid transparent;
}
QTabBar::tab:hover {
  color: #f1f5f9;
}
QTabBar::tab:selected {
  color: #ffffff;
  border-bottom: 2px solid #3b82f6;
  font-weight: bold;
}
QScrollArea {
  border: none;
  background: transparent;
}
QScrollBar:vertical {
  background: #181b22;
  width: 8px;
  margin: 0px;
}
QScrollBar::handle:vertical {
  background: #334155;
  min-height: 20px;
  border-radius: 4px;
}
QScrollBar::handle:vertical:hover {
  background: #475569;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
  height: 0px;
}
)";

inline const char *kTopFrameStyle =
    "QFrame#topFrame { background: #16191f; border: 1px solid #2d3340; border-radius: 4px; padding: 12px; }";

inline const char *kBotFrameStyle =
    "QFrame#botFrame { background: #16191f; border: 1px solid #2d3340; border-radius: 4px; padding: 10px; }";

} // namespace RobotStatusStyle
