// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "MenuInputTracker.h"
#include <spdlog/spdlog.h>

namespace CrashGuard {

    void MenuInputTracker::Install() {
        // DISABLED: Even read-only event sinks can interfere with input event ordering
        // This was causing QuickLoot gamepad inputs to not work correctly
        // Input tracking is now completely disabled to ensure zero interference
        
        spdlog::info("[MenuInputTracker] Input tracking DISABLED to prevent any interference");
        spdlog::info("[MenuInputTracker] All mods have complete, unmodified input event access");
        
        /* Original code - disabled
        auto inputDeviceManager = RE::BSInputDeviceManager::GetSingleton();
        if (inputDeviceManager) {
            inputDeviceManager->AddEventSink(this);
            spdlog::info("[MenuInputTracker] Installed input tracking for menu diagnostics");
        } else {
            spdlog::error("[MenuInputTracker] Failed to get input device manager");
        }
        */
    }

    std::string MenuInputTracker::GetCurrentOpenMenu() const {
        auto ui = RE::UI::GetSingleton();
        if (!ui) {
            return "";
        }

        // Check for open menus in priority order
        static const char* menuPriority[] = {
            "ContainerMenu", "BarterMenu", "GiftMenu", "DialogueMenu",
            "InventoryMenu", "MagicMenu", "FavoritesMenu", "BookMenu",
            "LevelUpMenu", "TrainingMenu", "StatsMenu", "JournalMenu",
            "MapMenu", nullptr
        };

        for (int i = 0; menuPriority[i] != nullptr; ++i) {
            if (ui->IsMenuOpen(menuPriority[i])) {
                return menuPriority[i];
            }
        }

        // Check all other open menus
        for (const auto& [name, menuEntry] : ui->menuMap) {
            if (menuEntry.menu) {
                return name.c_str();
            }
        }

        return "";
    }

    std::string MenuInputTracker::GetInputEventName(RE::InputEvent* event) const {
        if (!event) {
            return "Unknown";
        }

        // CommonLibSSE-NG: Use AsButtonEvent() for button events
        if (auto* buttonEvent = event->AsButtonEvent()) {
            auto eventName = buttonEvent->QUserEvent();
            if (!eventName.empty()) {
                return eventName.c_str();
            }
            return std::string("Button_") + std::to_string(buttonEvent->GetIDCode());
        }
        
        // CommonLibSSE-NG: Check event type for MouseMove and Char
        if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kMouseMove) {
            if (auto* idEvent = event->AsIDEvent()) {
                auto eventName = idEvent->QUserEvent();
                if (!eventName.empty()) {
                    return eventName.c_str();
                }
            }
            return "MouseMove";
        }
        
        if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kChar) {
            // CharEvent data - just return generic char event name
            return "CharEvent";
        }

        return "Unknown";
    }

    RE::BSEventNotifyControl MenuInputTracker::ProcessEvent(
        RE::InputEvent* const* a_event,
        RE::BSTEventSource<RE::InputEvent*>* a_eventSource) {
        
        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* event = *a_event;
        if (!event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Get the currently open menu
        std::string currentMenu = GetCurrentOpenMenu();
        if (currentMenu.empty()) {
            return RE::BSEventNotifyControl::kContinue;  // No menu open
        }

        // Track inputs for this menu
        auto& profile = m_menuProfiles[currentMenu];
        if (profile.menuName.empty()) {
            profile.menuName = currentMenu;
            spdlog::info("[MenuInputTracker] Started tracking inputs for menu: {}", currentMenu);
        }

        // Process all events in the linked list
        for (auto* currentEvent = event; currentEvent; currentEvent = currentEvent->next) {
            // CommonLibSSE-NG: Use AsButtonEvent() for button events
            if (auto* buttonEvent = currentEvent->AsButtonEvent()) {
                if (buttonEvent->IsDown() || buttonEvent->IsPressed()) {
                    std::string inputName = GetInputEventName(currentEvent);
                    uint32_t keyCode = buttonEvent->GetIDCode();
                    
                    // Track this input
                    bool isNew = profile.usedInputs.insert(inputName).second;
                    profile.usedKeyCodes.insert(keyCode);
                    profile.inputCount++;
                    
                    if (isNew) {
                        spdlog::debug("[MenuInputTracker] Menu '{}' uses input: {} (keycode: {})", 
                            currentMenu, inputName, keyCode);
                    }
                }
            }
            // CommonLibSSE-NG: Check event type for MouseMove
            else if (currentEvent->GetEventType() == RE::INPUT_EVENT_TYPE::kMouseMove) {
                if (auto* idEvent = currentEvent->AsIDEvent()) {
                    // Cast to MouseMoveEvent to access mouse delta
                    if (auto* mouseMoveEvent = static_cast<RE::MouseMoveEvent*>(idEvent)) {
                        if (mouseMoveEvent->mouseInputX != 0 || mouseMoveEvent->mouseInputY != 0) {
                            std::string inputName = GetInputEventName(currentEvent);
                            bool isNew = profile.usedInputs.insert(inputName).second;
                            profile.inputCount++;
                            
                            if (isNew) {
                                spdlog::debug("[MenuInputTracker] Menu '{}' uses input: {}", 
                                    currentMenu, inputName);
                            }
                        }
                    }
                }
            }
        }

        // Don't block - just observe
        return RE::BSEventNotifyControl::kContinue;
    }

    const MenuInputTracker::MenuInputProfile* MenuInputTracker::GetMenuProfile(const std::string& menuName) const {
        auto it = m_menuProfiles.find(menuName);
        if (it != m_menuProfiles.end()) {
            return &it->second;
        }
        return nullptr;
    }

    void MenuInputTracker::LogMenuProfile(const std::string& menuName) const {
        auto* profile = GetMenuProfile(menuName);
        if (!profile) {
            spdlog::info("[MenuInputTracker] No input profile found for menu: {}", menuName);
            return;
        }

        spdlog::info("[MenuInputTracker] ========================================");
        spdlog::info("[MenuInputTracker] Input Profile for: {}", profile->menuName);
        spdlog::info("[MenuInputTracker] Total inputs received: {}", profile->inputCount);
        spdlog::info("[MenuInputTracker] Unique inputs used: {}", profile->usedInputs.size());
        spdlog::info("[MenuInputTracker] ----------------------------------------");
        
        for (const auto& input : profile->usedInputs) {
            spdlog::info("[MenuInputTracker]   - {}", input);
        }
        
        spdlog::info("[MenuInputTracker] ========================================");
    }

    void MenuInputTracker::LogAllProfiles() const {
        if (m_menuProfiles.empty()) {
            spdlog::info("[MenuInputTracker] No menu input profiles recorded yet");
            return;
        }

        spdlog::info("[MenuInputTracker] ========================================");
        spdlog::info("[MenuInputTracker] ALL MENU INPUT PROFILES");
        spdlog::info("[MenuInputTracker] ========================================");
        
        for (const auto& [menuName, profile] : m_menuProfiles) {
            spdlog::info("[MenuInputTracker] ");
            spdlog::info("[MenuInputTracker] Menu: {}", profile.menuName);
            spdlog::info("[MenuInputTracker]   Total inputs: {}", profile.inputCount);
            spdlog::info("[MenuInputTracker]   Unique inputs: {}", profile.usedInputs.size());
            spdlog::info("[MenuInputTracker]   Used inputs:");
            
            for (const auto& input : profile.usedInputs) {
                spdlog::info("[MenuInputTracker]     - {}", input);
            }
        }
        
        spdlog::info("[MenuInputTracker] ========================================");
    }

}
