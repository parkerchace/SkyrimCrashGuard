// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "PCH.h"
#include "DiagnosticLogger.h"
#include "Config.h"
#include "Plugin.h"

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <random>
#include <iomanip>
#include <sstream>

namespace Diagnostics {

    // Static member initialization
    bool DiagnosticLogger::s_initialized = false;
    std::string DiagnosticLogger::s_logDirectory;
    LogSeverity DiagnosticLogger::s_minLogLevel = LogSeverity::Info;
    std::vector<OutputFormat> DiagnosticLogger::s_outputFormats = { OutputFormat::UserFriendly };
    bool DiagnosticLogger::s_realTimeLogging = true;
    bool DiagnosticLogger::s_communityReporting = false;
    
    std::vector<LogEntry> DiagnosticLogger::s_logBuffer;
    GameStateContext DiagnosticLogger::s_currentGameState;
    PerformanceSnapshot DiagnosticLogger::s_currentPerformance;
    std::unordered_map<std::string, uint32_t> DiagnosticLogger::s_crashPatterns;
    
    std::shared_mutex DiagnosticLogger::s_logMutex;  // Upgraded to shared_mutex
    std::ofstream DiagnosticLogger::s_logFile;
    std::string DiagnosticLogger::s_logFilePath;
    uint32_t DiagnosticLogger::s_logEntryCount = 0;
    uint32_t DiagnosticLogger::s_crashCount = 0;

    // ========================================================================
    // Initialization and Shutdown
    // ========================================================================

    bool DiagnosticLogger::Initialize(const std::string& logDirectory) {
        if (s_initialized) {
            return true;
        }

        // Determine log directory
        if (logDirectory.empty()) {
            auto skseLogDir = SKSE::log::log_directory();
            if (skseLogDir) {
                s_logDirectory = skseLogDir->string() + "/CrashGuard";
            } else {
                s_logDirectory = "logs/CrashGuard";
            }
        } else {
            s_logDirectory = logDirectory;
        }

        // Create directory if it doesn't exist
        try {
            std::filesystem::create_directories(s_logDirectory);
        } catch (const std::exception& e) {
            spdlog::error("Failed to create log directory {}: {}", s_logDirectory, e.what());
            return false;
        }

        // Open main log file
        s_logFilePath = s_logDirectory + "/CrashGuard.log";
        s_logFile.open(s_logFilePath, std::ios::out | std::ios::app);
        if (!s_logFile.is_open()) {
            spdlog::error("Failed to open log file: {}", s_logFilePath);
            return false;
        }

        // Write initialization header
        auto now = std::chrono::system_clock::now();
        s_logFile << "\n" << std::string(80, '=') << "\n";
        s_logFile << "SkyrimCrashGuard Advanced Diagnostic Log\n";
        s_logFile << "Session started: " << FormatTimestamp(now) << "\n";
        s_logFile << "Log directory: " << s_logDirectory << "\n";
        s_logFile << std::string(80, '=') << "\n\n";
        s_logFile.flush();

        s_initialized = true;
        
        Log(LogSeverity::Info, LogCategory::System, "DiagnosticLogger", 
            "Advanced diagnostic logging initialized",
            {{ "directory", s_logDirectory }});

        return true;
    }

    void DiagnosticLogger::Shutdown() {
        if (!s_initialized) {
            return;
        }

        Log(LogSeverity::Info, LogCategory::System, "DiagnosticLogger", 
            "Shutting down diagnostic logging",
            {{ "totalEntries", std::to_string(s_logEntryCount) },
             { "totalCrashes", std::to_string(s_crashCount) }});

        // Write shutdown footer
        auto now = std::chrono::system_clock::now();
        s_logFile << "\n" << std::string(80, '=') << "\n";
        s_logFile << "Session ended: " << FormatTimestamp(now) << "\n";
        s_logFile << "Total log entries: " << s_logEntryCount << "\n";
        s_logFile << "Total crashes: " << s_crashCount << "\n";
        s_logFile << std::string(80, '=') << "\n";

        if (s_logFile.is_open()) {
            s_logFile.close();
        }

        s_initialized = false;
    }

    // ========================================================================
    // Logging Functions
    // ========================================================================

    void DiagnosticLogger::Log(LogSeverity severity, LogCategory category, 
                              const std::string& component, const std::string& message,
                              const std::unordered_map<std::string, std::string>& metadata) {
        if (!s_initialized || severity < s_minLogLevel) {
            return;
        }

        LogEntry entry;
        entry.timestamp = std::chrono::system_clock::now();
        entry.severity = severity;
        entry.category = category;
        entry.component = component;
        entry.message = message;
        entry.metadata = metadata;
        entry.threadId = GetCurrentThreadId();
        entry.correlationId = "";

        WriteLogEntry(entry);
    }

    void DiagnosticLogger::LogCorrelated(const std::string& correlationId,
                                        LogSeverity severity, LogCategory category,
                                        const std::string& component, const std::string& message,
                                        const std::unordered_map<std::string, std::string>& metadata) {
        if (!s_initialized || severity < s_minLogLevel) {
            return;
        }

        LogEntry entry;
        entry.timestamp = std::chrono::system_clock::now();
        entry.severity = severity;
        entry.category = category;
        entry.component = component;
        entry.message = message;
        entry.metadata = metadata;
        entry.threadId = GetCurrentThreadId();
        entry.correlationId = correlationId;

        WriteLogEntry(entry);
    }

    // ========================================================================
    // Crash Reporting
    // ========================================================================

    std::unique_ptr<CrashReport> DiagnosticLogger::CreateCrashReport(
        uint32_t exceptionCode, void* crashAddress,
        const std::string& rootCause, const std::string& category,
        float confidence, const std::vector<std::string>& suspectedMods) {
        
        auto report = std::make_unique<CrashReport>();
        
        // Basic info
        report->reportId = GenerateReportId();
        report->timestamp = std::chrono::system_clock::now();
        report->exceptionCode = exceptionCode;
        report->crashAddress = crashAddress;
        
        // Exception description
        switch (exceptionCode) {
        case 0xC0000005: report->exceptionDescription = "Access Violation (Null Pointer/Bad Memory)"; break;
        case 0xC000001D: report->exceptionDescription = "Illegal Instruction (Corrupted Code)"; break;
        case 0xC00000FD: report->exceptionDescription = "Stack Overflow (Infinite Recursion)"; break;
        case 0xC0000094: report->exceptionDescription = "Integer Division by Zero"; break;
        case 0xC000008C: report->exceptionDescription = "Array Bounds Exceeded"; break;
        default: report->exceptionDescription = fmt::format("Unknown Exception ({:#x})", exceptionCode);
        }
        
        // Analysis
        report->rootCause = rootCause;
        report->category = category;
        report->confidence = confidence;
        report->suspectedMods = suspectedMods;
        
        // Context
        report->gameState = s_currentGameState;
        report->performance = s_currentPerformance;
        
        // Collect additional info
        CollectSystemInfo(*report);
        CollectModInfo(*report);
        CollectGameState(*report);
        CollectModListAndIdentification(*report);
        CollectRecentLogs(*report);
        
        // Pattern analysis
        std::vector<std::string> dummyCallStack; // Would be populated from actual crash
        report->crashSignature = GenerateCrashSignature(exceptionCode, crashAddress, dummyCallStack);
        
        auto patternIt = s_crashPatterns.find(report->crashSignature);
        if (patternIt != s_crashPatterns.end()) {
            report->similarCrashCount = patternIt->second;
            if (report->similarCrashCount > 10) {
                report->communityStatus = "Known issue";
            } else if (report->similarCrashCount > 3) {
                report->communityStatus = "Recurring pattern";
            } else {
                report->communityStatus = "Rare occurrence";
            }
        } else {
            report->similarCrashCount = 0;
            report->communityStatus = "New pattern";
        }
        
        // Generate fix suggestions
        report->suggestedFixes = GenerateFixSuggestions(*report);
        EnhanceSuggestedFixes(*report);
        
        s_crashCount++;
        UpdatePatternDatabase(report->crashSignature, category);
        TrackCrashPattern(*report);
        
        return report;
    }

    bool DiagnosticLogger::WriteCrashReport(const CrashReport& report, OutputFormat format,
                                           const std::string& filename) {
        if (!s_initialized) {
            return false;
        }

        std::string content;
        std::string extension;
        
        switch (format) {
        case OutputFormat::UserFriendly:
            content = GenerateUserSummary(report);
            extension = ".txt";
            break;
        case OutputFormat::Technical:
            content = GenerateTechnicalReport(report);
            extension = ".log";
            break;
        case OutputFormat::JSON:
            content = GenerateJSONReport(report);
            extension = ".json";
            break;
        case OutputFormat::Markdown:
            content = GenerateMarkdownReport(report);
            extension = ".md";
            break;
        case OutputFormat::HTML:
            content = GenerateHTMLReport(report);
            extension = ".html";
            break;
        default:
            content = GenerateUserSummary(report);
            extension = ".txt";
        }

        std::string outputFilename;
        if (filename.empty()) {
            outputFilename = s_logDirectory + "/crash_" + report.reportId + extension;
        } else {
            outputFilename = filename;
        }

        WriteToFile(content, outputFilename);
        
        Log(LogSeverity::Critical, LogCategory::System, "CrashReporter",
            "Crash report generated",
            {{ "reportId", report.reportId },
             { "format", std::to_string(static_cast<int>(format)) },
             { "filename", outputFilename }});

        return true;
    }

    // ========================================================================
    // In-Game Notification Integration
    // ========================================================================

    std::string DiagnosticLogger::GenerateInGameSummary(const CrashReport& report) {
        std::ostringstream ss;
        
        // Brief, user-friendly description
        ss << "CrashGuard detected a problem:\n\n";
        
        // What happened (simplified)
        if (report.category == "Mesh") {
            ss << "A 3D model file is corrupted\n";
        } else if (report.category == "Animation") {
            ss << "An animation file has issues\n";
        } else if (report.category == "Script") {
            ss << "A script encountered an error\n";
        } else if (report.category == "Memory") {
            ss << "A memory-related issue occurred\n";
        } else {
            ss << report.exceptionDescription << "\n";
        }
        
        // Recovery status
        if (report.recoverySuccessful) {
            ss << "Successfully fixed - game continues normally\n\n";
        } else {
            ss << "Could not fix automatically\n\n";
        }
        
        // Primary suspected file/mod (most important)
        if (!report.suspectedMods.empty()) {
            ss << "Likely cause: " << report.suspectedMods[0] << "\n";
        }
        
        // Confidence indicator
        if (report.confidence >= 0.8f) {
            ss << "Confidence: High\n";
        } else if (report.confidence >= 0.5f) {
            ss << "Confidence: Medium\n";
        } else {
            ss << "Confidence: Low\n";
        }
        
        return ss.str();
    }

    std::string DiagnosticLogger::GenerateNotificationMessage(const CrashReport& report,
                                                             bool includeFileDetails) {
        std::ostringstream ss;
        
        // Header with recovery status
        if (report.recoverySuccessful) {
            ss << "CRASH PREVENTED\n";
            ss << "CrashGuard successfully recovered from a crash.\n\n";
        } else {
            ss << "CRASH DETECTED\n";
            ss << "CrashGuard detected a crash but could not recover.\n\n";
        }
        
        // Problem description
        ss << "PROBLEM:\n";
        ss << report.rootCause << "\n\n";
        
        // File details if requested
        if (includeFileDetails && !report.suspectedMods.empty()) {
            ss << "AFFECTED FILES:\n";
            for (size_t i = 0; i < std::min(report.suspectedMods.size(), size_t(3)); ++i) {
                ss << "• " << report.suspectedMods[i] << "\n";
            }
            if (report.suspectedMods.size() > 3) {
                ss << "• ... and " << (report.suspectedMods.size() - 3) << " more\n";
            }
            ss << "\n";
        }
        
        // File relationship analysis
        if (includeFileDetails && report.suspectedMods.size() > 1) {
            ss << GenerateFileRelationshipSummary(report) << "\n";
        }
        
        // Recovery actions taken
        if (report.recoverySuccessful && !report.recoveryActions.empty()) {
            ss << "RECOVERY ACTIONS:\n";
            for (size_t i = 0; i < std::min(report.recoveryActions.size(), size_t(3)); ++i) {
                ss << "• " << report.recoveryActions[i] << "\n";
            }
            ss << "\n";
        }
        
        // Pattern information
        if (report.similarCrashCount > 0) {
            ss << "PATTERN: " << report.communityStatus;
            if (report.similarCrashCount > 1) {
                ss << " (" << report.similarCrashCount << " similar crashes)";
            }
            ss << "\n\n";
        }
        
        // Context (brief)
        ss << "CONTEXT: " << report.gameState.currentActivity;
        if (!report.gameState.playerLocation.empty()) {
            ss << " at " << report.gameState.playerLocation;
        }
        ss << "\n";
        
        return ss.str();
    }

    std::vector<std::string> DiagnosticLogger::GenerateQuickFixes(const CrashReport& report) {
        std::vector<std::string> quickFixes;
        
        // Category-specific quick fixes
        if (report.category == "Mesh") {
            quickFixes.push_back("Reinstall the mod with corrupted meshes");
            quickFixes.push_back("Check mesh files in NifSkope");
            quickFixes.push_back("Disable problematic armor/weapon mods temporarily");
        } else if (report.category == "Animation") {
            quickFixes.push_back("Reinstall animation mods");
            quickFixes.push_back("Check for animation conflicts");
            quickFixes.push_back("Reset to default animations");
        } else if (report.category == "Script") {
            quickFixes.push_back("Clean save and reload");
            quickFixes.push_back("Check script-heavy mods");
            quickFixes.push_back("Disable recently added script mods");
        } else if (report.category == "Memory") {
            quickFixes.push_back("Restart Skyrim to free memory");
            quickFixes.push_back("Reduce graphics settings");
            quickFixes.push_back("Close other applications");
        }
        
        // Mod-specific fixes
        if (!report.suspectedMods.empty()) {
            const auto& primaryMod = report.suspectedMods[0];
            quickFixes.insert(quickFixes.begin(), 
                            "Update or reinstall " + primaryMod);
        }
        
        // Pattern-based fixes
        if (report.communityStatus == "Known issue") {
            quickFixes.insert(quickFixes.begin(), 
                            "Check mod page for known fixes");
        }
        
        // Recovery-specific advice
        if (report.recoverySuccessful) {
            quickFixes.push_back("Continue playing - issue was fixed");
            quickFixes.push_back("Save your game soon");
        } else {
            quickFixes.push_back("Save and restart Skyrim");
            quickFixes.push_back("Load a previous save");
        }
        
        // Limit to most relevant fixes
        if (quickFixes.size() > 5) {
            quickFixes.resize(5);
        }
        
        return quickFixes;
    }

    std::string DiagnosticLogger::GenerateFileRelationshipSummary(const CrashReport& report) {
        if (report.suspectedMods.size() < 2) {
            return "";
        }
        
        std::ostringstream ss;
        ss << "FILE RELATIONSHIPS:\n";
        
        // Analyze relationships between suspected mods
        const auto& mods = report.suspectedMods;
        
        // Check for common patterns
        bool hasTextureAndMesh = false;
        bool hasAnimationConflict = false;
        bool hasScriptDependency = false;
        
        for (const auto& mod : mods) {
            std::string modLower = mod;
            std::transform(modLower.begin(), modLower.end(), modLower.begin(), ::tolower);
            
            if (modLower.find("texture") != std::string::npos || 
                modLower.find("hd") != std::string::npos ||
                modLower.find("visual") != std::string::npos) {
                hasTextureAndMesh = true;
            }
            
            if (modLower.find("animation") != std::string::npos ||
                modLower.find("anim") != std::string::npos ||
                modLower.find("behavior") != std::string::npos) {
                hasAnimationConflict = true;
            }
            
            if (modLower.find("script") != std::string::npos ||
                modLower.find("mcm") != std::string::npos ||
                modLower.find("framework") != std::string::npos) {
                hasScriptDependency = true;
            }
        }
        
        // Generate relationship analysis
        if (hasTextureAndMesh) {
            ss << "• Texture and mesh mods may be incompatible\n";
        }
        
        if (hasAnimationConflict) {
            ss << "• Multiple animation mods may conflict\n";
        }
        
        if (hasScriptDependency) {
            ss << "• Script mods may have missing dependencies\n";
        }
        
        // Load order analysis
        if (mods.size() >= 2) {
            ss << "• Check load order between " << mods[0] << " and " << mods[1] << "\n";
        }
        
        // Generic relationship advice
        if (mods.size() > 2) {
            ss << "• Multiple mods affecting the same game systems\n";
        }
        
        return ss.str();
    }

    std::string DiagnosticLogger::GenerateModConflictSummary(const std::vector<std::string>& suspectedMods) {
        if (suspectedMods.empty()) {
            return "No specific mods identified";
        }
        
        if (suspectedMods.size() == 1) {
            return "Primary suspect: " + suspectedMods[0];
        }
        
        std::ostringstream ss;
        ss << "Mod conflict detected:\n";
        ss << "• Primary: " << suspectedMods[0] << "\n";
        ss << "• Secondary: " << suspectedMods[1];
        
        if (suspectedMods.size() > 2) {
            ss << "\n• +" << (suspectedMods.size() - 2) << " others";
        }
        
        return ss.str();
    }

    std::string DiagnosticLogger::GenerateUserSummary(const CrashReport& report) {
        std::ostringstream ss;
        
        ss << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        ss << "║                        SKYRIM CRASH GUARD REPORT                            ║\n";
        ss << "╚══════════════════════════════════════════════════════════════════════════════╝\n\n";
        
        // Quick Summary
        ss << "WHAT HAPPENED\n";
        ss << "   " << report.exceptionDescription << "\n";
        ss << "   Root Cause: " << report.rootCause << "\n";
        ss << "   Confidence: " << std::fixed << std::setprecision(0) << (report.confidence * 100) << "%\n\n";
        
        // Recovery Status
        ss << "RECOVERY STATUS\n";
        if (report.recoverySuccessful) {
            ss << "   Successfully recovered using " << report.recoveryStrategy << "\n";
            ss << "   Game should continue normally\n";
        } else {
            ss << "   Recovery failed - game may crash\n";
            ss << "   Consider saving and restarting\n";
        }
        ss << "\n";
        
        // Suspected Mods
        if (!report.suspectedMods.empty()) {
            ss << "LIKELY CAUSE\n";
            for (const auto& mod : report.suspectedMods) {
                ss << "   • " << mod << "\n";
            }
            ss << "\n";
        }
        
        // Fix Suggestions
        if (!report.suggestedFixes.empty()) {
            ss << "SUGGESTED FIXES\n";
            for (size_t i = 0; i < report.suggestedFixes.size(); ++i) {
                ss << "   " << (i + 1) << ". " << report.suggestedFixes[i] << "\n";
            }
            ss << "\n";
        }
        
        // Pattern Information
        ss << "PATTERN ANALYSIS\n";
        ss << "   Status: " << report.communityStatus << "\n";
        if (report.similarCrashCount > 0) {
            ss << "   Similar crashes: " << report.similarCrashCount << " times\n";
        }
        ss << "\n";
        
        // Game Context
        ss << "GAME CONTEXT\n";
        ss << "   Location: " << report.gameState.playerLocation << "\n";
        ss << "   Activity: " << report.gameState.currentActivity << "\n";
        if (report.gameState.isInCombat) ss << "   In combat\n";
        if (report.gameState.isInDialogue) ss << "   In dialogue\n";
        if (report.gameState.isInMenu) ss << "   In menu\n";
        ss << "\n";
        
        // Performance Context
        ss << "PERFORMANCE\n";
        ss << "   FPS: " << std::fixed << std::setprecision(1) << report.performance.fps << "\n";
        ss << "   Memory: " << report.performance.memoryUsageMB << " MB\n";
        ss << "   CPU: " << std::fixed << std::setprecision(1) << report.performance.cpuUsagePercent << "%\n";
        ss << "   Loaded mods: " << report.performance.loadedMods << "\n";
        ss << "\n";
        
        // Technical Details (Brief)
        ss << "TECHNICAL DETAILS\n";
        ss << "   Report ID: " << report.reportId << "\n";
        ss << "   Time: " << FormatTimestamp(report.timestamp) << "\n";
        ss << "   Exception: " << fmt::format("{:#x}", report.exceptionCode) << "\n";
        ss << "   Address: " << fmt::format("{:#x}", reinterpret_cast<uintptr_t>(report.crashAddress)) << "\n";
        ss << "\n";
        
        ss << "For technical details, see: crash_" << report.reportId << "_technical.log\n";
        ss << "For community reporting: crash_" << report.reportId << ".json\n";
        
        return ss.str();
    }

    std::string DiagnosticLogger::GenerateTechnicalReport(const CrashReport& report) {
        std::ostringstream ss;
        
        ss << "SKYRIM CRASH GUARD - TECHNICAL CRASH REPORT\n";
        ss << std::string(80, '=') << "\n\n";
        
        // Header
        ss << "Report ID: " << report.reportId << "\n";
        ss << "Timestamp: " << FormatTimestamp(report.timestamp) << "\n";
        ss << "Exception Code: " << fmt::format("{:#x}", report.exceptionCode) << "\n";
        ss << "Crash Address: " << fmt::format("{:#x}", reinterpret_cast<uintptr_t>(report.crashAddress)) << "\n";
        ss << "Description: " << report.exceptionDescription << "\n\n";
        
        // Analysis
        ss << "ROOT CAUSE ANALYSIS\n";
        ss << std::string(40, '-') << "\n";
        ss << "Category: " << report.category << "\n";
        ss << "Root Cause: " << report.rootCause << "\n";
        ss << "Confidence: " << std::fixed << std::setprecision(2) << report.confidence << "\n";
        ss << "Pattern Signature: " << report.crashSignature << "\n";
        ss << "Similar Crashes: " << report.similarCrashCount << "\n";
        ss << "Community Status: " << report.communityStatus << "\n\n";
        
        // Suspected Mods
        if (!report.suspectedMods.empty()) {
            ss << "SUSPECTED MODS\n";
            ss << std::string(40, '-') << "\n";
            for (const auto& mod : report.suspectedMods) {
                ss << "- " << mod << "\n";
            }
            ss << "\n";
        }
        
        // Recovery Actions
        if (!report.recoveryActions.empty()) {
            ss << "RECOVERY ACTIONS\n";
            ss << std::string(40, '-') << "\n";
            ss << "Strategy: " << report.recoveryStrategy << "\n";
            ss << "Success: " << (report.recoverySuccessful ? "Yes" : "No") << "\n";
            ss << "Actions Performed:\n";
            for (size_t i = 0; i < report.recoveryActions.size(); ++i) {
                ss << "  " << (i + 1) << ". " << report.recoveryActions[i] << "\n";
            }
            ss << "\n";
        }
        
        // Game State
        ss << "GAME STATE\n";
        ss << std::string(40, '-') << "\n";
        ss << "Player Location: " << report.gameState.playerLocation << "\n";
        ss << "Current Activity: " << report.gameState.currentActivity << "\n";
        ss << "Weather: " << report.gameState.weatherCondition << "\n";
        ss << "Time of Day: " << report.gameState.timeOfDay << "\n";
        ss << "In Combat: " << (report.gameState.isInCombat ? "Yes" : "No") << "\n";
        ss << "In Dialogue: " << (report.gameState.isInDialogue ? "Yes" : "No") << "\n";
        ss << "In Menu: " << (report.gameState.isInMenu ? "Yes" : "No") << "\n";
        
        if (!report.gameState.activeQuests.empty()) {
            ss << "Active Quests:\n";
            for (const auto& quest : report.gameState.activeQuests) {
                ss << "  - " << quest << "\n";
            }
        }
        
        if (!report.gameState.nearbyNPCs.empty()) {
            ss << "Nearby NPCs:\n";
            for (const auto& npc : report.gameState.nearbyNPCs) {
                ss << "  - " << npc << "\n";
            }
        }
        ss << "\n";
        
        // Performance
        ss << "PERFORMANCE METRICS\n";
        ss << std::string(40, '-') << "\n";
        ss << "FPS: " << std::fixed << std::setprecision(1) << report.performance.fps << "\n";
        ss << "Memory Usage: " << report.performance.memoryUsageMB << " MB\n";
        ss << "CPU Usage: " << std::fixed << std::setprecision(1) << report.performance.cpuUsagePercent << "%\n";
        ss << "Loaded Mods: " << report.performance.loadedMods << "\n";
        ss << "Active NPCs: " << report.performance.activeNPCs << "\n";
        ss << "Current Cell: " << report.performance.currentCell << "\n";
        ss << "Session Duration: " << report.performance.sessionDuration.count() << " ms\n\n";
        
        // Recent Log Entries
        if (!report.recentLogs.empty()) {
            ss << "RECENT LOG ENTRIES\n";
            ss << std::string(40, '-') << "\n";
            for (const auto& entry : report.recentLogs) {
                ss << FormatTimestamp(entry.timestamp) << " ";
                ss << "[" << FormatSeverity(entry.severity) << "] ";
                ss << "[" << entry.component << "] ";
                ss << entry.message << "\n";
            }
            ss << "\n";
        }
        
        // Fix Suggestions
        if (!report.suggestedFixes.empty()) {
            ss << "SUGGESTED FIXES\n";
            ss << std::string(40, '-') << "\n";
            for (size_t i = 0; i < report.suggestedFixes.size(); ++i) {
                ss << (i + 1) << ". " << report.suggestedFixes[i] << "\n";
            }
            ss << "\n";
        }
        
        return ss.str();
    }

    std::string DiagnosticLogger::GenerateJSONReport(const CrashReport& report) {
        nlohmann::json j;
        
        // Basic info
        j["reportId"] = report.reportId;
        j["timestamp"] = FormatTimestamp(report.timestamp);
        j["exceptionCode"] = fmt::format("{:#x}", report.exceptionCode);
        j["crashAddress"] = fmt::format("{:#x}", reinterpret_cast<uintptr_t>(report.crashAddress));
        j["exceptionDescription"] = report.exceptionDescription;
        
        // Analysis
        j["analysis"]["rootCause"] = report.rootCause;
        j["analysis"]["category"] = report.category;
        j["analysis"]["confidence"] = report.confidence;
        j["analysis"]["suspectedMods"] = report.suspectedMods;
        j["analysis"]["crashSignature"] = report.crashSignature;
        j["analysis"]["similarCrashCount"] = report.similarCrashCount;
        j["analysis"]["communityStatus"] = report.communityStatus;
        
        // Recovery
        j["recovery"]["successful"] = report.recoverySuccessful;
        j["recovery"]["strategy"] = report.recoveryStrategy;
        j["recovery"]["actions"] = report.recoveryActions;
        
        // Game state
        j["gameState"]["playerLocation"] = report.gameState.playerLocation;
        j["gameState"]["currentActivity"] = report.gameState.currentActivity;
        j["gameState"]["weatherCondition"] = report.gameState.weatherCondition;
        j["gameState"]["timeOfDay"] = report.gameState.timeOfDay;
        j["gameState"]["isInCombat"] = report.gameState.isInCombat;
        j["gameState"]["isInDialogue"] = report.gameState.isInDialogue;
        j["gameState"]["isInMenu"] = report.gameState.isInMenu;
        j["gameState"]["activeQuests"] = report.gameState.activeQuests;
        j["gameState"]["nearbyNPCs"] = report.gameState.nearbyNPCs;
        j["gameState"]["recentActions"] = report.gameState.recentActions;
        
        // Performance
        j["performance"]["fps"] = report.performance.fps;
        j["performance"]["memoryUsageMB"] = report.performance.memoryUsageMB;
        j["performance"]["cpuUsagePercent"] = report.performance.cpuUsagePercent;
        j["performance"]["loadedMods"] = report.performance.loadedMods;
        j["performance"]["activeNPCs"] = report.performance.activeNPCs;
        j["performance"]["currentCell"] = report.performance.currentCell;
        j["performance"]["sessionDurationMs"] = report.performance.sessionDuration.count();
        
        // Suggestions
        j["suggestedFixes"] = report.suggestedFixes;
        
        return j.dump(2);
    }

    std::string DiagnosticLogger::GenerateMarkdownReport(const CrashReport& report) {
        std::ostringstream ss;
        
        ss << "# Skyrim Crash Guard Report\n\n";
        
        ss << "**Report ID:** `" << report.reportId << "`  \n";
        ss << "**Time:** " << FormatTimestamp(report.timestamp) << "  \n";
        ss << "**Status:** " << (report.recoverySuccessful ? "Recovered" : "Failed") << "\n\n";
        
        ss << "## What Happened\n\n";
        ss << "**Exception:** " << report.exceptionDescription << "  \n";
        ss << "**Root Cause:** " << report.rootCause << "  \n";
        ss << "**Confidence:** " << std::fixed << std::setprecision(0) << (report.confidence * 100) << "%\n\n";
        
        if (!report.suspectedMods.empty()) {
            ss << "## Suspected Mods\n\n";
            for (const auto& mod : report.suspectedMods) {
                ss << "- `" << mod << "`\n";
            }
            ss << "\n";
        }
        
        if (!report.suggestedFixes.empty()) {
            ss << "## Suggested Fixes\n\n";
            for (size_t i = 0; i < report.suggestedFixes.size(); ++i) {
                ss << (i + 1) << ". " << report.suggestedFixes[i] << "\n";
            }
            ss << "\n";
        }
        
        ss << "## Game Context\n\n";
        ss << "| Property | Value |\n";
        ss << "|----------|-------|\n";
        ss << "| Location | " << report.gameState.playerLocation << " |\n";
        ss << "| Activity | " << report.gameState.currentActivity << " |\n";
        ss << "| Weather | " << report.gameState.weatherCondition << " |\n";
        ss << "| Time | " << report.gameState.timeOfDay << " |\n";
        ss << "| Combat | " << (report.gameState.isInCombat ? "Yes" : "No") << " |\n";
        ss << "| Dialogue | " << (report.gameState.isInDialogue ? "Yes" : "No") << " |\n";
        ss << "| Menu | " << (report.gameState.isInMenu ? "Yes" : "No") << " |\n\n";
        
        ss << "## Performance\n\n";
        ss << "| Metric | Value |\n";
        ss << "|--------|-------|\n";
        ss << "| FPS | " << std::fixed << std::setprecision(1) << report.performance.fps << " |\n";
        ss << "| Memory | " << report.performance.memoryUsageMB << " MB |\n";
        ss << "| CPU | " << std::fixed << std::setprecision(1) << report.performance.cpuUsagePercent << "% |\n";
        ss << "| Mods | " << report.performance.loadedMods << " |\n";
        ss << "| NPCs | " << report.performance.activeNPCs << " |\n\n";
        
        ss << "## Technical Details\n\n";
        ss << "```\n";
        ss << "Exception Code: " << fmt::format("{:#x}", report.exceptionCode) << "\n";
        ss << "Crash Address:  " << fmt::format("{:#x}", reinterpret_cast<uintptr_t>(report.crashAddress)) << "\n";
        ss << "Pattern:        " << report.crashSignature << "\n";
        ss << "Occurrences:    " << report.similarCrashCount << "\n";
        ss << "```\n\n";
        
        return ss.str();
    }

    std::string DiagnosticLogger::GenerateHTMLReport(const CrashReport& report) {
        std::ostringstream ss;
        
        ss << "<!DOCTYPE html>\n<html>\n<head>\n";
        ss << "<title>Crash Report " << report.reportId << "</title>\n";
        ss << "<style>\n";
        ss << "body { font-family: 'Segoe UI', Arial, sans-serif; margin: 40px; background: #f5f5f5; }\n";
        ss << ".container { background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }\n";
        ss << ".header { border-bottom: 3px solid #007acc; padding-bottom: 20px; margin-bottom: 30px; }\n";
        ss << ".status-success { color: #28a745; font-weight: bold; }\n";
        ss << ".status-failed { color: #dc3545; font-weight: bold; }\n";
        ss << ".section { margin: 20px 0; }\n";
        ss << ".section h3 { color: #007acc; border-bottom: 1px solid #eee; padding-bottom: 5px; }\n";
        ss << ".code { background: #f8f9fa; padding: 10px; border-radius: 4px; font-family: monospace; }\n";
        ss << ".table { width: 100%; border-collapse: collapse; }\n";
        ss << ".table th, .table td { padding: 8px; text-align: left; border-bottom: 1px solid #ddd; }\n";
        ss << ".table th { background-color: #f8f9fa; }\n";
        ss << ".fix-list { background: #e7f3ff; padding: 15px; border-radius: 4px; border-left: 4px solid #007acc; }\n";
        ss << "</style>\n</head>\n<body>\n";
        
        ss << "<div class='container'>\n";
        ss << "<div class='header'>\n";
        ss << "<h1>Skyrim Crash Guard Report</h1>\n";
        ss << "<p><strong>Report ID:</strong> <code>" << report.reportId << "</code></p>\n";
        ss << "<p><strong>Time:</strong> " << FormatTimestamp(report.timestamp) << "</p>\n";
        ss << "<p><strong>Status:</strong> <span class='" << (report.recoverySuccessful ? "status-success" : "status-failed") << "'>";
        ss << (report.recoverySuccessful ? "Successfully Recovered" : "Recovery Failed") << "</span></p>\n";
        ss << "</div>\n";
        
        ss << "<div class='section'>\n";
        ss << "<h3>What Happened</h3>\n";
        ss << "<p><strong>Exception:</strong> " << report.exceptionDescription << "</p>\n";
        ss << "<p><strong>Root Cause:</strong> " << report.rootCause << "</p>\n";
        ss << "<p><strong>Confidence:</strong> " << std::fixed << std::setprecision(0) << (report.confidence * 100) << "%</p>\n";
        ss << "</div>\n";
        
        if (!report.suggestedFixes.empty()) {
            ss << "<div class='section'>\n";
            ss << "<h3>Suggested Fixes</h3>\n";
            ss << "<div class='fix-list'>\n";
            ss << "<ol>\n";
            for (const auto& fix : report.suggestedFixes) {
                ss << "<li>" << fix << "</li>\n";
            }
            ss << "</ol>\n";
            ss << "</div>\n";
            ss << "</div>\n";
        }
        
        ss << "<div class='section'>\n";
        ss << "<h3>Game Context</h3>\n";
        ss << "<table class='table'>\n";
        ss << "<tr><th>Property</th><th>Value</th></tr>\n";
        ss << "<tr><td>Location</td><td>" << report.gameState.playerLocation << "</td></tr>\n";
        ss << "<tr><td>Activity</td><td>" << report.gameState.currentActivity << "</td></tr>\n";
        ss << "<tr><td>Weather</td><td>" << report.gameState.weatherCondition << "</td></tr>\n";
        ss << "<tr><td>Time</td><td>" << report.gameState.timeOfDay << "</td></tr>\n";
        ss << "<tr><td>Combat</td><td>" << (report.gameState.isInCombat ? "Yes" : "No") << "</td></tr>\n";
        ss << "<tr><td>Dialogue</td><td>" << (report.gameState.isInDialogue ? "Yes" : "No") << "</td></tr>\n";
        ss << "<tr><td>Menu</td><td>" << (report.gameState.isInMenu ? "Yes" : "No") << "</td></tr>\n";
        ss << "</table>\n";
        ss << "</div>\n";
        
        ss << "<div class='section'>\n";
        ss << "<h3>Performance</h3>\n";
        ss << "<table class='table'>\n";
        ss << "<tr><th>Metric</th><th>Value</th></tr>\n";
        ss << "<tr><td>FPS</td><td>" << std::fixed << std::setprecision(1) << report.performance.fps << "</td></tr>\n";
        ss << "<tr><td>Memory</td><td>" << report.performance.memoryUsageMB << " MB</td></tr>\n";
        ss << "<tr><td>CPU</td><td>" << std::fixed << std::setprecision(1) << report.performance.cpuUsagePercent << "%</td></tr>\n";
        ss << "<tr><td>Loaded Mods</td><td>" << report.performance.loadedMods << "</td></tr>\n";
        ss << "<tr><td>Active NPCs</td><td>" << report.performance.activeNPCs << "</td></tr>\n";
        ss << "</table>\n";
        ss << "</div>\n";
        
        ss << "<div class='section'>\n";
        ss << "<h3>Technical Details</h3>\n";
        ss << "<div class='code'>\n";
        ss << "Exception Code: " << fmt::format("{:#x}", report.exceptionCode) << "<br>\n";
        ss << "Crash Address:  " << fmt::format("{:#x}", reinterpret_cast<uintptr_t>(report.crashAddress)) << "<br>\n";
        ss << "Pattern:        " << report.crashSignature << "<br>\n";
        ss << "Occurrences:    " << report.similarCrashCount << "<br>\n";
        ss << "</div>\n";
        ss << "</div>\n";
        
        ss << "</div>\n</body>\n</html>\n";
        
        return ss.str();
    }

    // ========================================================================
    // Helper Functions
    // ========================================================================

    void DiagnosticLogger::CheckAndRotateLogs() {
        // Check if log file needs rotation based on config
        try {
            if (s_logFilePath.empty() || !s_logFile.is_open()) {
                return;
            }
            
            // Check current file size
            auto fileSize = std::filesystem::file_size(s_logFilePath);
            const auto& config = ::Config::Get();
            size_t maxSize = static_cast<size_t>(config.maxLogSizeMB) * 1024 * 1024;
            
            if (fileSize < maxSize) {
                return;  // No rotation needed
            }
            
            // Close current log file
            s_logFile.close();
            
            // Rotate existing log files (e.g., .log -> .log.1 -> .log.2 -> deleted)
            int maxFiles = config.maxLogFiles;
            for (int i = maxFiles - 1; i >= 1; --i) {
                std::string oldPath = s_logFilePath + "." + std::to_string(i);
                std::string newPath = s_logFilePath + "." + std::to_string(i + 1);
                
                if (std::filesystem::exists(oldPath)) {
                    if (i == maxFiles - 1) {
                        // Delete the oldest file
                        std::filesystem::remove(oldPath);
                    } else {
                        // Rename to next number
                        std::filesystem::rename(oldPath, newPath);
                    }
                }
            }
            
            // Rename current log to .1
            if (std::filesystem::exists(s_logFilePath)) {
                std::filesystem::rename(s_logFilePath, s_logFilePath + ".1");
            }
            
            // Open fresh log file
            s_logFile.open(s_logFilePath, std::ios::out | std::ios::trunc);
            if (s_logFile.is_open()) {
                auto now = std::chrono::system_clock::now();
                s_logFile << "\n" << std::string(80, '=') << "\n";
                s_logFile << "SkyrimCrashGuard Log (Rotated)\n";
                s_logFile << "Session continued: " << FormatTimestamp(now) << "\n";
                s_logFile << std::string(80, '=') << "\n\n";
                s_logFile.flush();
            }
        } catch (const std::exception& e) {
            // Don't crash due to log rotation failures
            spdlog::warn("Log rotation failed: {}", e.what());
        }
    }

    void DiagnosticLogger::WriteLogEntry(const LogEntry& entry) {
        std::unique_lock<std::shared_mutex> lock(s_logMutex);  // Use unique_lock for write operation
        
        // Check if log rotation is needed
        CheckAndRotateLogs();
        
        // Add to buffer
        s_logBuffer.push_back(entry);
        if (s_logBuffer.size() > 1000) {
            s_logBuffer.erase(s_logBuffer.begin());
        }
        
        // Write to file
        if (s_logFile.is_open()) {
            s_logFile << FormatTimestamp(entry.timestamp) << " ";
            s_logFile << "[" << FormatSeverity(entry.severity) << "] ";
            s_logFile << "[" << FormatCategory(entry.category) << "] ";
            s_logFile << "[" << entry.component << "] ";
            if (!entry.correlationId.empty()) {
                s_logFile << "[" << entry.correlationId << "] ";
            }
            s_logFile << entry.message;
            if (!entry.metadata.empty()) {
                s_logFile << " " << FormatMetadata(entry.metadata);
            }
            s_logFile << "\n";
            s_logFile.flush();
        }
        
        // Console output for real-time logging
        if (s_realTimeLogging) {
            WriteToConsole(entry);
        }
        
        s_logEntryCount++;
    }

    std::string DiagnosticLogger::FormatTimestamp(const std::chrono::system_clock::time_point& time) {
        auto time_t = std::chrono::system_clock::to_time_t(time);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()) % 1000;
        
        std::ostringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    std::string DiagnosticLogger::FormatSeverity(LogSeverity severity) {
        switch (severity) {
        case LogSeverity::Debug: return "DEBUG";
        case LogSeverity::Info: return "INFO";
        case LogSeverity::Warning: return "WARN";
        case LogSeverity::Error: return "ERROR";
        case LogSeverity::Critical: return "CRIT";
        case LogSeverity::Fatal: return "FATAL";
        default: return "UNKNOWN";
        }
    }

    std::string DiagnosticLogger::FormatCategory(LogCategory category) {
        switch (category) {
        case LogCategory::System: return "SYS";
        case LogCategory::Engine: return "ENG";
        case LogCategory::Mod: return "MOD";
        case LogCategory::Performance: return "PERF";
        case LogCategory::Recovery: return "REC";
        case LogCategory::Pattern: return "PAT";
        case LogCategory::User: return "USER";
        default: return "UNK";
        }
    }

    std::string DiagnosticLogger::FormatMetadata(const std::unordered_map<std::string, std::string>& metadata) {
        if (metadata.empty()) {
            return "";
        }
        
        std::ostringstream ss;
        ss << "{";
        bool first = true;
        for (const auto& [key, value] : metadata) {
            if (!first) ss << ", ";
            ss << key << "=" << value;
            first = false;
        }
        ss << "}";
        return ss.str();
    }

    void DiagnosticLogger::WriteToFile(const std::string& content, const std::string& filename) {
        try {
            std::ofstream file(filename, std::ios::out | std::ios::trunc);
            if (file.is_open()) {
                file << content;
                file.close();
            } else {
                spdlog::error("Failed to write to file: {}", filename);
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception writing to file {}: {}", filename, e.what());
        }
    }

    void DiagnosticLogger::WriteToConsole(const LogEntry& entry) {
        // Use spdlog for console output
        std::string message = fmt::format("[{}] [{}] {}", 
                                         entry.component, 
                                         FormatCategory(entry.category),
                                         entry.message);
        
        switch (entry.severity) {
        case LogSeverity::Debug:
            spdlog::debug(message);
            break;
        case LogSeverity::Info:
            spdlog::info(message);
            break;
        case LogSeverity::Warning:
            spdlog::warn(message);
            break;
        case LogSeverity::Error:
            spdlog::error(message);
            break;
        case LogSeverity::Critical:
        case LogSeverity::Fatal:
            spdlog::critical(message);
            break;
        }
    }

    std::string DiagnosticLogger::GenerateReportId() {
        // Generate unique report ID using timestamp and random component
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        
        return fmt::format("{:x}_{:04x}", timestamp, dis(gen));
    }

    std::string DiagnosticLogger::GenerateCorrelationId() {
        // Generate correlation ID for linking related log entries
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis;
        
        return fmt::format("{:08x}", dis(gen));
    }

    void DiagnosticLogger::CollectSystemInfo(CrashReport& report) {
        // Populate the crash report's metadata map with system information.
        // "os" is always Windows — this plugin only runs on Windows (SKSE requirement).
        report.metadata["os"] = "Windows";

        // Skyrim version from CommonLibSSE at runtime
        {
            auto& mod = REL::Module::get();
            auto v = mod.version();
            report.metadata["skyrimVersion"] = fmt::format("{}.{}.{}.{}",
                v.major(), v.minor(), v.patch(), v.build());
        }

        // SKSE version stored at plugin load time from LoadInterface::SKSEVersion()
        report.metadata["skseVersion"] = Plugin::GetSKSEVersionString();

        // CrashGuard version from compile-time macros
        report.metadata["crashGuardVersion"] = fmt::format("{}.{}.{}",
            PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR, PLUGIN_VERSION_PATCH);
    }

    void DiagnosticLogger::CollectModInfo(CrashReport& report) {
        // Mod information would be collected from TESDataHandler
        try {
            auto dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) {
                report.metadata["totalMods"] = "0";
                return;
            }
            
            // Get all loaded files
            auto& files = dataHandler->files;
            uint32_t modCount = 0;
            
            for (auto& file : files) {
                if (file && file->fileName) {
                    modCount++;
                }
            }
            
            report.metadata["totalMods"] = std::to_string(modCount);
            s_currentPerformance.loadedMods = modCount;
            
            Log(LogSeverity::Debug, LogCategory::Mod, "ModInfoCollector",
                "Collected mod information",
                {{ "modCount", std::to_string(modCount) }});
                
        } catch (const std::exception& e) {
            spdlog::error("Failed to collect mod info: {}", e.what());
            report.metadata["totalMods"] = "Error";
        }
    }

    // ========================================================================
    // Mod List and Identification
    // ========================================================================

    void DiagnosticLogger::LogAllLoadedPlugins(CrashReport& report) {
        try {
            auto dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) {
                return;
            }
            
            std::vector<std::string> pluginList;
            auto& files = dataHandler->files;
            
            // Iterate through the BSSimpleList
            size_t index = 0;
            for (auto it = files.begin(); it != files.end(); ++it, ++index) {
                auto* file = *it;
                if (!file || !file->fileName) {
                    continue;
                }
                
                std::string pluginInfo = fmt::format("[{:02X}] {}", 
                                                     index, 
                                                     file->fileName);
                
                // Add flags
                std::vector<std::string> flags;
                if (file->recordFlags.all(RE::TESFile::RecordFlag::kMaster)) {
                    flags.push_back("ESM");
                }
                if (!file->recordFlags.all(RE::TESFile::RecordFlag::kDelocalized)) {
                    flags.push_back("Localized");
                }
                if (file->recordFlags.all(RE::TESFile::RecordFlag::kSmallFile)) {
                    flags.push_back("ESL");
                }
                
                if (!flags.empty()) {
                    std::string flagsStr;
                    for (size_t i = 0; i < flags.size(); ++i) {
                        if (i > 0) flagsStr += ", ";
                        flagsStr += flags[i];
                    }
                    pluginInfo += fmt::format(" ({})", flagsStr);
                }
                
                pluginList.push_back(pluginInfo);
            }
            
            // Store in metadata
            std::string pluginListStr;
            for (size_t i = 0; i < pluginList.size(); ++i) {
                if (i > 0) pluginListStr += "\n";
                pluginListStr += pluginList[i];
            }
            report.metadata["pluginList"] = pluginListStr;
            report.metadata["pluginCount"] = std::to_string(pluginList.size());
            
            Log(LogSeverity::Debug, LogCategory::Mod, "ModListLogger",
                "Logged all loaded plugins",
                {{ "count", std::to_string(pluginList.size()) }});
                
        } catch (const std::exception& e) {
            spdlog::error("Failed to log loaded plugins: {}", e.what());
        }
    }

    void DiagnosticLogger::HighlightSuspectedMods(CrashReport& report) {
        // Suspected mods should already be populated by RootCauseAnalyzer
        // This function adds additional highlighting and ranking
        
        if (report.suspectedMods.empty()) {
            return;
        }
        
        try {
            auto dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) {
                return;
            }
            
            // Add load order information for suspected mods
            std::vector<std::string> highlightedMods;
            
            for (const auto& modName : report.suspectedMods) {
                // Find the mod in the loaded files
                bool found = false;
                auto& files = dataHandler->files;
                
                // Iterate through the BSSimpleList
                size_t index = 0;
                for (auto it = files.begin(); it != files.end(); ++it, ++index) {
                    auto* file = *it;
                    if (!file || !file->fileName) {
                        continue;
                    }
                    
                    std::string fileName = file->fileName;
                    if (fileName.find(modName) != std::string::npos || modName.find(fileName) != std::string::npos) {
                        std::string highlighted = fmt::format("[{:02X}] {} (SUSPECTED)", index, fileName);
                        highlightedMods.push_back(highlighted);
                        found = true;
                        break;
                    }
                    ++index;
                }
                
                if (!found) {
                    highlightedMods.push_back(fmt::format("{} (SUSPECTED - not in load order)", modName));
                }
            }
            
            std::string highlightedModsStr;
            for (size_t i = 0; i < highlightedMods.size(); ++i) {
                if (i > 0) highlightedModsStr += "\n";
                highlightedModsStr += highlightedMods[i];
            }
            report.metadata["highlightedMods"] = highlightedModsStr;
            
            Log(LogSeverity::Info, LogCategory::Mod, "ModHighlighter",
                "Highlighted suspected mods",
                {{ "count", std::to_string(highlightedMods.size()) }});
                
        } catch (const std::exception& e) {
            spdlog::error("Failed to highlight suspected mods: {}", e.what());
        }
    }

    void DiagnosticLogger::RankModsByLikelihood(CrashReport& report) {
        // Rank suspected mods by likelihood of causing the crash
        // This uses confidence scores and pattern matching
        
        if (report.suspectedMods.empty()) {
            return;
        }
        
        try {
            // Create a ranked list with confidence scores
            std::vector<std::pair<std::string, float>> rankedMods;
            
            // Primary suspect gets highest confidence
            if (!report.suspectedMods.empty()) {
                rankedMods.push_back({ report.suspectedMods[0], report.confidence });
            }
            
            // Secondary suspects get reduced confidence
            for (size_t i = 1; i < report.suspectedMods.size(); ++i) {
                float confidence = report.confidence * (1.0f - (i * 0.15f));
                confidence = std::max(confidence, 0.1f);  // Minimum 10% confidence
                rankedMods.push_back({ report.suspectedMods[i], confidence });
            }
            
            // Sort by confidence (already sorted, but ensure it)
            std::sort(rankedMods.begin(), rankedMods.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });
            
            // Format ranked list
            std::vector<std::string> rankedList;
            for (size_t i = 0; i < rankedMods.size(); ++i) {
                const auto& [modName, confidence] = rankedMods[i];
                std::string rank = fmt::format("{}. {} (Confidence: {:.0f}%)", 
                                              i + 1, 
                                              modName, 
                                              confidence * 100);
                rankedList.push_back(rank);
            }
            
            std::string rankedListStr;
            for (size_t i = 0; i < rankedList.size(); ++i) {
                if (i > 0) rankedListStr += "\n";
                rankedListStr += rankedList[i];
            }
            report.metadata["rankedMods"] = rankedListStr;
            
            Log(LogSeverity::Info, LogCategory::Mod, "ModRanker",
                "Ranked mods by likelihood",
                {{ "topMod", rankedMods[0].first },
                 { "confidence", fmt::format("{:.0f}%", rankedMods[0].second * 100) }});
                
        } catch (const std::exception& e) {
            spdlog::error("Failed to rank mods: {}", e.what());
        }
    }

    void DiagnosticLogger::CollectModListAndIdentification(CrashReport& report) {
        // Collect complete mod list and identification information
        LogAllLoadedPlugins(report);
        HighlightSuspectedMods(report);
        RankModsByLikelihood(report);
        
        Log(LogSeverity::Info, LogCategory::Mod, "ModCollector",
            "Collected mod list and identification",
            {{ "reportId", report.reportId }});
    }

    // ========================================================================
    // Recovery Action Logging
    // ========================================================================

    void DiagnosticLogger::LogRecoveryAction(CrashReport& report, const std::string& action) {
        report.recoveryActions.push_back(action);
        
        Log(LogSeverity::Info, LogCategory::Recovery, "RecoveryLogger",
            "Logged recovery action",
            {{ "reportId", report.reportId },
             { "action", action }});
    }

    void DiagnosticLogger::LogRecoveryStrategy(CrashReport& report, const std::string& strategy) {
        report.recoveryStrategy = strategy;
        
        Log(LogSeverity::Info, LogCategory::Recovery, "RecoveryLogger",
            "Logged recovery strategy",
            {{ "reportId", report.reportId },
             { "strategy", strategy }});
    }

    void DiagnosticLogger::LogRecoverySuccess(CrashReport& report, bool success) {
        report.recoverySuccessful = success;
        
        Log(success ? LogSeverity::Info : LogSeverity::Error, 
            LogCategory::Recovery, 
            "RecoveryLogger",
            success ? "Recovery succeeded" : "Recovery failed",
            {{ "reportId", report.reportId },
             { "success", success ? "true" : "false" }});
    }

    void DiagnosticLogger::LogStateModification(CrashReport& report, const std::string& modification) {
        std::string action = fmt::format("State modification: {}", modification);
        report.recoveryActions.push_back(action);
        
        Log(LogSeverity::Info, LogCategory::Recovery, "StateModifier",
            "Logged state modification",
            {{ "reportId", report.reportId },
             { "modification", modification }});
    }

    void DiagnosticLogger::LogRecoveryAttempt(CrashReport& report, 
                                             const std::string& strategy,
                                             const std::vector<std::string>& actions,
                                             bool success) {
        // Log the complete recovery attempt
        LogRecoveryStrategy(report, strategy);
        
        for (const auto& action : actions) {
            LogRecoveryAction(report, action);
        }
        
        LogRecoverySuccess(report, success);
        
        // Create a summary log entry
        std::string summary = fmt::format("Recovery attempt: {} - {} actions - {}",
                                         strategy,
                                         actions.size(),
                                         success ? "SUCCESS" : "FAILED");
        
        Log(success ? LogSeverity::Info : LogSeverity::Warning,
            LogCategory::Recovery,
            "RecoveryOrchestrator",
            summary,
            {{ "reportId", report.reportId },
             { "strategy", strategy },
             { "actionCount", std::to_string(actions.size()) },
             { "success", success ? "true" : "false" }});
    }

    std::string DiagnosticLogger::FormatRecoveryActions(const CrashReport& report) {
        if (report.recoveryActions.empty()) {
            return "No recovery actions performed";
        }
        
        std::ostringstream ss;
        ss << "Recovery Actions Performed:\n";
        ss << "Strategy: " << report.recoveryStrategy << "\n";
        ss << "Success: " << (report.recoverySuccessful ? "Yes" : "No") << "\n";
        ss << "Actions:\n";
        
        for (size_t i = 0; i < report.recoveryActions.size(); ++i) {
            ss << "  " << (i + 1) << ". " << report.recoveryActions[i] << "\n";
        }
        
        return ss.str();
    }

    void DiagnosticLogger::WriteRecoveryLog(const CrashReport& report, const std::string& filename) {
        if (!s_initialized) {
            return;
        }
        
        std::ostringstream ss;
        
        ss << "CRASH RECOVERY LOG\n";
        ss << std::string(80, '=') << "\n\n";
        
        ss << "Report ID: " << report.reportId << "\n";
        ss << "Timestamp: " << FormatTimestamp(report.timestamp) << "\n";
        ss << "Crash Type: " << report.category << "\n";
        ss << "Root Cause: " << report.rootCause << "\n\n";
        
        ss << "RECOVERY ATTEMPT\n";
        ss << std::string(40, '-') << "\n";
        ss << "Strategy: " << report.recoveryStrategy << "\n";
        ss << "Success: " << (report.recoverySuccessful ? "Yes" : "No") << "\n\n";
        
        if (!report.recoveryActions.empty()) {
            ss << "ACTIONS PERFORMED\n";
            ss << std::string(40, '-') << "\n";
            for (size_t i = 0; i < report.recoveryActions.size(); ++i) {
                ss << (i + 1) << ". " << report.recoveryActions[i] << "\n";
            }
            ss << "\n";
        }
        
        if (!report.suspectedMods.empty()) {
            ss << "SUSPECTED MODS\n";
            ss << std::string(40, '-') << "\n";
            for (const auto& mod : report.suspectedMods) {
                ss << "- " << mod << "\n";
            }
            ss << "\n";
        }
        
        ss << "GAME STATE AT CRASH\n";
        ss << std::string(40, '-') << "\n";
        ss << "Location: " << report.gameState.playerLocation << "\n";
        ss << "Activity: " << report.gameState.currentActivity << "\n";
        ss << "In Combat: " << (report.gameState.isInCombat ? "Yes" : "No") << "\n";
        ss << "In Dialogue: " << (report.gameState.isInDialogue ? "Yes" : "No") << "\n";
        ss << "In Menu: " << (report.gameState.isInMenu ? "Yes" : "No") << "\n\n";
        
        ss << "PERFORMANCE METRICS\n";
        ss << std::string(40, '-') << "\n";
        ss << "FPS: " << std::fixed << std::setprecision(1) << report.performance.fps << "\n";
        ss << "Memory: " << report.performance.memoryUsageMB << " MB\n";
        ss << "CPU: " << std::fixed << std::setprecision(1) << report.performance.cpuUsagePercent << "%\n";
        ss << "Loaded Mods: " << report.performance.loadedMods << "\n";
        ss << "Active NPCs: " << report.performance.activeNPCs << "\n\n";
        
        std::string outputFilename = filename.empty() 
            ? s_logDirectory + "/recovery_" + report.reportId + ".log"
            : filename;
        
        WriteToFile(ss.str(), outputFilename);
        
        Log(LogSeverity::Info, LogCategory::Recovery, "RecoveryLogger",
            "Wrote recovery log",
            {{ "reportId", report.reportId },
             { "filename", outputFilename }});
    }

    // ========================================================================
    // Pattern Aggregation
    // ========================================================================

    struct CrashPatternData {
        std::string signature;
        std::string category;
        uint32_t occurrences;
        std::vector<std::string> involvedMods;
        std::chrono::system_clock::time_point firstSeen;
        std::chrono::system_clock::time_point lastSeen;
        float averageConfidence;
    };

    // Static storage for pattern data
    static std::unordered_map<std::string, CrashPatternData> s_patternData;
    static std::mutex s_patternMutex;

    void DiagnosticLogger::TrackCrashPattern(const CrashReport& report) {
        std::lock_guard<std::mutex> lock(s_patternMutex);
        
        auto& pattern = s_patternData[report.crashSignature];
        
        // Initialize if new pattern
        if (pattern.occurrences == 0) {
            pattern.signature = report.crashSignature;
            pattern.category = report.category;
            pattern.firstSeen = report.timestamp;
            pattern.averageConfidence = 0.0f;
        }
        
        // Update pattern data
        pattern.occurrences++;
        pattern.lastSeen = report.timestamp;
        
        // Update average confidence
        pattern.averageConfidence = ((pattern.averageConfidence * (pattern.occurrences - 1)) + report.confidence) 
                                   / pattern.occurrences;
        
        // Add involved mods (avoid duplicates)
        for (const auto& mod : report.suspectedMods) {
            if (std::find(pattern.involvedMods.begin(), pattern.involvedMods.end(), mod) 
                == pattern.involvedMods.end()) {
                pattern.involvedMods.push_back(mod);
            }
        }
        
        Log(LogSeverity::Info, LogCategory::Pattern, "PatternTracker",
            "Tracked crash pattern",
            {{ "signature", report.crashSignature },
             { "category", report.category },
             { "occurrences", std::to_string(pattern.occurrences) }});
    }

    std::vector<CrashPatternData> DiagnosticLogger::GetSimilarCrashPatterns(const std::string& signature) {
        std::lock_guard<std::mutex> lock(s_patternMutex);
        
        std::vector<CrashPatternData> similar;
        
        // Find patterns with similar signatures
        for (const auto& [sig, data] : s_patternData) {
            if (sig == signature) {
                similar.push_back(data);
            } else {
                // Check for partial signature match (same exception code)
                if (sig.substr(0, 8) == signature.substr(0, 8)) {
                    similar.push_back(data);
                }
            }
        }
        
        // Sort by occurrences (most frequent first)
        std::sort(similar.begin(), similar.end(),
                 [](const auto& a, const auto& b) { return a.occurrences > b.occurrences; });
        
        return similar;
    }

    void DiagnosticLogger::GroupSimilarCrashes(std::vector<CrashReport>& reports) {
        // Group crashes by signature
        std::unordered_map<std::string, std::vector<CrashReport*>> groups;
        
        for (auto& report : reports) {
            groups[report.crashSignature].push_back(&report);
        }
        
        Log(LogSeverity::Info, LogCategory::Pattern, "PatternGrouper",
            "Grouped similar crashes",
            {{ "totalCrashes", std::to_string(reports.size()) },
             { "uniquePatterns", std::to_string(groups.size()) }});
    }

    std::string DiagnosticLogger::GeneratePatternSummary() {
        std::lock_guard<std::mutex> lock(s_patternMutex);
        
        if (s_patternData.empty()) {
            return "No crash patterns recorded";
        }
        
        std::ostringstream ss;
        
        ss << "CRASH PATTERN SUMMARY\n";
        ss << std::string(80, '=') << "\n\n";
        
        ss << "Total Unique Patterns: " << s_patternData.size() << "\n";
        ss << "Total Crashes: " << s_crashCount << "\n\n";
        
        // Sort patterns by occurrence
        std::vector<CrashPatternData> sortedPatterns;
        for (const auto& [sig, data] : s_patternData) {
            sortedPatterns.push_back(data);
        }
        
        std::sort(sortedPatterns.begin(), sortedPatterns.end(),
                 [](const auto& a, const auto& b) { return a.occurrences > b.occurrences; });
        
        // Show top 10 patterns
        ss << "TOP CRASH PATTERNS\n";
        ss << std::string(40, '-') << "\n";
        
        size_t displayCount = std::min(sortedPatterns.size(), size_t(10));
        for (size_t i = 0; i < displayCount; ++i) {
            const auto& pattern = sortedPatterns[i];
            
            ss << (i + 1) << ". " << pattern.category << " (" << pattern.occurrences << " times)\n";
            ss << "   Signature: " << pattern.signature << "\n";
            ss << "   Confidence: " << std::fixed << std::setprecision(0) 
               << (pattern.averageConfidence * 100) << "%\n";
            
            if (!pattern.involvedMods.empty()) {
                ss << "   Mods: ";
                for (size_t j = 0; j < std::min(pattern.involvedMods.size(), size_t(3)); ++j) {
                    if (j > 0) ss << ", ";
                    ss << pattern.involvedMods[j];
                }
                if (pattern.involvedMods.size() > 3) {
                    ss << " (+" << (pattern.involvedMods.size() - 3) << " more)";
                }
                ss << "\n";
            }
            
            ss << "   First seen: " << FormatTimestamp(pattern.firstSeen) << "\n";
            ss << "   Last seen: " << FormatTimestamp(pattern.lastSeen) << "\n";
            ss << "\n";
        }
        
        // Category breakdown
        ss << "CRASH CATEGORIES\n";
        ss << std::string(40, '-') << "\n";
        
        std::unordered_map<std::string, uint32_t> categoryCount;
        for (const auto& pattern : sortedPatterns) {
            categoryCount[pattern.category] += pattern.occurrences;
        }
        
        std::vector<std::pair<std::string, uint32_t>> sortedCategories(
            categoryCount.begin(), categoryCount.end());
        std::sort(sortedCategories.begin(), sortedCategories.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (const auto& [category, count] : sortedCategories) {
            float percentage = (static_cast<float>(count) / s_crashCount) * 100.0f;
            ss << category << ": " << count << " (" << std::fixed << std::setprecision(1) 
               << percentage << "%)\n";
        }
        ss << "\n";
        
        // Mod involvement
        ss << "MOST PROBLEMATIC MODS\n";
        ss << std::string(40, '-') << "\n";
        
        std::unordered_map<std::string, uint32_t> modCount;
        for (const auto& pattern : sortedPatterns) {
            for (const auto& mod : pattern.involvedMods) {
                modCount[mod] += pattern.occurrences;
            }
        }
        
        std::vector<std::pair<std::string, uint32_t>> sortedMods(
            modCount.begin(), modCount.end());
        std::sort(sortedMods.begin(), sortedMods.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        size_t modDisplayCount = std::min(sortedMods.size(), size_t(10));
        for (size_t i = 0; i < modDisplayCount; ++i) {
            const auto& [mod, count] = sortedMods[i];
            ss << (i + 1) << ". " << mod << " (" << count << " crashes)\n";
        }
        
        return ss.str();
    }

    void DiagnosticLogger::WritePatternSummary(const std::string& filename) {
        if (!s_initialized) {
            return;
        }
        
        std::string summary = GeneratePatternSummary();
        
        std::string outputFilename = filename.empty()
            ? s_logDirectory + "/pattern_summary.txt"
            : filename;
        
        WriteToFile(summary, outputFilename);
        
        Log(LogSeverity::Info, LogCategory::Pattern, "PatternSummary",
            "Wrote pattern summary",
            {{ "filename", outputFilename },
             { "patternCount", std::to_string(s_patternData.size()) }});
    }

    void DiagnosticLogger::ExportPatternData(const std::string& filename) {
        std::lock_guard<std::mutex> lock(s_patternMutex);
        
        if (!s_initialized) {
            return;
        }
        
        nlohmann::json j;
        j["version"] = "1.0.0";
        j["exportTime"] = FormatTimestamp(std::chrono::system_clock::now());
        j["totalPatterns"] = s_patternData.size();
        j["totalCrashes"] = s_crashCount;
        
        nlohmann::json patterns = nlohmann::json::array();
        for (const auto& [sig, data] : s_patternData) {
            nlohmann::json pattern;
            pattern["signature"] = data.signature;
            pattern["category"] = data.category;
            pattern["occurrences"] = data.occurrences;
            pattern["involvedMods"] = data.involvedMods;
            pattern["firstSeen"] = FormatTimestamp(data.firstSeen);
            pattern["lastSeen"] = FormatTimestamp(data.lastSeen);
            pattern["averageConfidence"] = data.averageConfidence;
            patterns.push_back(pattern);
        }
        j["patterns"] = patterns;
        
        std::string outputFilename = filename.empty()
            ? s_logDirectory + "/pattern_data.json"
            : filename;
        
        WriteToFile(j.dump(2), outputFilename);
        
        Log(LogSeverity::Info, LogCategory::Pattern, "PatternExport",
            "Exported pattern data",
            {{ "filename", outputFilename },
             { "patternCount", std::to_string(s_patternData.size()) }});
    }

    // ========================================================================
    // Mesh-Specific Diagnostics
    // ========================================================================

    void DiagnosticLogger::LogMeshFilePath(CrashReport& report, const std::string& meshPath, const std::string& modName) {
        report.metadata["meshPath"] = meshPath;
        report.metadata["meshMod"] = modName;
        
        Log(LogSeverity::Info, LogCategory::Mod, "MeshDiagnostics",
            "Logged mesh file path",
            {{ "reportId", report.reportId },
             { "meshPath", meshPath },
             { "mod", modName }});
    }

    void DiagnosticLogger::LogMeshCounts(CrashReport& report, 
                                        uint32_t vertexCount, 
                                        uint32_t triangleCount, 
                                        uint32_t boneCount) {
        report.metadata["meshVertexCount"] = std::to_string(vertexCount);
        report.metadata["meshTriangleCount"] = std::to_string(triangleCount);
        report.metadata["meshBoneCount"] = std::to_string(boneCount);
        
        Log(LogSeverity::Debug, LogCategory::Engine, "MeshDiagnostics",
            "Logged mesh counts",
            {{ "reportId", report.reportId },
             { "vertices", std::to_string(vertexCount) },
             { "triangles", std::to_string(triangleCount) },
             { "bones", std::to_string(boneCount) }});
    }

    void DiagnosticLogger::LogMeshValidationFailures(CrashReport& report, 
                                                     const std::vector<std::string>& failures) {
        if (failures.empty()) {
            return;
        }
        
        std::string failureList;
        for (size_t i = 0; i < failures.size(); ++i) {
            if (i > 0) failureList += "; ";
            failureList += failures[i];
        }
        report.metadata["meshValidationFailures"] = failureList;
        
        // Add to recovery actions for visibility
        std::string action = fmt::format("Mesh validation failed: {}", failureList);
        report.recoveryActions.push_back(action);
        
        Log(LogSeverity::Warning, LogCategory::Engine, "MeshValidator",
            "Logged mesh validation failures",
            {{ "reportId", report.reportId },
             { "failureCount", std::to_string(failures.size()) },
             { "failures", failureList }});
    }

    void DiagnosticLogger::LogMeshRepairsApplied(CrashReport& report, 
                                                 const std::vector<std::string>& repairs) {
        if (repairs.empty()) {
            return;
        }
        
        std::string repairList;
        for (size_t i = 0; i < repairs.size(); ++i) {
            if (i > 0) repairList += "; ";
            repairList += repairs[i];
        }
        report.metadata["meshRepairs"] = repairList;
        
        // Add to recovery actions
        for (const auto& repair : repairs) {
            std::string action = fmt::format("Mesh repair: {}", repair);
            report.recoveryActions.push_back(action);
        }
        
        Log(LogSeverity::Info, LogCategory::Recovery, "MeshRepair",
            "Logged mesh repairs",
            {{ "reportId", report.reportId },
             { "repairCount", std::to_string(repairs.size()) },
             { "repairs", repairList }});
    }

    std::string DiagnosticLogger::FormatMeshDiagnostics(const CrashReport& report) {
        std::ostringstream ss;
        
        ss << "MESH DIAGNOSTICS\n";
        ss << std::string(40, '-') << "\n";
        
        // Mesh file info
        auto meshPathIt = report.metadata.find("meshPath");
        if (meshPathIt != report.metadata.end()) {
            ss << "File: " << meshPathIt->second << "\n";
        }
        
        auto meshModIt = report.metadata.find("meshMod");
        if (meshModIt != report.metadata.end()) {
            ss << "Mod: " << meshModIt->second << "\n";
        }
        
        // Mesh counts
        auto vertexIt = report.metadata.find("meshVertexCount");
        auto triangleIt = report.metadata.find("meshTriangleCount");
        auto boneIt = report.metadata.find("meshBoneCount");
        
        if (vertexIt != report.metadata.end()) {
            ss << "Vertices: " << vertexIt->second << "\n";
        }
        if (triangleIt != report.metadata.end()) {
            ss << "Triangles: " << triangleIt->second << "\n";
        }
        if (boneIt != report.metadata.end()) {
            ss << "Bones: " << boneIt->second << "\n";
        }
        
        // Validation failures
        auto failuresIt = report.metadata.find("meshValidationFailures");
        if (failuresIt != report.metadata.end()) {
            ss << "\nValidation Failures:\n";
            ss << failuresIt->second << "\n";
        }
        
        // Repairs applied
        auto repairsIt = report.metadata.find("meshRepairs");
        if (repairsIt != report.metadata.end()) {
            ss << "\nRepairs Applied:\n";
            ss << repairsIt->second << "\n";
        }
        
        return ss.str();
    }

    void DiagnosticLogger::WriteMeshDiagnosticReport(const CrashReport& report, const std::string& filename) {
        if (!s_initialized) {
            return;
        }
        
        // Only write if this is a mesh-related crash
        if (report.category != "Mesh") {
            return;
        }
        
        std::ostringstream ss;
        
        ss << "MESH CRASH DIAGNOSTIC REPORT\n";
        ss << std::string(80, '=') << "\n\n";
        
        ss << "Report ID: " << report.reportId << "\n";
        ss << "Timestamp: " << FormatTimestamp(report.timestamp) << "\n";
        ss << "Root Cause: " << report.rootCause << "\n\n";
        
        ss << FormatMeshDiagnostics(report);
        ss << "\n";
        
        // Game context
        ss << "GAME CONTEXT\n";
        ss << std::string(40, '-') << "\n";
        ss << "Location: " << report.gameState.playerLocation << "\n";
        ss << "Activity: " << report.gameState.currentActivity << "\n\n";
        
        // Recovery actions
        if (!report.recoveryActions.empty()) {
            ss << "RECOVERY ACTIONS\n";
            ss << std::string(40, '-') << "\n";
            for (size_t i = 0; i < report.recoveryActions.size(); ++i) {
                ss << (i + 1) << ". " << report.recoveryActions[i] << "\n";
            }
            ss << "\n";
        }
        
        // Suggested fixes
        if (!report.suggestedFixes.empty()) {
            ss << "SUGGESTED FIXES\n";
            ss << std::string(40, '-') << "\n";
            for (size_t i = 0; i < report.suggestedFixes.size(); ++i) {
                ss << (i + 1) << ". " << report.suggestedFixes[i] << "\n";
            }
            ss << "\n";
        }
        
        std::string outputFilename = filename.empty()
            ? s_logDirectory + "/mesh_crash_" + report.reportId + ".log"
            : filename;
        
        WriteToFile(ss.str(), outputFilename);
        
        Log(LogSeverity::Info, LogCategory::Engine, "MeshDiagnostics",
            "Wrote mesh diagnostic report",
            {{ "reportId", report.reportId },
             { "filename", outputFilename }});
    }

    void DiagnosticLogger::LogCompleteMeshDiagnostics(CrashReport& report,
                                                      const std::string& meshPath,
                                                      const std::string& modName,
                                                      uint32_t vertexCount,
                                                      uint32_t triangleCount,
                                                      uint32_t boneCount,
                                                      const std::vector<std::string>& failures,
                                                      const std::vector<std::string>& repairs) {
        // Log all mesh diagnostics in one call
        LogMeshFilePath(report, meshPath, modName);
        LogMeshCounts(report, vertexCount, triangleCount, boneCount);
        LogMeshValidationFailures(report, failures);
        LogMeshRepairsApplied(report, repairs);
        
        // Write dedicated mesh diagnostic report
        WriteMeshDiagnosticReport(report);
        
        Log(LogSeverity::Info, LogCategory::Engine, "MeshDiagnostics",
            "Logged complete mesh diagnostics",
            {{ "reportId", report.reportId },
             { "meshPath", meshPath }});
    }

    void DiagnosticLogger::CollectRecentLogs(CrashReport& report, uint32_t count) {
        std::shared_lock<std::shared_mutex> lock(s_logMutex);  // Use shared_lock for read-only operation
        
        // Get the most recent log entries
        size_t startIdx = s_logBuffer.size() > count ? s_logBuffer.size() - count : 0;
        report.recentLogs.assign(s_logBuffer.begin() + startIdx, s_logBuffer.end());
    }

    // ========================================================================
    // Game State Logging
    // ========================================================================

    void DiagnosticLogger::LogCurrentCell(CrashReport& report) {
        try {
            auto player = RE::PlayerCharacter::GetSingleton();
            if (player && player->parentCell) {
                auto cell = player->parentCell;
                
                // Get cell name
                if (cell->GetName() && strlen(cell->GetName()) > 0) {
                    report.gameState.playerLocation = cell->GetName();
                } else {
                    report.gameState.playerLocation = fmt::format("Cell [{:08X}]", cell->GetFormID());
                }
                
                // Get player position
                auto pos = player->GetPosition();
                report.metadata["playerX"] = fmt::format("{:.2f}", pos.x);
                report.metadata["playerY"] = fmt::format("{:.2f}", pos.y);
                report.metadata["playerZ"] = fmt::format("{:.2f}", pos.z);
                
                // Store in performance snapshot
                s_currentPerformance.currentCell = report.gameState.playerLocation;
                
                Log(LogSeverity::Debug, LogCategory::System, "GameStateLogger",
                    "Logged current cell and player position",
                    {{ "cell", report.gameState.playerLocation },
                     { "position", fmt::format("({:.1f}, {:.1f}, {:.1f})", pos.x, pos.y, pos.z) }});
            } else {
                report.gameState.playerLocation = "Unknown (player not loaded)";
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to log current cell: {}", e.what());
            report.gameState.playerLocation = "Error retrieving cell";
        }
    }

    void DiagnosticLogger::LogNearbyNPCs(CrashReport& report, uint32_t maxCount) {
        try {
            auto player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->parentCell) {
                return;
            }
            
            // Cell reference iteration not implemented
            // The cell->references API is not available in current CommonLibSSE version
            
            Log(LogSeverity::Debug, LogCategory::System, "GameStateLogger",
                "Nearby NPC logging not yet implemented",
                {{ "reason", "Cell reference iteration API not available" }});
                
        } catch (const std::exception& e) {
            spdlog::error("Failed to log nearby NPCs: {}", e.what());
        }
    }

    void DiagnosticLogger::LogActiveQuests(CrashReport& report, uint32_t maxCount) {
        try {
            auto questLog = RE::BGSStoryTeller::GetSingleton();
            if (!questLog) {
                return;
            }
            
            // Get active quests from the player
            auto player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return;
            }
            
            // Iterate through quest objectives
            // Note: This is a simplified implementation - actual quest tracking is more complex
            auto dataHandler = RE::TESDataHandler::GetSingleton();
            if (dataHandler) {
                for (auto& quest : dataHandler->GetFormArray<RE::TESQuest>()) {
                    if (!quest || !quest->IsActive()) {
                        continue;
                    }
                    
                    std::string questInfo;
                    if (quest->GetName() && strlen(quest->GetName()) > 0) {
                        questInfo = fmt::format("{} [{:08X}]", quest->GetName(), quest->GetFormID());
                    } else {
                        questInfo = fmt::format("Quest [{:08X}]", quest->GetFormID());
                    }
                    
                    report.gameState.activeQuests.push_back(questInfo);
                    
                    if (report.gameState.activeQuests.size() >= maxCount) {
                        break;
                    }
                }
            }
            
            Log(LogSeverity::Debug, LogCategory::System, "GameStateLogger",
                "Logged active quests",
                {{ "count", std::to_string(report.gameState.activeQuests.size()) }});
                
        } catch (const std::exception& e) {
            spdlog::error("Failed to log active quests: {}", e.what());
        }
    }

    void DiagnosticLogger::LogPlayerActivity(CrashReport& report) {
        try {
            auto player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                report.gameState.currentActivity = "Unknown (player not loaded)";
                return;
            }
            
            // Determine current activity
            std::vector<std::string> activities;
            
            // Check combat state
            if (player->IsInCombat()) {
                activities.push_back("Combat");
                report.gameState.isInCombat = true;
            } else {
                report.gameState.isInCombat = false;
            }
            
            // Check dialogue state
            auto menuManager = RE::UI::GetSingleton();
            if (menuManager && menuManager->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
                activities.push_back("Dialogue");
                report.gameState.isInDialogue = true;
            } else {
                report.gameState.isInDialogue = false;
            }
            
            // Check menu state
            if (menuManager && (menuManager->IsMenuOpen(RE::InventoryMenu::MENU_NAME) ||
                               menuManager->IsMenuOpen(RE::MagicMenu::MENU_NAME) ||
                               menuManager->IsMenuOpen(RE::MapMenu::MENU_NAME))) {
                activities.push_back("Menu");
                report.gameState.isInMenu = true;
            } else {
                report.gameState.isInMenu = false;
            }
            
            // Check if sneaking
            if (player->IsSneaking()) {
                activities.push_back("Sneaking");
            }
            
            // Check if swimming - method may not be available in all CommonLibSSE versions
            // if (player->IsSwimming()) {
            //     activities.push_back("Swimming");
            // }
            
            // Check if riding
            if (player->IsOnMount()) {
                activities.push_back("Riding");
            }
            
            // Combine activities
            if (activities.empty()) {
                report.gameState.currentActivity = "Exploring";
            } else {
                std::string activitiesStr;
                for (size_t i = 0; i < activities.size(); ++i) {
                    if (i > 0) activitiesStr += ", ";
                    activitiesStr += activities[i];
                }
                report.gameState.currentActivity = activitiesStr;
            }
            
            // Get weather and time
            auto sky = RE::Sky::GetSingleton();
            if (sky && sky->currentWeather) {
                auto weather = sky->currentWeather;
                if (weather->GetName() && strlen(weather->GetName()) > 0) {
                    report.gameState.weatherCondition = weather->GetName();
                } else {
                    report.gameState.weatherCondition = "Unknown";
                }
            } else {
                report.gameState.weatherCondition = "Unknown";
            }
            
            // Get time of day
            auto calendar = RE::Calendar::GetSingleton();
            if (calendar) {
                float gameHour = calendar->GetHour();
                int hour = static_cast<int>(gameHour);
                int minute = static_cast<int>((gameHour - hour) * 60);
                report.gameState.timeOfDay = fmt::format("{:02d}:{:02d}", hour, minute);
            } else {
                report.gameState.timeOfDay = "Unknown";
            }
            
            Log(LogSeverity::Debug, LogCategory::System, "GameStateLogger",
                "Logged player activity",
                {{ "activity", report.gameState.currentActivity },
                 { "weather", report.gameState.weatherCondition },
                 { "time", report.gameState.timeOfDay }});
                
        } catch (const std::exception& e) {
            spdlog::error("Failed to log player activity: {}", e.what());
            report.gameState.currentActivity = "Error retrieving activity";
        }
    }

    void DiagnosticLogger::CollectGameState(CrashReport& report) {
        // Collect all game state information
        LogCurrentCell(report);
        LogNearbyNPCs(report, 10);  // Log up to 10 nearby NPCs
        LogActiveQuests(report, 5);  // Log up to 5 active quests
        LogPlayerActivity(report);
        
        Log(LogSeverity::Info, LogCategory::System, "GameStateLogger",
            "Collected complete game state for crash report",
            {{ "reportId", report.reportId }});
    }

    std::string DiagnosticLogger::GenerateCrashSignature(uint32_t exceptionCode, void* address,
                                                        const std::vector<std::string>& callStack) {
        // Generate a unique signature for this crash pattern
        std::ostringstream signature;
        signature << fmt::format("{:08x}", exceptionCode);
        signature << "_";
        signature << fmt::format("{:016x}", reinterpret_cast<uintptr_t>(address) & 0xFFFFFF);
        
        // Include top 3 stack frames if available
        for (size_t i = 0; i < std::min(callStack.size(), size_t(3)); ++i) {
            signature << "_" << std::hash<std::string>{}(callStack[i]);
        }
        
        return signature.str();
    }

    void DiagnosticLogger::UpdatePatternDatabase(const std::string& signature, const std::string& category) {
        std::unique_lock<std::shared_mutex> lock(s_logMutex);  // Use unique_lock for write operation
        
        // Increment crash count for this signature
        s_crashPatterns[signature]++;
        
        // Log pattern update
        Log(LogSeverity::Info, LogCategory::Pattern, "PatternTracker",
            "Updated crash pattern",
            {{ "signature", signature },
             { "category", category },
             { "count", std::to_string(s_crashPatterns[signature]) }});
    }

    std::vector<std::string> DiagnosticLogger::GenerateFixSuggestions(const CrashReport& report) {
        std::vector<std::string> suggestions;
        
        // Category-specific suggestions
        if (report.category == "Mesh") {
            suggestions.push_back("Validate mesh files with NifSkope");
            suggestions.push_back("Reinstall mods with corrupted meshes");
            suggestions.push_back("Check for mesh conflicts in load order");
        } else if (report.category == "Animation") {
            suggestions.push_back("Reinstall animation mods");
            suggestions.push_back("Check for skeleton compatibility");
            suggestions.push_back("Verify FNIS/Nemesis generation");
        } else if (report.category == "Script") {
            suggestions.push_back("Clean save and reload");
            suggestions.push_back("Check Papyrus logs for errors");
            suggestions.push_back("Disable script-heavy mods temporarily");
        } else if (report.category == "Memory") {
            suggestions.push_back("Restart Skyrim to free memory");
            suggestions.push_back("Reduce texture quality settings");
            suggestions.push_back("Install memory optimization mods");
        } else if (report.category == "Cell") {
            suggestions.push_back("Validate cell references");
            suggestions.push_back("Check for navmesh conflicts");
            suggestions.push_back("Rebuild precombined meshes");
        } else if (report.category == "AI") {
            suggestions.push_back("Check AI package conflicts");
            suggestions.push_back("Validate NPC records");
            suggestions.push_back("Review AI overhaul mods");
        } else if (report.category == "GridBoundary") {
            suggestions.push_back("Adjust uGridsToLoad setting");
            suggestions.push_back("Check for AI packages crossing cell boundaries");
            suggestions.push_back("Install grid boundary fix mods");
        }
        
        // Mod-specific suggestions
        if (!report.suspectedMods.empty()) {
            suggestions.insert(suggestions.begin(), 
                             "Update or reinstall " + report.suspectedMods[0]);
        }
        
        // Pattern-based suggestions
        if (report.communityStatus == "Known issue") {
            suggestions.insert(suggestions.begin(), 
                             "Check mod page for known fixes and patches");
        }
        
        // Recovery-specific suggestions
        if (report.recoverySuccessful) {
            suggestions.push_back("Continue playing - issue was automatically fixed");
            suggestions.push_back("Save your game to preserve the fix");
        } else {
            suggestions.push_back("Save and restart Skyrim");
            suggestions.push_back("Load a previous save before the crash");
        }
        
        // Limit to most relevant suggestions
        if (suggestions.size() > 7) {
            suggestions.resize(7);
        }
        
        return suggestions;
    }

    // ========================================================================
    // Performance and State Tracking
    // ========================================================================

    PerformanceSnapshot DiagnosticLogger::GetCurrentPerformance() {
        return s_currentPerformance;
    }

    void DiagnosticLogger::UpdateGameState(const GameStateContext& state) {
        std::unique_lock<std::shared_mutex> lock(s_logMutex);  // Use unique_lock for write operation
        s_currentGameState = state;
    }

    GameStateContext DiagnosticLogger::GetCurrentGameState() {
        std::shared_lock<std::shared_mutex> lock(s_logMutex);  // Use shared_lock for read-only operation
        return s_currentGameState;
    }

    void DiagnosticLogger::RecordCrashPattern(const std::string& signature,
                                             const std::string& category) {
        UpdatePatternDatabase(signature, category);
    }

    std::string DiagnosticLogger::AnalyzeCrashPattern(const std::string& signature) {
        std::shared_lock<std::shared_mutex> lock(s_logMutex);  // Use shared_lock for read-only operation
        
        auto it = s_crashPatterns.find(signature);
        if (it != s_crashPatterns.end()) {
            uint32_t count = it->second;
            if (count > 10) {
                return "Known issue - occurs frequently";
            } else if (count > 3) {
                return "Recurring pattern - investigate further";
            } else {
                return "Rare occurrence";
            }
        }
        
        return "New pattern - first occurrence";
    }

    // ========================================================================
    // Configuration
    // ========================================================================

    void DiagnosticLogger::SetLogLevel(LogSeverity minLevel) {
        s_minLogLevel = minLevel;
        Log(LogSeverity::Info, LogCategory::System, "DiagnosticLogger",
            "Log level changed",
            {{ "newLevel", FormatSeverity(minLevel) }});
    }

    void DiagnosticLogger::SetOutputFormats(const std::vector<OutputFormat>& formats) {
        s_outputFormats = formats;
    }

    void DiagnosticLogger::EnableRealTimeLogging(bool enabled) {
        s_realTimeLogging = enabled;
    }

    void DiagnosticLogger::EnableCommunityReporting(bool enabled) {
        s_communityReporting = enabled;
    }

    // ========================================================================
    // Statistics
    // ========================================================================

    uint32_t DiagnosticLogger::GetLogEntryCount() {
        return s_logEntryCount;
    }

    uint32_t DiagnosticLogger::GetCrashCount() {
        return s_crashCount;
    }

    std::vector<std::string> DiagnosticLogger::GetTopCrashCategories() {
        // This would analyze crash patterns and return top categories
        // Placeholder implementation
        return { "Mesh", "Animation", "Script" };
    }

    std::unordered_map<std::string, uint32_t> DiagnosticLogger::GetModCrashStats() {
        // This would track which mods cause the most crashes
        // Placeholder implementation
        return {};
    }

    // ========================================================================
    // Community Features (Placeholders)
    // ========================================================================

    bool DiagnosticLogger::ExportForCommunity(const std::string& filename, bool anonymize) {
        if (!s_initialized) {
            return false;
        }
        
        // Export crash patterns in a community-shareable format
        nlohmann::json j;
        j["version"] = "1.0.0";
        j["anonymized"] = anonymize;
        j["crashCount"] = s_crashCount;
        
        nlohmann::json patterns = nlohmann::json::array();
        for (const auto& [signature, count] : s_crashPatterns) {
            nlohmann::json pattern;
            pattern["signature"] = signature;
            pattern["count"] = count;
            patterns.push_back(pattern);
        }
        j["patterns"] = patterns;
        
        WriteToFile(j.dump(2), filename);
        
        Log(LogSeverity::Info, LogCategory::System, "CommunityExport",
            "Exported crash patterns for community",
            {{ "filename", filename },
             { "patternCount", std::to_string(s_crashPatterns.size()) }});
        
        return true;
    }

    bool DiagnosticLogger::ImportCommunityPatterns(const std::string& filename) {
        if (!s_initialized) {
            return false;
        }
        
        try {
            std::ifstream file(filename);
            if (!file.is_open()) {
                spdlog::error("Failed to open community patterns file: {}", filename);
                return false;
            }
            
            nlohmann::json j;
            file >> j;
            file.close();
            
            // Import patterns
            if (j.contains("patterns")) {
                std::unique_lock<std::shared_mutex> lock(s_logMutex);  // Use unique_lock for write operation
                for (const auto& pattern : j["patterns"]) {
                    std::string signature = pattern["signature"];
                    uint32_t count = pattern["count"];
                    
                    // Merge with existing patterns
                    s_crashPatterns[signature] += count;
                }
            }
            
            Log(LogSeverity::Info, LogCategory::System, "CommunityImport",
                "Imported community crash patterns",
                {{ "filename", filename }});
            
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to import community patterns: {}", e.what());
            return false;
        }
    }

    // ========================================================================
    // Suggested Fixes Section
    // ========================================================================

    struct KnownIssue {
        std::string signature;
        std::string description;
        std::vector<std::string> fixes;
        std::string linkUrl;
    };

    // Static database of known issues
    static std::vector<KnownIssue> s_knownIssues = {
        {
            "mesh_nan_values",
            "Mesh contains NaN or infinite values",
            {
                "Validate mesh in NifSkope",
                "Reinstall the mod providing the mesh",
                "Use mesh repair tools like NIF Optimizer"
            },
            "https://wiki.nexusmods.com/index.php/Mesh_errors"
        },
        {
            "animation_skeleton_mismatch",
            "Animation skeleton doesn't match character skeleton",
            {
                "Regenerate FNIS/Nemesis animations",
                "Check skeleton compatibility",
                "Install XP32 Maximum Skeleton Extended (XPMSE)"
            },
            "https://wiki.nexusmods.com/index.php/Animation_issues"
        },
        {
            "script_null_reference",
            "Script accessed a null object reference",
            {
                "Clean save and reload",
                "Check Papyrus logs for the problematic script",
                "Disable or update the mod with the script"
            },
            "https://wiki.nexusmods.com/index.php/Papyrus_errors"
        },
        {
            "memory_exhaustion",
            "System ran out of available memory",
            {
                "Restart Skyrim to free memory",
                "Reduce texture quality in settings",
                "Install SSE Engine Fixes for memory optimizations",
                "Close other applications"
            },
            "https://wiki.nexusmods.com/index.php/Memory_issues"
        }
    };

    std::vector<std::string> DiagnosticLogger::GenerateActionableSuggestions(const CrashReport& report) {
        std::vector<std::string> suggestions;
        
        // Category-specific actionable suggestions
        if (report.category == "Mesh") {
            suggestions.push_back("ACTION: Open NifSkope and load the problematic mesh file");
            suggestions.push_back("ACTION: Check for red warnings or errors in NifSkope");
            suggestions.push_back("ACTION: Run NIF Optimizer on the mesh file");
            
            if (!report.suspectedMods.empty()) {
                suggestions.push_back(fmt::format("ACTION: Reinstall {} from Nexus Mods", 
                                                 report.suspectedMods[0]));
            }
        } else if (report.category == "Animation") {
            suggestions.push_back("ACTION: Run FNIS/Nemesis to regenerate animations");
            suggestions.push_back("ACTION: Check Data/meshes/actors/character/animations for conflicts");
            suggestions.push_back("ACTION: Verify skeleton.nif matches your character skeleton");
        } else if (report.category == "Script") {
            suggestions.push_back("ACTION: Open Data/SKSE/Plugins/Papyrus.0.log");
            suggestions.push_back("ACTION: Search for the script name in the log");
            suggestions.push_back("ACTION: Create a clean save (remove script-heavy mods, save, re-add)");
        } else if (report.category == "Memory") {
            suggestions.push_back("ACTION: Close Skyrim and restart it");
            suggestions.push_back("ACTION: Lower texture quality in Skyrim Launcher");
            suggestions.push_back("ACTION: Install SSE Engine Fixes from Nexus Mods");
        } else if (report.category == "Cell") {
            suggestions.push_back("ACTION: Use xEdit to check for cell conflicts");
            suggestions.push_back("ACTION: Rebuild precombined meshes with SSELODGen");
            suggestions.push_back("ACTION: Check for navmesh conflicts in the cell");
        } else if (report.category == "AI") {
            suggestions.push_back("ACTION: Use xEdit to check AI package records");
            suggestions.push_back("ACTION: Look for AI overhaul mods in your load order");
            suggestions.push_back("ACTION: Disable AI mods one at a time to isolate the issue");
        } else if (report.category == "GridBoundary") {
            suggestions.push_back("ACTION: Adjust uGridsToLoad in Skyrim.ini (default is 5)");
            suggestions.push_back("ACTION: Check for NPCs with AI packages crossing cell boundaries");
            suggestions.push_back("ACTION: Install grid boundary fix mods");
        }
        
        // Confidence-based suggestions
        if (report.confidence >= 0.8f) {
            suggestions.insert(suggestions.begin(), 
                             "HIGH CONFIDENCE: The identified cause is very likely correct");
        } else if (report.confidence < 0.5f) {
            suggestions.insert(suggestions.begin(),
                             "LOW CONFIDENCE: Multiple possible causes - try fixes one at a time");
        }
        
        return suggestions;
    }

    std::string DiagnosticLogger::LinkToKnownIssues(const CrashReport& report) {
        // Try to match the crash to known issues
        for (const auto& issue : s_knownIssues) {
            // Simple keyword matching - could be more sophisticated
            if (report.rootCause.find(issue.description) != std::string::npos ||
                report.category.find(issue.signature) != std::string::npos) {
                
                std::ostringstream ss;
                ss << "KNOWN ISSUE DETECTED\n";
                ss << std::string(40, '-') << "\n";
                ss << "Issue: " << issue.description << "\n";
                ss << "More info: " << issue.linkUrl << "\n\n";
                ss << "Recommended fixes:\n";
                for (size_t i = 0; i < issue.fixes.size(); ++i) {
                    ss << (i + 1) << ". " << issue.fixes[i] << "\n";
                }
                return ss.str();
            }
        }
        
        return "";
    }

    std::vector<std::string> DiagnosticLogger::SuggestConfigurationChanges(const CrashReport& report) {
        std::vector<std::string> configSuggestions;
        
        // Memory-related configuration
        if (report.category == "Memory" || report.performance.memoryUsageMB > 8000) {
            configSuggestions.push_back("CONFIG: Add 'DefaultHeapInitialAllocMB=1024' to Skyrim.ini [General]");
            configSuggestions.push_back("CONFIG: Set 'iMaxAllocatedMemoryBytes=4096' in enblocal.ini");
        }
        
        // Performance-related configuration
        if (report.performance.fps < 30.0f) {
            configSuggestions.push_back("CONFIG: Lower 'iShadowMapResolution' in SkyrimPrefs.ini");
            configSuggestions.push_back("CONFIG: Disable 'bDrawLandShadows' in Skyrim.ini");
        }
        
        // Grid boundary issues
        if (report.category == "GridBoundary") {
            configSuggestions.push_back("CONFIG: Reset 'uGridsToLoad=5' in Skyrim.ini [General]");
            configSuggestions.push_back("CONFIG: Avoid changing uGridsToLoad mid-playthrough");
        }
        
        // Script-heavy load
        if (report.category == "Script") {
            configSuggestions.push_back("CONFIG: Increase 'iMaxAllocatedMemoryBytes' in Skyrim.ini");
            configSuggestions.push_back("CONFIG: Set 'bEnableLogging=0' in Skyrim.ini [Papyrus] for performance");
        }
        
        // Mod count
        if (report.performance.loadedMods > 200) {
            configSuggestions.push_back("CONFIG: Consider merging plugins with zMerge");
            configSuggestions.push_back("CONFIG: Convert some ESPs to ESL format");
        }
        
        return configSuggestions;
    }

    std::string DiagnosticLogger::GenerateCompleteSuggestedFixes(const CrashReport& report) {
        std::ostringstream ss;
        
        ss << "SUGGESTED FIXES AND ACTIONS\n";
        ss << std::string(80, '=') << "\n\n";
        
        // Immediate actions
        ss << "IMMEDIATE ACTIONS\n";
        ss << std::string(40, '-') << "\n";
        auto actionable = GenerateActionableSuggestions(report);
        for (size_t i = 0; i < actionable.size(); ++i) {
            ss << (i + 1) << ". " << actionable[i] << "\n";
        }
        ss << "\n";
        
        // Known issues
        std::string knownIssue = LinkToKnownIssues(report);
        if (!knownIssue.empty()) {
            ss << knownIssue << "\n";
        }
        
        // Configuration changes
        auto configChanges = SuggestConfigurationChanges(report);
        if (!configChanges.empty()) {
            ss << "CONFIGURATION CHANGES\n";
            ss << std::string(40, '-') << "\n";
            for (size_t i = 0; i < configChanges.size(); ++i) {
                ss << (i + 1) << ". " << configChanges[i] << "\n";
            }
            ss << "\n";
        }
        
        // General suggestions from report
        if (!report.suggestedFixes.empty()) {
            ss << "GENERAL SUGGESTIONS\n";
            ss << std::string(40, '-') << "\n";
            for (size_t i = 0; i < report.suggestedFixes.size(); ++i) {
                ss << (i + 1) << ". " << report.suggestedFixes[i] << "\n";
            }
            ss << "\n";
        }
        
        // Community resources
        ss << "COMMUNITY RESOURCES\n";
        ss << std::string(40, '-') << "\n";
        ss << "1. Nexus Mods Wiki: https://wiki.nexusmods.com/\n";
        ss << "2. r/skyrimmods: https://www.reddit.com/r/skyrimmods/\n";
        ss << "3. STEP Guide: https://stepmodifications.org/\n";
        ss << "4. Skyrim Modding Discord communities\n";
        ss << "\n";
        
        // Pattern-based advice
        if (report.similarCrashCount > 3) {
            ss << "PATTERN DETECTED\n";
            ss << std::string(40, '-') << "\n";
            ss << "This crash has occurred " << report.similarCrashCount << " times.\n";
            ss << "Consider addressing the root cause to prevent future occurrences.\n";
            ss << "\n";
        }
        
        return ss.str();
    }

    void DiagnosticLogger::WriteSuggestedFixesReport(const CrashReport& report, const std::string& filename) {
        if (!s_initialized) {
            return;
        }
        
        std::string fixes = GenerateCompleteSuggestedFixes(report);
        
        std::string outputFilename = filename.empty()
            ? s_logDirectory + "/fixes_" + report.reportId + ".txt"
            : filename;
        
        WriteToFile(fixes, outputFilename);
        
        Log(LogSeverity::Info, LogCategory::System, "SuggestedFixes",
            "Wrote suggested fixes report",
            {{ "reportId", report.reportId },
             { "filename", outputFilename }});
    }

    void DiagnosticLogger::EnhanceSuggestedFixes(CrashReport& report) {
        // Enhance the suggested fixes in the report with more detailed information
        auto actionable = GenerateActionableSuggestions(report);
        auto configChanges = SuggestConfigurationChanges(report);
        
        // Combine all suggestions
        std::vector<std::string> enhanced = report.suggestedFixes;
        enhanced.insert(enhanced.end(), actionable.begin(), actionable.end());
        enhanced.insert(enhanced.end(), configChanges.begin(), configChanges.end());
        
        // Remove duplicates
        std::sort(enhanced.begin(), enhanced.end());
        enhanced.erase(std::unique(enhanced.begin(), enhanced.end()), enhanced.end());
        
        report.suggestedFixes = enhanced;
        
        // Write dedicated fixes report
        WriteSuggestedFixesReport(report);
        
        Log(LogSeverity::Info, LogCategory::System, "SuggestedFixes",
            "Enhanced suggested fixes",
            {{ "reportId", report.reportId },
             { "fixCount", std::to_string(enhanced.size()) }});
    }

}  // namespace Diagnostics
