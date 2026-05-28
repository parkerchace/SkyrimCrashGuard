// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

// DynamicFixApplicator.cpp
//
// Provides the RecoveryStrategy enum and RecoveryStrategyToString helper used
// by VEH.cpp and PatternLearningSystem.cpp for logging and pattern tracking.
//
// History: This file previously contained PatchToNOP / PatchToReturn /
// MakeMemoryWritable (VirtualProtect + memset NOP writes to game executable
// memory).  Those functions were dead code — ApplyFix() had zero external
// callers — and writing to game code pages via VirtualProtect is unsafe and
// unnecessary.  They were removed in v2.4.  The VEH recovery chain achieves
// crash recovery solely by modifying CPU register context (ctx->Rip, zeroing
// faulting registers); it never writes to game executable memory.

#include "DynamicFixApplicator.h"

namespace DynamicFix {

    const char* RecoveryStrategyToString(RecoveryStrategy strategy) {
        switch (strategy) {
            case RecoveryStrategy::MeshRepair:         return "MeshRepair";
            case RecoveryStrategy::MeshFallback:       return "MeshFallback";
            case RecoveryStrategy::AnimationRetry:     return "AnimationRetry";
            case RecoveryStrategy::AnimationFallback:  return "AnimationFallback";
            case RecoveryStrategy::ScriptSkip:         return "ScriptSkip";
            case RecoveryStrategy::ScriptTerminate:    return "ScriptTerminate";
            case RecoveryStrategy::CellReload:         return "CellReload";
            case RecoveryStrategy::CellTeleport:       return "CellTeleport";
            case RecoveryStrategy::MemoryFree:         return "MemoryFree";
            case RecoveryStrategy::InstructionPatch:   return "InstructionPatch";
            case RecoveryStrategy::StateRollback:      return "StateRollback";
            case RecoveryStrategy::NullPointerFix:     return "NullPointerFix";
            case RecoveryStrategy::MissingResourceFix: return "MissingResourceFix";
            case RecoveryStrategy::Unknown:            return "Unknown";
            default:                                   return "Invalid";
        }
    }

}  // namespace DynamicFix
