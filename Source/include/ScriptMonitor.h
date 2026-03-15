// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

// CommonLibSSE must come before any Windows headers
#include <RE/Skyrim.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <mutex>

/// Script Monitor for proactive validation
/// Monitors Papyrus script execution and handles errors to prevent crashes
namespace ScriptValidation {

    /// Script exception information
    struct ScriptException {
        std::string scriptName;
        uint32_t lineNumber;
        std::string errorMessage;
        std::string modName;
        std::chrono::steady_clock::time_point timestamp;
    };

    /// Script timeout tracking
    struct ScriptTimeout {
        std::chrono::steady_clock::time_point startTime;
        uint32_t maxExecutionMs = 5000;
        std::string scriptName;
        uint32_t taskletId;
    };

    /// Script blacklist entry
    struct ScriptBlacklistEntry {
        std::string scriptName;
        std::string reason;
        std::chrono::steady_clock::time_point blacklistedAt;
        uint32_t failureCount;
    };

    /// Main script monitor class
    class ScriptMonitor {
    public:
        /// Initialize the script monitor
        static bool Initialize();

        /// Wrap script execution with error handling
        static bool ExecuteScriptSafe(RE::BSScript::Internal::VirtualMachine* vm,
                                     RE::BSScript::Internal::CodeTasklet* tasklet);

        /// Handle script exception
        static void HandleScriptException(const ScriptException& exception);

        /// Check if script is blacklisted
        static bool IsScriptBlacklisted(const std::string& scriptName);

        /// Terminate runaway script
        static void TerminateScript(RE::BSScript::Internal::CodeTasklet* tasklet,
                                   const std::string& reason);

        /// Start timeout tracking for script
        static void StartScriptTimeout(uint32_t taskletId, const std::string& scriptName);

        /// Stop timeout tracking for script
        static void StopScriptTimeout(uint32_t taskletId);

        /// Check for and handle script timeouts
        static void CheckScriptTimeouts();

        /// Blacklist problematic script
        static void BlacklistScript(const std::string& scriptName, const std::string& reason);

        /// Get statistics
        static size_t GetBlacklistSize();
        static size_t GetExecutionCount();
        static size_t GetFailureCount();
        static size_t GetTimeoutCount();

        /// Clear blacklist (for testing)
        static void ClearBlacklist();

        /// Set script timeout duration
        static void SetScriptTimeout(uint32_t timeoutMs);

    private:
        /// Validate script tasklet before execution
        static bool ValidateTasklet(RE::BSScript::Internal::CodeTasklet* tasklet);

        /// Extract script name from tasklet
        static std::string ExtractScriptName(RE::BSScript::Internal::CodeTasklet* tasklet);

        /// Extract mod name from script
        static std::string ExtractModName(const std::string& scriptName);

        /// Check for null reference access in script
        static bool CheckForNullReferences(RE::BSScript::Internal::CodeTasklet* tasklet);

        /// Generate safe default return value for script
        static bool GenerateSafeDefault(RE::BSScript::Internal::VirtualMachine* vm,
                                       RE::BSScript::Internal::CodeTasklet* tasklet);

        /// Log script execution details
        static void LogScriptExecution(const std::string& scriptName, bool success, 
                                      const std::string& details = "");

        /// Check if script execution should be allowed
        static bool ShouldAllowExecution(const std::string& scriptName);

        /// Get unique tasklet ID
        static uint32_t GetTaskletId(RE::BSScript::Internal::CodeTasklet* tasklet);

        // State tracking
        static bool s_initialized;
        static std::unordered_set<std::string> s_blacklistedScripts;
        static std::vector<ScriptBlacklistEntry> s_blacklistEntries;
        static std::unordered_map<uint32_t, ScriptTimeout> s_runningScripts;
        static size_t s_executionCount;
        static size_t s_failureCount;
        static size_t s_timeoutCount;
        static uint32_t s_scriptTimeoutMs;
        static std::shared_mutex s_blacklistMutex;
        static std::shared_mutex s_timeoutMutex;
    };

}  // namespace ScriptValidation