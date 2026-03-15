// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <string>
#include <cstdint>

/// FormID Validation Layer
/// Validates FormID lookups before they reach the engine
namespace FormIDValidation {

    /// FormID validation result
    struct ValidationResult {
        bool isValid;
        std::string errorMessage;
        RE::TESForm* form;  // nullptr if invalid
    };

    /// FormID validation statistics
    struct ValidationStats {
        size_t totalLookups;
        size_t validLookups;
        size_t invalidLookups;
        size_t cachedLookups;
        size_t preventedCrashes;
    };

    /// FormID Validator class
    /// Provides safe FormID lookup with validation and caching
    class FormIDValidator {
    public:
        /// Initialize the FormID validator
        static bool Initialize();

        /// Validate and lookup a FormID
        /// Returns nullptr for invalid FormIDs without crashing
        static RE::TESForm* LookupFormSafe(RE::FormID formID);

        /// Validate and lookup a FormID with type checking
        template<typename T>
        static T* LookupFormSafe(RE::FormID formID);

        /// Validate FormID exists in TESDataHandler
        static bool ValidateFormID(RE::FormID formID);

        /// Check if plugin is loaded for FormID
        static bool IsPluginLoaded(RE::FormID formID);

        /// Get plugin name for FormID
        static std::string GetPluginName(RE::FormID formID);

        /// Clear the validation cache
        static void ClearCache();

        /// Get validation statistics
        static ValidationStats GetStats();

        /// Increment prevention counter
        static void IncrementPreventionCount();

    private:
        /// Validate FormID format
        static bool ValidateFormIDFormat(RE::FormID formID);

        /// Extract plugin index from FormID
        static uint8_t GetPluginIndex(RE::FormID formID);

        /// Extract light plugin index from FormID
        static uint16_t GetLightPluginIndex(RE::FormID formID);

        /// Check if FormID is from a light plugin
        static bool IsLightPlugin(RE::FormID formID);

        /// Get TESDataHandler singleton
        static RE::TESDataHandler* GetDataHandler();

        /// Log validation failure
        static void LogValidationFailure(RE::FormID formID, const std::string& reason);

        /// Log validation success
        static void LogValidationSuccess(RE::FormID formID);

        // Cache for valid FormID lookups
        static std::unordered_map<RE::FormID, RE::TESForm*> s_validFormCache;
        static std::shared_mutex s_cacheMutex;

        // Set of known invalid FormIDs to avoid repeated lookups
        static std::unordered_set<RE::FormID> s_invalidFormIDs;
        static std::shared_mutex s_invalidMutex;

        // Statistics
        static size_t s_totalLookups;
        static size_t s_validLookups;
        static size_t s_invalidLookups;
        static size_t s_cachedLookups;
        static size_t s_preventionCount;

        // Initialization flag
        static bool s_initialized;
    };

    // ========================================================================
    // Template Implementations
    // ========================================================================

    /// Validate and lookup a FormID with type checking
    template<typename T>
    T* FormIDValidator::LookupFormSafe(RE::FormID formID) {
        // First do the basic lookup
        RE::TESForm* form = LookupFormSafe(formID);
        
        if (!form) {
            return nullptr;
        }
        
        // Try to cast to the requested type
        T* typedForm = form->As<T>();
        
        if (!typedForm) {
            spdlog::warn("FormID {:08X} exists but is not of the requested type", formID);
            LogValidationFailure(formID, "Type mismatch");
            IncrementPreventionCount();
            return nullptr;
        }
        
        return typedForm;
    }

}  // namespace FormIDValidation
