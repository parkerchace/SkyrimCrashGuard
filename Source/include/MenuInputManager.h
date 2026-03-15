// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.
//
// Input mode management adapted from Auto Input Switch by Parapets (Exit-9B)
// https://github.com/Exit-9B/AutoInputSwitch
// Licensed under MIT License

#pragma once

namespace CrashGuard {

    // Manages temporary input mode switching for F11 menu
    // When menu opens: forces keyboard/mouse mode to allow mouse input
    // When menu closes: restores gamepad mode if it was active
    class MenuInputManager {
    public:
        static MenuInputManager& GetSingleton() {
            static MenuInputManager instance;
            return instance;
        }

        // Call when F11 menu opens - forces KB/M mode
        void EnableMenuInput();

        // Call when F11 menu closes - restores gamepad mode
        void RestoreGameInput();

        // Check if we successfully enabled menu input
        bool IsMenuInputActive() const { return m_menuInputActive; }

        // Log current input state for diagnostics
        void LogInputState() const;

    private:
        MenuInputManager() = default;
        ~MenuInputManager() = default;
        MenuInputManager(const MenuInputManager&) = delete;
        MenuInputManager& operator=(const MenuInputManager&) = delete;

        bool m_menuInputActive = false;
        bool m_wasIgnoringKeyboardMouse = false;
    };

}
