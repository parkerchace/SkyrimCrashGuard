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

namespace CrashGuard {

    class InputBlocker {
    public:
        static InputBlocker& GetSingleton() {
            static InputBlocker instance;
            return instance;
        }

        void SetBlocked(bool blocked);
        bool IsBlocked() const { return m_blocked; }

        /**
         * @brief Block camera zoom controls when game menus are open
         * This prevents mouse wheel from zooming camera while allowing menu scrolling
         */
        void SetCameraZoomBlocked(bool blocked);
        bool IsCameraZoomBlocked() const { return m_cameraZoomBlocked; }

        /**
         * @brief Block favorites menu (D-pad Down) when loot menus are open
         * This prevents D-pad from opening favorites while allowing QuickLoot scrolling
         */
        void SetFavoritesMenuBlocked(bool blocked);
        bool IsFavoritesMenuBlocked() const { return m_favoritesMenuBlocked; }

        /**
         * @brief Block wait/tween menu (Back/Select button) when menus are open
         * This prevents back button from opening wait menu while in other menus
         */
        void SetWaitMenuBlocked(bool blocked);
        bool IsWaitMenuBlocked() const { return m_waitMenuBlocked; }

        /**
         * @brief Block all combat controls when menus are open
         * Prevents accidental attacks, blocks, shouts while navigating menus
         */
        void SetCombatBlocked(bool blocked);
        bool IsCombatBlocked() const { return m_combatBlocked; }

        /**
         * @brief Block quick slots (1-8 keys, D-pad left/right) when menus are open
         * Prevents accidental item/spell switching while in menus
         */
        void SetQuickSlotsBlocked(bool blocked);
        bool IsQuickSlotsBlocked() const { return m_quickSlotsBlocked; }

    private:
        InputBlocker() = default;
        ~InputBlocker() = default;
        InputBlocker(const InputBlocker&) = delete;
        InputBlocker& operator=(const InputBlocker&) = delete;

        bool m_blocked = false;
        bool m_cameraZoomBlocked = false;
        bool m_favoritesMenuBlocked = false;
        bool m_waitMenuBlocked = false;
        bool m_combatBlocked = false;
        bool m_quickSlotsBlocked = false;

        // Store original mappings for restoration
        std::string m_originalZoomIn;
        std::string m_originalZoomOut;
        std::string m_originalFavorites;
        std::string m_originalTween;
        std::string m_originalAttack;
        std::string m_originalBlock;
        std::string m_originalShout;
        std::string m_originalReadyWeapon;
    };

}
