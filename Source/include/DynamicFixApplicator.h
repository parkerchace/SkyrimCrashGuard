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

#include <string>
#include <vector>

/// Dynamic Fix Applicator
/// Provides recovery strategy metadata used by the VEH recovery chain
/// and the pattern learning system.
///
/// Note: This module previously contained instruction-patching code
/// (PatchToNOP, PatchToReturn, VirtualProtect writes to game executable memory).
/// That code was removed because ApplyFix() had no callers outside this module
/// and writing NOP bytes to game executable memory is unsafe and unnecessary.
/// The VEH recovery chain modifies CPU context (registers + RIP) only and
/// never writes to game code pages.
namespace DynamicFix {

    /// Recovery strategy types (used by VEH for logging and pattern learning)
    enum class RecoveryStrategy {
        MeshRepair,
        MeshFallback,
        AnimationRetry,
        AnimationFallback,
        ScriptSkip,
        ScriptTerminate,
        CellReload,
        CellTeleport,
        MemoryFree,
        InstructionPatch,
        StateRollback,
        NullPointerFix,
        MissingResourceFix,
        Unknown
    };

    /// Recovery result metadata
    struct RecoveryResult {
        bool success = false;
        RecoveryStrategy strategyUsed = RecoveryStrategy::Unknown;
        std::vector<std::string> actionsPerformed;
        std::string failureReason;
    };

    /// Convert recovery strategy enum to display string
    const char* RecoveryStrategyToString(RecoveryStrategy strategy);

}  // namespace DynamicFix
