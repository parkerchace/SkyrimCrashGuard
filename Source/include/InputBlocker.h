// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

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
