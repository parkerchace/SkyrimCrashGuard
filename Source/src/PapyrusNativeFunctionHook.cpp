// Copyright (C) 2026 Parker Chace
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

        // If critical parameters are invalid, block the call to prevent the crash.
        // This wrapper is only registered when SmartHarvest is absent, so there is no
        // original function to forward to — any Papyrus script calling shse_pluginproxy::
        // NotifyActivated without SmartHarvest installed is already a misconfigured load order.
        if (!itemForm || baseName.empty()) {
            spdlog::error("[PapyrusValidation] Critical parameters invalid — "
                          "blocking shse_pluginproxy::NotifyActivated call");
            return;
        }

        // Parameters are valid. The wrapper is registered only when SmartHarvest is absent;
        // log so it's clear the call was received and validated but there is no downstream
        // handler (SmartHarvest's native code is not present).
        spdlog::debug("[PapyrusValidation] NotifyActivated — parameters valid, "
                      "no SmartHarvest handler present (expected if SmartHarvest is not installed)");
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

            // Registration is deferred to SKSEPlugin_Load via:
            //   SKSE::GetPapyrusInterface()->Register(RegisterValidationWrappers)
            // That call is wired in main.cpp and invokes RegisterValidationWrappers()
            // once the Papyrus VM is available. SmartHarvest detection happens there,
            // not here, so Initialize() just prepares the rule table.

            s_installed = true;
            spdlog::info("[PapyrusValidation] Validation system initialized");
            
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
            // SmartHarvest compatibility:
            // RE::IVirtualMachine::RegisterFunction always replaces an existing registration
            // for the same (function, class) pair. If SmartHarvest is installed it will
            // have already registered shse_pluginproxy::NotifyActivated with its own
            // C++ function. Overriding it with our wrapper — which returns without
            // forwarding to the original — silently breaks SmartHarvest's harvest
            // notification system for every item collected.
            //
            // Instead: only register the wrapper if SmartHarvest's DLL is NOT loaded.
            // When SmartHarvest IS present, crash protection for this specific function
            // is handled reactively by the VEH recovery chain (L1–L6), which catches
            // any access violation before it propagates.

            const bool shInstalled =
                (GetModuleHandleA("SHSE_PlugIn.dll")       != nullptr) ||
                (GetModuleHandleA("SmartHarvestSE.dll")     != nullptr) ||
                (GetModuleHandleA("po3_SmartHarvestSE.dll") != nullptr);

            if (!shInstalled) {
                vm->RegisterFunction(
                    "NotifyActivated",
                    "shse_pluginproxy",
                    ValidatedNotifyActivated,
                    false  // isLatent: false — this is a void, synchronous function
                );
                s_registeredFunctions++;
                spdlog::info("[PapyrusValidation] Registered NotifyActivated wrapper "
                             "(SmartHarvest not present)");
            } else {
                spdlog::info("[PapyrusValidation] SmartHarvest detected — "
                             "NotifyActivated not overridden; VEH handles crash protection");
            }

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
