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

#include <Windows.h>
#include <cstdint>

namespace CrashGuard {

    /// Diagnostic logging system for F11 menu input debugging
    /// Traces input flow from Windows messages through ImGui to widget activation
    class InputDiagnostics {
    public:
        /// Log when F11 menu opens
        static void LogMenuOpen();

        /// Log when F11 menu closes
        static void LogMenuClose();

        /// Log ControlMap state (ignoreKeyboardMouse flag)
        static void LogControlMapState();

        /// Log Windows WndProc message received
        static void LogWndProcMessage(UINT msg, WPARAM wParam, LPARAM lParam);

        /// Log ImGui mouse button state
        static void LogImGuiMouseState();

        /// Log widget interaction (hover/click)
        static void LogWidgetInteraction(const char* widgetType, bool hovered, bool clicked);

        /// Log WndProc hook installation
        static void LogWndProcHookInstalled(void* hookAddress);

        /// Log WndProc hook removal
        static void LogWndProcHookRemoved();

        /// Check if diagnostic mode is enabled
        static bool IsEnabled();

    private:
        InputDiagnostics() = delete;
    };

}  // namespace CrashGuard
