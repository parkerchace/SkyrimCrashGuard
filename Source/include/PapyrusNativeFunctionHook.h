// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <RE/Skyrim.h>
#include <cstdint>

/// Papyrus Native Function Validation System
/// Uses Papyrus function registration to wrap problematic functions
/// and validate parameters before they cause crashes
namespace PapyrusValidation {

    /// Native function validation manager
    class NativeFunctionHook {
    public:
        /// Initialize validation system
        static bool Initialize();

        /// Shutdown and cleanup
        static void Shutdown();

        /// Check if system is initialized
        static bool IsInstalled();

        /// Get count of validation calls
        static size_t GetInterceptedCallCount();

    private:
        /// Installation state
        static inline bool s_installed = false;
    };

    /// Register validation wrappers with Papyrus VM
    /// Call this from SKSEPlugin_Load via SKSE::GetPapyrusInterface()->Register()
    /// @param vm The Papyrus virtual machine
    /// @return true if registration succeeded
    bool RegisterValidationWrappers(RE::BSScript::IVirtualMachine* vm);

}  // namespace PapyrusValidation

