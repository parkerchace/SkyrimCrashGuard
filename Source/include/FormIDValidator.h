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

#include <atomic>
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

        // Statistics — atomic for thread-safe increment from multiple callers
        static std::atomic<size_t> s_totalLookups;
        static std::atomic<size_t> s_validLookups;
        static std::atomic<size_t> s_invalidLookups;
        static std::atomic<size_t> s_cachedLookups;
        static std::atomic<size_t> s_preventionCount;

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
