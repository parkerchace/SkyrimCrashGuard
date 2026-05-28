// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

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
