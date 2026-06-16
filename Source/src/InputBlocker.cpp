// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "InputBlocker.h"
#include <SKSE/SKSE.h>

namespace CrashGuard {

    void InputBlocker::SetBlocked(bool blocked) {
        // Only change state if it's actually different
        if (m_blocked == blocked) {
            return;  // Already in this state, don't toggle again
        }
        
        m_blocked = blocked;
        
        // Safety check - don't try to access game systems during early initialization
        if (!RE::Main::GetSingleton()) {
            return;
        }
        
        // Block player controls directly
        auto playerControls = RE::PlayerControls::GetSingleton();
        if (playerControls) {
            // Block movement and looking
            auto movementHandler = playerControls->movementHandler;
            auto lookHandler = playerControls->lookHandler;
            
            if (blocked) {
                if (movementHandler) {
                    movementHandler->inputEventHandlingEnabled = false;
                }
                if (lookHandler) {
                    lookHandler->inputEventHandlingEnabled = false;
                }
            } else {
                if (movementHandler) {
                    movementHandler->inputEventHandlingEnabled = true;
                }
                if (lookHandler) {
                    lookHandler->inputEventHandlingEnabled = true;
                }
            }
        }
        
        // Block UI input processing
        auto controlMap = RE::ControlMap::GetSingleton();
        if (controlMap) {
            try {
                if (blocked) {
                    // Disable input processing for all menus
                    // This prevents gamepad from controlling game menus while F11 menu is open
                    // CommonLibSSE-NG: Direct member access (no GetRuntimeData())
                    controlMap->ignoreKeyboardMouse = true;
                    controlMap->ignoreActivateDisabledEvents = true;
                    
                    // Aggressively disable ALL control types to prevent gamepad from controlling game
                    using UEFlag = RE::UserEvents::USER_EVENT_FLAG;
                    controlMap->ToggleControls(UEFlag::kAll, false);
                    spdlog::debug("[InputBlocker] All controls blocked for F11 menu");
                } else {
                    // Re-enable input processing
                    // CommonLibSSE-NG: Direct member access (no GetRuntimeData())
                    controlMap->ignoreKeyboardMouse = false;
                    controlMap->ignoreActivateDisabledEvents = false;
                    
                    // Re-enable all controls
                    using UEFlag = RE::UserEvents::USER_EVENT_FLAG;
                    controlMap->ToggleControls(UEFlag::kAll, true);
                    spdlog::debug("[InputBlocker] All controls unblocked");
                }
            } catch (const std::exception& e) {
                spdlog::error("[InputBlocker] Failed to access ControlMap data: {}", e.what());
                // Graceful degradation - input blocking will be incomplete but won't crash
            }
        }
    }

    void InputBlocker::SetCameraZoomBlocked(bool blocked) {
        m_cameraZoomBlocked = blocked;
        
        if (!RE::Main::GetSingleton()) {
            return;
        }
        
        auto userEvents = RE::UserEvents::GetSingleton();
        if (!userEvents) {
            return;
        }
        
        if (blocked) {
            m_originalZoomIn = userEvents->zoomIn.c_str();
            m_originalZoomOut = userEvents->zoomOut.c_str();
            userEvents->zoomIn = "";
            userEvents->zoomOut = "";
            spdlog::debug("[InputBlocker] Camera zoom blocked");
        } else {
            userEvents->zoomIn = m_originalZoomIn.empty() ? "Zoom In" : m_originalZoomIn;
            userEvents->zoomOut = m_originalZoomOut.empty() ? "Zoom Out" : m_originalZoomOut;
            spdlog::debug("[InputBlocker] Camera zoom unblocked");
        }
    }

    void InputBlocker::SetFavoritesMenuBlocked(bool blocked) {
        m_favoritesMenuBlocked = blocked;

        if (!RE::Main::GetSingleton()) {
            return;
        }

        auto userEvents = RE::UserEvents::GetSingleton();
        if (!userEvents) {
            return;
        }

        if (blocked) {
            m_originalFavorites = userEvents->favorites.c_str();
            userEvents->favorites = "";
            spdlog::debug("[InputBlocker] Favorites menu blocked");
        } else {
            userEvents->favorites = m_originalFavorites.empty() ? "Favorites" : m_originalFavorites;
            spdlog::debug("[InputBlocker] Favorites menu unblocked");
        }
    }

    void InputBlocker::SetWaitMenuBlocked(bool blocked) {
        m_waitMenuBlocked = blocked;

        if (!RE::Main::GetSingleton()) {
            return;
        }

        auto controlMap = RE::ControlMap::GetSingleton();
        if (!controlMap) {
            return;
        }

        try {
            if (blocked) {
                // Disable activate events which includes the wait/tween menu
                // CommonLibSSE-NG: Direct member access (no GetRuntimeData())
                controlMap->ignoreActivateDisabledEvents = true;
                spdlog::debug("[InputBlocker] Wait/tween menu blocked via ignoreActivateDisabledEvents");
            } else {
                controlMap->ignoreActivateDisabledEvents = false;
                spdlog::debug("[InputBlocker] Wait/tween menu unblocked");
            }
        } catch (const std::exception& e) {
            spdlog::error("[InputBlocker] Failed to access ControlMap data for wait menu: {}", e.what());
            // Graceful degradation - wait menu blocking will be incomplete but won't crash
        }
    }

    void InputBlocker::SetCombatBlocked(bool blocked) {
        m_combatBlocked = blocked;

        if (!RE::Main::GetSingleton()) {
            return;
        }

        auto userEvents = RE::UserEvents::GetSingleton();
        if (!userEvents) {
            return;
        }

        if (blocked) {
            // Save and clear ALL combat-related controls
            m_originalAttack = userEvents->leftAttack.c_str();
            m_originalBlock = userEvents->rightAttack.c_str();
            m_originalShout = userEvents->shout.c_str();
            m_originalReadyWeapon = userEvents->readyWeapon.c_str();
            
            userEvents->leftAttack = "";
            userEvents->rightAttack = "";
            userEvents->shout = "";
            userEvents->readyWeapon = "";
            userEvents->dualAttack = "";
            userEvents->blockStart = "";
            userEvents->blockStop = "";
            userEvents->blockBash = "";
            userEvents->attackStart = "";
            userEvents->attackPowerStart = "";
            
            spdlog::debug("[InputBlocker] Combat controls blocked");
        } else {
            // Restore combat controls
            userEvents->leftAttack = m_originalAttack.empty() ? "Left Attack/Block" : m_originalAttack;
            userEvents->rightAttack = m_originalBlock.empty() ? "Right Attack/Block" : m_originalBlock;
            userEvents->shout = m_originalShout.empty() ? "Shout" : m_originalShout;
            userEvents->readyWeapon = m_originalReadyWeapon.empty() ? "Ready Weapon" : m_originalReadyWeapon;
            userEvents->dualAttack = "Dual Attack";
            userEvents->blockStart = "blockStart";
            userEvents->blockStop = "blockStop";
            userEvents->blockBash = "blockBash";
            userEvents->attackStart = "attackStart";
            userEvents->attackPowerStart = "attackPowerStart";
            
            spdlog::debug("[InputBlocker] Combat controls unblocked");
        }
    }

    void InputBlocker::SetQuickSlotsBlocked(bool blocked) {
        m_quickSlotsBlocked = blocked;

        if (!RE::Main::GetSingleton()) {
            return;
        }

        auto userEvents = RE::UserEvents::GetSingleton();
        if (!userEvents) {
            return;
        }

        if (blocked) {
            // Block ALL quick slot and equipment controls
            userEvents->hotkey1 = "";
            userEvents->hotkey2 = "";
            userEvents->hotkey3 = "";
            userEvents->hotkey4 = "";
            userEvents->hotkey5 = "";
            userEvents->hotkey6 = "";
            userEvents->hotkey7 = "";
            userEvents->hotkey8 = "";
            userEvents->leftEquip = "";
            userEvents->rightEquip = "";
            userEvents->toggleFavorite = "";
            
            // Also block movement controls that could interfere
            userEvents->jump = "";
            userEvents->sprint = "";
            userEvents->sneak = "";
            userEvents->sprintStart = "";
            userEvents->sprintStop = "";
            userEvents->sneakStart = "";
            userEvents->sneakStop = "";
            userEvents->toggleRun = "";
            userEvents->autoMove = "";
            
            // Block camera/POV
            userEvents->togglePOV = "";
            
            // Block interaction
            userEvents->grab = "";
            
            // Block quick menus
            userEvents->quickInventory = "";
            userEvents->quickMagic = "";
            userEvents->quickStats = "";
            userEvents->quickMap = "";
            
            // Block save/load
            userEvents->quicksave = "";
            userEvents->quickload = "";
            
            spdlog::debug("[InputBlocker] Quick slots and gameplay controls blocked");
        } else {
            // Restore ALL controls
            userEvents->hotkey1 = "Hotkey1";
            userEvents->hotkey2 = "Hotkey2";
            userEvents->hotkey3 = "Hotkey3";
            userEvents->hotkey4 = "Hotkey4";
            userEvents->hotkey5 = "Hotkey5";
            userEvents->hotkey6 = "Hotkey6";
            userEvents->hotkey7 = "Hotkey7";
            userEvents->hotkey8 = "Hotkey8";
            userEvents->leftEquip = "LeftEquip";
            userEvents->rightEquip = "RightEquip";
            userEvents->toggleFavorite = "ToggleFavorite";
            
            userEvents->jump = "Jump";
            userEvents->sprint = "Sprint";
            userEvents->sneak = "Sneak";
            userEvents->sprintStart = "SprintStart";
            userEvents->sprintStop = "SprintStop";
            userEvents->sneakStart = "sneakStart";
            userEvents->sneakStop = "sneakStop";
            userEvents->toggleRun = "Toggle Always Run";
            userEvents->autoMove = "Auto-Move";
            
            userEvents->togglePOV = "Toggle POV";
            userEvents->grab = "Grab";
            
            userEvents->quickInventory = "Quick Inventory";
            userEvents->quickMagic = "Quick Magic";
            userEvents->quickStats = "Quick Stats";
            userEvents->quickMap = "Quick Map";
            
            userEvents->quicksave = "Quicksave";
            userEvents->quickload = "Quickload";
            
            spdlog::debug("[InputBlocker] Quick slots and gameplay controls unblocked");
        }
    }

}