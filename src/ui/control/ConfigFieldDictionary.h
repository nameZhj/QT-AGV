/**
 * @file    ConfigFieldDictionary.h
 * @layer   GUILayer
 * @brief   Static dictionary for configuration field metadata (display names and units).
 * @deps    none
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QPair>

class ConfigFieldDictionary
{
public:
    static QPair<QString, QString> fieldMetaFor(const QString &key);
};
