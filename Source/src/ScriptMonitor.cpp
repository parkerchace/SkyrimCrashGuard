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
    size_t ScriptMonitor::s_executionCount = 0;
    size_t ScriptMonitor::s_failureCount = 0;
    size_t ScriptMonitor::s_timeoutCount = 0;
    uint32_t ScriptMonitor::s_scriptTimeoutMs = 5000;  // 5 second default timeout
    std::shared_mutex ScriptMonitor::s_blacklistMutex;  // Upgraded to shared_mutex
    std::shared_mutex ScriptMonitor::s_timeoutMutex;    // Upgraded to shared_mutex

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

        // Initialize blacklist and tracking structures
        s_blacklistedScripts.clear();
        s_blacklistEntries.clear();
        s_runningScripts.clear();

        // Reset counters
        s_executionCount = 0;
        s_failureCount = 0;
        s_timeoutCount = 0;

        s_initialized = true;
        spdlog::info("ScriptMonitor initialized successfully with {}ms timeout", s_scriptTimeoutMs);
        
        return true;
    }

    // ========================================================================
    // Script Execution Monitoring
    // ========================================================================

    bool ScriptMonitor::ExecuteScriptSafe(RE::BSScript::Internal::VirtualMachine* vm,
                                          RE::BSScript::Internal::CodeTasklet* tasklet) {
        if (!s_initialized) {
            spdlog::error("ScriptMonitor not initialized");
            return false;
        }

        if (!vm || !tasklet) {
            spdlog::error("Invalid parameters: vm={}, tasklet={}", 
                         static_cast<void*>(vm), static_cast<void*>(tasklet));
            return false;
        }

        s_executionCount++;

        // Validate tasklet before execution
        if (!ValidateTasklet(tasklet)) {
            // spdlog::debug("Script tasklet validation failed");
            s_failureCount++;
            
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementScriptsMonitored();
            
            return GenerateSafeDefault(vm, tasklet);
        }

        // Extract script information
        std::string scriptName = ExtractScriptName(tasklet);
        uint32_t taskletId = GetTaskletId(tasklet);

        // Check if script is blacklisted
        if (IsScriptBlacklisted(scriptName)) {
            // spdlog::debug("Script {} is blacklisted, execution blocked", scriptName);
            LogScriptExecution(scriptName, false, "Blacklisted");
            s_failureCount++;
            
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementScriptsMonitored();
            
            return GenerateSafeDefault(vm, tasklet);
        }

        // Check for null reference access
        if (!CheckForNullReferences(tasklet)) {
            spdlog::warn("Script {} contains null reference access, blocking execution", scriptName);
            ScriptException exception{
                .scriptName = scriptName,
                .lineNumber = 0,  // Line number extraction would require more complex analysis
                .errorMessage = "Null reference access detected",
                .modName = ExtractModName(scriptName),
                .timestamp = std::chrono::steady_clock::now()
            };
            HandleScriptException(exception);
            s_failureCount++;
            
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementScriptsMonitored();
            
            return GenerateSafeDefault(vm, tasklet);
        }

        // Start timeout tracking
        StartScriptTimeout(taskletId, scriptName);

        try {
            // Execute the script with monitoring
            // spdlog::trace("Executing script: {}", scriptName);
            
            // Note: In a real implementation, we would need to hook into the VM's execution
            // For now, we simulate safe execution by checking conditions and returning success
            // The actual VM execution would happen here with proper error handling
            
            // Stop timeout tracking on successful completion
            StopScriptTimeout(taskletId);
            
            LogScriptExecution(scriptName, true);
            
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementScriptsMonitored();
            
            return true;

        } catch (const std::exception& e) {
            // Handle any C++ exceptions during script execution
            spdlog::error("Exception during script execution {}: {}", scriptName, e.what());
            
            ScriptException exception{
                .scriptName = scriptName,
                .lineNumber = 0,
                .errorMessage = e.what(),
                .modName = ExtractModName(scriptName),
                .timestamp = std::chrono::steady_clock::now()
            };
            
            HandleScriptException(exception);
            StopScriptTimeout(taskletId);
            s_failureCount++;
            
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementScriptsMonitored();
            
            return GenerateSafeDefault(vm, tasklet);
        }
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

    void ScriptMonitor::TerminateScript(RE::BSScript::Internal::CodeTasklet* tasklet,
                                       const std::string& reason) {
        if (!s_initialized || !tasklet) {
            return;
        }

        uint32_t taskletId = GetTaskletId(tasklet);
        std::string scriptName = ExtractScriptName(tasklet);
        
        spdlog::warn("Terminating script {}: {}", scriptName, reason);
        
        // Stop timeout tracking
        StopScriptTimeout(taskletId);
        
        // In a real implementation, we would need to properly terminate the script
        // This might involve setting the tasklet state or calling VM termination functions
        // For now, we log the termination
        
        LogScriptExecution(scriptName, false, fmt::format("Terminated: {}", reason));
        s_failureCount++;
    }

    // ========================================================================
    // Validation and Helper Functions
    // ========================================================================

    bool ScriptMonitor::ValidateTasklet(RE::BSScript::Internal::CodeTasklet* tasklet) {
        if (!tasklet) {
            return false;
        }

        // Basic tasklet validation
        // In a real implementation, we would check:
        // - Tasklet state is valid
        // - Script bytecode is not corrupted
        // - Required objects are available
        
        // For now, we perform basic null checks
        return true;  // Simplified validation
    }

    std::string ScriptMonitor::ExtractScriptName(RE::BSScript::Internal::CodeTasklet* tasklet) {
        if (!tasklet) {
            return "Unknown";
        }

        // Script name extraction requires accessing internal tasklet structures
        // which are not exposed through the public API. Using a formatted pointer
        // as a unique identifier for tracking purposes.
        
        return fmt::format("Script_{:p}", static_cast<void*>(tasklet));
    }

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

    bool ScriptMonitor::CheckForNullReferences(RE::BSScript::Internal::CodeTasklet* tasklet) {
        if (!tasklet) {
            return false;
        }

        // In a real implementation, we would analyze the script bytecode or execution context
        // to detect potential null reference access before it happens
        // This is complex and would require deep knowledge of Papyrus VM internals
        
        // For now, we return true (assume no null references) as a simplified implementation
        return true;
    }

    bool ScriptMonitor::GenerateSafeDefault(RE::BSScript::Internal::VirtualMachine* vm,
                                           RE::BSScript::Internal::CodeTasklet* tasklet) {
        if (!vm || !tasklet) {
            return false;
        }

        // In a real implementation, we would:
        // 1. Determine the expected return type of the script
        // 2. Generate an appropriate safe default value (0, false, null, empty string, etc.)
        // 3. Set the tasklet result to this safe default
        // 4. Mark the tasklet as completed
        
        // For now, we simply return false to indicate the script should not execute
        return false;
    }

    uint32_t ScriptMonitor::GetTaskletId(RE::BSScript::Internal::CodeTasklet* tasklet) {
        if (!tasklet) {
            return 0;
        }

        // Generate a unique ID for the tasklet
        // In a real implementation, we might use the tasklet's memory address or an internal ID
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
        return s_executionCount;
    }

    size_t ScriptMonitor::GetFailureCount() {
        return s_failureCount;
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