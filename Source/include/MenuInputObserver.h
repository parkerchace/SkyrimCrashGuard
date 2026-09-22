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
#include <unordered_set>
#include <string>

namespace CrashGuard {

    /**
     * @brief Observes menu open/close events to prevent input conflicts
     * 
     * Many Skyrim menus use mouse wheel and D-pad for scrolling, but these inputs
     * also control camera zoom and favorites menu. This observer automatically
     * blocks conflicting inputs when scrollable menus are open.
     * 
     * Features:
     * - Detects vanilla Skyrim scrollable menus
     * - Auto-detects modded menus using heuristics
     * - Supports user-defined custom menus via TOML config
     * - Pattern-based detection for common mod menu naming
     * 
     * Affected menus:
     * - DialogueMenu: Mouse wheel scrolls dialogue options but also zooms camera
     * - ContainerMenu: Mouse wheel scrolls items but also zooms camera
     * - BarterMenu: Mouse wheel scrolls items but also zooms camera
     * - And many more (see implementation)
     */
    class MenuInputObserver : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    public:
        static MenuInputObserver& GetSingleton() {
            static MenuInputObserver instance;
            return instance;
        }

        void Install();

        RE::BSEventNotifyControl ProcessEvent(
            const RE::MenuOpenCloseEvent* a_event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource) override;

        /**
         * @brief Get the set of auto-detected modded menus
         * @return Reference to the set of detected menu names
         */
        const std::unordered_set<std::string>& GetDetectedMenus() const {
            return m_detectedModdedMenus;
        }

    private:
        MenuInputObserver() = default;
        ~MenuInputObserver() = default;
        MenuInputObserver(const MenuInputObserver&) = delete;
        MenuInputObserver& operator=(const MenuInputObserver&) = delete;

        /**
         * @brief Check if a menu requires input blocking
         * @param menuName The name of the menu to check
         * @return true if the menu has scrollable content that conflicts with camera/favorites
         */
        bool IsScrollableMenu(const RE::BSFixedString& menuName) const;

        /**
         * @brief Check if menu name matches common modded menu patterns
         * @param menuName The name of the menu to check
         * @return true if the menu name suggests it's a scrollable modded menu
         */
        bool MatchesModdedMenuPattern(const std::string& menuName) const;

        /**
         * @brief Check if menu is whitelisted (should NOT have input blocking)
         * @param menuName The name of the menu to check
         * @return true if the menu is safe and should not have input blocking
         */
        bool IsMenuWhitelisted(const std::string& menuName) const;

        /**
         * @brief Update input blocking state based on currently open menus
         */
        void UpdateInputBlocking();

        /**
         * @brief Cache of detected modded menus to avoid repeated pattern matching
         */
        mutable std::unordered_set<std::string> m_detectedModdedMenus;
    };

}
