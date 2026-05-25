// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "VEH.h"
#include "GameObjectIntrospector.h"
#include <string>
#include <vector>
#include <cstdint>

/// Root Cause Analyzer - Diagnoses the underlying cause of crashes
namespace RootCauseAnalysis {

    /// Crash category classification
    enum class CrashCategory {
        Mesh,
        Animation,
        Script,
        AI,
        Cell,
        Memory,
        GridBoundary,
        Unknown
    };

    /// Grid boundary crash information
    struct GridBoundaryInfo {
        bool isGridBoundaryCrash;
        void* actorPtr;
        std::string actorEditorID;
        std::string actorModName;
        float distanceToNearestBoundary;
        std::string aiPackageInfo;
        std::string targetCellInfo;
        bool targetCellLoaded;
    };

    /// Interior cell lighting crash information
    struct InteriorCellLightingInfo {
        bool isInteriorCellLightingCrash;
        std::string cellName;
        std::string lightingSystemType;  // e.g., "BSShadowFrustumLight", "Lighting", "Shadows"
        bool isShadowRelated;
        bool isParticleLightingRelated;
        std::vector<std::string> involvedLightMods;
        std::string suggestedRecoveryAction;
    };

    /// Root cause analysis result
    struct RootCauseResult {
        CrashCategory category;
        std::string description;
        std::vector<std::string> involvedMods;
        std::vector<std::string> suggestedFixes;
        float confidence;
        GridBoundaryInfo gridBoundaryInfo;
        InteriorCellLightingInfo interiorCellLightingInfo;
    };

    /// Main root cause analyzer class
    class RootCauseAnalyzer {
    public:
        /// Analyze crash and determine root cause
        static RootCauseResult AnalyzeCrash(const VEH::CrashContext& context);

        /// Identify involved game objects from crash context
        static std::vector<GameObjectIntrospection::GameObjectInfo> 
            IdentifyInvolvedObjects(const VEH::CrashContext& context);

        /// Determine crash category based on context
        static CrashCategory ClassifyCrash(const VEH::CrashContext& context);

        /// Calculate confidence score for root cause analysis
        static float CalculateConfidence(const RootCauseResult& result);

        /// Implement grid boundary crash detection
        static GridBoundaryInfo DetectGridBoundaryCrash(const VEH::CrashContext& context);

        /// Rank suspected mods by likelihood
        static std::vector<std::string> RankSuspectedMods(
            const std::vector<GameObjectIntrospection::GameObjectInfo>& objects,
            const VEH::CrashContext& context);

        /// Generate suggested fixes based on root cause
        static std::vector<std::string> GenerateSuggestedFixes(
            const RootCauseResult& result);

    private:
        /// Check if crash is null pointer related
        static bool IsNullPointerCrash(const VEH::CrashContext& context);

        /// Check if crash is mesh related
        static bool IsMeshCrash(const VEH::CrashContext& context);

        /// Check if crash is animation related
        static bool IsAnimationCrash(const VEH::CrashContext& context);

        /// Check if crash is script related
        static bool IsScriptCrash(const VEH::CrashContext& context);

        /// Check if crash is AI related
        static bool IsAICrash(const VEH::CrashContext& context);

        /// Check if crash is cell loading related
        static bool IsCellCrash(const VEH::CrashContext& context);

        /// Check if crash is memory related
        static bool IsMemoryCrash(const VEH::CrashContext& context);

        /// Check if crash is grid boundary related
        static bool IsGridBoundaryCrash(const VEH::CrashContext& context);

        /// Check if crash is interior cell lighting related
        static bool IsInteriorCellLightingCrash(const VEH::CrashContext& context);

        /// Detect interior cell lighting crash details
        static InteriorCellLightingInfo DetectInteriorCellLightingCrash(
            const VEH::CrashContext& context);

        /// Analyze call stack for signature patterns
        static std::string AnalyzeCallStackSignature(
            const std::vector<VEH::StackFrame>& callStack);

        /// Check if actor is near cell boundary
        static bool IsNearCellBoundary(void* actorPtr, float& outDistance);

        /// Get AI package information for actor
        static std::string GetAIPackageInfo(void* actorPtr);

        /// Check if AI package target is in unloaded cell
        static bool IsTargetInUnloadedCell(void* actorPtr, std::string& outCellInfo);
    };

    /// Convert crash category to string
    const char* CrashCategoryToString(CrashCategory category);

}  // namespace RootCauseAnalysis
