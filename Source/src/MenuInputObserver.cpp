// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "MenuInputObserver.h"
#include "InputBlocker.h"
#include "Config.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace CrashGuard {

    void MenuInputObserver::Install() {
        // DISABLED: This observer was interfering with modded menus
        // Input blocking should ONLY happen when the F11 menu is open
        // Other mods need full control of inputs for their own menus
        
        spdlog::info("[MenuInputObserver] Menu observer DISABLED to prevent mod conflicts");
        spdlog::info("[MenuInputObserver] Input blocking only active when F11 menu is open");
        spdlog::info("[MenuInputObserver] Modded menus (QuickLoot, SkyUI, etc.) have full input control");
        
        /* Original code - disabled
        auto ui = RE::UI::GetSingleton();
        if (ui) {
            ui->AddEventSink(this);
            spdlog::info("[MenuInputObserver] Installed menu event observer");
            
            const auto& config = Config::Get();
            if (config.autoDetectModdedMenus) {
                spdlog::info("[MenuInputObserver] Auto-detection of modded menus enabled");
            }
            if (!config.customScrollableMenus.empty()) {
                spdlog::info("[MenuInputObserver] Loaded {} custom scrollable menus from config", 
                    config.customScrollableMenus.size());
            }
            
            // Log helpful message for users
            spdlog::info("[MenuInputObserver] Input conflict prevention active");
            spdlog::info("[MenuInputObserver] If camera zooms in a modded menu, enable debug logging to see menu name");
        } else {
            spdlog::error("[MenuInputObserver] Failed to get UI singleton");
        }
        */
    }

    bool MenuInputObserver::MatchesModdedMenuPattern(const std::string& menuName) const {
        // Common patterns in modded menu names that suggest scrollable content
        static const char* patterns[] = {
            "List",        // SomeModListMenu
            "Inventory",   // CustomInventoryMenu
            "Container",   // ModdedContainerMenu
            "Trade",       // TradeMenu
            "Shop",        // ShopMenu
            "Vendor",      // VendorMenu
            "Barter",      // CustomBarterMenu
            "Dialogue",    // ModdedDialogueMenu
            "Choice",      // ChoiceMenu
            "Selection",   // SelectionMenu
            "Scroll",      // ScrollableMenu
            "Browse",      // BrowseMenu
            "Catalog",     // CatalogMenu
            "MCM",         // Mod Configuration Menu
            "Config",      // ConfigMenu
            "Settings",    // SettingsMenu
            "Options",     // OptionsMenu
            "Loot",        // QuickLoot, etc.
            "Item",        // ItemMenu
            "Equip",       // EquipMenu
            "Craft",       // CraftingMenu
            "Enchant",     // EnchantingMenu
            "Alchemy",     // AlchemyMenu
            "Smith",       // SmithingMenu
            "Perk",        // PerkMenu
            "Skill",       // SkillMenu
            "Quest",       // QuestMenu
            "Book",        // BookMenu
            "Note",        // NoteMenu
            "Letter",      // LetterMenu
            "Message",     // MessageMenu
            "Notification",// NotificationMenu
            "HUD",         // Custom HUD menus
            "Widget",      // Widget menus
            "Panel",       // Panel menus
            "Window",      // Window menus
            "UI",          // Generic UI menus
            nullptr
        };

        std::string lowerMenuName = menuName;
        std::transform(lowerMenuName.begin(), lowerMenuName.end(), lowerMenuName.begin(), ::tolower);

        for (int i = 0; patterns[i] != nullptr; ++i) {
            std::string pattern = patterns[i];
            std::transform(pattern.begin(), pattern.end(), pattern.begin(), ::tolower);
            
            if (lowerMenuName.find(pattern) != std::string::npos) {
                return true;
            }
        }

        return false;
    }

    bool MenuInputObserver::IsMenuWhitelisted(const std::string& menuName) const {
        // Menus that should NOT have input blocking (safe menus)
        static const char* whitelistedMenus[] = {
            "HUDMenu",           // Main HUD - never block
            "Console",           // Console - has its own input handling
            "Main Menu",         // Main menu
            "Loading Menu",      // Loading screen
            "MessageBoxMenu",    // Simple message boxes
            "Sleep/Wait Menu",   // Sleep/wait (intentional)
            "Cursor Menu",       // Cursor
            "Overlay Menu",      // Overlays
            "Overlay Interaction Menu",
            "TitleSequence Menu",
            "Credits Menu",
            "Mist Menu",         // Transition effects
            "Tutorial Menu",     // Tutorials
            "Kinect Menu",       // Kinect
            "Top Menu",          // Top-level menu
            "RaceSex Menu",      // Character creation
            "CustomMenu",        // Generic custom (too broad, but some mods use it)
            nullptr
        };

        for (int i = 0; whitelistedMenus[i] != nullptr; ++i) {
            if (menuName == whitelistedMenus[i]) {
                return true;
            }
        }

        return false;
    }

    bool MenuInputObserver::IsScrollableMenu(const RE::BSFixedString& menuName) const {
        const auto& config = Config::Get();
        
        // Check if input conflict prevention is enabled
        if (!config.enableInputConflictPrevention) {
            return false;
        }

        std::string menuNameStr = menuName.c_str();

        // 1. Check vanilla Skyrim scrollable menus
        static const char* vanillaScrollableMenus[] = {
            "DialogueMenu",      // Dialogue options - mouse wheel scrolls but also zooms camera
            "ContainerMenu",     // Container inventory - mouse wheel scrolls but also zooms camera
            "BarterMenu",        // Merchant trading - mouse wheel scrolls but also zooms camera
            "GiftMenu",          // Gift giving - mouse wheel scrolls but also zooms camera
            "InventoryMenu",     // Player inventory - mouse wheel scrolls but also zooms camera
            "MagicMenu",         // Spell/power menu - mouse wheel scrolls but also zooms camera
            "FavoritesMenu",     // Favorites quick menu - D-pad scrolls but conflicts with opening favorites
            "LevelUpMenu",       // Level up perks - mouse wheel scrolls but also zooms camera
            "BookMenu",          // Reading books - mouse wheel scrolls pages but also zooms camera
            "TrainingMenu",      // Skill training - mouse wheel scrolls but also zooms camera
            "StatsMenu",         // Character stats - mouse wheel scrolls but also zooms camera
            "JournalMenu",       // Quest journal - mouse wheel scrolls but also zooms camera
            "MapMenu",           // World map - mouse wheel zooms map (intentional, but can conflict)
            nullptr
        };

        for (int i = 0; vanillaScrollableMenus[i] != nullptr; ++i) {
            if (menuNameStr == vanillaScrollableMenus[i]) {
                return true;
            }
        }

        // 2. Check user-defined custom menus from TOML config
        for (const auto& customMenu : config.customScrollableMenus) {
            if (menuNameStr == customMenu) {
                spdlog::debug("[MenuInputObserver] Matched custom menu from config: {}", menuNameStr);
                return true;
            }
        }

        // 3. Check if already detected as modded menu (cached)
        if (m_detectedModdedMenus.find(menuNameStr) != m_detectedModdedMenus.end()) {
            return true;
        }

        // 4. Auto-detect modded menus using pattern matching
        if (config.autoDetectModdedMenus) {
            if (MatchesModdedMenuPattern(menuNameStr)) {
                // Cache the detection to avoid repeated pattern matching
                m_detectedModdedMenus.insert(menuNameStr);
                spdlog::info("[MenuInputObserver] Auto-detected modded scrollable menu: {}", menuNameStr);
                return true;
            }
            
            // 5. AGGRESSIVE FALLBACK: Block ANY menu that's not whitelisted
            // This ensures we catch ALL custom menus, even ones with unusual names
            if (!IsMenuWhitelisted(menuNameStr)) {
                m_detectedModdedMenus.insert(menuNameStr);
                spdlog::info("[MenuInputObserver] Auto-detected unknown menu (blocking by default): {}", menuNameStr);
                return true;
            }
        }

        return false;
    }

    void MenuInputObserver::UpdateInputBlocking() {
        const auto& config = Config::Get();
        
        if (!config.enableInputConflictPrevention) {
            return;
        }

        auto ui = RE::UI::GetSingleton();
        if (!ui) {
            return;
        }

        // Check if any scrollable menu is currently open
        bool shouldBlockInput = false;
        std::string openMenuName;
        
        // Check each menu we care about directly
        static const char* scrollableMenus[] = {
            "DialogueMenu", "ContainerMenu", "BarterMenu", "GiftMenu",
            "InventoryMenu", "MagicMenu", "FavoritesMenu", "LevelUpMenu",
            "BookMenu", "TrainingMenu", "StatsMenu", "JournalMenu", "MapMenu",
            nullptr
        };
        
        // First check vanilla menus
        for (int i = 0; scrollableMenus[i] != nullptr; ++i) {
            if (ui->IsMenuOpen(scrollableMenus[i])) {
                shouldBlockInput = true;
                openMenuName = scrollableMenus[i];
                spdlog::debug("[MenuInputObserver] Vanilla scrollable menu open: {}", openMenuName);
                break;
            }
        }
        
        // If no vanilla menu found, check custom and auto-detected menus
        if (!shouldBlockInput) {
            auto& menuMap = ui->menuMap;
            for (const auto& [name, menuEntry] : menuMap) {
                if (menuEntry.menu && IsScrollableMenu(name)) {
                    shouldBlockInput = true;
                    openMenuName = name.c_str();
                    spdlog::debug("[MenuInputObserver] Custom/modded scrollable menu open: {}", openMenuName);
                    break;
                }
            }
        }

        // Update input blocking state
        auto& inputBlocker = InputBlocker::GetSingleton();
        
        if (shouldBlockInput) {
            // Block camera zoom to prevent mouse wheel from zooming while scrolling menus
            if (config.blockCameraZoomInMenus) {
                inputBlocker.SetCameraZoomBlocked(true);
            }
            // Block favorites menu to prevent D-pad from opening favorites while scrolling
            if (config.blockFavoritesInMenus) {
                inputBlocker.SetFavoritesMenuBlocked(true);
            }
            // Block wait menu to prevent back button from opening wait menu in menus
            inputBlocker.SetWaitMenuBlocked(true);
            // Block combat controls to prevent accidental attacks/blocks in menus
            inputBlocker.SetCombatBlocked(true);
            // Block quick slots to prevent accidental item switching in menus
            inputBlocker.SetQuickSlotsBlocked(true);
            spdlog::debug("[MenuInputObserver] Blocking inputs for menu: {}", openMenuName);
        } else {
            // Only unblock if CrosshairObserver isn't blocking (for QuickLoot compatibility)
            // CrosshairObserver handles container crosshair detection separately
            // We check if we're looking at a container before unblocking
            auto crosshairRef = RE::CrosshairPickData::GetSingleton();
            bool lookingAtContainer = false;
            
            if (crosshairRef) {
                auto ref = crosshairRef->target.get();
                if (!ref) {
                    spdlog::warn("[MenuInputObserver] Null crosshair target handle encountered");
                } else {
                    auto baseObject = ref->GetBaseObject();
                    if (baseObject) {
                        if (baseObject->GetFormType() == RE::FormType::Container) {
                            lookingAtContainer = true;
                        } else if (auto actor = ref->As<RE::Actor>()) {
                            if (actor->IsDead()) {
                                lookingAtContainer = true;
                            }
                        }
                    }
                }
            }
            
            // Only unblock if not looking at a container (CrosshairObserver handles that case)
            if (!lookingAtContainer) {
                if (config.blockCameraZoomInMenus) {
                    inputBlocker.SetCameraZoomBlocked(false);
                }
                if (config.blockFavoritesInMenus) {
                    inputBlocker.SetFavoritesMenuBlocked(false);
                }
                inputBlocker.SetWaitMenuBlocked(false);
                inputBlocker.SetCombatBlocked(false);
                inputBlocker.SetQuickSlotsBlocked(false);
                spdlog::debug("[MenuInputObserver] Unblocking inputs (no scrollable menu open)");
            } else {
                spdlog::debug("[MenuInputObserver] Not unblocking - still looking at container");
            }
        }
    }

    RE::BSEventNotifyControl MenuInputObserver::ProcessEvent(
        const RE::MenuOpenCloseEvent* a_event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource) {
        
        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        const auto& config = Config::Get();
        if (!config.enableInputConflictPrevention) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Check if this menu needs input blocking
        if (IsScrollableMenu(a_event->menuName)) {
            spdlog::debug("[MenuInputObserver] Menu {} {}", 
                a_event->menuName.c_str(), 
                a_event->opening ? "opened" : "closed");
            
            // Update input blocking state whenever a scrollable menu opens or closes
            UpdateInputBlocking();
        }

        return RE::BSEventNotifyControl::kContinue;
    }

}
