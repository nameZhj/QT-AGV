/**
 * @file    DeviceCardLayout.h
 * @layer   GUILayer
 * @brief   Constructs child widget hierarchy and layout for DeviceCard.
 * @deps    DeviceCard.h, QVBoxLayout
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

class DeviceCard;
class QVBoxLayout;

class DeviceCardLayout
{
public:
    static void buildHeader(DeviceCard *card, QVBoxLayout *root);
    static void buildBody(DeviceCard *card, QVBoxLayout *root);
    static void buildFooter(DeviceCard *card, QVBoxLayout *root);
};
