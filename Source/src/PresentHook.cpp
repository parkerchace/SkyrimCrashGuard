// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "PCH.h"
#include "PresentHook.h"
#include "Config.h"
#include "ImGuiRenderer.h"
#include "ImGuiConfigMenu.h"
#include "InputBlocker.h"
#include "CrosshairObserver.h"
#include "MenuInputObserver.h"
#include "HotkeyManager.h"
#include "PerformanceMetrics.h"
#include "RecoveryNotifications.h"
#include "MemoryPressureDetector.h"
#include "PhaseTracker.h"

namespace CrashGuard {

    PresentHook::Present_t PresentHook::s_originalPresent = nullptr;

    int __stdcall PresentHook::Present_Hook(IDXGISwapChain* swapChain, unsigned int syncInterval, unsigned int flags) {
        // Update performance metrics exactly once per frame (internal counters only, safe)
        auto updateStart = std::chrono::high_resolution_clock::now();
        PerformanceMonitor::GetSingleton().Update();
        auto updateEnd = std::chrono::high_resolution_clock::now();
        
        // ═══════════════════════════════════════════════════════════════════════
        // !! CRITICAL: DO NOT ACCESS GAME STATE FROM RENDER THREAD !!
        // ═══════════════════════════════════════════════════════════════════════
        // Present runs on the RENDER THREAD, not the main game thread.
        // Accessing RE::Sky, RE::PlayerCharacter, RE::ProcessLists, etc. from
        // here causes race conditions that corrupt weather, lighting, and actors.
        // ═══════════════════════════════════════════════════════════════════════
        
        // Log performance overhead on first few frames to verify <0.1ms requirement
        static int frameCount = 0;
        if (frameCount < 10) {
            auto updateDuration = std::chrono::duration_cast<std::chrono::microseconds>(updateEnd - updateStart);
            float updateMs = updateDuration.count() / 1000.0f;
            spdlog::debug("[PresentHook] PerformanceMonitor::Update() took {:.3f}ms (target: <0.1ms)", updateMs);
            frameCount++;
        }
        
        // Initialize ImGui on first frame
        static bool initialized = false;
        if (!initialized) {
            if (ImGuiRenderer::GetInstance().Initialize()) {
                initialized = true;
                
                // Initialize hotkey system
                HotkeyManager::GetSingleton().Initialize();
                
                // Register menu toggle hotkeys (both keyboard and gamepad)
                HotkeyManager::GetSingleton().RegisterHotkey(
                    "ToggleMenu_Keyboard",
                    HotkeyManager::GetDefaultKeyboardBinding(),
                    []() { ImGuiConfigMenu::GetSingleton().Toggle(); }
                );
                
                HotkeyManager::GetSingleton().RegisterHotkey(
                    "ToggleMenu_Gamepad",
                    HotkeyManager::GetDefaultGamepadBinding(),
                    []() { ImGuiConfigMenu::GetSingleton().Toggle(); }
                );
                
                // Install crosshair observer to detect when player is looking at containers
                // This prevents camera zoom when QuickLootIE or other loot menus are active
                CrosshairObserver::GetSingleton().Install();
                
                // Install menu input observer to prevent input conflicts in scrollable menus
                // This prevents camera zoom and favorites menu conflicts in dialogue, containers, etc.
                MenuInputObserver::GetSingleton().Install();
            }
        }

        // Update hotkey system
        if (initialized) {
            HotkeyManager::GetSingleton().Update();
        }

        bool menuVisible = ImGuiConfigMenu::GetSingleton().IsVisible();
        
        // Check if any standard Skyrim UI menu is open (inventory, map, etc.)
        bool gameMenuOpen = false;
        auto ui = RE::UI::GetSingleton();
        if (ui) {
            static const char* menuNames[] = {
                "InventoryMenu", "MagicMenu", "MapMenu", "StatsMenu",
                "ContainerMenu", "DialogueMenu", "BarterMenu", "GiftMenu",
                "JournalMenu", "LockpickingMenu", "TweenMenu", "FavoritesMenu",
                "CraftingMenu", "TrainingMenu", "TutorialMenu", "CustomMenu",
                nullptr
            };
            
            for (int i = 0; menuNames[i] != nullptr; ++i) {
                if (ui->IsMenuOpen(menuNames[i])) {
                    gameMenuOpen = true;
                    break;
                }
            }
        }
        
        // Note: Camera zoom blocking for containers (QuickLootIE) is handled by CrosshairObserver

        // Render ImGui overlay
        if (initialized && ImGuiRenderer::GetInstance().IsInitialized()) {
            ImGuiRenderer::GetInstance().NewFrame();
            
            // Render the menu (ImGui input capture is handled in ImGuiRenderer::NewFrame)
            ImGuiConfigMenu::GetSingleton().Render();
            
            // Render recovery toast notifications (always visible, even when menu is closed)
            CrashGuard::RecoveryNotifications::GetSingleton().RenderToasts();
            
            ImGuiRenderer::GetInstance().RenderDrawData();
        }

        // Call original Present
        return s_originalPresent(swapChain, syncInterval, flags);
    }

    bool PresentHook::Install() {
        spdlog::info("[PresentHook] Installing Present hook");
        
        // Skyrim VR uses the SteamVR compositor instead of a D3D11 swap chain.
        // The ImGui overlay only works on the standard Present hook, which doesn't
        // exist in VR. To show ImGui in VR you'd need to hook IVRCompositor::Submit,
        // which is out of scope. Unless the user explicitly opts in via allowInVR = true,
        // we skip overlay installation on VR.
        if (REL::Module::IsVR() && !Config::Get().allowImGuiInVR) {
            spdlog::info("[PresentHook] VR detected - ImGui overlay disabled (use TOML config)");
            spdlog::info("[PresentHook] To configure CrashGuard on VR, edit SkyrimCrashGuard.toml and set allowInVR = true in [ImGui]");
            return false;  // Not a failure, just not supported
        }
        if (REL::Module::IsVR() && Config::Get().allowImGuiInVR) {
            spdlog::info("[PresentHook] VR detected - ImGui overlay enabled by config (allowInVR=true). Proceeding, overlay may require additional hooks to fully work in VR");
        }
        
        // Get the swap chain from BSRenderManager (replaces BSGraphics::Renderer in v3.6.0+)
        auto renderer = RE::BSRenderManager::GetSingleton();
        if (!renderer) {
            spdlog::error("[PresentHook] Failed to get BSRenderManager");
            return false;
        }
        
        auto& render_data = renderer->GetRuntimeData();
        auto swapChain = render_data.swapChain;
        if (!swapChain) {
            spdlog::error("[PresentHook] Failed to get swap chain");
            return false;
        }
        
        // Get the vtable
        void** vtable = *reinterpret_cast<void***>(swapChain);
        
        // Hook Present (index 8 in IDXGISwapChain vtable)
        s_originalPresent = reinterpret_cast<Present_t>(vtable[8]);
        
        // Replace with our hook
        DWORD oldProtect;
        VirtualProtect(&vtable[8], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
        vtable[8] = reinterpret_cast<void*>(&Present_Hook);
        VirtualProtect(&vtable[8], sizeof(void*), oldProtect, &oldProtect);
        
        spdlog::info("[PresentHook] Present hook installed successfully");
        return true;
    }

    void PresentHook::Uninstall() {
        if (s_originalPresent) {
            spdlog::info("[PresentHook] Uninstalling Present hook");
            
            // Get the swap chain
            auto renderer = RE::BSRenderManager::GetSingleton();
            if (renderer) {
                auto& render_data = renderer->GetRuntimeData();
                auto swapChain = render_data.swapChain;
                if (swapChain) {
                    void** vtable = *reinterpret_cast<void***>(swapChain);
                    
                    // Restore original
                    DWORD oldProtect;
                    VirtualProtect(&vtable[8], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
                    vtable[8] = reinterpret_cast<void*>(s_originalPresent);
                    VirtualProtect(&vtable[8], sizeof(void*), oldProtect, &oldProtect);
                }
            }
            
            s_originalPresent = nullptr;
            spdlog::info("[PresentHook] Present hook uninstalled");
        }
    }

}
