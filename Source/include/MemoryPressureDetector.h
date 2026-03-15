// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <atomic>
#include <string>

namespace CrashGuard {

    /// Memory pressure levels
    enum class MemoryPressure : uint8_t {
        Normal,      // <70% usage, all good
        Elevated,    // 70-85% usage, warning
        High,        // 85-95% usage, critical
        Critical     // >95% usage, crash imminent
    };

    /// Detects memory pressure and predicts crashes
    /// Monitors RAM, stack usage, and allocation patterns
    class MemoryPressureDetector {
    public:
        static MemoryPressureDetector& GetSingleton() {
            static MemoryPressureDetector instance;
            return instance;
        }

        /// Initialize detector
        void Initialize();

        /// Update pressure readings (call periodically)
        void Update();

        /// Get current memory pressure level
        MemoryPressure GetPressureLevel() const { return m_pressureLevel.load(); }

        /// Get detailed stats
        struct Stats {
            uint64_t totalRAM;
            uint64_t availableRAM;
            uint64_t usedRAM;
            float usagePercent;
            
            uint64_t processMemory;
            uint64_t peakProcessMemory;
            
            uint32_t actorCount;
            uint32_t referenceCount;
            
            bool stackPressure;
            bool heapFragmentation;
            bool allocationSpike;
            
            MemoryPressure pressureLevel;
            std::string recommendation;
        };
        Stats GetStats() const;

        /// Check if safe to spawn N actors
        bool IsSafeToSpawnActors(uint32_t count) const;

        /// Get warning message for current pressure
        std::string GetWarningMessage() const;

        /// Enable/disable
        void SetEnabled(bool enabled) { m_enabled = enabled; }
        bool IsEnabled() const { return m_enabled; }

    private:
        MemoryPressureDetector() = default;
        ~MemoryPressureDetector() = default;
        MemoryPressureDetector(const MemoryPressureDetector&) = delete;
        MemoryPressureDetector& operator=(const MemoryPressureDetector&) = delete;

        /// Update system memory stats
        void UpdateSystemMemory();

        /// Update process memory stats
        void UpdateProcessMemory();

        /// Detect allocation patterns
        void DetectAllocationPatterns();

        /// Calculate pressure level
        MemoryPressure CalculatePressureLevel() const;

        /// Generate recommendation
        std::string GenerateRecommendation() const;

        // Configuration
        bool m_enabled = true;
        float m_updateInterval = 2.0f;  // Check every 2 seconds (was 1.0f)
        float m_timeSinceUpdate = 0.0f;

        // Thresholds (adjusted for high-end systems)
        float m_elevatedThreshold = 0.80f;   // 80% (was 70%)
        float m_highThreshold = 0.90f;       // 90% (was 85%)
        float m_criticalThreshold = 0.97f;   // 97% (was 95%)

        // Current stats
        std::atomic<uint64_t> m_totalRAM{0};
        std::atomic<uint64_t> m_availableRAM{0};
        std::atomic<uint64_t> m_processMemory{0};
        std::atomic<uint64_t> m_peakProcessMemory{0};
        std::atomic<MemoryPressure> m_pressureLevel{MemoryPressure::Normal};

        // Pattern detection
        uint64_t m_lastProcessMemory = 0;
        uint64_t m_allocationRate = 0;  // Bytes per second
        bool m_allocationSpike = false;
    };

}
