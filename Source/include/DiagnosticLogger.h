// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <fstream>
#include <shared_mutex>

/// Advanced Diagnostic Logger for SkyrimCrashGuard
/// Provides next-generation crash logging with user-friendly format
/// Inspired by Unity, Chrome DevTools, Visual Studio, and modern APM tools
namespace Diagnostics {

    /// Severity levels for log entries
    enum class LogSeverity {
        Debug,      // Development information
        Info,       // General information
        Warning,    // Potential issues
        Error,      // Recoverable errors
        Critical,   // Unrecoverable errors
        Fatal       // System-ending errors
    };

    /// Log entry categories for organization
    enum class LogCategory {
        System,         // OS, hardware, environment
        Engine,         // Skyrim engine internals
        Mod,           // Mod-related issues
        Performance,   // Performance metrics
        Recovery,      // Recovery actions
        Pattern,       // Pattern learning
        User          // User actions/context
    };

    /// Structured log entry with rich metadata
    struct LogEntry {
        std::chrono::system_clock::time_point timestamp;
        LogSeverity severity;
        LogCategory category;
        std::string message;
        std::string component;
        std::unordered_map<std::string, std::string> metadata;
        std::vector<std::string> tags;
        std::string stackTrace;
        uint64_t threadId;
        std::string correlationId;  // Links related entries
    };

    /// Performance metrics for context
    struct PerformanceSnapshot {
        float fps;
        uint64_t memoryUsageMB;
        float cpuUsagePercent;
        uint32_t loadedMods;
        uint32_t activeNPCs;
        std::string currentCell;
        std::chrono::milliseconds sessionDuration;
    };

    /// Game state context for crash analysis
    struct GameStateContext {
        std::string playerLocation;
        std::vector<std::string> activeQuests;
        std::vector<std::string> nearbyNPCs;
        std::string currentActivity;
        std::vector<std::string> recentActions;
        std::string weatherCondition;
        std::string timeOfDay;
        bool isInCombat;
        bool isInDialogue;
        bool isInMenu;
    };

    /// Crash report with comprehensive analysis
    struct CrashReport {
        std::string reportId;
        std::chrono::system_clock::time_point timestamp;
        
        // Basic crash info
        uint32_t exceptionCode;
        void* crashAddress;
        std::string exceptionDescription;
        
        // Analysis results
        std::string rootCause;
        std::string category;
        float confidence;
        std::vector<std::string> suspectedMods;
        std::vector<std::string> suggestedFixes;
        
        std::string severityLevel;              // "Safe", "Warning", "Critical", "Fatal"
        std::string severityExplanation;        // Plain English explanation
        std::vector<std::string> severityRisks; // List of potential risks
        std::string severityRecommendation;     // Recommended user action
        float severityConfidence;               // 0.0-1.0 confidence in classification
        std::string detectionMethod;            // How severity was determined
        
        // Context
        GameStateContext gameState;
        PerformanceSnapshot performance;
        std::vector<LogEntry> recentLogs;
        
        // Recovery
        std::vector<std::string> recoveryActions;
        bool recoverySuccessful;
        std::string recoveryStrategy;
        
        // Pattern data
        std::string crashSignature;
        uint32_t similarCrashCount;
        std::string communityStatus;  // "Known issue", "Rare", "New pattern"
        
        // Additional metadata
        std::unordered_map<std::string, std::string> metadata;
    };

    /// Output format options
    enum class OutputFormat {
        UserFriendly,   // Human-readable summary
        Technical,      // Full technical details
        JSON,          // Structured JSON
        XML,           // Structured XML
        Markdown,      // GitHub-style markdown
        HTML           // Rich HTML with styling
    };

    /// Main diagnostic logger class
    class DiagnosticLogger {
    public:
        /// Initialize the logger with configuration
        static bool Initialize(const std::string& logDirectory = "");
        
        /// Shutdown and cleanup
        static void Shutdown();
        
        /// Log a structured entry
        static void Log(LogSeverity severity, LogCategory category, 
                       const std::string& component, const std::string& message,
                       const std::unordered_map<std::string, std::string>& metadata = {});
        
        /// Log with correlation ID for linking related entries
        static void LogCorrelated(const std::string& correlationId,
                                 LogSeverity severity, LogCategory category,
                                 const std::string& component, const std::string& message,
                                 const std::unordered_map<std::string, std::string>& metadata = {});
        
        /// Create comprehensive crash report
        static std::unique_ptr<CrashReport> CreateCrashReport(
            uint32_t exceptionCode, void* crashAddress,
            const std::string& rootCause, const std::string& category,
            float confidence, const std::vector<std::string>& suspectedMods);
        
        /// Write crash report in specified format
        static bool WriteCrashReport(const CrashReport& report, OutputFormat format,
                                   const std::string& filename = "");
        
        /// Generate user-friendly crash summary for in-game notifications
        static std::string GenerateInGameSummary(const CrashReport& report);
        
        /// Generate detailed notification message with file relationships
        static std::string GenerateNotificationMessage(const CrashReport& report,
                                                      bool includeFileDetails = true);
        
        /// Generate quick fix suggestions for MessageBox
        static std::vector<std::string> GenerateQuickFixes(const CrashReport& report);
        
        /// Generate file relationship analysis for notifications
        static std::string GenerateFileRelationshipSummary(const CrashReport& report);
        
        /// Create notification-friendly mod conflict summary
        static std::string GenerateModConflictSummary(const std::vector<std::string>& suspectedMods);
        
        /// Generate user-friendly crash summary
        static std::string GenerateUserSummary(const CrashReport& report);
        
        /// Generate technical details for developers
        static std::string GenerateTechnicalReport(const CrashReport& report);
        
        /// Generate actionable fix suggestions
        static std::vector<std::string> GenerateFixSuggestions(const CrashReport& report);
        
        /// Export logs for community analysis
        static bool ExportForCommunity(const std::string& filename,
                                      bool anonymize = true);
        
        /// Import community patterns
        static bool ImportCommunityPatterns(const std::string& filename);
        
        /// Returns the current performance snapshot. Values are zero/empty until
        /// updated externally (no automatic polling is done by DiagnosticLogger itself).
        static PerformanceSnapshot GetCurrentPerformance();
        
        /// Game state tracking
        static void UpdateGameState(const GameStateContext& state);
        static GameStateContext GetCurrentGameState();
        
        /// Pattern analysis
        static void RecordCrashPattern(const std::string& signature,
                                     const std::string& category);
        static std::string AnalyzeCrashPattern(const std::string& signature);
        
        /// Configuration
        static void SetLogLevel(LogSeverity minLevel);
        static void SetOutputFormats(const std::vector<OutputFormat>& formats);
        static void EnableRealTimeLogging(bool enabled);
        static void EnableCommunityReporting(bool enabled);
        
        /// Statistics
        static uint32_t GetLogEntryCount();
        static uint32_t GetCrashCount();
        static std::vector<std::string> GetTopCrashCategories();
        static std::unordered_map<std::string, uint32_t> GetModCrashStats();

    private:
        /// Internal log writing
        static void WriteLogEntry(const LogEntry& entry);
        static void WriteToFile(const std::string& content, const std::string& filename);
        static void WriteToConsole(const LogEntry& entry);
        
        /// Formatting helpers
        static std::string FormatTimestamp(const std::chrono::system_clock::time_point& time);
        static std::string FormatSeverity(LogSeverity severity);
        static std::string FormatCategory(LogCategory category);
        static std::string FormatMetadata(const std::unordered_map<std::string, std::string>& metadata);
        
        /// Report generation helpers
        static std::string GenerateReportId();
        static std::string GenerateCorrelationId();
        static void CollectSystemInfo(CrashReport& report);
        static void CollectModInfo(CrashReport& report);
        static void CollectRecentLogs(CrashReport& report, uint32_t count = 50);
        static void CollectGameState(CrashReport& report);
        
        static void LogCurrentCell(CrashReport& report);
        static void LogNearbyNPCs(CrashReport& report, uint32_t maxCount = 10);
        static void LogActiveQuests(CrashReport& report, uint32_t maxCount = 5);
        static void LogPlayerActivity(CrashReport& report);
        
        static void LogAllLoadedPlugins(CrashReport& report);
        static void HighlightSuspectedMods(CrashReport& report);
        static void RankModsByLikelihood(CrashReport& report);
        static void CollectModListAndIdentification(CrashReport& report);
        
        static void LogRecoveryAction(CrashReport& report, const std::string& action);
        static void LogRecoveryStrategy(CrashReport& report, const std::string& strategy);
        static void LogRecoverySuccess(CrashReport& report, bool success);
        static void LogStateModification(CrashReport& report, const std::string& modification);
        static void LogRecoveryAttempt(CrashReport& report, 
                                      const std::string& strategy,
                                      const std::vector<std::string>& actions,
                                      bool success);
        static std::string FormatRecoveryActions(const CrashReport& report);
        static void WriteRecoveryLog(const CrashReport& report, const std::string& filename = "");
        
        static void TrackCrashPattern(const CrashReport& report);
        static std::vector<struct CrashPatternData> GetSimilarCrashPatterns(const std::string& signature);
        static void GroupSimilarCrashes(std::vector<CrashReport>& reports);
        static std::string GeneratePatternSummary();
        static void WritePatternSummary(const std::string& filename = "");
        static void ExportPatternData(const std::string& filename = "");
        
        static void LogMeshFilePath(CrashReport& report, const std::string& meshPath, const std::string& modName);
        static void LogMeshCounts(CrashReport& report, uint32_t vertexCount, uint32_t triangleCount, uint32_t boneCount);
        static void LogMeshValidationFailures(CrashReport& report, const std::vector<std::string>& failures);
        static void LogMeshRepairsApplied(CrashReport& report, const std::vector<std::string>& repairs);
        static std::string FormatMeshDiagnostics(const CrashReport& report);
        static void WriteMeshDiagnosticReport(const CrashReport& report, const std::string& filename = "");
        static void LogCompleteMeshDiagnostics(CrashReport& report,
                                              const std::string& meshPath,
                                              const std::string& modName,
                                              uint32_t vertexCount,
                                              uint32_t triangleCount,
                                              uint32_t boneCount,
                                              const std::vector<std::string>& failures,
                                              const std::vector<std::string>& repairs);
        
        static std::vector<std::string> GenerateActionableSuggestions(const CrashReport& report);
        static std::string LinkToKnownIssues(const CrashReport& report);
        static std::vector<std::string> SuggestConfigurationChanges(const CrashReport& report);
        static std::string GenerateCompleteSuggestedFixes(const CrashReport& report);
        static void WriteSuggestedFixesReport(const CrashReport& report, const std::string& filename = "");
        static void EnhanceSuggestedFixes(CrashReport& report);
        
        /// Pattern analysis helpers
        static std::string GenerateCrashSignature(uint32_t exceptionCode, void* address,
                                                 const std::vector<std::string>& callStack);
        static void UpdatePatternDatabase(const std::string& signature, const std::string& category);
        
        /// Report format generators
        static std::string GenerateJSONReport(const CrashReport& report);
        static std::string GenerateMarkdownReport(const CrashReport& report);
        static std::string GenerateHTMLReport(const CrashReport& report);
        
        /// State management
        static bool s_initialized;
        static std::string s_logDirectory;
        static LogSeverity s_minLogLevel;
        static std::vector<OutputFormat> s_outputFormats;
        static bool s_realTimeLogging;
        static bool s_communityReporting;
        
        static std::vector<LogEntry> s_logBuffer;
        static GameStateContext s_currentGameState;
        static PerformanceSnapshot s_currentPerformance;
        static std::unordered_map<std::string, uint32_t> s_crashPatterns;
        
        static std::shared_mutex s_logMutex;
        static std::ofstream s_logFile;
        static std::string s_logFilePath;  // Current log file path for rotation
        static uint32_t s_logEntryCount;
        static uint32_t s_crashCount;
        
        /// Log rotation helper
        static void CheckAndRotateLogs();
    };

    /// Convenience macros for logging
    #define LOG_DEBUG(component, message, ...) \
        Diagnostics::DiagnosticLogger::Log(Diagnostics::LogSeverity::Debug, \
                                          Diagnostics::LogCategory::System, \
                                          component, message, ##__VA_ARGS__)
    
    #define LOG_INFO(component, message, ...) \
        Diagnostics::DiagnosticLogger::Log(Diagnostics::LogSeverity::Info, \
                                          Diagnostics::LogCategory::System, \
                                          component, message, ##__VA_ARGS__)
    
    #define LOG_WARNING(component, message, ...) \
        Diagnostics::DiagnosticLogger::Log(Diagnostics::LogSeverity::Warning, \
                                          Diagnostics::LogCategory::System, \
                                          component, message, ##__VA_ARGS__)
    
    #define LOG_ERROR(component, message, ...) \
        Diagnostics::DiagnosticLogger::Log(Diagnostics::LogSeverity::Error, \
                                          Diagnostics::LogCategory::System, \
                                          component, message, ##__VA_ARGS__)
    
    #define LOG_CRITICAL(component, message, ...) \
        Diagnostics::DiagnosticLogger::Log(Diagnostics::LogSeverity::Critical, \
                                          Diagnostics::LogCategory::System, \
                                          component, message, ##__VA_ARGS__)

}  // namespace Diagnostics