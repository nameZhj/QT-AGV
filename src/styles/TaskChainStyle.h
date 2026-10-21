/**
 * @file    TaskChainStyle.h
 * @layer   GUIStyles
 * @brief   Static QSS styling constants and themes for task chain workflow panels and cards.
 * @deps    QString
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>

namespace TaskChainStyle {

// === [Panels and Titles] ===
inline constexpr const char *S_PANEL_TITLE =
    "color:#b0b8c0; font-size:13px; font-weight:bold; padding:4px 6px; "
    "background:#222831; border-radius:4px;";

inline constexpr const char *S_LIST_STYLE =
    "QListWidget { background:#181d23; border:1px solid #2a3038; border-radius:4px; outline:none; }"
    "QListWidget::item { color:#c0c8d0; padding:6px 10px; border-bottom:1px solid #22272d; }"
    "QListWidget::item:selected, QListWidget::item:selected:!active { background:#2a5d8f; color:#fff; }"
    "QListWidget::item:hover { background:rgba(42,93,143,0.3); }";

inline constexpr const char *S_BTN_STYLE =
    "QPushButton { color:#c0c8d0; background:#2a313a; border:1px solid #3a4048; "
    "padding:5px 12px; border-radius:3px; font-size:12px; }"
    "QPushButton:hover { background:#323a44; color:#fff; }"
    "QPushButton:pressed { background:#222831; }"
    "QPushButton:disabled { color:#555; background:#222831; border-color:#2a3038; }";

inline constexpr const char *S_BOTTOM_BAR =
    "QWidget { background:#1a1f26; border-top:1px solid #2a3038; }"
    "QLabel { background:transparent; color:#8899aa; font-size:12px; }"
    "QPushButton { color:#c0c8d0; background:#2a313a; border:1px solid #3a4048; padding:5px 16px; border-radius:3px; font-size:12px; }"
    "QPushButton:hover { background:#323a44; color:#fff; }"
    "QPushButton:pressed { background:#222831; }"
    "QPushButton:disabled { color:#555; background:#222831; border-color:#2a3038; }";

// === [Cards and Items] ===
inline constexpr const char *kCheckBoxStyle =
    "QCheckBox{spacing:4px;} "
    "QCheckBox::indicator{width:16px;height:16px;border:1px solid #3a3f4a;border-radius:3px;background-color:#22262e;} "
    "QCheckBox::indicator:checked{background-color:#1e6fd4;border-color:#1e6fd4;image:url(:/check.svg);}";

inline constexpr const char *kDelBtnStyle =
    "QToolButton{background:transparent;border:none;color:#8899aa;font-size:13px;}QToolButton:hover{color:#e05555;}";

inline constexpr const char *kDelTagStyle =
    "QToolButton{background:transparent;border:none;color:#8899aa;font-size:12px;}QToolButton:hover{color:#e05555;}";

inline constexpr const char *kParamNoParamStyle =
    "QPushButton{text-align:left; background:#1e2329; border:1px solid #252b33; border-radius:3px; padding:6px 10px; color:#66707a; font-size:11px; margin-left:8px;}";

inline constexpr const char *kParamEditStyle =
    "QPushButton{text-align:left; background:#22272d; border:1px solid #2a3038; border-radius:3px; padding:6px 10px; color:#999; font-size:11px; margin-left:8px;}QPushButton:hover{border-color:#00bcd4; color:#c0c8d0;}";

inline constexpr const char *kInputStyle =
    "background:#22272d; border:1px solid #2a3038; border-radius:3px; padding:5px; color:#c0c8d0; font-size:12px;";

inline constexpr const char *kNoteEditStyle =
    "QTextEdit{background:#22272d; border:1px solid #2a3038; "
    "border-radius:3px; padding:3px 6px; color:#c0c8d0; font-size:11px;}"
    "QTextEdit:focus{border-color:#00bcd4;}";

inline constexpr const char *kTaskCardRoot =
    "#taskCardRoot { background: transparent; border: 1px solid transparent; border-radius: 4px; }";

inline constexpr const char *kGroupCardRoot =
    "#groupCardRoot { background: transparent; border: 1px solid transparent; border-radius: 4px; }";

// === [Runtime Card Highlights] ===
inline constexpr const char *kTaskCardActive =
    "#taskCardRoot { background: rgba(0, 188, 212, 0.20); border: 2px solid #00bcd4; border-radius: 4px; }";

inline constexpr const char *kGroupCardActive =
    "#groupCardRoot { background: rgba(56, 189, 248, 0.20); border: 2px solid #38bdf8; border-radius: 4px; }";

inline constexpr const char *kCardLongPressDrag =
    "background: rgba(0, 188, 212, 0.16); border: 2px dashed #00bcd4; border-radius: 4px;";

// === [Row Action ToolButtons] ===
inline constexpr const char *kExportBtnStyle =
    "QToolButton{background:transparent;border:none;color:#8899aa;font-size:14px;}QToolButton:hover{color:#00bcd4;}";

inline constexpr const char *kRenameBtnStyle =
    "QToolButton{background:transparent;border:none;color:#8899aa;font-size:14px;}QToolButton:hover{color:#f0a060;}";

inline constexpr const char *kDeleteBtnStyle =
    "QToolButton{background:transparent;border:none;color:#8899aa;font-size:14px;}QToolButton:hover{color:#e05555;}";

} // namespace TaskChainStyle

using namespace TaskChainStyle;
