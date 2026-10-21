/**
 * @file    DeviceCardDisplay.h
 * @layer   GUILayer
 * @brief   Handles visual style transitions and text field updates for DeviceCard.
 * @deps    DeviceCard.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

class DeviceCard;

class DeviceCardDisplay
{
public:
    static void setOnlineUI(DeviceCard *card, bool online);
    static void updateCardDisplay(DeviceCard *card);
};
