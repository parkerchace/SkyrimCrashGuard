// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <chrono>

namespace CrashGuard {

    /// Resource types that can be monitored
    enum class ResourceType : uint8_t {
        Actors,
        References,
        Particles,
        Scripts,
        Sounds,
        Quests
    };

    /// Baseline data for a specific location/context
    struct LocationBaseline {
        std::string locationKey;        // Cell + Weather + TimeOfDay
        std::vector<uint32_t> samples;  // Rolling window of samples
        uint32_t baseline;              // Calculated median
        float stdDev;                   // Standard deviation
        bool isStable;                  // Is baseline reliable?
        uint64_t lastUpdate;            // Timestamp of last update
        uint32_t sampleCount;           // Total samples collected
        
        LocationBaseline() 
            : baseline(0), stdDev(0.0f), isStable(false), 
              lastUpdate(0), sampleCount(0) {}
    };

    /// Delta information for current resource usage
    struct ResourceDelta {
        ResourceType type;
        uint32_t baseline;              // Learned baseline for location
        uint32_t current;               // Current count
        int32_t delta;                  // Current - Baseline (can be negative)
        uint32_t safeMargin;            // How much above baseline is safe
        float usagePercent;             // delta / safeMargin * 100
        bool isWarning;                 // > 75% of safe margin
        bool isCritical;                // > 95% of safe margin
        std::string locationKey;        // Current location
        
        ResourceDelta() 
            : type(ResourceType::Actors), baseline(0), current(0), 
              delta(0), safeMargin(0), usagePercent(0.0f),
              isWarning(false), isCritical(false) {}
    };

    /// Adaptive resource monitoring with baseline learning
    class AdaptiveResourceMonitor {
    public:
        static AdaptiveResourceMonitor& GetSingleton() {
            static AdaptiveResourceMonitor instance;
            return instance;
        }

        /// Initialize the monitor
        void Initialize();

        /// Update monitoring (called every frame)
        void Update();

        /// Get current delta for a resource type
        ResourceDelta GetDelta(ResourceType type) const;

        /// Get baseline for current location
        uint32_t GetBaseline(ResourceType type) const;

        /// Check if baseline is stable for current location
        bool IsBaselineStable(ResourceType type) const;

        /// Get learning progress (0.0 - 1.0)
        float GetLearningProgress() const;

        /// Check if learning period is complete
        bool IsLearningComplete() const;

        /// Reset all learned baselines
        void ResetBaselines();

        /// Save baselines to file
        bool SaveBaselines(const std::string& filepath);

        /// Load baselines from file
        bool LoadBaselines(const std::string& filepath);

        /// Enable/disable learning mode
        void SetLearningEnabled(bool enabled) { m_learningEnabled = enabled; }
        bool IsLearningEnabled() const { return m_learningEnabled; }

        /// Configuration
        void SetLearningPeriod(uint32_t seconds) { m_learningPeriodSeconds = seconds; }
        void SetBaselineWindowSize(uint32_t seconds) { m_baselineWindowSeconds = seconds; }
        void SetStabilityThreshold(float threshold) { m_stabilityThreshold = threshold; }
        void SetWarningThreshold(float threshold) { m_warningThreshold = threshold; }
        void SetCriticalThreshold(float threshold) { m_criticalThreshold = threshold; }

        /// Get statistics
        struct Stats {
            uint32_t locationsLearned;
            uint32_t totalSamples;
            uint32_t stableBaselines;
            float learningProgress;
            bool learningComplete;
        };
        Stats GetStats() const;

    private:
        AdaptiveResourceMonitor() = default;
        ~AdaptiveResourceMonitor() = default;
        AdaptiveResourceMonitor(const AdaptiveResourceMonitor&) = delete;
        AdaptiveResourceMonitor& operator=(const AdaptiveResourceMonitor&) = delete;

        /// Get location key for current context
        std::string GetCurrentLocationKey() const;

        /// Update baseline for a resource type
        void UpdateBaseline(ResourceType type, uint32_t count);

        /// Calculate median of samples
        uint32_t CalculateMedian(const std::vector<uint32_t>& samples) const;

        /// Calculate standard deviation
        float CalculateStdDev(const std::vector<uint32_t>& samples, uint32_t mean) const;

        /// Detect if count represents a spawn event (sudden jump)
        bool IsSpawnEvent(ResourceType type, uint32_t count) const;

        /// Calculate safe margin for current location
        uint32_t CalculateSafeMargin(ResourceType type) const;

        /// Get current count for resource type
        uint32_t GetCurrentCount(ResourceType type) const;

        // Configuration
        bool m_learningEnabled = true;
        uint32_t m_learningPeriodSeconds = 1800;  // 30 minutes
        uint32_t m_baselineWindowSeconds = 300;   // 5-minute rolling window
        float m_stabilityThreshold = 0.05f;       // 5% StdDev tolerance
        uint32_t m_minSamplesForBaseline = 20;    // Minimum samples before declaring stable
        float m_warningThreshold = 0.75f;         // Warn at 75% of safe margin
        float m_criticalThreshold = 0.95f;        // Critical at 95% of safe margin

        // Baseline storage (per resource type, per location)
        std::unordered_map<ResourceType, std::unordered_map<std::string, LocationBaseline>> m_baselines;

        // Learning state
        std::atomic<uint64_t> m_learningStartTime{0};
        std::atomic<bool> m_learningComplete{false};

        // Last known counts (for spawn detection)
        std::unordered_map<ResourceType, uint32_t> m_lastCounts;
        std::unordered_map<ResourceType, uint64_t> m_lastCountTime;

        // Current location
        mutable std::string m_currentLocationKey;
        mutable uint64_t m_lastLocationUpdate = 0;

        mutable std::shared_mutex m_mutex;
    };

}
