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

namespace CrashGuard {

    /**
     * @brief Low-level input event handler that intercepts ALL input before processing
     * This is the comprehensive solution for blocking inputs reliably
     */
    class InputEventHandler : public RE::BSTEventSink<RE::InputEvent*> {
    public:
        static InputEventHandler& GetSingleton() {
            static InputEventHandler instance;
            return instance;
        }

        void Install();

        // Process input events - return kStop to block, kContinue to allow
        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;

        // Control what gets blocked
        void SetBlockWaitMenu(bool block) { m_blockWaitMenu = block; }
        void SetBlockFavorites(bool block) { m_blockFavorites = block; }
        void SetBlockCombat(bool block) { m_blockCombat = block; }
        void SetBlockQuickSlots(bool block) { m_blockQuickSlots = block; }
        void SetBlockCameraZoom(bool block) { m_blockCameraZoom = block; }

        bool IsWaitMenuBlocked() const { return m_blockWaitMenu; }
        bool IsFavoritesBlocked() const { return m_blockFavorites; }
        bool IsCombatBlocked() const { return m_blockCombat; }
        bool IsQuickSlotsBlocked() const { return m_blockQuickSlots; }
        bool IsCameraZoomBlocked() const { return m_blockCameraZoom; }

    private:
        InputEventHandler() = default;
        ~InputEventHandler() = default;
        InputEventHandler(const InputEventHandler&) = delete;
        InputEventHandler& operator=(const InputEventHandler&) = delete;

        bool ShouldBlockButton(RE::ButtonEvent* button);
        bool ShouldBlockMouseWheel(RE::MouseMoveEvent* mouseMove);

        // Blocking flags
        bool m_blockWaitMenu = false;
        bool m_blockFavorites = false;
        bool m_blockCombat = false;
        bool m_blockQuickSlots = false;
        bool m_blockCameraZoom = false;
    };

}
