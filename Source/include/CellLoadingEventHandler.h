// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.
//
// CellLoadingEventHandler.h
// Event-based cell loading hook using BSTEventSink

#pragma once

#include "CellManager.h"
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>

namespace CrashGuard {

    /// Event handler for cell loading events
    /// Registers with RE::ScriptEventSourceHolder to receive notifications
    /// when cells are loaded/attached, allowing validation before use
    class CellLoadingEventHandler : 
        public RE::BSTEventSink<RE::TESCellFullyLoadedEvent>,
        public RE::BSTEventSink<RE::TESCellAttachDetachEvent>
    {
    public:
        /// Get singleton instance
        static CellLoadingEventHandler* GetSingleton() {
            static CellLoadingEventHandler singleton;
            return &singleton;
        }
        
        /// Register event handlers with the game
        static bool Register() {
            auto* eventHolder = RE::ScriptEventSourceHolder::GetSingleton();
            if (!eventHolder) {
                spdlog::error("[CellLoadingEventHandler] Failed to get ScriptEventSourceHolder");
                return false;
            }
            
            auto* handler = GetSingleton();
            eventHolder->AddEventSink<RE::TESCellFullyLoadedEvent>(handler);
            eventHolder->AddEventSink<RE::TESCellAttachDetachEvent>(handler);
            
            spdlog::info("[CellLoadingEventHandler] Registered for cell loading events");
            return true;
        }
        
        /// Unregister event handlers
        static void Unregister() {
            auto* eventHolder = RE::ScriptEventSourceHolder::GetSingleton();
            if (!eventHolder) return;
            
            auto* handler = GetSingleton();
            eventHolder->RemoveEventSink<RE::TESCellFullyLoadedEvent>(handler);
            eventHolder->RemoveEventSink<RE::TESCellAttachDetachEvent>(handler);
            
            spdlog::info("[CellLoadingEventHandler] Unregistered from cell loading events");
        }
        
        /// Process TESCellFullyLoadedEvent
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESCellFullyLoadedEvent* a_event,
            RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*) override
        {
            if (!a_event || !a_event->cell) {
                return RE::BSEventNotifyControl::kContinue;
            }
            
            RE::TESObjectCELL* cell = a_event->cell;
            
            // Check if cell is blacklisted
            if (CellValidation::CellManager::IsCellBlacklisted(cell)) {
                spdlog::warn("[CellLoadingEventHandler] Blacklisted cell loaded: {:08X}", 
                             cell->GetFormID());
                // Could trigger safe teleport here if needed
            }
            
            // Validate cell references
            auto result = CellValidation::CellManager::ValidateCellReferences(cell);
            if (!result.isValid) {
                spdlog::warn("[CellLoadingEventHandler] Cell {:08X} has {} invalid references", 
                             cell->GetFormID(), result.invalidReferenceCount);
                
                if (result.invalidReferenceCount > 10) {
                    // Many invalid refs - blacklist this cell
                    CellValidation::CellManager::BlacklistCell(cell, 
                        "Too many invalid references (" + std::to_string(result.invalidReferenceCount) + ")");
                }
            }
            
            m_cellsProcessed++;
            
            return RE::BSEventNotifyControl::kContinue;
        }
        
        /// Process TESCellAttachDetachEvent
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESCellAttachDetachEvent* a_event,
            RE::BSTEventSource<RE::TESCellAttachDetachEvent>*) override
        {
            if (!a_event) {
                return RE::BSEventNotifyControl::kContinue;
            }
            
            // Event fires when cell is attached (loaded into 3D) or detached
            // Can use this to track cell transitions
            if (a_event->attached) {
                m_cellsAttached++;
            } else {
                m_cellsDetached++;
            }
            
            return RE::BSEventNotifyControl::kContinue;
        }
        
        /// Get statistics
        uint32_t GetCellsProcessed() const { return m_cellsProcessed; }
        uint32_t GetCellsAttached() const { return m_cellsAttached; }
        uint32_t GetCellsDetached() const { return m_cellsDetached; }
        
    private:
        CellLoadingEventHandler() = default;
        ~CellLoadingEventHandler() = default;
        CellLoadingEventHandler(const CellLoadingEventHandler&) = delete;
        CellLoadingEventHandler& operator=(const CellLoadingEventHandler&) = delete;
        
        std::atomic<uint32_t> m_cellsProcessed{0};
        std::atomic<uint32_t> m_cellsAttached{0};
        std::atomic<uint32_t> m_cellsDetached{0};
    };

}  // namespace CrashGuard
