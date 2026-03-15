// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "VEH.h"
#include "RootCauseAnalyzer.h"
#include <string>
#include <vector>
#include <cstdint>

/// Dynamic Fix Applicator
/// Applies runtime fixes to resolve crashes
namespace DynamicFix {

    /// Recovery strategy types
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

    /// Recovery result information
    struct RecoveryResult {
        bool success;
        RecoveryStrategy strategyUsed;
        std::vector<std::string> actionsPerformed;
        std::string failureReason;
    };

    /// Main dynamic fix applicator class
    class DynamicFixApplicator {
    public:
        /// Apply fix based on root cause analysis
        static RecoveryResult ApplyFix(VEH::CrashContext& context,
                                      const RootCauseAnalysis::RootCauseResult& rootCause);

        /// Fix null pointer dereference
        static bool FixNullPointer(VEH::CrashContext& context);

        /// Fix missing resource
        static bool FixMissingResource(VEH::CrashContext& context);

        /// Fix script error
        static bool FixScriptError(VEH::CrashContext& context);

        /// Fix animation error
        static bool FixAnimationError(VEH::CrashContext& context);

        /// Patch instruction at crash site
        static bool PatchInstruction(VEH::CrashContext& context);

    private:
        /// Allocate safe default value for null pointer
        static bool AllocateSafeDefault(VEH::CrashContext& context);

        /// Load fallback resource
        static bool LoadFallbackResource(VEH::CrashContext& context);

        /// Skip problematic script statement
        static bool SkipScriptStatement(VEH::CrashContext& context);

        /// Retry animation with default parameters
        static bool RetryWithDefaults(VEH::CrashContext& context);

        /// Analyze instruction at crash address
        static bool AnalyzeInstruction(void* address, void* outInstruction);

        /// Patch instruction to NOP (no operation)
        static bool PatchToNOP(void* address, size_t length);

        /// Patch instruction to return with value
        static bool PatchToReturn(void* address, uint64_t returnValue);

        /// Update instruction pointer after patch
        static bool UpdateInstructionPointer(VEH::CrashContext& context, void* newAddress);

        /// Make memory page writable for patching
        static bool MakeMemoryWritable(void* address, size_t size, DWORD& oldProtection);

        /// Restore original memory protection
        static bool RestoreMemoryProtection(void* address, size_t size, DWORD oldProtection);

        /// Flush instruction cache after patching
        static void FlushInstructionCache(void* address, size_t size);
    };

    /// Convert recovery strategy to string
    const char* RecoveryStrategyToString(RecoveryStrategy strategy);

}  // namespace DynamicFix
