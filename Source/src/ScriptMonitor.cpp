// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "PCH.h"
#include "ScriptMonitor.h"
#include "Config.h"
#include "PerformanceMetrics.h"
#include <shared_mutex>  // For std::shared_mutex and std::shared_lock
#include <bitset>
#include <atomic>

namespace ScriptValidation {

    // ════════════════════════════════════════════════════════════════════════
    // Bloom filter for fast path rejection
    // ════════════════════════════════════════════════════════════════════════
    // A simple 256-bit bloom filter to avoid mutex acquisition for non-blacklisted
    // scripts. False positives are OK (we still do the full check), but false 
    // negatives are not possible.
    static constexpr size_t BLOOM_BITS = 256;
    static std::atomic<uint64_t> s_bloomFilter[4] = {0, 0, 0, 0};  // 256 bits

    static inline void BloomAdd(const std::string& name) {
        size_t h1 = std::hash<std::string>{}(name);
        size_t h2 = std::hash<std::string>{}(name + "_salt");
        s_bloomFilter[(h1 >> 6) & 3].fetch_or(1ULL << (h1 & 63), std::memory_order_relaxed);
        s_bloomFilter[(h2 >> 6) & 3].fetch_or(1ULL << (h2 & 63), std::memory_order_relaxed);
    }

    static inline bool BloomMayContain(const std::string& name) {
        size_t h1 = std::hash<std::string>{}(name);
        size_t h2 = std::hash<std::string>{}(name + "_salt");
        bool bit1 = (s_bloomFilter[(h1 >> 6) & 3].load(std::memory_order_relaxed) >> (h1 & 63)) & 1;
        bool bit2 = (s_bloomFilter[(h2 >> 6) & 3].load(std::memory_order_relaxed) >> (h2 & 63)) & 1;
        return bit1 && bit2;
    }

    // Static member initialization
    bool ScriptMonitor::s_initialized = false;
    std::unordered_set<std::string> ScriptMonitor::s_blacklistedScripts;
    std::vector<ScriptBlacklistEntry> ScriptMonitor::s_blacklistEntries;
    std::unordered_map<uint32_t, ScriptTimeout> ScriptMonitor::s_runningScripts;
    size_t ScriptMonitor::s_timeoutCount = 0;
    uint32_t ScriptMonitor::s_scriptTimeoutMs = 5000;  // 5 second default timeout
    std::shared_mutex ScriptMonitor::s_blacklistMutex;
    std::shared_mutex ScriptMonitor::s_timeoutMutex;

    // ========================================================================
    // Initialization
    // ========================================================================

    bool ScriptMonitor::Initialize() {
        if (s_initialized) {
            spdlog::warn("ScriptMonitor already initialized");
            return true;
        }

        spdlog::info("╔════════════════════════════════════════╗");
        spdlog::info("║      Script Monitor Initializing      ║");
        spdlog::info("╚════════════════════════════════════════╝");

        // Set up the blacklist structures and timeout counter
        s_blacklistedScripts.clear();
        s_blacklistEntries.clear();
        s_runningScripts.clear();
        s_timeoutCount = 0;

        s_initialized = true;

        // Note: a Papyrus VM hook is not installed, so execution-level interception
        // (running before each script, counting executions) is not active yet.
        // What IS working: the script blacklist, timeout tracking, and exception handling.
        spdlog::info("ScriptMonitor: Blacklist and timeout tracking ready ({}ms timeout). "
                     "VM hook not installed - execution interception pending.",
                     s_scriptTimeoutMs);

        return true;
    }

    // ========================================================================
    // Exception Handling
    // ========================================================================

    void ScriptMonitor::HandleScriptException(const ScriptException& exception) {
        if (!s_initialized) {
            return;
        }

        spdlog::error("Script exception in {}: {}", exception.scriptName, exception.errorMessage);
        
        // Log detailed exception information
        spdlog::error("  Script: {}", exception.scriptName);
        spdlog::error("  Mod: {}", exception.modName);
        spdlog::error("  Line: {}", exception.lineNumber);
        spdlog::error("  Error: {}", exception.errorMessage);

        // Determine if script should be blacklisted based on error type
        bool shouldBlacklist = false;
        std::string blacklistReason;

        if (exception.errorMessage.find("null") != std::string::npos ||
            exception.errorMessage.find("None") != std::string::npos) {
            shouldBlacklist = true;
            blacklistReason = "Null reference access";
        } else if (exception.errorMessage.find("timeout") != std::string::npos) {
            shouldBlacklist = true;
            blacklistReason = "Script timeout";
        } else if (exception.errorMessage.find("infinite") != std::string::npos ||
                   exception.errorMessage.find("loop") != std::string::npos) {
            shouldBlacklist = true;
            blacklistReason = "Infinite loop detected";
        }

        if (shouldBlacklist) {
            BlacklistScript(exception.scriptName, blacklistReason);
        }
    }

    // ========================================================================
    // Blacklist Management
    // ========================================================================

    bool ScriptMonitor::IsScriptBlacklisted(const std::string& scriptName) {
        if (!s_initialized || scriptName.empty()) {
            return false;
        }

        // Fast path: bloom filter check (no mutex needed)
        // If bloom filter says "definitely not", skip the full lookup
        if (!BloomMayContain(scriptName)) {
            return false;
        }

        // Slow path: actual set lookup (bloom filter said "maybe")
        std::shared_lock<std::shared_mutex> lock(s_blacklistMutex);
        return s_blacklistedScripts.find(scriptName) != s_blacklistedScripts.end();
    }

    void ScriptMonitor::BlacklistScript(const std::string& scriptName, const std::string& reason) {
        if (!s_initialized || scriptName.empty()) {
            return;
        }

        std::unique_lock<std::shared_mutex> lock(s_blacklistMutex);  // Use unique_lock for write operation
        
        // Check if already blacklisted
        if (s_blacklistedScripts.find(scriptName) != s_blacklistedScripts.end()) {
            // Update failure count for existing entry
            for (auto& entry : s_blacklistEntries) {
                if (entry.scriptName == scriptName) {
                    entry.failureCount++;
                    break;
                }
            }
            return;
        }

        // Add to blacklist
        s_blacklistedScripts.insert(scriptName);
        
        // Update bloom filter (must be done after adding to set)
        BloomAdd(scriptName);
        
        ScriptBlacklistEntry entry{
            .scriptName = scriptName,
            .reason = reason,
            .blacklistedAt = std::chrono::steady_clock::now(),
            .failureCount = 1
        };
        
        s_blacklistEntries.push_back(entry);
        
        spdlog::warn("Script blacklisted: {} (Reason: {})", scriptName, reason);
    }

    // ========================================================================
    // Timeout Management
    // ========================================================================

    void ScriptMonitor::StartScriptTimeout(uint32_t taskletId, const std::string& scriptName) {
        if (!s_initialized) {
            return;
        }

        std::unique_lock<std::shared_mutex> lock(s_timeoutMutex);  // Use unique_lock for write operation
        
        ScriptTimeout timeout{
            .startTime = std::chrono::steady_clock::now(),
            .maxExecutionMs = s_scriptTimeoutMs,
            .scriptName = scriptName,
            .taskletId = taskletId
        };
        
        s_runningScripts[taskletId] = timeout;
        // spdlog::trace("Started timeout tracking for script {} (tasklet {})", scriptName, taskletId);
    }

    void ScriptMonitor::StopScriptTimeout(uint32_t taskletId) {
        if (!s_initialized) {
            return;
        }

        std::unique_lock<std::shared_mutex> lock(s_timeoutMutex);  // Use unique_lock for write operation
        
        auto it = s_runningScripts.find(taskletId);
        if (it != s_runningScripts.end()) {
            // spdlog::trace("Stopped timeout tracking for tasklet {}", taskletId);
            s_runningScripts.erase(it);
        }
    }

    void ScriptMonitor::CheckScriptTimeouts() {
        if (!s_initialized) {
            return;
        }

        std::unique_lock<std::shared_mutex> lock(s_timeoutMutex);  // Use unique_lock for write operation
        
        auto now = std::chrono::steady_clock::now();
        std::vector<uint32_t> timedOutScripts;
        
        for (const auto& [taskletId, timeout] : s_runningScripts) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - timeout.startTime);
            
            if (elapsed.count() > timeout.maxExecutionMs) {
                spdlog::warn("Script timeout detected: {} ({}ms elapsed)", 
                           timeout.scriptName, elapsed.count());
                
                timedOutScripts.push_back(taskletId);
                s_timeoutCount++;
                
                // Create timeout exception
                ScriptException exception{
                    .scriptName = timeout.scriptName,
                    .lineNumber = 0,
                    .errorMessage = fmt::format("Script execution timeout ({}ms)", elapsed.count()),
                    .modName = ExtractModName(timeout.scriptName),
                    .timestamp = now
                };
                
                HandleScriptException(exception);
            }
        }
        
        // Remove timed out scripts from tracking
        for (uint32_t taskletId : timedOutScripts) {
            s_runningScripts.erase(taskletId);
        }
    }

    // ========================================================================
    // Helper Functions
    // ========================================================================

    std::string ScriptMonitor::ExtractModName(const std::string& scriptName) {
        // Extract mod name from script name
        // Script names often follow patterns like "ModName:ScriptName" or "ModName_ScriptName"
        
        size_t colonPos = scriptName.find(':');
        if (colonPos != std::string::npos) {
            return scriptName.substr(0, colonPos);
        }
        
        size_t underscorePos = scriptName.find('_');
        if (underscorePos != std::string::npos) {
            return scriptName.substr(0, underscorePos);
        }
        
        return "Unknown";
    }

    uint32_t ScriptMonitor::GetTaskletId(RE::BSScript::Internal::CodeTasklet* tasklet) {
        if (!tasklet) {
            return 0;
        }

        // Use the lower 32 bits of the tasklet's memory address as a unique ID.
        // Each tasklet is a distinct heap allocation, so its address is unique for
        // the duration of the script call. Truncating to 32 bits is fine for logging
        // and timeout-tracking purposes — full uniqueness isn't required.
        return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(tasklet) & 0xFFFFFFFF);
    }

    void ScriptMonitor::LogScriptExecution(const std::string& scriptName, bool success, 
                                          const std::string& details) {
        // Only log successful executions if detailed logging is enabled
        if (success && Config::Get().enableDetailedLogging) {
            spdlog::trace("Script executed successfully: {}", scriptName);
        } else if (!success) {
            // Always log failures
            spdlog::debug("Script execution failed: {} - {}", scriptName, details);
        }
    }

    bool ScriptMonitor::ShouldAllowExecution(const std::string& scriptName) {
        return !IsScriptBlacklisted(scriptName);
    }

    // ========================================================================
    // Statistics and Management
    // ========================================================================

    size_t ScriptMonitor::GetBlacklistSize() {
        if (!s_initialized) {
            return 0;
        }
        
        std::shared_lock<std::shared_mutex> lock(s_blacklistMutex);  // Use shared_lock for read-only operation
        return s_blacklistedScripts.size();
    }

    size_t ScriptMonitor::GetExecutionCount() {
        // Returns 0 until a Papyrus VM hook is installed.
        // The execution pipeline requires vtable interception which is pending.
        return 0;
    }

    size_t ScriptMonitor::GetFailureCount() {
        // Returns 0 until a Papyrus VM hook is installed.
        return 0;
    }

    size_t ScriptMonitor::GetTimeoutCount() {
        return s_timeoutCount;
    }

    void ScriptMonitor::ClearBlacklist() {
        if (!s_initialized) {
            return;
        }
        
        std::unique_lock<std::shared_mutex> lock(s_blacklistMutex);  // Use unique_lock for write operation
        s_blacklistedScripts.clear();
        s_blacklistEntries.clear();
        spdlog::info("Script blacklist cleared");
    }

    void ScriptMonitor::SetScriptTimeout(uint32_t timeoutMs) {
        s_scriptTimeoutMs = timeoutMs;
        spdlog::info("Script timeout set to {}ms", timeoutMs);
    }

}  // namespace ScriptValidation