// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace VEH {
    struct CrashContext;
}

namespace UnifiedCrashReport {
    struct UnifiedReport;
    struct GameObjectData;
    struct RecoveryAction;
}

/// Post-Mortem Coordination with trainwreck-post-mortem plugin
/// Shares extended crash data for enhanced post-crash analysis
namespace PostMortemCoordination {

    /// Shared data types for post-mortem analysis
    struct SharedGameObjectInfo {
        std::string type;
        std::string formID;
        std::string editorID;
        std::string modName;
        uintptr_t address;
        bool isValid;
        std::unordered_map<std::string, std::string> properties;
    };

    struct SharedRootCauseInfo {
        std::string category;
        std::string description;
        float confidence;
        std::vector<std::string> suspectedMods;
        std::vector<std::string> suggestedFixes;
        std::string analysisMethod;
    };

    struct SharedRecoveryInfo {
        std::string strategy;
        bool success;
        std::vector<std::string> actions;
        std::string timestamp;
        std::string failureReason;
        std::unordered_map<std::string, std::string> metadata;
    };

    /// Post-mortem plugin interface
    struct PostMortemAPI {
        // Function signatures for trainwreck-post-mortem plugin
        using ShareGameObjectInfo_t = void(*)(const char* jsonData);
        using ShareRootCauseInfo_t = void(*)(const char* jsonData);
        using ShareRecoveryHistory_t = void(*)(const char* jsonData);
        using RegisterDataProvider_t = bool(*)(const char* providerName, const char* version);
        using IsPostMortemActive_t = bool(*)();
    };

    /// Post-mortem coordination manager
    class Coordinator {
    public:
        /// Initialize post-mortem coordination
        static bool Initialize();
        
        /// Shutdown coordination
        static void Shutdown();
        
        /// Check if trainwreck-post-mortem plugin is available
        static bool IsPostMortemAvailable();
        
        /// Share game object introspection data
        static bool ShareGameObjectIntrospection(
            const std::vector<SharedGameObjectInfo>& objects);
        
        /// Share root cause analysis results
        static bool ShareRootCauseAnalysis(const SharedRootCauseInfo& rootCause);
        
        /// Share recovery action history
        static bool ShareRecoveryActionHistory(
            const std::vector<SharedRecoveryInfo>& recoveryHistory);
        
        /// Share complete unified report
        static bool ShareUnifiedReport(const UnifiedCrashReport::UnifiedReport& report);
        
        /// Convert VEH crash context to shared format
        static SharedRootCauseInfo ConvertCrashContextToShared(const VEH::CrashContext& context);
        
        /// Convert unified report game objects to shared format
        static std::vector<SharedGameObjectInfo> ConvertGameObjectsToShared(
            const std::vector<UnifiedCrashReport::GameObjectData>& objects);
        
        /// Convert unified report recovery actions to shared format
        static std::vector<SharedRecoveryInfo> ConvertRecoveryActionsToShared(
            const std::vector<UnifiedCrashReport::RecoveryAction>& actions);

    private:
        /// Load post-mortem plugin API
        static bool LoadPostMortemAPI();
        
        /// Unload post-mortem plugin API
        static void UnloadPostMortemAPI();
        
        /// Register as data provider
        static bool RegisterAsDataProvider();
        
        /// Convert shared data to JSON
        static std::string SerializeGameObjectInfo(
            const std::vector<SharedGameObjectInfo>& objects);
        static std::string SerializeRootCauseInfo(const SharedRootCauseInfo& rootCause);
        static std::string SerializeRecoveryHistory(
            const std::vector<SharedRecoveryInfo>& history);
        
        /// Resolve API function
        template<typename T>
        static bool ResolveFunction(HMODULE module, const char* name, T& outFunc);
        
        // API function pointers
        static PostMortemAPI::ShareGameObjectInfo_t s_shareGameObjectInfo;
        static PostMortemAPI::ShareRootCauseInfo_t s_shareRootCauseInfo;
        static PostMortemAPI::ShareRecoveryHistory_t s_shareRecoveryHistory;
        static PostMortemAPI::RegisterDataProvider_t s_registerDataProvider;
        static PostMortemAPI::IsPostMortemActive_t s_isPostMortemActive;
        
        // Module handle
        static HMODULE s_postMortemModule;
        static bool s_initialized;
        static bool s_apiAvailable;
    };

}  // namespace PostMortemCoordination