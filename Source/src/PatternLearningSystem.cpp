// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "PatternLearningSystem.h"
#include "PerformanceMetrics.h"
#include "BatchOperations.h"
#include "Config.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <shared_mutex>  // For std::shared_mutex and std::shared_lock

using json = nlohmann::json;

namespace PatternLearning {

    // Static member initialization
    std::unordered_map<std::string, PatternEntry> PatternLearningSystem::s_patternDatabase;
    std::shared_mutex PatternLearningSystem::s_databaseMutex;  // Upgraded to shared_mutex for read-heavy operations
    size_t PatternLearningSystem::s_totalCrashes = 0;
    std::string PatternLearningSystem::s_databasePath = "Data/SKSE/Plugins/CrashGuard/patterns.json";
    bool PatternLearningSystem::s_initialized = false;
    bool PatternLearningSystem::s_patternsLoaded = false;

    void PatternLearningSystem::EnsurePatternsLoaded() {
        // Lazy loading: Only load patterns on first access
        // Requirements: Performance - Defer pattern database loading
        if (s_patternsLoaded) {
            return;
        }
        
        std::unique_lock<std::shared_mutex> lock(s_databaseMutex);
        
        // Double-check after acquiring lock
        if (s_patternsLoaded) {
            return;
        }
        
        spdlog::info("Lazy loading pattern database from disk...");
        LoadPatternsFromFile(s_databasePath);
        s_patternsLoaded = true;
        spdlog::info("Pattern database loaded: {} patterns", s_patternDatabase.size());
    }

    bool PatternLearningSystem::Initialize() {
        if (s_initialized) {
            return true;
        }

        // Lazy initialization: Don't load patterns from file until first crash
        // Requirements: Performance - Defer pattern database loading
        spdlog::info("PatternLearningSystem initialized (patterns will be loaded on first crash)");

        s_initialized = true;
        return true;
    }

    void PatternLearningSystem::Shutdown() {
        if (!s_initialized) {
            return;
        }

        // Flush any batched pattern writes before saving
        Performance::BatchPatternWriter::GetInstance().Flush();

        // Save patterns to file
        SavePatternsToFile(s_databasePath);

        s_initialized = false;
    }

    void PatternLearningSystem::RecordSuccess(const VEH::CrashContext& context,
                                             DynamicFix::RecoveryStrategy strategy) {
        // Lazy loading: Ensure patterns are loaded before accessing
        EnsurePatternsLoaded();
        
        std::unique_lock<std::shared_mutex> lock(s_databaseMutex);  // Use unique_lock for write operation

        // Generate crash signature
        std::string signature = GenerateCrashSignature(context);

        // Find or create pattern entry
        auto& pattern = s_patternDatabase[signature];
        
        // Initialize pattern if new
        if (pattern.signature.empty()) {
            pattern.signature = signature;
            pattern.category = RootCauseAnalysis::RootCauseAnalyzer::ClassifyCrash(context);
            pattern.totalOccurrences = 0;
            pattern.firstSeen = context.timestamp;
            
            // Increment patterns learned counter
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementPatternsLearned();
        }

        // Update pattern metadata
        pattern.totalOccurrences++;
        pattern.lastSeen = context.timestamp;

        // Emit aggregated log entry when this crash site has been seen before
        if (Config::Get().aggregatePatterns && pattern.totalOccurrences > 1) {
            spdlog::info("[AGGREGATED x{}] crash signature {} ({})",
                pattern.totalOccurrences,
                signature,
                DynamicFix::RecoveryStrategyToString(strategy));
        }

        // Find or create strategy record
        auto strategyIt = std::find_if(pattern.strategies.begin(), pattern.strategies.end(),
            [strategy](const StrategyRecord& record) {
                return record.strategy == strategy;
            });

        if (strategyIt != pattern.strategies.end()) {
            // Update existing strategy
            strategyIt->successCount++;
        } else {
            // Add new strategy
            StrategyRecord newRecord;
            newRecord.strategy = strategy;
            newRecord.successCount = 1;
            newRecord.failureCount = 0;
            newRecord.successRate = 1.0f;
            pattern.strategies.push_back(newRecord);
        }

        // Update success rates
        UpdateSuccessRates(pattern);

        s_totalCrashes++;
        
        // Batch pattern write instead of immediate write
        Performance::BatchPatternWriter::GetInstance().AddUpdate(signature, "success");
    }

    void PatternLearningSystem::RecordFailure(const VEH::CrashContext& context,
                                             DynamicFix::RecoveryStrategy strategy) {
        // Lazy loading: Ensure patterns are loaded before accessing
        EnsurePatternsLoaded();
        
        std::unique_lock<std::shared_mutex> lock(s_databaseMutex);  // Use unique_lock for write operation

        // Generate crash signature
        std::string signature = GenerateCrashSignature(context);

        // Find or create pattern entry
        auto& pattern = s_patternDatabase[signature];
        
        // Initialize pattern if new
        if (pattern.signature.empty()) {
            pattern.signature = signature;
            pattern.category = RootCauseAnalysis::RootCauseAnalyzer::ClassifyCrash(context);
            pattern.totalOccurrences = 0;
            pattern.firstSeen = context.timestamp;
        }

        // Update pattern metadata
        pattern.totalOccurrences++;
        pattern.lastSeen = context.timestamp;

        // Find or create strategy record
        auto strategyIt = std::find_if(pattern.strategies.begin(), pattern.strategies.end(),
            [strategy](const StrategyRecord& record) {
                return record.strategy == strategy;
            });

        if (strategyIt != pattern.strategies.end()) {
            // Update existing strategy
            strategyIt->failureCount++;
        } else {
            // Add new strategy
            StrategyRecord newRecord;
            newRecord.strategy = strategy;
            newRecord.successCount = 0;
            newRecord.failureCount = 1;
            newRecord.successRate = 0.0f;
            pattern.strategies.push_back(newRecord);
        }

        // Update success rates
        UpdateSuccessRates(pattern);

        s_totalCrashes++;
        
        // Batch pattern write instead of immediate write
        Performance::BatchPatternWriter::GetInstance().AddUpdate(signature, "failure");
    }

    DynamicFix::RecoveryStrategy PatternLearningSystem::GetBestStrategy(
        const VEH::CrashContext& context) {
        // Lazy loading: Ensure patterns are loaded before accessing
        EnsurePatternsLoaded();
        
        std::shared_lock<std::shared_mutex> lock(s_databaseMutex);  // Use shared_lock for read-only operation

        // Generate crash signature
        std::string signature = GenerateCrashSignature(context);

        // Look up pattern in database
        auto it = s_patternDatabase.find(signature);
        
        if (it != s_patternDatabase.end() && !it->second.strategies.empty()) {
            // Found pattern with strategies
            const auto& pattern = it->second;
            
            // Sort strategies by success rate (descending)
            auto sortedStrategies = pattern.strategies;
            std::sort(sortedStrategies.begin(), sortedStrategies.end(),
                [](const StrategyRecord& a, const StrategyRecord& b) {
                    return a.successRate > b.successRate;
                });

            // Increment patterns applied counter
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementPatternsApplied();

            // Return highest-rated strategy
            return sortedStrategies[0].strategy;
        }

        // Pattern not found, use category-based default
        RootCauseAnalysis::CrashCategory category = 
            RootCauseAnalysis::RootCauseAnalyzer::ClassifyCrash(context);
        
        return GetDefaultStrategy(category);
    }

    bool PatternLearningSystem::ExportPatterns(const std::string& filepath) {
        std::unique_lock<std::shared_mutex> lock(s_databaseMutex);  // Use unique_lock for write operation

        try {
            json exportData;
            
            // Add metadata
            exportData["metadata"]["version"] = "1.0.0";
            exportData["metadata"]["exportDate"] = std::chrono::system_clock::now().time_since_epoch().count();
            exportData["metadata"]["patternCount"] = s_patternDatabase.size();
            exportData["metadata"]["totalCrashes"] = s_totalCrashes;

            // Add patterns
            json patternsArray = json::array();
            
            for (const auto& [signature, pattern] : s_patternDatabase) {
                json patternObj;
                patternObj["signature"] = pattern.signature;
                patternObj["category"] = RootCauseAnalysis::CrashCategoryToString(pattern.category);
                patternObj["totalOccurrences"] = pattern.totalOccurrences;
                patternObj["firstSeen"] = pattern.firstSeen;
                patternObj["lastSeen"] = pattern.lastSeen;

                // Add strategies
                json strategiesArray = json::array();
                for (const auto& strategy : pattern.strategies) {
                    json strategyObj;
                    strategyObj["name"] = DynamicFix::RecoveryStrategyToString(strategy.strategy);
                    strategyObj["successCount"] = strategy.successCount;
                    strategyObj["failureCount"] = strategy.failureCount;
                    strategyObj["successRate"] = strategy.successRate;
                    strategiesArray.push_back(strategyObj);
                }
                patternObj["strategies"] = strategiesArray;

                patternsArray.push_back(patternObj);
            }
            
            exportData["patterns"] = patternsArray;

            // Write to file
            std::ofstream file(filepath);
            if (!file.is_open()) {
                return false;
            }

            file << exportData.dump(2);  // Pretty print with 2-space indent
            file.close();

            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    bool PatternLearningSystem::ImportPatterns(const std::string& filepath) {
        // NOTE: Caller (EnsurePatternsLoaded) already holds s_databaseMutex.
        // Do NOT lock here — std::shared_mutex is not recursive.

        try {
            std::ifstream file(filepath);
            if (!file.is_open()) {
                return false;
            }

            json importData;
            file >> importData;
            file.close();

            // Validate metadata
            if (!importData.contains("metadata") || !importData.contains("patterns")) {
                return false;
            }

            // Import patterns
            for (const auto& patternObj : importData["patterns"]) {
                PatternEntry pattern;
                pattern.signature = patternObj["signature"];
                
                // Parse category
                std::string categoryStr = patternObj["category"];
                // Convert string to category enum (simplified)
                if (categoryStr == "Mesh") pattern.category = RootCauseAnalysis::CrashCategory::Mesh;
                else if (categoryStr == "Animation") pattern.category = RootCauseAnalysis::CrashCategory::Animation;
                else if (categoryStr == "Script") pattern.category = RootCauseAnalysis::CrashCategory::Script;
                else if (categoryStr == "AI") pattern.category = RootCauseAnalysis::CrashCategory::AI;
                else if (categoryStr == "Cell") pattern.category = RootCauseAnalysis::CrashCategory::Cell;
                else if (categoryStr == "Memory") pattern.category = RootCauseAnalysis::CrashCategory::Memory;
                else if (categoryStr == "GridBoundary") pattern.category = RootCauseAnalysis::CrashCategory::GridBoundary;
                else pattern.category = RootCauseAnalysis::CrashCategory::Unknown;

                pattern.totalOccurrences = patternObj["totalOccurrences"];
                pattern.firstSeen = patternObj["firstSeen"];
                pattern.lastSeen = patternObj["lastSeen"];

                // Import strategies
                for (const auto& strategyObj : patternObj["strategies"]) {
                    StrategyRecord strategy;
                    
                    // Parse strategy enum from string
                    std::string strategyStr = strategyObj["name"];
                    if (strategyStr == "MeshRepair") strategy.strategy = DynamicFix::RecoveryStrategy::MeshRepair;
                    else if (strategyStr == "MeshFallback") strategy.strategy = DynamicFix::RecoveryStrategy::MeshFallback;
                    else if (strategyStr == "AnimationRetry") strategy.strategy = DynamicFix::RecoveryStrategy::AnimationRetry;
                    else if (strategyStr == "AnimationFallback") strategy.strategy = DynamicFix::RecoveryStrategy::AnimationFallback;
                    else if (strategyStr == "ScriptSkip") strategy.strategy = DynamicFix::RecoveryStrategy::ScriptSkip;
                    else if (strategyStr == "ScriptTerminate") strategy.strategy = DynamicFix::RecoveryStrategy::ScriptTerminate;
                    else if (strategyStr == "CellReload") strategy.strategy = DynamicFix::RecoveryStrategy::CellReload;
                    else if (strategyStr == "CellTeleport") strategy.strategy = DynamicFix::RecoveryStrategy::CellTeleport;
                    else if (strategyStr == "MemoryFree") strategy.strategy = DynamicFix::RecoveryStrategy::MemoryFree;
                    else if (strategyStr == "InstructionPatch") strategy.strategy = DynamicFix::RecoveryStrategy::InstructionPatch;
                    else if (strategyStr == "StateRollback") strategy.strategy = DynamicFix::RecoveryStrategy::StateRollback;
                    else if (strategyStr == "NullPointerFix") strategy.strategy = DynamicFix::RecoveryStrategy::NullPointerFix;
                    else if (strategyStr == "MissingResourceFix") strategy.strategy = DynamicFix::RecoveryStrategy::MissingResourceFix;
                    else strategy.strategy = DynamicFix::RecoveryStrategy::Unknown;

                    strategy.successCount = strategyObj["successCount"];
                    strategy.failureCount = strategyObj["failureCount"];
                    strategy.successRate = strategyObj["successRate"];

                    pattern.strategies.push_back(strategy);
                }

                // Validate pattern before adding
                if (ValidatePatternData(pattern)) {
                    s_patternDatabase[pattern.signature] = pattern;
                }
            }

            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    size_t PatternLearningSystem::GetPatternCount() {
        std::shared_lock<std::shared_mutex> lock(s_databaseMutex);  // Use shared_lock for read-only operation
        return s_patternDatabase.size();
    }

    size_t PatternLearningSystem::GetTotalCrashes() {
        std::shared_lock<std::shared_mutex> lock(s_databaseMutex);  // Use shared_lock for read-only operation
        return s_totalCrashes;
    }

    void PatternLearningSystem::ClearPatterns() {
        std::unique_lock<std::shared_mutex> lock(s_databaseMutex);  // Use unique_lock for write operation
        s_patternDatabase.clear();
        s_totalCrashes = 0;
    }

    std::string PatternLearningSystem::GenerateCrashSignature(const VEH::CrashContext& context) {
        // Generate signature from exception code, crash address, and top 3 stack frames
        std::ostringstream signature;
        
        // Add exception code
        signature << std::hex << std::setfill('0') << std::setw(8) << context.exceptionCode;
        signature << "_";
        
        // Add crash address (lower 32 bits for consistency)
        signature << std::hex << std::setfill('0') << std::setw(8) 
                  << (reinterpret_cast<uintptr_t>(context.crashAddress) & 0xFFFFFFFF);
        signature << "_";
        
        // Add top 3 stack frames
        size_t frameCount = std::min(size_t(3), context.callStack.size());
        for (size_t i = 0; i < frameCount; ++i) {
            signature << std::hex << std::setfill('0') << std::setw(8)
                      << (reinterpret_cast<uintptr_t>(context.callStack[i].address) & 0xFFFFFFFF);
            if (i < frameCount - 1) {
                signature << "_";
            }
        }

        return signature.str();
    }

    void PatternLearningSystem::UpdateSuccessRates(PatternEntry& pattern) {
        for (auto& strategy : pattern.strategies) {
            uint32_t total = strategy.successCount + strategy.failureCount;
            if (total > 0) {
                strategy.successRate = static_cast<float>(strategy.successCount) / static_cast<float>(total);
            } else {
                strategy.successRate = 0.0f;
            }
        }
    }

    DynamicFix::RecoveryStrategy PatternLearningSystem::GetDefaultStrategy(
        RootCauseAnalysis::CrashCategory category) {
        // Return category-based default strategies
        switch (category) {
            case RootCauseAnalysis::CrashCategory::Mesh:
                return DynamicFix::RecoveryStrategy::MeshRepair;
            
            case RootCauseAnalysis::CrashCategory::Animation:
                return DynamicFix::RecoveryStrategy::AnimationRetry;
            
            case RootCauseAnalysis::CrashCategory::Script:
                return DynamicFix::RecoveryStrategy::ScriptSkip;
            
            case RootCauseAnalysis::CrashCategory::Cell:
                return DynamicFix::RecoveryStrategy::CellReload;
            
            case RootCauseAnalysis::CrashCategory::Memory:
                return DynamicFix::RecoveryStrategy::MemoryFree;
            
            case RootCauseAnalysis::CrashCategory::AI:
            case RootCauseAnalysis::CrashCategory::GridBoundary:
                return DynamicFix::RecoveryStrategy::CellTeleport;
            
            case RootCauseAnalysis::CrashCategory::Unknown:
            default:
                return DynamicFix::RecoveryStrategy::StateRollback;
        }
    }

    bool PatternLearningSystem::LoadPatternsFromFile(const std::string& filepath) {
        // Try to load existing patterns
        return ImportPatterns(filepath);
    }

    bool PatternLearningSystem::SavePatternsToFile(const std::string& filepath) {
        // Save patterns to file
        return ExportPatterns(filepath);
    }

    bool PatternLearningSystem::ValidatePatternData(const PatternEntry& pattern) {
        // Validate pattern has required fields
        if (pattern.signature.empty()) {
            return false;
        }

        // Validate strategies have valid counts
        for (const auto& strategy : pattern.strategies) {
            if (strategy.successCount == 0 && strategy.failureCount == 0) {
                return false;
            }
        }

        return true;
    }

}  // namespace PatternLearning
