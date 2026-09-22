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

