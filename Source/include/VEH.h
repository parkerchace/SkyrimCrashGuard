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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "LayerTrace.h"
#include <string>
#include <vector>
#include <cstdint>

/// Vectored Exception Handler with 6-Level Recovery Chain
/// Catches crashes that proactive patches missed, logs them
/// for the CrashCollector, and attempts 6-level recovery.
namespace VEH {

    // Forward declarations
    struct StackFrame;
    struct CrashContext;
    enum class SeverityLevel;

    /// Stack frame information
    struct StackFrame {
        void* address;
        std::string moduleName;
        std::string functionName;
        uint64_t offset;
        std::string sourceFile;
        uint32_t lineNumber;
    };

    /// Severity classification for crashes
    enum class SeverityLevel {
        Safe,       // Visual glitches only
        Warning,    // Missing resources, null pointers
        Critical,   // Save data or persistent state
        Fatal,      // Stack corruption, unrecoverable
        Unknown     // Cannot determine
    };

    /// Complete crash context captured during exception
    struct CrashContext {
        DWORD exceptionCode;
        void* crashAddress;
        CONTEXT cpuContext;
        std::vector<StackFrame> callStack;
        void* involvedObject;
        SeverityLevel severity;
        std::string rootCause;
        uint64_t timestamp;
    };

    /// Main VEH Exception Handler class
    class VEHExceptionHandler {
    public:
        /// Initialize and register VEH handler
        static bool Initialize();

        /// Remove VEH handler
        static void Shutdown();

        /// Get count of crashes caught since install
        static size_t GetCrashCount();

        /// Log recovery statistics
        static void LogStats();

        /// Per-layer recovery statistics snapshot
        struct LayerStats {
            uint64_t total;
            uint64_t knownSite;      ///< L1  – pre-analysed known crash sites
            uint64_t instrPattern;   ///< L1b – instruction-pattern match (Zydis)
            uint64_t learnedSite;    ///< L2  – previously decoded, cached fix
            uint64_t regFixup;       ///< L3  – redirect faulting register to safety buffer
            uint64_t instrSkip;      ///< L4  – decode, zero dest, advance RIP
            uint64_t funcReturn;     ///< L5  – synthetic function return
            uint64_t deepWalk;       ///< L6  – deep stack walk for return address
            uint64_t unrecoverable;  ///< Crashes that could not be recovered
        };

        /// Get snapshot of per-layer recovery statistics
        static LayerStats GetLayerStats();

        /// Allow VEH to recover exceptions whose RIP is inside CrashGuard's own
        /// module on the calling thread.  Intended only for the internal diagnostic
        /// test suite — call Disable when the test kernel returns.
        static void EnableThreadTestMode();
        static void DisableThreadTestMode();

        /// Marks the calling thread as running CrashGuard's own test suite, so
        /// recoveries recorded while the scope is open are labelled as self-tests
        /// instead of being reported as game crashes. This is labelling only — it
        /// does not relax any recovery rule, unlike EnableThreadTestMode. The
        /// stub-page test tiers need it because their faults happen outside
        /// CrashGuard's module and so are indistinguishable from a real crash.
        /// Nestable; Begin and End must be paired.
        static void BeginSelfTestScope();
        static void EndSelfTestScope();

        /// Returns the LayerTrace captured during the most recent test-mode recovery
        /// on this thread.  Valid only after a test kernel has run and been joined.
        /// Included via forward declaration; include LayerTrace.h for the full type.
        static CrashGuard::LayerTrace GetLastTestTrace();

        /// Main exception filter callback
        static LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS* exceptionInfo);

        /// Analyze exception and build crash context
        static CrashContext AnalyzeException(EXCEPTION_POINTERS* exceptionInfo);

        /// Determine if exception is recoverable
        static bool IsRecoverable(const CrashContext& context);

        /// Pause all game threads except current
        static void PauseGameThreads();

        /// Resume all paused game threads
        static void ResumeGameThreads();

    private:
        /// Build call stack from exception context
        static std::vector<StackFrame> BuildCallStack(CONTEXT* context);

        /// Resolve symbol information for an address
        static bool ResolveSymbol(void* address, StackFrame& frame);

        /// Classify crash severity
        static SeverityLevel ClassifySeverity(DWORD exceptionCode, void* crashAddress);

        /// Extract exception code and crash address
        static void ExtractExceptionInfo(EXCEPTION_POINTERS* exceptionInfo,
                                        DWORD& outCode, void*& outAddress);

        /// Capture CPU registers
        static void CaptureCPURegisters(CONTEXT* source, CONTEXT& dest);

        static PVOID s_handler;
        static std::vector<HANDLE> s_pausedThreads;
    };

    // Legacy API for backward compatibility
    void Install();
    void Remove();
    size_t GetCrashCount();
    void LogStats();
    VEHExceptionHandler::LayerStats GetLayerStats();

}  // namespace VEH
