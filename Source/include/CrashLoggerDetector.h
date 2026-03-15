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

/// Crash Logger Detection and Compatibility Management
/// Detects installed crash loggers and enables appropriate compatibility modes
namespace CrashLoggerDetector {

    /// Supported crash logger types
    enum class LoggerType {
        CrashLogger,    // Traditional CrashLogger by meh321
        Trainwreck,     // Trainwreck by Ryan-rsm-McKenzie
        Unknown
    };

    /// Information about a detected crash logger
    struct LoggerInfo {
        LoggerType type;
        std::string name;
        std::string version;
        std::string dllPath;
        HMODULE moduleHandle;
        bool isActive;
    };

    /// Crash logger detection and management class
    class Detector {
    public:
        /// Initialize detector and scan for crash loggers
        static bool Initialize();
        
        /// Shutdown detector
        static void Shutdown();
        
        /// Detect all installed crash loggers
        static std::vector<LoggerInfo> DetectInstalledLoggers();
        
        /// Check if CrashLogger is present
        static bool IsCrashLoggerPresent();
        
        /// Check if Trainwreck is present
        static bool IsTrainwreckPresent();
        
        /// Get CrashLogger information
        static std::optional<LoggerInfo> GetCrashLoggerInfo();
        
        /// Get Trainwreck information
        static std::optional<LoggerInfo> GetTrainwreckInfo();
        
        /// Enable compatibility mode for detected loggers
        static void EnableCompatibilityModes();
        
        /// Get status string for logging
        static std::string GetDetectionStatusString();
        
        /// Get list of all detected loggers
        static const std::vector<LoggerInfo>& GetDetectedLoggers();

    private:
        /// Detect CrashLogger DLL presence
        static std::optional<LoggerInfo> DetectCrashLogger();
        
        /// Detect Trainwreck DLL presence
        static std::optional<LoggerInfo> DetectTrainwreck();
        
        /// Get version information from DLL
        static std::string GetDLLVersion(HMODULE module);
        
        /// Get full path of loaded module
        static std::string GetModulePath(HMODULE module);
        
        /// Check if module exports expected functions
        static bool ValidateLoggerModule(HMODULE module, LoggerType type);
        
        static std::vector<LoggerInfo> s_detectedLoggers;
        static bool s_initialized;
    };

}  // namespace CrashLoggerDetector