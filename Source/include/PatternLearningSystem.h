// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "VEH.h"
#include "RootCauseAnalyzer.h"
#include "DynamicFixApplicator.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <cstdint>

/// Pattern Learning System
/// Learns from crashes and improves recovery strategies
namespace PatternLearning {

    /// Strategy record tracking success/failure rates
    struct StrategyRecord {
        DynamicFix::RecoveryStrategy strategy;
        uint32_t successCount;
        uint32_t failureCount;
        float successRate;
    };

    /// Pattern entry in the learning database
    struct PatternEntry {
        std::string signature;
        RootCauseAnalysis::CrashCategory category;
        std::vector<StrategyRecord> strategies;
        uint32_t totalOccurrences;
        uint64_t firstSeen;
        uint64_t lastSeen;
    };

    /// Main pattern learning system class
    class PatternLearningSystem {
    public:
        /// Initialize the learning system
        static bool Initialize();

        /// Shutdown and persist patterns
        static void Shutdown();

        /// Record successful recovery
        static void RecordSuccess(const VEH::CrashContext& context,
                                 DynamicFix::RecoveryStrategy strategy);

        /// Record failed recovery
        static void RecordFailure(const VEH::CrashContext& context,
                                 DynamicFix::RecoveryStrategy strategy);

        /// Get best strategy for crash type
        static DynamicFix::RecoveryStrategy GetBestStrategy(const VEH::CrashContext& context);

        /// Export learned patterns to file
        static bool ExportPatterns(const std::string& filepath);

        /// Import learned patterns from file
        static bool ImportPatterns(const std::string& filepath);

        /// Get pattern database statistics
        static size_t GetPatternCount();

        /// Get total crashes recorded
        static size_t GetTotalCrashes();

        /// Clear all learned patterns
        static void ClearPatterns();

    private:
        /// Generate crash signature from context
        static std::string GenerateCrashSignature(const VEH::CrashContext& context);

        /// Update success rates for all strategies in a pattern
        static void UpdateSuccessRates(PatternEntry& pattern);

        /// Get default strategy for crash category
        static DynamicFix::RecoveryStrategy GetDefaultStrategy(
            RootCauseAnalysis::CrashCategory category);

        /// Load patterns from JSON file
        static bool LoadPatternsFromFile(const std::string& filepath);

        /// Save patterns to JSON file
        static bool SavePatternsToFile(const std::string& filepath);

        /// Validate imported pattern data
        static bool ValidatePatternData(const PatternEntry& pattern);

        /// Pattern database (signature -> pattern entry)
        static std::unordered_map<std::string, PatternEntry> s_patternDatabase;

        /// Shared mutex for thread-safe access to pattern database
        static std::shared_mutex s_databaseMutex;

        /// Total number of crashes recorded
        static size_t s_totalCrashes;

        /// Path to pattern database file
        static std::string s_databasePath;

        /// Whether the system is initialized
        static bool s_initialized;
        
        /// Whether patterns have been loaded from disk (lazy loading)
        static bool s_patternsLoaded;
        
        /// Ensure patterns are loaded (lazy initialization helper)
        static void EnsurePatternsLoaded();
    };

}  // namespace PatternLearning
