#pragma once

#include <QColor>

/**
 * @brief 集中管理自绘控件（QPainter）与动态渲染使用的具名主题颜色体系。
 * 遵循样式与业务逻辑解耦规范，杜绝在 paintEvent 中直接硬编码裸色值。
 */
namespace ThemeColors {

// ── 基础背景与面板色 ──
inline const QColor BackgroundDark()     { return QColor(18, 21, 26); }       // #12151a
inline const QColor BackgroundSurface()  { return QColor(26, 29, 35); }       // #1a1d23
inline const QColor BackgroundElevated() { return QColor(30, 34, 40); }       // #1e2228
inline const QColor BackgroundItem()     { return QColor(34, 38, 46); }       // #22262e
inline const QColor BorderDark()         { return QColor(45, 51, 64); }       // #2d3340

// ── 业务与状态语义色 ──
inline const QColor Primary()            { return QColor(30, 111, 212); }     // #1e6fd4
inline const QColor PrimaryHover()       { return QColor(41, 128, 232); }     // #2980e8
inline const QColor Success()            { return QColor(16, 185, 129); }     // #10b981
inline const QColor Warning()            { return QColor(245, 158, 11); }     // #f59e0b
inline const QColor Danger()             { return QColor(239, 68, 68); }      // #ef4444
inline const QColor Info()               { return QColor(2, 132, 199); }      // #0284c7

// ── 车辆与方向盘自绘控件专用色 ──
namespace DirectionPad {
    inline const QColor Normal()         { return QColor(28, 114, 159); }     // #1c729f 科技蓝
    inline const QColor Hover()          { return QColor(36, 138, 194); }     // #248ac2 悬停亮蓝
    inline const QColor Pressed()        { return QColor(157, 36, 20); }      // #9d2414 激活按下红
}

// ── 地图与画布自绘控件色 ──
namespace Canvas {
    inline const QColor Background()     { return QColor(18, 21, 26); }       // #12151a
    inline const QColor BlackBg()        { return QColor(0, 0, 0); }          // #000000
    inline const QColor BrushCursor()    { return QColor(24, 144, 255); }     // #1890ff
    inline const QColor GridLine()       { return QColor(45, 51, 64, 120); }  // 半透明网格
    inline const QColor PathLine()       { return QColor(74, 158, 255); }     // 路径连线蓝
    inline const QColor StationPoint()   { return QColor(255, 170, 0); }      // 站点橙黄
}

// ── ToggleSwitch 自绘色 ──
namespace Toggle {
    inline const QColor TrackOn()        { return QColor(30, 111, 212); }     // #1e6fd4
    inline const QColor TrackOff()       { return QColor(58, 63, 74); }       // #3a3f4a
    inline const QColor Knob()           { return QColor(255, 255, 255); }    // #ffffff
    inline const QColor KnobDisabled()   { return QColor(240, 240, 240, 180); }
}

// ── Toast 提示色 ──
namespace ToastColor {
    inline const QColor CardBg()         { return QColor(30, 41, 59, 242); }  // rgba(30,41,59,0.95)
    inline const QColor CardBorder()     { return QColor(255, 255, 255, 25); } // rgba(255,255,255,0.10)
    inline const QColor SuccessAccent()  { return QColor(16, 185, 129); }     // #10b981
    inline const QColor ErrorAccent()    { return QColor(239, 68, 68); }      // #ef4444
    inline const QColor WarningAccent()  { return QColor(245, 158, 11); }     // #f59e0b
    inline const QColor InfoAccent()     { return QColor(2, 132, 199); }      // #0284c7
}

} // namespace ThemeColors
