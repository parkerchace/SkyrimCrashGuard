// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "PapyrusNativeFunctionHook.h"
#include "PapyrusValidator.h"
#include "Config.h"
#include <spdlog/spdlog.h>

namespace PapyrusValidation {

    // ========================================================================
    // Papyrus Function Registration Approach
    // Instead of hooking at the native function dispatcher level, we register
    // validation wrappers for specific problematic functions using SKSE's
    // Papyrus registration system. This is safer and doesn't require finding
    // hook addresses.
    // ========================================================================

    namespace {
        // Track registered functions
        std::atomic<size_t> s_registeredFunctions{0};
        std::atomic<size_t> s_validationCalls{0};
    }

    // ========================================================================
    // SmartHarvest NotifyActivated Wrapper
    // ========================================================================
    
    // This is a validation wrapper for SmartHarvest's NotifyActivated function
    // Original signature from crash log:
    // void NotifyActivated(Form itemForm, int itemType, bool collectible, int refrID,
    //                      int baseID, bool notify, string baseName, int count,
    //                      bool isContainer, bool isWhitelisted, bool isBlacklisted)
    
    void ValidatedNotifyActivated(
        RE::BSScript::IVirtualMachine* vm,
        RE::VMStackID stackID,
        RE::StaticFunctionTag*,
        RE::TESForm* itemForm,
        std::int32_t itemType,
        bool collectible,
        std::int32_t refrID,
        std::int32_t baseID,
        bool notify,
        RE::BSFixedString baseName,
        std::int32_t count,
        bool isContainer,
        bool isWhitelisted,
        bool isBlacklisted
    ) {
        s_validationCalls++;

        if (!Config::Get().papyrusValidationEnabled) {
            // Validation disabled, just log and return
            spdlog::trace("[PapyrusValidation] NotifyActivated called (validation disabled)");
            return;
        }

        // Validate parameters
        bool hasIssues = false;
        std::vector<std::string> issues;

        // Validate itemForm
        if (!itemForm) {
            issues.push_back("itemForm is null");
            hasIssues = true;
        }

        // Validate baseName (this was null in the crash)
        if (baseName.empty() || !baseName.c_str()) {
            issues.push_back("baseName is null or empty");
            hasIssues = true;
            baseName = RE::BSFixedString("");  // Replace with empty string
        }

        // Validate FormIDs
        if (refrID == 0) {
            issues.push_back("refrID is 0");
            hasIssues = true;
        }
        if (baseID == 0) {
            issues.push_back("baseID is 0");
            hasIssues = true;
        }

        // Log validation results
        if (hasIssues && Config::Get().papyrusValidationLogFailures) {
            spdlog::warn("[PapyrusValidation] SmartHarvest::NotifyActivated - Validation issues detected:");
            for (const auto& issue : issues) {
                spdlog::warn("[PapyrusValidation]   - {}", issue);
            }
            spdlog::warn("[PapyrusValidation] Call blocked to prevent crash");
            
            // Update statistics
            std::lock_guard<std::mutex> lock(ParameterValidator::s_statsMutex);
            auto& stats = ParameterValidator::s_stats;
            stats.failedValidations++;
            stats.crashesPrevented++;
        }

        // If critical parameters are invalid, don't call the original function
        if (!itemForm || baseName.empty()) {
            spdlog::error("[PapyrusValidation] Critical parameters invalid - blocking SmartHarvest::NotifyActivated call");
            return;
        }

        // Parameters are valid, but we can't call the original SmartHarvest function
        // because we don't have access to it. This wrapper is meant to REPLACE
        // the problematic function, not call it.
        // 
        // In practice, SmartHarvest would need to be patched to use our validated
        // version, or we'd need to hook at a lower level (which requires finding
        // the correct address).
        
        spdlog::trace("[PapyrusValidation] NotifyActivated validated successfully");
    }

    // ========================================================================
    // Registration System
    // ========================================================================

    bool NativeFunctionHook::Initialize() {
        if (s_installed) {
            spdlog::warn("[PapyrusValidation] Already initialized");
            return true;
        }

        try {
            // Load default validation rules
            FunctionRegistry::LoadDefaultRules();

            // Note: Papyrus function registration approach
            // This system provides validation wrappers for problematic functions.
            // However, it requires the mod (SmartHarvest) to be modified to call
            // our validated versions, OR we need to hook at the registration level
            // to intercept when mods register their functions.
            //
            // For now, we're just setting up the infrastructure. The actual
            // registration would happen in SKSEPlugin_Load via:
            //   SKSE::GetPapyrusInterface()->Register(RegisterValidationWrappers)
            
            s_installed = true;
            spdlog::info("[PapyrusValidation] Validation system initialized");
            spdlog::info("[PapyrusValidation] Ready to register validation wrappers");
            spdlog::info("[PapyrusValidation] Note: Requires Papyrus function registration in SKSEPlugin_Load");
            
            return true;

        } catch (const std::exception& e) {
            spdlog::error("[PapyrusValidation] Failed to initialize: {}", e.what());
            return false;
        }
    }

    void NativeFunctionHook::Shutdown() {
        if (!s_installed) {
            return;
        }

        // Log final statistics
        spdlog::info("[PapyrusValidation] Shutdown - Registered functions: {}, Validation calls: {}",
                    s_registeredFunctions.load(), s_validationCalls.load());

        auto stats = ParameterValidator::GetStats();
        spdlog::info("[PapyrusValidation] Total validations: {}, Failures: {}, Crashes prevented: {}",
                    stats.validatedCalls, stats.failedValidations, stats.crashesPrevented);

        s_installed = false;
    }

    bool NativeFunctionHook::IsInstalled() {
        return s_installed;
    }

    size_t NativeFunctionHook::GetInterceptedCallCount() {
        return s_validationCalls.load();
    }

    // ========================================================================
    // Papyrus Registration Function
    // This should be called from SKSEPlugin_Load
    // ========================================================================

    bool RegisterValidationWrappers(RE::BSScript::IVirtualMachine* vm) {
        if (!vm) {
            spdlog::error("[PapyrusValidation] Cannot register - VM is null");
            return false;
        }

        if (!Config::Get().papyrusValidationEnabled) {
            spdlog::info("[PapyrusValidation] Registration skipped - validation disabled in config");
            return true;
        }

        try {
            // Register SmartHarvest NotifyActivated wrapper
            // Note: This will only work if SmartHarvest hasn't registered yet,
            // or if we can override existing registrations
            
            vm->RegisterFunction(
                "NotifyActivated",
                "shse_pluginproxy",
                ValidatedNotifyActivated,
                true  // Allow override
            );

            s_registeredFunctions++;
            spdlog::info("[PapyrusValidation] Registered validation wrapper for SmartHarvest::NotifyActivated");

            // Add more function wrappers here as needed
            // Example:
            // vm->RegisterFunction("OtherFunction", "OtherClass", ValidatedOtherFunction, true);

            spdlog::info("[PapyrusValidation] Successfully registered {} validation wrappers", 
                        s_registeredFunctions.load());
            return true;

        } catch (const std::exception& e) {
            spdlog::error("[PapyrusValidation] Failed to register wrappers: {}", e.what());
            return false;
        }
    }

}  // namespace PapyrusValidation
