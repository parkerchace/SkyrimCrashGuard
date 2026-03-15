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
#include <filesystem>
#include <optional>

namespace VEH {
    struct CrashContext;
}

/// CrashLogger Integration for Log Injection and Coordination
/// Injects CrashGuard data into CrashLogger output files
namespace CrashLoggerIntegration {

    /// CrashLogger log file information
    struct LogFileInfo {
        std::filesystem::path filePath;
        std::string timestamp;
        size_t originalSize;
        bool hasInjection;
    };

    /// Recovery information to inject into logs
    struct RecoveryInfo {
        std::string strategy;
        bool success;
        std::vector<std::string> actions;
        std::string timestamp;
        std::string rootCause;
        std::string severity;
    };

    /// CrashLogger integration manager
    class LogInjector {
    public:
        /// Initialize CrashLogger integration
        static bool Initialize();
        
        /// Shutdown integration
        static void Shutdown();
        
        /// Check if CrashLogger is active
        static bool IsCrashLoggerActive();
        
        /// Inject warning header into CrashLogger output
        static bool InjectWarningHeader();
        
        /// Append recovery information to CrashLogger logs
        static bool AppendRecoveryInfo(const RecoveryInfo& recovery);
        
        /// Inject crash context data
        static bool InjectCrashContext(const VEH::CrashContext& context);
        
        /// Coordinate to avoid duplicate logging
        static void SetDuplicateLoggingPrevention(bool enabled);
        
        /// Separate CrashGuard data from CrashLogger data
        static bool CreateSeparateSection(const std::string& sectionName, 
                                         const std::string& content);
        
        /// Find latest CrashLogger log file
        static std::optional<LogFileInfo> FindLatestLogFile();
        
        /// Get CrashLogger log directory
        static std::filesystem::path GetLogDirectory();

    private:
        /// Inject content into log file
        static bool InjectIntoLogFile(const std::filesystem::path& logFile,
                                     const std::string& content,
                                     bool atBeginning = false);
        
        /// Create warning header content
        static std::string CreateWarningHeader();
        
        /// Create recovery section content
        static std::string CreateRecoverySection(const RecoveryInfo& recovery);
        
        /// Create crash context section
        static std::string CreateCrashContextSection(const VEH::CrashContext& context);
        
        /// Check if file already has CrashGuard injection
        static bool HasExistingInjection(const std::filesystem::path& logFile);
        
        /// Get file modification time
        static std::optional<std::filesystem::file_time_type> GetFileModTime(
            const std::filesystem::path& file);
        
        static bool s_initialized;
        static bool s_preventDuplicateLogging;
        static std::filesystem::path s_logDirectory;
        static std::vector<LogFileInfo> s_processedLogs;
    };

}  // namespace CrashLoggerIntegration