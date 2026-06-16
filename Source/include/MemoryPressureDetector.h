// Copyright (C) 2026 Parker Chace
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
            uint64_t totalRAM       = 0;  // Total physical RAM in bytes
            uint64_t availableRAM   = 0;  // Free physical RAM in bytes
            uint64_t usedRAM        = 0;  // totalRAM - availableRAM
            float    usagePercent   = 0;  // 0-100, system-wide RAM usage

            uint64_t processMemory     = 0;  // Skyrim's current working set (bytes)
            uint64_t peakProcessMemory = 0;  // Highest working set since launch (bytes)

            // Number of active NPCs and creatures the game is currently simulating.
            // Read from RE::ProcessLists each update. Useful for understanding
            // why memory is high (populated areas load many NPC meshes and scripts).
            uint32_t actorCount = 0;

            // True when memory usage is growing faster than the spike threshold
            // (usually means a large area load or many spawned actors at once)
            bool allocationSpike = false;

            MemoryPressure pressureLevel = MemoryPressure::Normal;
            std::string    recommendation;
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

        // Cached actor count — updated in Update() so GetStats() never reads
        // game data directly. Reads from GetStats() only touch this atomic.
        std::atomic<uint32_t> m_actorCount{0};

        // Pattern detection
        uint64_t m_lastProcessMemory = 0;
        uint64_t m_allocationRate = 0;  // Bytes per second
        bool m_allocationSpike = false;
    };

}
