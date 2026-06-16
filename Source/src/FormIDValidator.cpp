// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "PCH.h"
#include "FormIDValidator.h"

namespace FormIDValidation {

    // Static member initialization
    std::unordered_map<RE::FormID, RE::TESForm*> FormIDValidator::s_validFormCache;
    std::shared_mutex FormIDValidator::s_cacheMutex;
    std::unordered_set<RE::FormID> FormIDValidator::s_invalidFormIDs;
    std::shared_mutex FormIDValidator::s_invalidMutex;
    std::atomic<size_t> FormIDValidator::s_totalLookups{0};
    std::atomic<size_t> FormIDValidator::s_validLookups{0};
    std::atomic<size_t> FormIDValidator::s_invalidLookups{0};
    std::atomic<size_t> FormIDValidator::s_cachedLookups{0};
    std::atomic<size_t> FormIDValidator::s_preventionCount{0};
    bool FormIDValidator::s_initialized = false;

    // ========================================================================
    // Initialization
    // ========================================================================

    bool FormIDValidator::Initialize() {
        if (s_initialized) {
            spdlog::warn("FormIDValidator already initialized");
            return true;
        }

        spdlog::info("╔════════════════════════════════════════╗");
        spdlog::info("║    FormID Validator Initializing      ║");
        spdlog::info("╚════════════════════════════════════════╝");

        // Verify TESDataHandler is available
        auto* dataHandler = GetDataHandler();
        if (!dataHandler) {
            // This is expected during plugin init - game data isn't loaded yet
            spdlog::error("[PLUGIN INIT] FormIDValidator: TESDataHandler not available");
            spdlog::info("  → This is EXPECTED during startup - will retry at main menu");
            spdlog::info("  → Severity: LOW (not a problem, just timing)");
            return false;
        }

        spdlog::info("FormIDValidator: TESDataHandler available");
        spdlog::info("FormIDValidator: Loaded mods: {}", dataHandler->GetLoadedModCount());
        spdlog::info("FormIDValidator: Loaded light mods: {}", dataHandler->GetLoadedLightModCount());

        s_initialized = true;
        spdlog::info("FormIDValidator initialized successfully");
        
        return true;
    }

    // ========================================================================
    // FormID Validation
    // ========================================================================

    bool FormIDValidator::ValidateFormID(RE::FormID formID) {
        s_totalLookups++;

        // Check if FormID is in invalid cache
        {
            std::shared_lock lock(s_invalidMutex);
            if (s_invalidFormIDs.find(formID) != s_invalidFormIDs.end()) {
                s_invalidLookups++;
                s_cachedLookups++;
                spdlog::trace("FormID {:08X} is in invalid cache", formID);
                return false;
            }
        }

        // Check if FormID is in valid cache
        {
            std::shared_lock lock(s_cacheMutex);
            if (s_validFormCache.find(formID) != s_validFormCache.end()) {
                s_validLookups++;
                s_cachedLookups++;
                spdlog::trace("FormID {:08X} is in valid cache", formID);
                return true;
            }
        }

        // Validate FormID format
        if (!ValidateFormIDFormat(formID)) {
            LogValidationFailure(formID, "Invalid FormID format");
            
            // Add to invalid cache
            {
                std::unique_lock lock(s_invalidMutex);
                s_invalidFormIDs.insert(formID);
            }
            
            s_invalidLookups++;
            return false;
        }

        // Check if plugin is loaded
        if (!IsPluginLoaded(formID)) {
            LogValidationFailure(formID, "Plugin not loaded");
            
            // Add to invalid cache
            {
                std::unique_lock lock(s_invalidMutex);
                s_invalidFormIDs.insert(formID);
            }
            
            s_invalidLookups++;
            return false;
        }

        // Try to lookup the form in TESDataHandler
        auto* dataHandler = GetDataHandler();
        if (!dataHandler) {
            LogValidationFailure(formID, "TESDataHandler not available");
            s_invalidLookups++;
            return false;
        }

        // Use TESForm::LookupByID to check if form exists
        RE::TESForm* form = RE::TESForm::LookupByID(formID);
        
        if (!form) {
            LogValidationFailure(formID, "Form not found in TESDataHandler");
            
            // Add to invalid cache
            {
                std::unique_lock lock(s_invalidMutex);
                s_invalidFormIDs.insert(formID);
            }
            
            s_invalidLookups++;
            return false;
        }

        // Form is valid
        LogValidationSuccess(formID);
        
        // Add to valid cache
        {
            std::unique_lock lock(s_cacheMutex);
            s_validFormCache[formID] = form;
        }
        
        s_validLookups++;
        return true;
    }

    RE::TESForm* FormIDValidator::LookupFormSafe(RE::FormID formID) {
        s_totalLookups++;

        // Check if FormID is in invalid cache
        {
            std::shared_lock lock(s_invalidMutex);
            if (s_invalidFormIDs.find(formID) != s_invalidFormIDs.end()) {
                s_invalidLookups++;
                s_cachedLookups++;
                spdlog::trace("FormID {:08X} is in invalid cache, returning nullptr", formID);
                IncrementPreventionCount();
                return nullptr;
            }
        }

        // Check if FormID is in valid cache
        {
            std::shared_lock lock(s_cacheMutex);
            auto it = s_validFormCache.find(formID);
            if (it != s_validFormCache.end()) {
                s_validLookups++;
                s_cachedLookups++;
                spdlog::trace("FormID {:08X} found in valid cache", formID);
                return it->second;
            }
        }

        // Validate FormID format
        if (!ValidateFormIDFormat(formID)) {
            LogValidationFailure(formID, "Invalid FormID format");
            
            // Add to invalid cache
            {
                std::unique_lock lock(s_invalidMutex);
                s_invalidFormIDs.insert(formID);
            }
            
            s_invalidLookups++;
            IncrementPreventionCount();
            return nullptr;
        }

        // Check if plugin is loaded
        if (!IsPluginLoaded(formID)) {
            LogValidationFailure(formID, "Plugin not loaded");
            
            // Add to invalid cache
            {
                std::unique_lock lock(s_invalidMutex);
                s_invalidFormIDs.insert(formID);
            }
            
            s_invalidLookups++;
            IncrementPreventionCount();
            return nullptr;
        }

        // Try to lookup the form in TESDataHandler
        auto* dataHandler = GetDataHandler();
        if (!dataHandler) {
            LogValidationFailure(formID, "TESDataHandler not available");
            s_invalidLookups++;
            IncrementPreventionCount();
            return nullptr;
        }

        // Use TESForm::LookupByID to check if form exists
        RE::TESForm* form = RE::TESForm::LookupByID(formID);
        
        if (!form) {
            LogValidationFailure(formID, "Form not found in TESDataHandler");
            
            // Add to invalid cache
            {
                std::unique_lock lock(s_invalidMutex);
                s_invalidFormIDs.insert(formID);
            }
            
            s_invalidLookups++;
            IncrementPreventionCount();
            return nullptr;
        }

        // Form is valid
        LogValidationSuccess(formID);
        
        // Add to valid cache
        {
            std::unique_lock lock(s_cacheMutex);
            s_validFormCache[formID] = form;
        }
        
        s_validLookups++;
        return form;
    }

    // ========================================================================
    // Plugin Validation
    // ========================================================================

    bool FormIDValidator::IsPluginLoaded(RE::FormID formID) {
        auto* dataHandler = GetDataHandler();
        if (!dataHandler) {
            return false;
        }

        if (IsLightPlugin(formID)) {
            // Light plugin (ESL)
            uint16_t lightIndex = GetLightPluginIndex(formID);
            
            // Check if light plugin index is valid
            if (lightIndex >= dataHandler->GetLoadedLightModCount()) {
                spdlog::debug("Light plugin index {} out of range (max: {})", 
                             lightIndex, dataHandler->GetLoadedLightModCount());
                return false;
            }
            
            // Try to get the light plugin file
            const RE::TESFile* file = dataHandler->LookupLoadedLightModByIndex(lightIndex);
            if (!file) {
                spdlog::debug("Light plugin at index {} not found", lightIndex);
                return false;
            }
            
            spdlog::trace("Light plugin {} is loaded at index {}", file->fileName, lightIndex);
            return true;
            
        } else {
            // Regular plugin (ESP/ESM)
            uint8_t pluginIndex = GetPluginIndex(formID);
            
            // Check if plugin index is valid
            if (pluginIndex >= dataHandler->GetLoadedModCount()) {
                spdlog::debug("Plugin index {} out of range (max: {})", 
                             pluginIndex, dataHandler->GetLoadedModCount());
                return false;
            }
            
            // Try to get the plugin file
            const RE::TESFile* file = dataHandler->LookupLoadedModByIndex(pluginIndex);
            if (!file) {
                spdlog::debug("Plugin at index {} not found", pluginIndex);
                return false;
            }
            
            spdlog::trace("Plugin {} is loaded at index {}", file->fileName, pluginIndex);
            return true;
        }
    }

    std::string FormIDValidator::GetPluginName(RE::FormID formID) {
        auto* dataHandler = GetDataHandler();
        if (!dataHandler) {
            return "Unknown";
        }

        const RE::TESFile* file = nullptr;
        
        if (IsLightPlugin(formID)) {
            uint16_t lightIndex = GetLightPluginIndex(formID);
            file = dataHandler->LookupLoadedLightModByIndex(lightIndex);
        } else {
            uint8_t pluginIndex = GetPluginIndex(formID);
            file = dataHandler->LookupLoadedModByIndex(pluginIndex);
        }

        if (file && file->fileName) {
            return std::string(file->fileName);
        }

        return "Unknown";
    }

    // ========================================================================
    // FormID Format Validation
    // ========================================================================

    bool FormIDValidator::ValidateFormIDFormat(RE::FormID formID) {
        // FormID 0x00000000 is invalid (null form)
        if (formID == 0) {
            return false;
        }

        // FormID 0xFFFFFFFF is invalid (sentinel value)
        if (formID == 0xFFFFFFFF) {
            return false;
        }

        // Check if it's a light plugin FormID (0xFE??????)
        if (IsLightPlugin(formID)) {
            // Light plugin FormID format: 0xFExxxyyy
            // xxx = light plugin index (12 bits)
            // yyy = form index (12 bits)
            uint16_t lightIndex = GetLightPluginIndex(formID);
            uint16_t formIndex = formID & 0x00000FFF;
            
            // Light plugin index should be reasonable (< 4096)
            if (lightIndex >= 4096) {
                return false;
            }
            
            // Form index should be non-zero
            if (formIndex == 0) {
                return false;
            }
            
            return true;
        } else {
            // Regular FormID format: 0xXXyyyyyy
            // XX = plugin index (8 bits)
            // yyyyyy = form index (24 bits)
            uint8_t pluginIndex = GetPluginIndex(formID);
            uint32_t formIndex = formID & 0x00FFFFFF;
            
            // Plugin index should be reasonable (< 256)
            // This is always true for uint8_t, but we check for completeness
            
            // Form index should be non-zero
            if (formIndex == 0) {
                return false;
            }
            
            return true;
        }
    }

    bool FormIDValidator::IsLightPlugin(RE::FormID formID) {
        // Light plugins have FormIDs starting with 0xFE
        return (formID & 0xFF000000) == 0xFE000000;
    }

    uint8_t FormIDValidator::GetPluginIndex(RE::FormID formID) {
        // Regular plugin index is the top 8 bits
        return static_cast<uint8_t>((formID & 0xFF000000) >> 24);
    }

    uint16_t FormIDValidator::GetLightPluginIndex(RE::FormID formID) {
        // Light plugin index is bits 12-23 (12 bits)
        return static_cast<uint16_t>((formID & 0x00FFF000) >> 12);
    }

    // ========================================================================
    // Cache Management
    // ========================================================================

    void FormIDValidator::ClearCache() {
        spdlog::info("Clearing FormID validation cache");
        
        {
            std::unique_lock lock(s_cacheMutex);
            s_validFormCache.clear();
        }
        
        {
            std::unique_lock lock(s_invalidMutex);
            s_invalidFormIDs.clear();
        }
        
        spdlog::info("FormID validation cache cleared");
    }

    // ========================================================================
    // Statistics
    // ========================================================================

    ValidationStats FormIDValidator::GetStats() {
        ValidationStats stats;
        stats.totalLookups      = s_totalLookups.load(std::memory_order_relaxed);
        stats.validLookups      = s_validLookups.load(std::memory_order_relaxed);
        stats.invalidLookups    = s_invalidLookups.load(std::memory_order_relaxed);
        stats.cachedLookups     = s_cachedLookups.load(std::memory_order_relaxed);
        stats.preventedCrashes  = s_preventionCount.load(std::memory_order_relaxed);
        return stats;
    }

    void FormIDValidator::IncrementPreventionCount() {
        s_preventionCount++;
    }

    // ========================================================================
    // Helpers
    // ========================================================================

    RE::TESDataHandler* FormIDValidator::GetDataHandler() {
        return RE::TESDataHandler::GetSingleton();
    }

    void FormIDValidator::LogValidationFailure(RE::FormID formID, const std::string& reason) {
        spdlog::warn("⚠ FormID validation failed: {:08X} - {}", formID, reason);
        
        // Log additional diagnostic information
        spdlog::debug("FormID validation failure details:");
        spdlog::debug("  FormID: {:08X}", formID);
        spdlog::debug("  Reason: {}", reason);
        spdlog::debug("  Is Light Plugin: {}", IsLightPlugin(formID));
        
        if (IsLightPlugin(formID)) {
            spdlog::debug("  Light Plugin Index: {}", GetLightPluginIndex(formID));
        } else {
            spdlog::debug("  Plugin Index: {}", GetPluginIndex(formID));
        }
        
        spdlog::debug("  Thread ID: {}", GetCurrentThreadId());
    }

    void FormIDValidator::LogValidationSuccess(RE::FormID formID) {
        spdlog::trace("✓ FormID validation passed: {:08X}", formID);
    }

}  // namespace FormIDValidation
