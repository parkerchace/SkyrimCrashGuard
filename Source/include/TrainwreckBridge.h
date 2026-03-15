// Copyright (C) 2024-2025 Parker Chace
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
#include <optional>
#include <functional>
#include <memory>

namespace VEH {
    struct CrashContext;
}

namespace CrashLoggerDetector {
    struct LoggerInfo;
}

/// Bridge to Trainwreck crash logger API
/// Provides version-independent crash logging integration
/// Uses runtime API loading for maximum compatibility
namespace TrainwreckBridge {

    /// Trainwreck API function signatures (C exports)
    namespace API {
        // Register a plugin with Trainwreck
        using RegisterPlugin_t = bool(*)(const char* pluginName, const char* version);
        
        // Add custom crash information
        using AddCrashInfo_t = void(*)(const char* key, const char* value);
        
        // Add a custom section to crash log
        using AddSection_t = void(*)(const char* sectionName, const char* content);
        
        // Check if Trainwreck is available
        using IsAvailable_t = bool(*)();
        
        // Get Trainwreck version
        using GetVersion_t = const char*(*)();
        
        // Extended crash information callback
        using SetCrashCallback_t = void(*)(void(*callback)(void* context), void* context);
        
        // Plugin status callback
        using SetStatusCallback_t = void(*)(void(*callback)(const char* status), void* context);
    }

    /// Trainwreck API wrapper class
    /// Handles runtime loading and provides modern C++ interface
    class TrainwreckAPI {
    public:
        /// Initialize and load Trainwreck API
        static bool Initialize();
        
        /// Shutdown and cleanup
        static void Shutdown();
        
        /// Check if Trainwreck is available
        static bool IsAvailable();
        
        /// Register CrashGuard as a Trainwreck plugin
        static bool RegisterPlugin();
        
        /// Set up crash data export callbacks
        static bool SetupCrashCallbacks();
        
        /// Provide extended crash information to Trainwreck
        static void ProvideExtendedCrashInfo(const VEH::CrashContext& context);
        
        /// Export crash context to Trainwreck
        static void ExportCrashContext(const VEH::CrashContext& context);
        
        /// Add custom crash information
        static void AddCrashInfo(const std::string& key, const std::string& value);
        
        /// Add a custom section to the crash log
        static void AddSection(const std::string& sectionName, const std::string& content);
        
        /// Export game object information
        static void ExportGameObjectInfo(void* object, const std::string& type,
                                        const std::string& formID, const std::string& editorID);
        
        /// Export root cause analysis
        static void ExportRootCause(const std::string& category, const std::string& description,
                                   float confidence, const std::vector<std::string>& suspectedMods);
        
        /// Export recovery actions
        static void ExportRecoveryActions(const std::vector<std::string>& actions,
                                         const std::string& strategy, bool success);
        
        /// Export severity classification
        static void ExportSeverity(const std::string& severity, const std::string& reason);
        
        /// Export pattern learning data
        static void ExportPatternData(const std::string& signature, uint32_t occurrences,
                                     const std::string& bestStrategy, float successRate);
        
        /// Get Trainwreck version string
        static std::string GetVersion();

    private:
        /// Load Trainwreck DLL and resolve API functions
        static bool LoadAPI();
        
        /// Unload Trainwreck DLL
        static void UnloadAPI();
        
        /// Resolve a single API function
        template<typename T>
        static bool ResolveFunction(HMODULE module, const char* name, T& outFunc);
        
        // API function pointers
        static API::RegisterPlugin_t s_registerPlugin;
        static API::AddCrashInfo_t s_addCrashInfo;
        static API::AddSection_t s_addSection;
        static API::IsAvailable_t s_isAvailable;
        static API::GetVersion_t s_getVersion;
        static API::SetCrashCallback_t s_setCrashCallback;
        static API::SetStatusCallback_t s_setStatusCallback;
        
        // Module handle
        static HMODULE s_trainwreckModule;
        static bool s_initialized;
    };

    /// Unified crash reporter that coordinates between VEH, CrashLogger, and Trainwreck
    class UnifiedCrashReporter {
    public:
        /// Initialize all available crash loggers
        static void Initialize();
        
        /// Shutdown all crash loggers
        static void Shutdown();
        
        /// Report a crash to all available loggers
        static void ReportCrash(const VEH::CrashContext& context);
        
        /// Add recovery information to all loggers
        static void ReportRecovery(const std::string& strategy, bool success,
                                  const std::vector<std::string>& actions);
        
        /// Check which loggers are available
        static bool IsCrashLoggerAvailable();
        static bool IsTrainwreckAvailable();
        
        /// Get status string for logging
        static std::string GetStatusString();
        
        /// Get detected logger information
        static std::vector<CrashLoggerDetector::LoggerInfo> GetDetectedLoggers();

    private:
        static bool s_crashLoggerAvailable;
        static bool s_trainwreckAvailable;
    };

}  // namespace TrainwreckBridge
