// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

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
