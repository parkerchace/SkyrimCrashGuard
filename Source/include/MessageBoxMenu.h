// Copyright (C) 2024-2026 Parker Chace
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of Skyrim Crash Guard.
//
// Skyrim Crash Guard is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version.
//
// Skyrim Crash Guard is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <https://www.gnu.org/licenses/>.

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
