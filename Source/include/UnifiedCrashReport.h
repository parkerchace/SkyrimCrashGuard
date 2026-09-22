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
#include <optional>
#include <memory>

namespace VEH {
    struct CrashContext;
    struct StackFrame;
    enum class SeverityLevel;
}

namespace CrashLoggerDetector {
    struct LoggerInfo;
}

/// Unified Crash Report Format
/// Merges data from VEH, CrashLogger, and Trainwreck sources
namespace UnifiedCrashReport {

    /// Source of crash data
    enum class DataSource {
        VEH,            // CrashGuard VEH handler
        CrashLogger,    // Traditional CrashLogger
        Trainwreck,     // Trainwreck crash logger
        External        // Other sources
    };

    /// Crash report section
    struct ReportSection {
        std::string name;
        std::string content;
        DataSource source;
        std::string timestamp;
        std::unordered_map<std::string, std::string> metadata;
    };

    /// Game object information
    struct GameObjectData {
        std::string type;
        std::string formID;
        std::string editorID;
        std::string modName;
        uintptr_t address;
        bool isValid;
    };

    /// Recovery action information
    struct RecoveryAction {
        std::string strategy;
        bool success;
        std::vector<std::string> actions;
        std::string timestamp;
        std::string failureReason;
    };

    /// System information
    struct SystemInfo {
        std::string skyrimVersion;
        std::string skseVersion;
        std::string crashGuardVersion;
        std::string osVersion;
        std::vector<std::string> loadedMods;
        std::vector<CrashLoggerDetector::LoggerInfo> activeCrashLoggers;
    };

    /// Unified crash report data structure
    struct UnifiedReport {
        // Core crash information
        DWORD exceptionCode;
        uintptr_t crashAddress;
        std::string severity;
        std::string rootCause;
        float confidence;
        std::string timestamp;
        
        // Call stack and registers
        std::vector<VEH::StackFrame> callStack;
        CONTEXT cpuRegisters;
        
        // Game state
        std::optional<GameObjectData> involvedObject;
        std::string currentCell;
        std::string playerPosition;
        std::vector<std::string> activeQuests;
        std::vector<std::string> nearbyNPCs;
        
        // Recovery information
        std::vector<RecoveryAction> recoveryActions;
        bool recoveryAttempted;
        bool recoverySuccessful;
        
        // System information
        SystemInfo systemInfo;
        
        // Report sections from different sources
        std::vector<ReportSection> sections;
        
        // Metadata
        std::unordered_map<std::string, std::string> metadata;
        std::string reportID;
        DataSource primarySource;
    };

    /// C-compatible data export structure
    struct CExportData {
        const char* key;
        const char* value;
        const char* source;
    };

    /// Unified crash report manager
    class ReportManager {
    public:
        /// Initialize report manager
        static bool Initialize();
        
        /// Shutdown report manager
        static void Shutdown();
        
        /// Create new unified report from VEH context
        static std::shared_ptr<UnifiedReport> CreateReport(const VEH::CrashContext& context);
        
        /// Merge data from CrashLogger
        static void MergeCrashLoggerData(std::shared_ptr<UnifiedReport> report,
                                        const std::string& crashLoggerData);
        
        /// Merge data from Trainwreck
        static void MergeTrainwreckData(std::shared_ptr<UnifiedReport> report,
                                       const std::unordered_map<std::string, std::string>& trainwreckData);
        
        /// Add recovery information
        static void AddRecoveryInfo(std::shared_ptr<UnifiedReport> report,
                                   const RecoveryAction& recovery);
        
        /// Add custom section
        static void AddSection(std::shared_ptr<UnifiedReport> report,
                              const std::string& name, const std::string& content,
                              DataSource source = DataSource::VEH);
        
        /// Export to C-compatible format for external APIs
        static std::vector<CExportData> ExportToCFormat(const UnifiedReport& report);
        
        /// Generate formatted report text
        static std::string GenerateReportText(const UnifiedReport& report);
        
        /// Generate JSON report
        static std::string GenerateJSONReport(const UnifiedReport& report);
        
        /// Ensure version-independent operation
        static bool ValidateVersionCompatibility();
        
        /// Get current report format version
        static std::string GetFormatVersion();

    private:
        /// Convert VEH context to unified format
        static void ConvertVEHContext(const VEH::CrashContext& context, UnifiedReport& report);
        
        /// Extract system information
        static SystemInfo GatherSystemInfo();
        
        /// Generate unique report ID
        static std::string GenerateReportID();
        
        /// Format timestamp
        static std::string FormatTimestamp();
        
        /// Convert severity enum to string
        static std::string SeverityToString(VEH::SeverityLevel severity);
        
        static bool s_initialized;
        static std::string s_formatVersion;
    };

}  // namespace UnifiedCrashReport