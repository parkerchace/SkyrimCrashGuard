// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.
//
// Input mode management adapted from Auto Input Switch by Parapets (Exit-9B)
// https://github.com/Exit-9B/AutoInputSwitch
// Licensed under MIT License

#include "MenuInputManager.h"
#include "ImGuiRenderer.h"
#include "InputDiagnostics.h"
#include <spdlog/spdlog.h>
#include <SKSE/SKSE.h>

namespace CrashGuard {

    void MenuInputManager::EnableMenuInput() {
        if (m_menuInputActive) {
            return; // Already enabled
        }

        // Log component responsibilities on first call
        static bool firstCall = true;
        if (firstCall) {
            spdlog::info("[MenuInputManager] Component Responsibilities:");
            spdlog::info("[MenuInputManager]   - ImGuiRenderer: Manages WndProc hook for Windows message interception");
            spdlog::info("[MenuInputManager]   - MenuInputManager: Manages ControlMap.ignoreKeyboardMouse flag only");
            firstCall = false;
        }

        spdlog::info("[MenuInputManager] Enabling menu input (forcing KB/M mode)");
        InputDiagnostics::LogMenuOpen();

        // === CRITICAL SECTION: ControlMap Input Mode Switching ===
        // Get ControlMap to manipulate Skyrim's input mode settings.
        // ControlMap.ignoreKeyboardMouse controls whether Skyrim processes KB/M input.
        // When gamepad is active, this flag is true, blocking mouse input.
        auto controlMap = RE::ControlMap::GetSingleton();
        if (!controlMap) {
            spdlog::error("[MenuInputManager] Failed to get ControlMap - using fallback mode");
            spdlog::error("[MenuInputManager] Menu will attempt to work with WndProc hook only");
            m_menuInputActive = true;
            return;
        }

        try {
            // CommonLibSSE-NG: Direct member access (no GetRuntimeData())
            // Store original state so we can restore it when menu closes
            m_wasIgnoringKeyboardMouse = controlMap->ignoreKeyboardMouse;
            
            // Log state before change (diagnostic logging point)
            spdlog::info("[MenuInputManager] ControlMap.ignoreKeyboardMouse before: {}", 
                controlMap->ignoreKeyboardMouse);
            InputDiagnostics::LogControlMapState();
            
            // === KEY FIX: Force keyboard/mouse input to be enabled ===
            // When Skyrim is in gamepad mode, ignoreKeyboardMouse is true.
            // Setting it to false tells Skyrim to process mouse input, allowing menu clicks.
            // This works in conjunction with the WndProc hook to enable full mouse functionality.
            controlMap->ignoreKeyboardMouse = false;
            
            m_menuInputActive = true;
            
            spdlog::info("[MenuInputManager] Menu input enabled (was ignoring KB/M: {})", 
                m_wasIgnoringKeyboardMouse);
            spdlog::info("[MenuInputManager] ControlMap.ignoreKeyboardMouse after: {}", 
                controlMap->ignoreKeyboardMouse);
            
            // Log state after change
            InputDiagnostics::LogControlMapState();
        } catch (const std::exception& e) {
            spdlog::error("[MenuInputManager] Failed to access ControlMap data: {}", e.what());
            spdlog::error("[MenuInputManager] Menu will attempt to work with WndProc hook only");
            m_menuInputActive = true;
        }
    }

    void MenuInputManager::RestoreGameInput() {
        if (!m_menuInputActive) {
            return; // Nothing to restore
        }

        spdlog::info("[MenuInputManager] Restoring game input mode");
        InputDiagnostics::LogMenuClose();  // Diagnostic logging point

        auto controlMap = RE::ControlMap::GetSingleton();
        if (!controlMap) {
            spdlog::error("[MenuInputManager] Failed to get ControlMap");
            m_menuInputActive = false;
            return;
        }

        try {
            // CommonLibSSE-NG: Direct member access (no GetRuntimeData())
            // === CRITICAL: Restore original input mode state ===
            // This ensures gamepad mode is re-enabled if it was active before menu opened.
            // Preserves the user's input mode preference (Requirement 4.2).
            controlMap->ignoreKeyboardMouse = m_wasIgnoringKeyboardMouse;
            
            m_menuInputActive = false;
            
            spdlog::info("[MenuInputManager] Game input restored (ignoring KB/M: {})", 
                m_wasIgnoringKeyboardMouse);
            
            // Log final state
            InputDiagnostics::LogControlMapState();
        } catch (const std::exception& e) {
            spdlog::error("[MenuInputManager] Failed to access ControlMap data: {}", e.what());
            m_menuInputActive = false;
        }
    }



    void MenuInputManager::LogInputState() const {
        spdlog::info("[MenuInputManager] === Input State Diagnostic ===");
        spdlog::info("[MenuInputManager]   m_menuInputActive: {}", m_menuInputActive);
        spdlog::info("[MenuInputManager]   m_wasIgnoringKeyboardMouse: {}", m_wasIgnoringKeyboardMouse);

        // Log ControlMap state
        auto controlMap = RE::ControlMap::GetSingleton();
        if (controlMap) {
            try {
                // CommonLibSSE-NG: Direct member access (no GetRuntimeData())
                spdlog::info("[MenuInputManager]   ControlMap.ignoreKeyboardMouse: {}", 
                    controlMap->ignoreKeyboardMouse);
            } catch (const std::exception& e) {
                spdlog::warn("[MenuInputManager]   ControlMap data access failed: {}", e.what());
            }
        } else {
            spdlog::warn("[MenuInputManager]   ControlMap: NOT AVAILABLE");
        }

        spdlog::info("[MenuInputManager] ================================");
    }

}
