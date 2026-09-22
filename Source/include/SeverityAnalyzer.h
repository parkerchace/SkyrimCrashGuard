// Copyright (C) 2024-2026 Parker Chace
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of Skyrim Crash Guard.
//
// Skyrim Crash Guard is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version.
//
// Skyrim Crash Guard is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "VEH.h"
#include <string>
#include <vector>

// Forward declarations
namespace RootCauseAnalysis {
    struct RootCauseResult;
}

/// Severity Analysis - Provides detailed crash severity classification with human-readable explanations
/// Implements requirements from user-controlled-crash-notifications spec
namespace CrashGuard {

    /// Detailed severity analysis result with explanations and risk assessment
    struct SeverityAnalysis {
        VEH::SeverityLevel level;              // Safe, Warning, Critical, Fatal, Unknown
        std::string technicalReason;           // Technical description (e.g., "Crash in SaveGame function")
        std::string userExplanation;           // Plain English explanation for users
        std::string recommendation;            // Recommended user action
        std::vector<std::string> risks;        // List of potential risks (e.g., "Save corruption", "Quest data loss")
        float confidenceScore;                 // 0.0-1.0 confidence in classification
        std::string detectionMethod;           // How severity was determined (e.g., "Call stack analysis")
        
        // Boolean flags for specific risk categories
        bool affectsSaveData;                  // True if crash may corrupt save files
        bool affectsQuestData;                 // True if crash may affect quest progress
        bool affectsInventory;                 // True if crash may corrupt inventory
        bool isRecoverable;                    // True if crash can be safely recovered
    };

    /// Main severity analyzer class - analyzes crashes and provides detailed severity classification
    class SeverityAnalyzer {
    public:
        /// Analyze crash and generate detailed severity analysis
        /// @param context The crash context from VEH
        /// @param rootCause The root cause analysis result
        /// @return Detailed severity analysis with explanations and recommendations
        static SeverityAnalysis AnalyzeCrash(
            const VEH::CrashContext& context,
            const RootCauseAnalysis::RootCauseResult& rootCause);
        
    private:
        /// Classify severity based on call stack analysis
        /// Scans for dangerous function names (save/load, quest, inventory)
        static VEH::SeverityLevel ClassifyByCallStack(
            const std::vector<VEH::StackFrame>& callStack);
        
        /// Classify severity based on memory region
        /// Identifies crashes in critical memory regions
        static VEH::SeverityLevel ClassifyByMemoryRegion(
            void* crashAddress);
        
        /// Classify severity based on crash patterns
        /// Uses pattern learning system data
        static VEH::SeverityLevel ClassifyByPattern(
            const VEH::CrashContext& context);
        
        /// Generate plain English explanation for users
        static std::string GenerateUserExplanation(
            VEH::SeverityLevel level,
            const std::string& technicalReason);
        
        /// Generate recommendation based on severity and recovery status
        static std::string GenerateRecommendation(
            VEH::SeverityLevel level,
            bool recoverySuccessful);
        
        /// Identify potential risks based on severity and context
        static std::vector<std::string> IdentifyRisks(
            VEH::SeverityLevel level,
            const VEH::CrashContext& context);
        
        // Dangerous function patterns for call stack analysis
        static const std::vector<std::string> s_saveFunctions;
        static const std::vector<std::string> s_questFunctions;
        static const std::vector<std::string> s_inventoryFunctions;
        static const std::vector<std::string> s_playerDataFunctions;
        
        // Safe function patterns (rendering, UI, etc.)
        static const std::vector<std::string> s_renderingFunctions;
    };

}  // namespace CrashGuard
