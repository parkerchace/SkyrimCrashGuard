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

// ScriptMonitor tracks script blacklists and timeout events.
//
// What it does RIGHT NOW:
//   - Maintains a fast blacklist (bloom filter + hash set) of scripts
//     that have caused errors
//   - Tracks per-script timeout windows
//   - Parses exception messages to decide which scripts to blacklist
//
// What requires a future VM hook (not yet installed):
//   - Intercepting execution before each script runs
//   - Counting how many times each script has executed
//   - Blocking a script from running at all
//
// A "VM hook" means modifying Skyrim's Papyrus virtual machine vtable
// so that CrashGuard's code is called before each script executes.
// This requires knowing the exact function offset for each game version,
// which must be validated before it is safe to install.

namespace ScriptValidation {

    // Describes a script error that was caught during execution
    struct ScriptException {
        std::string scriptName;
        uint32_t lineNumber;
        std::string errorMessage;
        std::string modName;
        std::chrono::steady_clock::time_point timestamp;
    };

    // Tracks how long a single script has been running so we can detect hangs
    struct ScriptTimeout {
        std::chrono::steady_clock::time_point startTime;
        uint32_t maxExecutionMs = 5000;
        std::string scriptName;
        uint32_t taskletId;
    };

    // Records why a script was added to the blacklist and how many times it has failed
    struct ScriptBlacklistEntry {
        std::string scriptName;
        std::string reason;
        std::chrono::steady_clock::time_point blacklistedAt;
        uint32_t failureCount;
    };

    // Monitors script execution health.
    // All methods are static because there is only one global script monitoring state.
    class ScriptMonitor {
    public:
        // Set up the blacklist structures. Call once at plugin startup.
        static bool Initialize();

        // Called when a script causes an error. Parses the error message to
        // decide if this script should be blacklisted to prevent future problems.
        static void HandleScriptException(const ScriptException& exception);

        // Returns true if the given script has been added to the blacklist.
        // Uses a bloom filter for speed (avoids mutex lock on most checks).
        static bool IsScriptBlacklisted(const std::string& scriptName);

        // Begin tracking how long a script has been running.
        // Call this when a script starts. Pair with StopScriptTimeout.
        static void StartScriptTimeout(uint32_t taskletId, const std::string& scriptName);

        // Stop tracking a script that has finished running.
        static void StopScriptTimeout(uint32_t taskletId);

        // Scan for scripts that have been running longer than the timeout limit.
        // Call this periodically from the main loop.
        static void CheckScriptTimeouts();

        // Add a script to the blacklist with a reason.
        static void BlacklistScript(const std::string& scriptName, const std::string& reason);

        // Statistics
        static size_t GetBlacklistSize();

        // Returns 0 until a Papyrus VM hook is installed.
        // The VM hook is required to count actual script executions.
        static size_t GetExecutionCount();
        static size_t GetFailureCount();

        static size_t GetTimeoutCount();

        // Remove all scripts from the blacklist (useful for testing)
        static void ClearBlacklist();

        // Change how long a script can run before it is flagged as timed out
        static void SetScriptTimeout(uint32_t timeoutMs);

    private:
        // Parse a script name like "ModName:ScriptName" to extract just the mod part
        static std::string ExtractModName(const std::string& scriptName);

        // Write a log entry for a script execution result (only when detailed logging is on)
        static void LogScriptExecution(const std::string& scriptName, bool success,
                                      const std::string& details = "");

        // Check if a script is allowed to run (not blacklisted)
        static bool ShouldAllowExecution(const std::string& scriptName);

        // Turn a CodeTasklet pointer into a stable numeric ID for tracking purposes
        static uint32_t GetTaskletId(RE::BSScript::Internal::CodeTasklet* tasklet);

        // Internal state
        static bool s_initialized;
        static std::unordered_set<std::string> s_blacklistedScripts;
        static std::vector<ScriptBlacklistEntry> s_blacklistEntries;
        static std::unordered_map<uint32_t, ScriptTimeout> s_runningScripts;
        static size_t s_timeoutCount;
        static uint32_t s_scriptTimeoutMs;
        static std::shared_mutex s_blacklistMutex;
        static std::shared_mutex s_timeoutMutex;
    };

}  // namespace ScriptValidation
