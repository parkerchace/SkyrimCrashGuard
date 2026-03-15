// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/// Proactive engine patching system.
/// Hooks vulnerable game functions and adds pointer validation
/// BEFORE the engine dereferences bad pointers.
/// Standalone — no SKSE/CommonLib dependency.
namespace PatchEngine {

    /// A single engine patch.
    struct Patch {
        std::string name;              // Human-readable name
        std::string description;       // What it fixes
        bool        enabled = true;    // Can be toggled in INI
        bool        applied = false;   // Set after successful application

        // The install function — called during Apply()
        std::function<bool()> install;
    };

    /// Initialize the patch engine.
    void Init();

    /// Register a patch. Does not apply it yet.
    void Register(Patch patch);

    /// Apply all registered & enabled patches. Returns count of successful patches.
    size_t ApplyAll();

    /// Get all registered patches (for logging/MCM).
    const std::vector<Patch>& GetPatches();

    /// Get count of successfully applied patches.
    size_t GetAppliedCount();

}  // namespace PatchEngine
