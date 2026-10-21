/**
 * @file    StyleLoader.h
 * @module  GUILayer
 * @brief   Modular QSS stylesheet aggregator and dynamic runtime loader.
 * @deps    QApplication, QFile, QDir
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>

/**
 * @brief 样式加载器，负责加载并合并 resources/qss/ 下的所有模块化样式表。
 * 实现 QSS 与业务 C++ 逻辑的彻底解耦。
 */
class StyleLoader
{
public:
    /**
     * @brief 加载所有模块化 QSS 文件并拼接为统一的样式表字符串
     * @return 完整的合并后样式表内容
     */
    static QString loadAllStylesheets();

    /**
     * @brief 加载所有样式并将其应用到 QApplication 实例中
     */
    static void applyApplicationStyle();
};
