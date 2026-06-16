// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "InputDiagnostics.h"
#include "Config.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <SKSE/SKSE.h>

namespace CrashGuard {

    bool InputDiagnostics::IsEnabled() {
        return Config::Get().enableInputDiagnostics;
    }

    void InputDiagnostics::LogMenuOpen() {
        // === Diagnostic logging point: Menu opened ===
        // Marks the start of a menu session (Requirement 8.1).
        // Used to correlate subsequent input events with menu state.
        if (!IsEnabled()) return;
        spdlog::info("[InputDiagnostics] Menu opened");
    }

    void InputDiagnostics::LogMenuClose() {
        // === Diagnostic logging point: Menu closed ===
        // Marks the end of a menu session (Requirement 8.1).
        // Used to verify input state restoration after menu closes.
        if (!IsEnabled()) return;
        spdlog::info("[InputDiagnostics] Menu closed");
    }

    void InputDiagnostics::LogControlMapState() {
        // === Diagnostic logging point: ControlMap state ===
        // Logs Skyrim's ControlMap.ignoreKeyboardMouse flag (Requirement 2.1, 8.1).
        // This flag controls whether Skyrim processes keyboard/mouse input.
        // When true (gamepad mode), mouse input is blocked. When false, mouse input works.
        if (!IsEnabled()) return;

        auto controlMap = RE::ControlMap::GetSingleton();
        if (!controlMap) {
            spdlog::warn("[InputDiagnostics] ControlMap not available");
            return;
        }

        try {
            // CommonLibSSE-NG: Direct member access (no GetRuntimeData())
            spdlog::info("[InputDiagnostics] ControlMap.ignoreKeyboardMouse: {}", 
                controlMap->ignoreKeyboardMouse);
        } catch (const std::exception& e) {
            spdlog::error("[InputDiagnostics] Failed to access ControlMap data: {}", e.what());
        }
    }

    void InputDiagnostics::LogWndProcMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
        // === Diagnostic logging point: Windows message interception ===
        // Logs Windows messages received by the WndProc hook (Requirement 2.4, 6.1, 8.3).
        // Only logs mouse button messages to reduce noise in the log output.
        // This helps trace the input flow: Windows → WndProc → ImGui handler → io.MouseDown
        if (!IsEnabled()) return;

        // Only log mouse button messages to reduce noise
        switch (msg) {
            case WM_LBUTTONDOWN:
                spdlog::debug("[InputDiagnostics] WM_LBUTTONDOWN received at ({}, {})", 
                    LOWORD(lParam), HIWORD(lParam));
                break;
            case WM_LBUTTONUP:
                spdlog::debug("[InputDiagnostics] WM_LBUTTONUP received at ({}, {})", 
                    LOWORD(lParam), HIWORD(lParam));
                break;
            case WM_RBUTTONDOWN:
                spdlog::debug("[InputDiagnostics] WM_RBUTTONDOWN received at ({}, {})", 
                    LOWORD(lParam), HIWORD(lParam));
                break;
            case WM_RBUTTONUP:
                spdlog::debug("[InputDiagnostics] WM_RBUTTONUP received at ({}, {})", 
                    LOWORD(lParam), HIWORD(lParam));
                break;
            case WM_MBUTTONDOWN:
                spdlog::debug("[InputDiagnostics] WM_MBUTTONDOWN received at ({}, {})", 
                    LOWORD(lParam), HIWORD(lParam));
                break;
            case WM_MBUTTONUP:
                spdlog::debug("[InputDiagnostics] WM_MBUTTONUP received at ({}, {})", 
                    LOWORD(lParam), HIWORD(lParam));
                break;
            default:
                // Don't log other messages to avoid spam
                break;
        }
    }

    void InputDiagnostics::LogImGuiMouseState() {
        // === Diagnostic logging point: ImGui mouse state ===
        // Logs ImGui's internal mouse state (Requirement 2.7, 8.3).
        // This shows whether ImGui successfully received and processed mouse input.
        // Used to verify the input flow: Windows → WndProc → ImGui handler → io.MouseDown
        if (!IsEnabled()) return;

        ImGuiIO& io = ImGui::GetIO();
        
        spdlog::debug("[InputDiagnostics] ImGui.io.MouseDrawCursor: {}", io.MouseDrawCursor);
        spdlog::debug("[InputDiagnostics] ImGui.io.MousePos: ({}, {})", io.MousePos.x, io.MousePos.y);
        spdlog::debug("[InputDiagnostics] ImGui.io.MouseDown: [{}, {}, {}, {}, {}]",
            io.MouseDown[0], io.MouseDown[1], io.MouseDown[2], io.MouseDown[3], io.MouseDown[4]);
    }

    void InputDiagnostics::LogWidgetInteraction(const char* widgetType, bool hovered, bool clicked) {
        // === Diagnostic logging point: Widget interaction ===
        // Logs when ImGui widgets are hovered or clicked (Requirement 8.4).
        // This is the final step in the input flow, confirming clicks reached the widget.
        // Used to verify: Windows → WndProc → ImGui handler → io.MouseDown → widget detection
        if (!IsEnabled()) return;

        if (clicked) {
            spdlog::info("[InputDiagnostics] Widget '{}' clicked (hovered: {})", widgetType, hovered);
        } else if (hovered) {
            spdlog::debug("[InputDiagnostics] Widget '{}' hovered", widgetType);
        }
    }

    void InputDiagnostics::LogWndProcHookInstalled(void* hookAddress) {
        // === Diagnostic logging point: Hook installation ===
        // Logs when the WndProc hook is successfully installed (Requirement 8.2).
        // This confirms the hook is active and ready to intercept Windows messages.
        if (!IsEnabled()) return;
        spdlog::info("[InputDiagnostics] WndProc hook installed at {:p}", hookAddress);
    }

    void InputDiagnostics::LogWndProcHookRemoved() {
        // === Diagnostic logging point: Hook removal ===
        // Logs when the WndProc hook is removed (Requirement 8.2).
        // This confirms cleanup when the menu closes or renderer shuts down.
        if (!IsEnabled()) return;
        spdlog::info("[InputDiagnostics] WndProc hook removed");
    }

}  // namespace CrashGuard
