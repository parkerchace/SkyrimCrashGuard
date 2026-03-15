// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <RE/Skyrim.h>
#include <functional>

namespace CrashGuard {

/**
 * @brief Simple message box-based configuration menu
 * 
 * Uses Skyrim's native message box system - no ESP, SWF, or Papyrus needed!
 * Can be triggered via hotkey or console command.
 */
class MessageBoxMenu {
public:
    /**
     * @brief Show the main configuration menu
     */
    static void ShowMainMenu();
    
    /**
     * @brief Show validation settings submenu
     */
    static void ShowValidationMenu();
    
    /**
     * @brief Show advanced settings submenu
     */
    static void ShowAdvancedMenu();
    
    /**
     * @brief Show current status
     */
    static void ShowStatus();
    
    /**
     * @brief Register hotkey to open menu
     * Default: F11 key
     */
    static void RegisterHotkey();

private:
    /**
     * @brief Helper to show a message box with callback
     */
    static void ShowMessageBox(
        const std::string& title,
        const std::vector<std::string>& buttons,
        std::function<void(unsigned int)> callback
    );
    
    /**
     * @brief Helper to show a simple notification
     */
    static void ShowNotification(const std::string& message);
    
    /**
     * @brief Toggle a boolean setting
     */
    static void ToggleSetting(bool& setting, const std::string& name);
};

} // namespace CrashGuard
