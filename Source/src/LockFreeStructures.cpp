// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "PCH.h"
#include "LockFreeStructures.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>

namespace ThreadSafety {

    // Static member initialization
    bool LockContentionProfiler::s_initialized = false;
    bool LockContentionProfiler::s_enabled = false;
    std::unordered_map<std::string, LockContentionProfiler::LockStats> LockContentionProfiler::s_lockStats;
    std::mutex LockContentionProfiler::s_statsMutex;

    void LockContentionProfiler::Initialize() {
        if (s_initialized) {
            return;
        }

        spdlog::info("╔════════════════════════════════════════╗");
        spdlog::info("║  Lock Contention Profiler Initializing║");
        spdlog::info("╚════════════════════════════════════════╝");

        s_lockStats.clear();
        s_enabled = false;  // Disabled by default for performance

        s_initialized = true;
        spdlog::info("LockContentionProfiler initialized (disabled by default)");
    }

    void LockContentionProfiler::Shutdown() {
        if (!s_initialized) {
            return;
        }

        if (s_enabled) {
            // Export final statistics
            ExportStats("Data/SKSE/Plugins/CrashGuard/lock_contention_stats.txt");
        }

        spdlog::info("LockContentionProfiler shutting down");
        
        std::lock_guard<std::mutex> lock(s_statsMutex);
        s_lockStats.clear();
        s_initialized = false;
    }

    void LockContentionProfiler::RecordAcquisition(const std::string& lockName, 
                                                   std::chrono::microseconds waitTime) {
        if (!s_initialized || !s_enabled) {
            return;
        }

        // Get or create stats for this lock
        LockStats* stats = nullptr;
        {
            std::lock_guard<std::mutex> lock(s_statsMutex);
            stats = &s_lockStats[lockName];
        }

        // Update counters (lock-free)
        stats->acquisitionCount.increment();
        
        uint64_t waitMicros = waitTime.count();
        stats->totalWaitTimeMicros.fetch_add(waitMicros, std::memory_order_relaxed);
        
        // Update max wait time
        uint64_t currentMax = stats->maxWaitTimeMicros.load(std::memory_order_relaxed);
        while (waitMicros > currentMax) {
            if (stats->maxWaitTimeMicros.compare_exchange_weak(currentMax, waitMicros,
                                                              std::memory_order_relaxed)) {
                break;
            }
        }
    }

    void LockContentionProfiler::RecordContention(const std::string& lockName) {
        if (!s_initialized || !s_enabled) {
            return;
        }

        // Get or create stats for this lock
        LockStats* stats = nullptr;
        {
            std::lock_guard<std::mutex> lock(s_statsMutex);
            stats = &s_lockStats[lockName];
        }

        // Update contention counter (lock-free)
        stats->contentionCount.increment();
    }

    LockContentionProfiler::ContentionStats LockContentionProfiler::GetStats(const std::string& lockName) {
        ContentionStats stats;
        stats.lockName = lockName;
        stats.acquisitionCount = 0;
        stats.contentionCount = 0;
        stats.totalWaitTime = std::chrono::microseconds(0);
        stats.maxWaitTime = std::chrono::microseconds(0);
        stats.avgWaitTime = std::chrono::microseconds(0);
        stats.contentionRate = 0.0f;

        if (!s_initialized) {
            return stats;
        }

        std::lock_guard<std::mutex> lock(s_statsMutex);
        
        auto it = s_lockStats.find(lockName);
        if (it != s_lockStats.end()) {
            const auto& lockStats = it->second;
            
            stats.acquisitionCount = lockStats.acquisitionCount.get();
            stats.contentionCount = lockStats.contentionCount.get();
            stats.totalWaitTime = std::chrono::microseconds(
                lockStats.totalWaitTimeMicros.load(std::memory_order_relaxed));
            stats.maxWaitTime = std::chrono::microseconds(
                lockStats.maxWaitTimeMicros.load(std::memory_order_relaxed));
            
            if (stats.acquisitionCount > 0) {
                stats.avgWaitTime = std::chrono::microseconds(
                    stats.totalWaitTime.count() / stats.acquisitionCount);
                stats.contentionRate = static_cast<float>(stats.contentionCount) / 
                                      static_cast<float>(stats.acquisitionCount);
            }
        }

        return stats;
    }

    std::vector<LockContentionProfiler::ContentionStats> LockContentionProfiler::GetAllStats() {
        std::vector<ContentionStats> allStats;

        if (!s_initialized) {
            return allStats;
        }

        std::lock_guard<std::mutex> lock(s_statsMutex);
        
        for (const auto& [lockName, _] : s_lockStats) {
            // Note: We can't call GetStats here because it would deadlock
            // So we duplicate the logic
            ContentionStats stats;
            stats.lockName = lockName;
            
            const auto& lockStats = s_lockStats[lockName];
            stats.acquisitionCount = lockStats.acquisitionCount.get();
            stats.contentionCount = lockStats.contentionCount.get();
            stats.totalWaitTime = std::chrono::microseconds(
                lockStats.totalWaitTimeMicros.load(std::memory_order_relaxed));
            stats.maxWaitTime = std::chrono::microseconds(
                lockStats.maxWaitTimeMicros.load(std::memory_order_relaxed));
            
            if (stats.acquisitionCount > 0) {
                stats.avgWaitTime = std::chrono::microseconds(
                    stats.totalWaitTime.count() / stats.acquisitionCount);
                stats.contentionRate = static_cast<float>(stats.contentionCount) / 
                                      static_cast<float>(stats.acquisitionCount);
            }
            
            allStats.push_back(stats);
        }

        return allStats;
    }

    std::vector<LockContentionProfiler::ContentionStats> LockContentionProfiler::GetTopContendedLocks(size_t count) {
        auto allStats = GetAllStats();
        
        // Sort by contention rate (descending)
        std::sort(allStats.begin(), allStats.end(),
            [](const ContentionStats& a, const ContentionStats& b) {
                return a.contentionRate > b.contentionRate;
            });
        
        // Return top N
        if (allStats.size() > count) {
            allStats.resize(count);
        }
        
        return allStats;
    }

    void LockContentionProfiler::ResetStats() {
        if (!s_initialized) {
            return;
        }

        std::lock_guard<std::mutex> lock(s_statsMutex);
        s_lockStats.clear();
        spdlog::info("Lock contention statistics reset");
    }

    void LockContentionProfiler::SetEnabled(bool enabled) {
        s_enabled = enabled;
        spdlog::info("Lock contention profiling {}", enabled ? "enabled" : "disabled");
    }

    bool LockContentionProfiler::ExportStats(const std::string& filename) {
        if (!s_initialized) {
            return false;
        }

        auto allStats = GetAllStats();
        
        if (allStats.empty()) {
            spdlog::info("No lock contention statistics to export");
            return true;
        }

        try {
            std::ofstream file(filename);
            if (!file.is_open()) {
                spdlog::error("Failed to open file for lock contention stats: {}", filename);
                return false;
            }

            file << "Lock Contention Statistics\n";
            file << "==========================\n\n";
            file << "Generated: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n\n";

            // Sort by contention rate
            std::sort(allStats.begin(), allStats.end(),
                [](const ContentionStats& a, const ContentionStats& b) {
                    return a.contentionRate > b.contentionRate;
                });

            file << std::left << std::setw(40) << "Lock Name"
                 << std::right << std::setw(15) << "Acquisitions"
                 << std::setw(15) << "Contentions"
                 << std::setw(15) << "Rate (%)"
                 << std::setw(15) << "Avg Wait (μs)"
                 << std::setw(15) << "Max Wait (μs)"
                 << "\n";
            file << std::string(115, '-') << "\n";

            for (const auto& stats : allStats) {
                file << std::left << std::setw(40) << stats.lockName
                     << std::right << std::setw(15) << stats.acquisitionCount
                     << std::setw(15) << stats.contentionCount
                     << std::setw(15) << std::fixed << std::setprecision(2) 
                     << (stats.contentionRate * 100.0f)
                     << std::setw(15) << stats.avgWaitTime.count()
                     << std::setw(15) << stats.maxWaitTime.count()
                     << "\n";
            }

            file << "\n\nSummary:\n";
            file << "--------\n";
            file << "Total locks tracked: " << allStats.size() << "\n";
            
            size_t totalAcquisitions = 0;
            size_t totalContentions = 0;
            for (const auto& stats : allStats) {
                totalAcquisitions += stats.acquisitionCount;
                totalContentions += stats.contentionCount;
            }
            
            file << "Total acquisitions: " << totalAcquisitions << "\n";
            file << "Total contentions: " << totalContentions << "\n";
            
            if (totalAcquisitions > 0) {
                float overallRate = static_cast<float>(totalContentions) / 
                                   static_cast<float>(totalAcquisitions);
                file << "Overall contention rate: " << std::fixed << std::setprecision(2) 
                     << (overallRate * 100.0f) << "%\n";
            }

            file.close();
            spdlog::info("Lock contention statistics exported to: {}", filename);
            return true;

        } catch (const std::exception& e) {
            spdlog::error("Failed to export lock contention stats: {}", e.what());
            return false;
        }
    }

}  // namespace ThreadSafety
