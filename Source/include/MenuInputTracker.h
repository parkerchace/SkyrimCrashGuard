// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <RE/Skyrim.h>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

namespace CrashGuard {

    /**
     * @brief Tracks which inputs are used by each menu for diagnostic purposes
     * 
     * This system monitors input events while menus are open to detect which
     * controls each menu actually uses. This helps users and mod developers
     * understand input conflicts without interfering with the mod's functionality.
     */
    class MenuInputTracker : public RE::BSTEventSink<RE::InputEvent*> {
    public:
        struct MenuInputProfile {
            std::string menuName;
            std::unordered_set<std::string> usedInputs;  // Input event names
            std::unordered_set<uint32_t> usedKeyCodes;   // Raw key codes
            uint32_t inputCount = 0;                      // Total inputs received
        };

        static MenuInputTracker& GetSingleton() {
            static MenuInputTracker instance;
            return instance;
        }

        void Install();

        /**
         * @brief Process input events and track which menu is using them
         */
        RE::BSEventNotifyControl ProcessEvent(
            RE::InputEvent* const* a_event,
            RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;

        /**
         * @brief Get the input profile for a specific menu
         */
        const MenuInputProfile* GetMenuProfile(const std::string& menuName) const;

        /**
         * @brief Get all tracked menu profiles
         */
        const std::unordered_map<std::string, MenuInputProfile>& GetAllProfiles() const {
            return m_menuProfiles;
        }

        /**
         * @brief Log the input profile for a menu
         */
        void LogMenuProfile(const std::string& menuName) const;

        /**
         * @brief Log all menu profiles
         */
        void LogAllProfiles() const;

    private:
        MenuInputTracker() = default;
        ~MenuInputTracker() = default;
        MenuInputTracker(const MenuInputTracker&) = delete;
        MenuInputTracker& operator=(const MenuInputTracker&) = delete;

        std::string GetCurrentOpenMenu() const;
        std::string GetInputEventName(RE::InputEvent* event) const;

        std::unordered_map<std::string, MenuInputProfile> m_menuProfiles;
    };

}
