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
#include <SKSE/SKSE.h>

namespace CrashGuard {

    /**
     * @brief Observes crosshair target changes to detect when player is looking at containers
     * 
     * This is used to prevent camera zoom when QuickLootIE or other loot menus would be active.
     * When the player's crosshair is on a lootable container, we block camera zoom to prevent
     * mouse wheel from zooming the camera instead of scrolling the loot menu.
     */
    class CrosshairObserver : public RE::BSTEventSink<SKSE::CrosshairRefEvent> {
    public:
        static CrosshairObserver& GetSingleton() {
            static CrosshairObserver instance;
            return instance;
        }

        void Install();
        
        RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* a_event, RE::BSTEventSource<SKSE::CrosshairRefEvent>* a_eventSource) override;

        bool IsLookingAtContainer() const { return m_lookingAtContainer; }

    private:
        CrosshairObserver() = default;
        ~CrosshairObserver() = default;
        CrosshairObserver(const CrosshairObserver&) = delete;
        CrosshairObserver& operator=(const CrosshairObserver&) = delete;

        bool m_lookingAtContainer = false;

        /**
         * @brief Check if a reference is a lootable container
         */
        bool IsLootableContainer(RE::TESObjectREFR* ref) const;
    };

}
