// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "CrosshairObserver.h"
#include "InputBlocker.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <string>

namespace CrashGuard {

    void CrosshairObserver::Install() {
        // DISABLED: This observer was interfering with QuickLoot and other mods
        // Input blocking should ONLY happen when the F11 menu is open
        // Other mods need full control of inputs for their own menus
        
        spdlog::info("[CrosshairObserver] Crosshair observer DISABLED to prevent mod conflicts");
        spdlog::info("[CrosshairObserver] Input blocking only active when F11 menu is open");
        
        /* Original code - disabled
        auto crosshairSource = SKSE::GetCrosshairRefEventSource();
        if (crosshairSource) {
            crosshairSource->AddEventSink(this);
            spdlog::info("[CrosshairObserver] Installed crosshair event observer");
        } else {
            spdlog::error("[CrosshairObserver] Failed to get crosshair event source");
        }
        */
    }

    bool CrosshairObserver::IsLootableContainer(RE::TESObjectREFR* ref) const {
        if (!ref) {
            return false;
        }

        auto baseObject = ref->GetBaseObject();
        if (!baseObject) {
            return false;
        }

        auto formType = baseObject->GetFormType();

        // Containers (chests, barrels, etc.)
        if (formType == RE::FormType::Container) {
            return true;
        }

        // Dead actors (corpses)
        if (auto actor = ref->As<RE::Actor>()) {
            if (actor->IsDead()) {
                return true;
            }
        }

        // Flora (plants, ingredients) - some mods add loot menus for these
        if (formType == RE::FormType::Flora) {
            return true;
        }

        // Activators that might trigger menus
        // Many modded containers/loot systems use activators
        if (formType == RE::FormType::Activator) {
            // Check if it's a known loot-related activator
            auto name = baseObject->GetName();
            if (name && strlen(name) > 0) {
                std::string nameStr = name;
                std::transform(nameStr.begin(), nameStr.end(), nameStr.begin(), ::tolower);
                
                // Common patterns for loot activators
                if (nameStr.find("chest") != std::string::npos ||
                    nameStr.find("container") != std::string::npos ||
                    nameStr.find("loot") != std::string::npos ||
                    nameStr.find("corpse") != std::string::npos ||
                    nameStr.find("body") != std::string::npos ||
                    nameStr.find("sack") != std::string::npos ||
                    nameStr.find("barrel") != std::string::npos ||
                    nameStr.find("crate") != std::string::npos ||
                    nameStr.find("urn") != std::string::npos ||
                    nameStr.find("vase") != std::string::npos) {
                    return true;
                }
            }
        }

        // Furniture that might have inventory (mannequins, weapon racks, etc.)
        if (formType == RE::FormType::Furniture) {
            if (auto furniture = baseObject->As<RE::TESFurniture>()) {
                // Check if it has a container (weapon rack, mannequin, etc.)
                if (ref->GetContainer()) {
                    return true;
                }
            }
        }

        // NPCs (living) - dialogue menus, barter, etc.
        if (auto actor = ref->As<RE::Actor>()) {
            if (!actor->IsDead()) {
                // Living NPCs can trigger dialogue, barter, training menus
                return true;
            }
        }

        // Books, notes, and readable objects
        if (formType == RE::FormType::Book) {
            return true;
        }

        // Crafting stations (forges, alchemy tables, enchanting tables, etc.)
        if (formType == RE::FormType::Furniture) {
            auto name = baseObject->GetName();
            if (name && strlen(name) > 0) {
                std::string nameStr = name;
                std::transform(nameStr.begin(), nameStr.end(), nameStr.begin(), ::tolower);
                
                if (nameStr.find("forge") != std::string::npos ||
                    nameStr.find("anvil") != std::string::npos ||
                    nameStr.find("workbench") != std::string::npos ||
                    nameStr.find("grindstone") != std::string::npos ||
                    nameStr.find("alchemy") != std::string::npos ||
                    nameStr.find("arcane") != std::string::npos ||
                    nameStr.find("enchant") != std::string::npos ||
                    nameStr.find("cooking") != std::string::npos ||
                    nameStr.find("smelter") != std::string::npos ||
                    nameStr.find("tanning") != std::string::npos) {
                    return true;
                }
            }
        }

        return false;
    }

    RE::BSEventNotifyControl CrosshairObserver::ProcessEvent(const SKSE::CrosshairRefEvent* a_event, RE::BSTEventSource<SKSE::CrosshairRefEvent>* a_eventSource) {
        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto ref = a_event->crosshairRef.get();
        bool wasLookingAtContainer = m_lookingAtContainer;
        m_lookingAtContainer = IsLootableContainer(ref);

        if (m_lookingAtContainer != wasLookingAtContainer) {
            if (m_lookingAtContainer) {
                // Log what type of object we're looking at
                if (ref) {
                    auto baseObject = ref->GetBaseObject();
                    if (baseObject) {
                        auto formType = baseObject->GetFormType();
                        const char* typeName = "Unknown";
                        
                        switch (formType) {
                            case RE::FormType::Container: typeName = "Container"; break;
                            case RE::FormType::Flora: typeName = "Flora"; break;
                            case RE::FormType::Activator: typeName = "Activator"; break;
                            case RE::FormType::Furniture: typeName = "Furniture"; break;
                            case RE::FormType::Book: typeName = "Book"; break;
                            case RE::FormType::NPC: typeName = "NPC"; break;
                            default: break;
                        }
                        
                        spdlog::debug("[CrosshairObserver] Looking at interactive object ({}) - blocking inputs", typeName);
                    }
                }
                
                InputBlocker::GetSingleton().SetCameraZoomBlocked(true);
                InputBlocker::GetSingleton().SetFavoritesMenuBlocked(true);
                InputBlocker::GetSingleton().SetWaitMenuBlocked(true);  // Block wait menu to prevent conflict
                InputBlocker::GetSingleton().SetCombatBlocked(true);
                InputBlocker::GetSingleton().SetQuickSlotsBlocked(true);
            } else {
                // Only unblock if no scrollable menu is open
                // MenuInputObserver handles menu-based blocking
                auto ui = RE::UI::GetSingleton();
                bool menuOpen = false;
                
                if (ui) {
                    static const char* scrollableMenus[] = {
                        "DialogueMenu", "ContainerMenu", "BarterMenu", "GiftMenu",
                        "InventoryMenu", "MagicMenu", "FavoritesMenu", "LevelUpMenu",
                        "BookMenu", "TrainingMenu", nullptr
                    };
                    
                    for (int i = 0; scrollableMenus[i] != nullptr; ++i) {
                        if (ui->IsMenuOpen(scrollableMenus[i])) {
                            menuOpen = true;
                            break;
                        }
                    }
                }
                
                if (!menuOpen) {
                    spdlog::debug("[CrosshairObserver] Not looking at interactive object - unblocking inputs");
                    InputBlocker::GetSingleton().SetCameraZoomBlocked(false);
                    InputBlocker::GetSingleton().SetFavoritesMenuBlocked(false);
                    InputBlocker::GetSingleton().SetWaitMenuBlocked(false);
                    InputBlocker::GetSingleton().SetCombatBlocked(false);
                    InputBlocker::GetSingleton().SetQuickSlotsBlocked(false);
                }
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }

}
