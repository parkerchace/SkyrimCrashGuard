// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <cstdint>
#include <chrono>
#include <mutex>

// MemoryManager monitors system and process memory usage.
// It polls Windows memory counters at regular intervals and warns the player
// when RAM is running low so they can save before a crash happens.
//
// What this does:
//   - Reads system RAM and process working set from Windows every few seconds
//   - Shows an in-game toast when memory reaches warning or critical levels
//   - Logs suggestions (save, reduce settings, close background apps)
//
// What this does NOT do:
//   - It does not free game memory — Skyrim's engine manages its own allocations
//   - It does not hook the allocator — that causes crashes (see ALLOCATION_HOOK_ANALYSIS.md)

namespace CrashGuard {

class MemoryManager {
public:
    static MemoryManager& GetInstance();

    // Call once during plugin startup to start memory monitoring
    bool Initialize();

    // Call during plugin shutdown to clean up
    void Shutdown();

    // Registration stub — allocation hooking is disabled.
    // See ALLOCATION_HOOK_ANALYSIS.md for why this was abandoned.
    void RegisterAllocationHook();

    // Call periodically from the main loop to check memory and warn if needed
    void MonitorMemoryUsage();

    // Returns true if available RAM is critically low right now
    bool IsMemoryCriticallyLow();

    // Show an in-game toast and log a warning about low memory
    void WarnUserAboutMemory();

    // Log suggestions for how to reduce memory usage
    void SuggestMemoryReduction();

    // Snapshot of the current memory readings
    struct MemoryStats {
        size_t totalPhysical     = 0;
        size_t availablePhysical = 0;
        size_t totalVirtual      = 0;
        size_t availableVirtual  = 0;
        size_t processWorkingSet = 0;
        size_t processPrivateBytes = 0;
        float  usagePercent      = 0.0f;
        std::chrono::steady_clock::time_point lastUpdate;
    };

    MemoryStats GetMemoryStats();
    void UpdateMemoryStats();

private:
    MemoryManager() = default;
    ~MemoryManager() = default;
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    void CheckMemoryThresholds();
    bool ShouldWarnUser();
    void RecordMemoryWarning();

    // Warning thresholds
    static constexpr size_t CRITICAL_MEMORY_THRESHOLD = 512 * 1024 * 1024;   // 512 MB free
    static constexpr size_t WARNING_MEMORY_THRESHOLD  = 1024 * 1024 * 1024;  // 1 GB free
    static constexpr float  CRITICAL_USAGE_PERCENT    = 90.0f;
    static constexpr float  WARNING_USAGE_PERCENT     = 80.0f;
    static constexpr auto   WARNING_COOLDOWN          = std::chrono::minutes(5);

    MemoryStats m_currentStats{};
    std::mutex  m_statsMutex;
    std::chrono::steady_clock::time_point m_lastWarningTime;
    uint32_t    m_warningCount      = 0;
    bool        m_initialized       = false;
    bool        m_criticalMemoryMode = false;
};

} // namespace CrashGuard
