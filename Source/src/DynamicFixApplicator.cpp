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
