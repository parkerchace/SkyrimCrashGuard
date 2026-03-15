// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
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

}  // namespace VEH
