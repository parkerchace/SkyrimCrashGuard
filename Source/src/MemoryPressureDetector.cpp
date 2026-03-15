// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT

#include "MemoryPressureDetector.h"
#include "RecoveryNotifications.h"
#include <spdlog/spdlog.h>
#include <Windows.h>
#include <Psapi.h>

namespace CrashGuard {

    void MemoryPressureDetector::Initialize() {
        spdlog::info("[MemoryPressure] Initializing memory pressure detector");
        
        // Get initial readings
        UpdateSystemMemory();
        UpdateProcessMemory();
        
        spdlog::info("[MemoryPressure] Total RAM: {} MB", m_totalRAM.load() / (1024 * 1024));
    }

    void MemoryPressureDetector::Update() {
        if (!m_enabled) return;

        m_timeSinceUpdate += 0.016f;  // Assume ~60fps
        if (m_timeSinceUpdate < m_updateInterval) return;
        m_timeSinceUpdate = 0.0f;

        UpdateSystemMemory();
        UpdateProcessMemory();
        DetectAllocationPatterns();

        MemoryPressure newLevel = CalculatePressureLevel();
        MemoryPressure oldLevel = m_pressureLevel.load();

        if (newLevel != oldLevel) {
            m_pressureLevel.store(newLevel);
            
            // Log pressure changes
            const char* levelNames[] = {"Normal", "Elevated", "High", "CRITICAL"};
            
            // Only log High and above
            if (newLevel >= MemoryPressure::High) {
                spdlog::warn("[MemoryPressure] Pressure level changed: {} -> {}",
                            levelNames[static_cast<int>(oldLevel)],
                            levelNames[static_cast<int>(newLevel)]);
            }
            
            // Only notify user at Critical level (not High)
            if (newLevel == MemoryPressure::Critical) {
                spdlog::error("[MemoryPressure] {}", GetWarningMessage());
                
                RecoveryNotifications::GetSingleton().AddResourceWarning(
                    "Memory Pressure",
                    GetWarningMessage(),
                    true  // Always critical
                );
            }
        }
    }

    void MemoryPressureDetector::UpdateSystemMemory() {
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        
        if (GlobalMemoryStatusEx(&memInfo)) {
            m_totalRAM.store(memInfo.ullTotalPhys);
            m_availableRAM.store(memInfo.ullAvailPhys);
        }
    }

    void MemoryPressureDetector::UpdateProcessMemory() {
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            uint64_t current = pmc.WorkingSetSize;
            m_processMemory.store(current);
            
            uint64_t peak = pmc.PeakWorkingSetSize;
            if (peak > m_peakProcessMemory.load()) {
                m_peakProcessMemory.store(peak);
            }
        }
    }

    void MemoryPressureDetector::DetectAllocationPatterns() {
        uint64_t current = m_processMemory.load();
        uint64_t delta = (current > m_lastProcessMemory) ? (current - m_lastProcessMemory) : 0;
        
        // Calculate allocation rate (bytes per second)
        m_allocationRate = static_cast<uint64_t>(delta / m_updateInterval);
        
        // Scale spike threshold with total RAM
        // 16GB = 100MB/sec, 32GB = 200MB/sec, 64GB = 400MB/sec
        uint64_t totalGB = m_totalRAM.load() / (1024 * 1024 * 1024);
        uint64_t spikeThreshold = (totalGB / 16) * 100 * 1024 * 1024;  // Scale with RAM
        if (spikeThreshold < 100 * 1024 * 1024) {
            spikeThreshold = 100 * 1024 * 1024;  // Minimum 100MB/sec
        }
        
        // Detect spikes
        m_allocationSpike = (m_allocationRate > spikeThreshold);
        
        if (m_allocationSpike) {
            spdlog::warn("[MemoryPressure] Allocation spike detected: {} MB/sec (threshold: {} MB/sec)",
                        m_allocationRate / (1024 * 1024),
                        spikeThreshold / (1024 * 1024));
        }
        
        m_lastProcessMemory = current;
    }

    MemoryPressure MemoryPressureDetector::CalculatePressureLevel() const {
        uint64_t total = m_totalRAM.load();
        uint64_t available = m_availableRAM.load();
        
        if (total == 0) return MemoryPressure::Normal;
        
        uint64_t used = total - available;
        float usagePercent = static_cast<float>(used) / total;
        
        // Check for allocation spikes
        if (m_allocationSpike) {
            return MemoryPressure::High;  // Spike = immediate concern
        }
        
        // Check usage thresholds
        if (usagePercent >= m_criticalThreshold) {
            return MemoryPressure::Critical;
        } else if (usagePercent >= m_highThreshold) {
            return MemoryPressure::High;
        } else if (usagePercent >= m_elevatedThreshold) {
            return MemoryPressure::Elevated;
        }
        
        return MemoryPressure::Normal;
    }

    std::string MemoryPressureDetector::GenerateRecommendation() const {
        MemoryPressure level = m_pressureLevel.load();
        
        switch (level) {
            case MemoryPressure::Normal:
                return "Memory usage normal";
                
            case MemoryPressure::Elevated:
                return "Memory usage elevated. Consider reducing actor spawns or fast traveling.";
                
            case MemoryPressure::High:
                return "Memory usage HIGH! Avoid spawning more actors. Save your game soon.";
                
            case MemoryPressure::Critical:
                return "CRITICAL MEMORY PRESSURE! Crash imminent. Save immediately and restart game.";
                
            default:
                return "Unknown memory state";
        }
    }

    bool MemoryPressureDetector::IsSafeToSpawnActors(uint32_t count) const {
        MemoryPressure level = m_pressureLevel.load();
        
        // Conservative thresholds
        switch (level) {
            case MemoryPressure::Normal:
                return count < 100;  // Allow large spawns
                
            case MemoryPressure::Elevated:
                return count < 50;   // Reduce spawn size
                
            case MemoryPressure::High:
                return count < 10;   // Very small spawns only
                
            case MemoryPressure::Critical:
                return false;        // Block all spawns
                
            default:
                return false;
        }
    }

    std::string MemoryPressureDetector::GetWarningMessage() const {
        return GenerateRecommendation();
    }

    MemoryPressureDetector::Stats MemoryPressureDetector::GetStats() const {
        Stats stats{};
        
        stats.totalRAM = m_totalRAM.load();
        stats.availableRAM = m_availableRAM.load();
        stats.usedRAM = stats.totalRAM - stats.availableRAM;
        stats.usagePercent = (stats.totalRAM > 0) ? 
            (static_cast<float>(stats.usedRAM) / stats.totalRAM * 100.0f) : 0.0f;
        
        stats.processMemory = m_processMemory.load();
        stats.peakProcessMemory = m_peakProcessMemory.load();
        
        stats.stackPressure = false;
        stats.heapFragmentation = false;
        stats.allocationSpike = m_allocationSpike;
        
        stats.pressureLevel = m_pressureLevel.load();
        stats.recommendation = GenerateRecommendation();
        
        return stats;
    }

}
