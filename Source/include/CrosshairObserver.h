// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

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
