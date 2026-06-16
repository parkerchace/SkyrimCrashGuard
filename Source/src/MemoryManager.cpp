#include "MemoryManager.h"
#include "RecoveryNotifications.h"
#include "Config.h"
#include <windows.h>
#include <psapi.h>
#include <fmt/format.h>
#include <SKSE/SKSE.h>

namespace CrashGuard {

MemoryManager& MemoryManager::GetInstance() {
    static MemoryManager instance;
    return instance;
}

bool MemoryManager::Initialize() {
    if (m_initialized) {
        return true;
    }

    SKSE::log::info("Initializing MemoryManager...");

    // Read the current memory state so we have a baseline before the first warning check
    UpdateMemoryStats();

    // Start the warning cooldown timer in the past so the first warning can fire immediately
    m_lastWarningTime = std::chrono::steady_clock::now() - WARNING_COOLDOWN;
    m_warningCount = 0;
    m_criticalMemoryMode = false;

    m_initialized = true;
    SKSE::log::info("MemoryManager initialized successfully");
    return true;
}

void MemoryManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    SKSE::log::info("Shutting down MemoryManager...");
    m_initialized = false;
}

// ============================================================================
// Allocation Failure Handling
// ============================================================================

void MemoryManager::RegisterAllocationHook() {
    // Allocation hooking is permanently disabled.
    // We tried hooking the CRT allocator and Windows heap API but ran into:
    //   - Crashes during C++ runtime startup (VCRUNTIME140)
    //   - Infinite recursion (the hook itself calls malloc)
    //   - Massive performance overhead (millions of allocations per second in Skyrim)
    //
    // Instead, MemoryManager::MonitorMemoryUsage() is called from the main loop
    // to poll system memory via Windows API — no hooking required.
    SKSE::log::debug("[MemoryManager] Allocation hook disabled - using polling-based monitoring");
}

// ============================================================================
// Memory Warnings
// ============================================================================

void MemoryManager::MonitorMemoryUsage() {
    // Called periodically from the main loop to check memory pressure.
    // Initialized in main.cpp via MemoryManager::GetInstance().Initialize()
    if (!m_initialized) {
        return;
    }

    UpdateMemoryStats();
    CheckMemoryThresholds();
}

bool MemoryManager::IsMemoryCriticallyLow() {
    if (!m_initialized) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_currentStats.availablePhysical < CRITICAL_MEMORY_THRESHOLD ||
           m_currentStats.usagePercent > CRITICAL_USAGE_PERCENT;
}

void MemoryManager::WarnUserAboutMemory() {
    if (!m_initialized || !ShouldWarnUser()) {
        return;
    }

    RecordMemoryWarning();

    // Build the warning message with current numbers
    std::string message = fmt::format(
        "Low memory: {} MB available ({:.0f}% used){}",
        m_currentStats.availablePhysical / (1024 * 1024),
        m_currentStats.usagePercent,
        m_criticalMemoryMode
            ? " - CRITICAL: save and restart."
            : " - consider closing background apps."
    );

    SKSE::log::warn("[MemoryManager] {}", message);

    // Show an in-game overlay toast so the player sees it without checking the log.
    // RecoveryNotifications is rendered each frame by PresentHook.
    RecoveryNotifications::GetSingleton().AddResourceWarning(
        "Memory", message, m_criticalMemoryMode);
}

void MemoryManager::SuggestMemoryReduction() {
    if (!m_initialized) {
        return;
    }

    // These are genuine suggestions — reducing these settings really does lower RAM usage
    SKSE::log::info("Memory reduction suggestions:");
    SKSE::log::info("  - Reduce texture quality in Display settings");
    SKSE::log::info("  - Reduce shadow draw distance");
    SKSE::log::info("  - Reduce actor fade distance");
    SKSE::log::info("  - Close background applications");

    if (m_warningCount > 3) {
        SKSE::log::warn("  - RECOMMENDED: Save your game and restart Skyrim");
    }
}

// ============================================================================
// Memory Statistics
// ============================================================================

MemoryManager::MemoryStats MemoryManager::GetMemoryStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_currentStats;
}

void MemoryManager::UpdateMemoryStats() {
    // Read system-wide memory from Windows. This tells us how much RAM is
    // available to all running programs, not just Skyrim.
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);

    if (!GlobalMemoryStatusEx(&memStatus)) {
        SKSE::log::error("Failed to get system memory status");
        return;
    }

    // Read Skyrim's own memory usage from the current process
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (!GetProcessMemoryInfo(GetCurrentProcess(),
                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                              sizeof(pmc))) {
        SKSE::log::error("Failed to get process memory info");
        return;
    }

    std::lock_guard<std::mutex> lock(m_statsMutex);

    m_currentStats.totalPhysical     = memStatus.ullTotalPhys;
    m_currentStats.availablePhysical = memStatus.ullAvailPhys;
    m_currentStats.totalVirtual      = memStatus.ullTotalVirtual;
    m_currentStats.availableVirtual  = memStatus.ullAvailVirtual;
    m_currentStats.processWorkingSet = pmc.WorkingSetSize;
    m_currentStats.processPrivateBytes = pmc.PrivateUsage;
    m_currentStats.usagePercent      = static_cast<float>(memStatus.dwMemoryLoad);
    m_currentStats.lastUpdate        = std::chrono::steady_clock::now();
}

// ============================================================================
// Internal Helpers
// ============================================================================

void MemoryManager::CheckMemoryThresholds() {
    std::lock_guard<std::mutex> lock(m_statsMutex);

    bool wasCritical = m_criticalMemoryMode;

    // Critical: very little RAM left or system usage very high
    m_criticalMemoryMode = m_currentStats.availablePhysical < CRITICAL_MEMORY_THRESHOLD ||
                           m_currentStats.usagePercent > CRITICAL_USAGE_PERCENT;

    // Warning: approaching but not yet critical
    bool shouldWarn = m_currentStats.availablePhysical < WARNING_MEMORY_THRESHOLD ||
                      m_currentStats.usagePercent > WARNING_USAGE_PERCENT;

    if (m_criticalMemoryMode && !wasCritical) {
        // Just entered critical state — warn immediately and log suggestions
        SKSE::log::error("Memory entered CRITICAL state!");
        WarnUserAboutMemory();
        SuggestMemoryReduction();
    } else if (shouldWarn && ShouldWarnUser()) {
        WarnUserAboutMemory();
    }
}

bool MemoryManager::ShouldWarnUser() {
    // Limit how often we show warnings — once every WARNING_COOLDOWN (5 minutes by default)
    auto now = std::chrono::steady_clock::now();
    return (now - m_lastWarningTime) >= WARNING_COOLDOWN;
}

void MemoryManager::RecordMemoryWarning() {
    m_lastWarningTime = std::chrono::steady_clock::now();
    m_warningCount++;
}

} // namespace CrashGuard
