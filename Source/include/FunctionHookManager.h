// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

/// Function Hook Manager for SKSE Trampoline Hooks
/// Manages installation of proactive validation hooks for mesh loading, animations, scripts, and cells
namespace FunctionHooks {

    /// Hook installation result
    struct HookResult {
        bool success;
        std::string hookName;
        std::string errorMessage;
    };

    /// Hook statistics
    struct HookStats {
        size_t totalHooks;
        size_t installedHooks;
        size_t failedHooks;
        size_t validationsPrevented;
    };

    /// Main function hook manager class
    class FunctionHookManager {
    public:
        /// Initialize the hook manager and allocate SKSE trampoline space
        static bool Initialize();

        /// Install all registered hooks
        static bool InstallAllHooks();

        /// Install mesh loading hooks
        static HookResult InstallMeshLoadingHooks();

        /// Install animation hooks
        static HookResult InstallAnimationHooks();

        /// Install script execution hooks
        static HookResult InstallScriptHooks();

        /// Install cell loading hooks
        static HookResult InstallCellLoadingHooks();

        /// Install FormID validation hooks
        static HookResult InstallFormIDValidationHooks();

        /// Get hook statistics
        static HookStats GetStats();

        /// Increment validation prevention counter
        static void IncrementPreventionCount();

        /// Check if hooks are installed
        static bool AreHooksInstalled();

    private:
        // ========================================================================
        // Mesh Loading Hook Implementation
        // ========================================================================
        
        /// Hooked mesh loading function (TESObjectREFR::Load3D)
        /// Validates mesh after engine loads it
        static RE::NiAVObject* Hook_LoadNif(RE::TESObjectREFR* a_this, bool a_backgroundLoading);
        
        /// Original Load3D function pointer (trampoline)
        static inline REL::Relocation<decltype(Hook_LoadNif)> _LoadNif;

        // ========================================================================
        // Animation Hook Implementation
        // ========================================================================
        
        /// Hooked animation playback function
        /// Validates animation before playback
        static bool Hook_NotifyAnimationGraph(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName);
        
        /// Original animation playback function pointer (trampoline)
        static inline REL::Relocation<decltype(Hook_NotifyAnimationGraph)> _NotifyAnimationGraph;

        // ========================================================================
        // Script Execution Hook Implementation
        // ========================================================================
        
        /// Hooked script execution function
        /// Wraps script calls with error handling
        static bool Hook_ExecuteScript(RE::BSScript::Internal::VirtualMachine* vm,
                                      RE::BSScript::Internal::CodeTasklet* tasklet);
        
        /// Original script execution function pointer (trampoline)
        static inline REL::Relocation<decltype(Hook_ExecuteScript)> _ExecuteScript;

        // ========================================================================
        // Cell Loading Hook Implementation
        // ========================================================================
        
        /// Hooked cell loading function
        /// Validates cell references before spawning
        static void Hook_LoadCell(RE::TESObjectCELL* a_cell);
        
        /// Original cell loading function pointer (trampoline)
        static inline REL::Relocation<decltype(Hook_LoadCell)> _LoadCell;

        // ========================================================================
        // Logging and Validation
        // ========================================================================
        /// Log hook installation success
        static void LogHookSuccess(const std::string& hookName, uintptr_t address);

        /// Log hook installation failure
        static void LogHookFailure(const std::string& hookName, const std::string& reason);

        /// Log null pointer violation for diagnostics
        static void LogNullPointerViolation(const std::string& functionName, const std::string& parameterName, void* address = nullptr);

        /// Log bounds violation for diagnostics
        static void LogBoundsViolation(const std::string& functionName, const std::string& arrayName, size_t index, size_t size);

        /// Log FormID validation violation for diagnostics
        static void LogFormIDViolation(const std::string& functionName, RE::FormID formID, const std::string& reason);

        /// Clamp index to valid array bounds
        template<typename T>
        static size_t ClampArrayIndex(size_t index, size_t arraySize, const std::string& functionName, const std::string& arrayName);

        /// Validate array access is within bounds
        static bool ValidateArrayAccess(size_t index, size_t arraySize, const std::string& functionName, const std::string& arrayName);

        /// Validate hook target address
        static bool ValidateHookTarget(uintptr_t address, const std::string& hookName);

        // State tracking
        static bool s_initialized;
        static bool s_hooksInstalled;
        static size_t s_installedCount;
        static size_t s_failedCount;
        static size_t s_preventionCount;
    };

    // ========================================================================
    // Template Implementations
    // ========================================================================

    /// Clamp index to valid array bounds
    template<typename T>
    size_t FunctionHookManager::ClampArrayIndex(size_t index, size_t arraySize, const std::string& functionName, const std::string& arrayName) {
        // Handle empty array case
        if (arraySize == 0) {
            spdlog::error("Array bounds check in {}: {} has size 0, returning 0", functionName, arrayName);
            IncrementPreventionCount();
            return 0;
        }
        
        // Check if index is out of bounds
        if (index >= arraySize) {
            LogBoundsViolation(functionName, arrayName, index, arraySize);
            IncrementPreventionCount();
            
            // Clamp to last valid index
            size_t clampedIndex = arraySize - 1;
            spdlog::debug("Clamping index {} to {} for array {} in {}", 
                         index, clampedIndex, arrayName, functionName);
            return clampedIndex;
        }
        
        // Index is valid
        return index;
    }

}  // namespace FunctionHooks
