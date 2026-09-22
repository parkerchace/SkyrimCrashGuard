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
