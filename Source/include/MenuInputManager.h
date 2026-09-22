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
