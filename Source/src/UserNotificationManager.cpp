// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "UserNotificationManager.h"
#include "DiagnosticLogger.h"
#include "SeverityAnalyzer.h"
#include "VEH.h"
#include "CellManager.h"
#include <spdlog/spdlog.h>
#include <Windows.h>
#include <shellapi.h>
#include <thread>
#include <future>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>

namespace UserNotifications {

    // Static member initialization
    bool UserNotificationManager::s_initialized = false;
    bool UserNotificationManager::s_enabled = true;
    NotificationOptions UserNotificationManager::s_defaultOptions = {};
    bool UserNotificationManager::s_autoRecoverSafe = true;
    bool UserNotificationManager::s_autoRecoverWarning = false;
    std::vector<BatchedNotification> UserNotificationManager::s_batchedNotifications;
    std::chrono::steady_clock::time_point UserNotificationManager::s_lastNotificationTime;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> UserNotificationManager::s_recentNotifications;
    uint32_t UserNotificationManager::s_notificationCount = 0;
    uint32_t UserNotificationManager::s_userInteractionCount = 0;
    std::vector<std::string> UserNotificationManager::s_userChoiceHistory;
    std::mutex UserNotificationManager::s_notificationMutex;

    bool UserNotificationManager::Initialize() {
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        
        if (s_initialized) {
            return true;
        }

        spdlog::info("UserNotificationManager: Initializing...");
        
        // Set default options
        s_defaultOptions.showFileDetails = true;
        s_defaultOptions.showQuickFixes = true;
        s_defaultOptions.showTechnicalDetails = false;
        s_defaultOptions.allowCrashAnyway = false;
        s_defaultOptions.timeoutSeconds = 30;
        s_defaultOptions.batchSimilar = true;
        
        s_initialized = true;
        spdlog::info("UserNotificationManager: Initialized successfully");
        return true;
    }


    void UserNotificationManager::Shutdown() {
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        
        if (!s_initialized) {
            return;
        }

        spdlog::info("UserNotificationManager: Shutting down...");
        
        // Flush any remaining batched notifications
        if (!s_batchedNotifications.empty()) {
            spdlog::warn("UserNotificationManager: {} batched notifications not flushed", 
                        s_batchedNotifications.size());
        }
        
        s_batchedNotifications.clear();
        s_recentNotifications.clear();
        s_userChoiceHistory.clear();
        
        s_initialized = false;
        spdlog::info("UserNotificationManager: Shutdown complete");
    }

    UserChoice UserNotificationManager::ShowCrashNotification(
        const Diagnostics::CrashReport& report,
        const NotificationOptions& options) {
        
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        
        if (!s_initialized || !s_enabled) {
            spdlog::warn("UserNotificationManager: Not initialized or disabled");
            return UserChoice::Continue;
        }

        s_notificationCount++;
        
        // Auto-recover for safe crashes if configured
        if (report.category == "Safe" && s_autoRecoverSafe) {
            spdlog::info("UserNotificationManager: Auto-recovering safe crash");
            return UserChoice::Continue;
        }
        
        // Auto-recover for warning crashes if configured
        if (report.category == "Warning" && s_autoRecoverWarning) {
            spdlog::info("UserNotificationManager: Auto-recovering warning crash");
            return UserChoice::Continue;
        }

        // Format message and generate choices
        std::string message = FormatCrashMessage(report, options);
        std::vector<std::string> choices = GenerateChoices(report, options);
        
        // Show MessageBox with timeout
        int result = ShowMessageBox("Skyrim Crash Guard - Crash Detected", message, choices, options.timeoutSeconds);
        
        UserChoice choice = ConvertToUserChoice(result, choices);
        
        s_userInteractionCount++;
        s_userChoiceHistory.push_back(std::to_string(static_cast<int>(choice)));
        
        spdlog::info("UserNotificationManager: User selected choice: {}", static_cast<int>(choice));
        
        return choice;
    }


    std::string UserNotificationManager::FormatCrashMessage(
        const Diagnostics::CrashReport& report,
        const NotificationOptions& options) {
        
        std::ostringstream oss;
        
        // Header
        oss << "Skyrim Crash Guard has detected and analyzed a crash.\n\n";
        
        // Quick summary
        oss << "CRASH TYPE: " << report.category << "\n";
        oss << "ROOT CAUSE: " << report.rootCause << "\n";
        
        if (report.confidence > 0.0f) {
            oss << "CONFIDENCE: " << std::fixed << std::setprecision(0) 
                << (report.confidence * 100.0f) << "%\n";
        }
        
        oss << "\n";
        
        // Recovery status
        if (report.recoverySuccessful) {
            oss << "RECOVERY: Successful (" << report.recoveryStrategy << ")\n";
            if (!report.recoveryActions.empty()) {
                oss << "Actions: " << report.recoveryActions[0];
                if (report.recoveryActions.size() > 1) {
                    oss << " (+" << (report.recoveryActions.size() - 1) << " more)";
                }
                oss << "\n";
            }
        } else {
            oss << "RECOVERY: Failed or not attempted\n";
        }
        
        oss << "\n";
        
        // File details
        if (options.showFileDetails && !report.suspectedMods.empty()) {
            oss << FormatFileDetails(report.suspectedMods, report.category);
            oss << "\n";
        }
        
        // Quick fixes
        if (options.showQuickFixes && !report.suggestedFixes.empty()) {
            oss << FormatQuickFixes(report.suggestedFixes);
            oss << "\n";
        }
        
        // Technical details
        if (options.showTechnicalDetails) {
            oss << FormatTechnicalDetails(report);
            oss << "\n";
        }
        
        // Pattern info
        if (report.similarCrashCount > 1) {
            oss << "NOTE: This crash pattern has occurred " << report.similarCrashCount 
                << " times in this session.\n";
        }
        
        if (!report.communityStatus.empty()) {
            oss << "COMMUNITY: " << report.communityStatus << "\n";
        }
        
        return oss.str();
    }


    std::vector<std::string> UserNotificationManager::GenerateChoices(
        const Diagnostics::CrashReport& report,
        const NotificationOptions& options) {
        
        std::vector<std::string> choices;
        
        // Always offer Continue if recovery was successful
        if (report.recoverySuccessful) {
            choices.push_back("Continue Playing");
        }
        
        // Offer Load Last Save for critical/fatal crashes
        if (report.category == "Critical" || report.category == "Fatal") {
            choices.push_back("Load Last Save");
        }
        
        // Offer Teleport to Safe Cell for location-based crashes
        if (report.category == "Cell" || report.rootCause.find("cell") != std::string::npos ||
            report.rootCause.find("grid") != std::string::npos) {
            choices.push_back("Teleport to Safe Cell");
        }
        
        // Always offer View Log
        choices.push_back("View Crash Log");
        
        // Offer Crash Anyway if allowed (for debugging)
        if (options.allowCrashAnyway) {
            choices.push_back("Crash Anyway");
        }
        
        // Default to Continue if no other options
        if (choices.empty()) {
            choices.push_back("Continue");
        }
        
        return choices;
    }

    std::string UserNotificationManager::FormatFileDetails(
        const std::vector<std::string>& suspectedMods,
        const std::string& category) {
        
        std::ostringstream oss;
        oss << "SUSPECTED MOD(S):\n";
        
        for (size_t i = 0; i < std::min(suspectedMods.size(), size_t(3)); ++i) {
            oss << "  " << (i + 1) << ". " << suspectedMods[i] << "\n";
        }
        
        if (suspectedMods.size() > 3) {
            oss << "  ... and " << (suspectedMods.size() - 3) << " more\n";
        }
        
        return oss.str();
    }

    std::string UserNotificationManager::FormatQuickFixes(
        const std::vector<std::string>& fixes) {
        
        std::ostringstream oss;
        oss << "SUGGESTED FIXES:\n";
        
        for (size_t i = 0; i < std::min(fixes.size(), size_t(3)); ++i) {
            oss << "  - " << fixes[i] << "\n";
        }
        
        if (fixes.size() > 3) {
            oss << "  ... and " << (fixes.size() - 3) << " more (see log)\n";
        }
        
        return oss.str();
    }


    std::string UserNotificationManager::FormatTechnicalDetails(
        const Diagnostics::CrashReport& report) {
        
        std::ostringstream oss;
        oss << "TECHNICAL DETAILS:\n";
        oss << "  Exception: 0x" << std::hex << std::uppercase << report.exceptionCode << std::dec << "\n";
        oss << "  Address: " << report.crashAddress << "\n";
        oss << "  Signature: " << report.crashSignature << "\n";
        oss << "  Report ID: " << report.reportId << "\n";
        
        return oss.str();
    }

    int UserNotificationManager::ShowMessageBox(
        const std::string& title,
        const std::string& message,
        const std::vector<std::string>& buttons,
        uint32_t timeoutSeconds) {
        
        // Win32 MessageBox with async timeout. We run it in a separate thread so
        // the timeout can close the dialog if the user doesn't respond in time.
        // MB_SYSTEMMODAL keeps the dialog on top during a crash event.
        UINT type = MB_ICONWARNING | MB_SYSTEMMODAL | MB_SETFOREGROUND;
        
        // Map button count to MessageBox types
        if (buttons.size() == 1) {
            type |= MB_OK;
        } else if (buttons.size() == 2) {
            type |= MB_YESNO;
        } else if (buttons.size() == 3) {
            type |= MB_ABORTRETRYIGNORE;
        } else {
            type |= MB_OK;
        }
        
        // Create a future for timeout handling
        auto future = std::async(std::launch::async, [&]() {
            return MessageBoxA(nullptr, message.c_str(), title.c_str(), type);
        });
        
        // Wait with timeout
        if (future.wait_for(std::chrono::seconds(timeoutSeconds)) == std::future_status::timeout) {
            spdlog::warn("UserNotificationManager: MessageBox timed out after {} seconds", timeoutSeconds);
            return IDOK; // Default to first choice on timeout
        }
        
        return future.get();
    }

    UserChoice UserNotificationManager::ConvertToUserChoice(
        int result,
        const std::vector<std::string>& choices) {
        
        // Handle timeout
        if (result == IDOK && choices.empty()) {
            return UserChoice::Timeout;
        }
        
        // Map MessageBox results to UserChoice
        // This is a simplified mapping - full implementation would need custom dialog
        if (result == IDYES || result == IDOK) {
            // First choice
            if (!choices.empty()) {
                if (choices[0].find("Continue") != std::string::npos) {
                    return UserChoice::Continue;
                } else if (choices[0].find("Load") != std::string::npos) {
                    return UserChoice::LoadLastSave;
                }
            }
            return UserChoice::Continue;
        } else if (result == IDNO) {
            // Second choice
            if (choices.size() > 1) {
                if (choices[1].find("Load") != std::string::npos) {
                    return UserChoice::LoadLastSave;
                } else if (choices[1].find("Teleport") != std::string::npos) {
                    return UserChoice::TeleportToSafe;
                } else if (choices[1].find("View") != std::string::npos) {
                    return UserChoice::ViewLog;
                }
            }
            return UserChoice::ViewLog;
        } else if (result == IDCANCEL || result == IDABORT) {
            return UserChoice::CrashAnyway;
        }
        
        return UserChoice::Continue;
    }


    void UserNotificationManager::BatchNotification(
        const std::string& message,
        NotificationSeverity severity,
        const std::string& correlationId) {
        
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        
        if (!s_initialized || !s_enabled) {
            return;
        }

        // Check if batching is enabled
        if (!s_defaultOptions.batchSimilar) {
            // Show immediately
            ShowNotification("Skyrim Crash Guard", message, severity);
            return;
        }
        
        // Check if should batch
        if (!ShouldBatch(message, severity)) {
            ShowNotification("Skyrim Crash Guard", message, severity);
            return;
        }
        
        // Add to batch
        BatchedNotification notification;
        notification.message = message;
        notification.severity = severity;
        notification.timestamp = std::chrono::steady_clock::now();
        notification.correlationId = correlationId;
        
        s_batchedNotifications.push_back(notification);
        
        spdlog::debug("UserNotificationManager: Batched notification (total: {})", 
                     s_batchedNotifications.size());
    }

    UserChoice UserNotificationManager::FlushBatchedNotifications() {
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        
        if (!s_initialized || !s_enabled || s_batchedNotifications.empty()) {
            return UserChoice::Continue;
        }

        std::string message = FormatBatchedNotifications();
        
        // Determine highest severity
        NotificationSeverity maxSeverity = NotificationSeverity::Info;
        for (const auto& notif : s_batchedNotifications) {
            if (static_cast<int>(notif.severity) > static_cast<int>(maxSeverity)) {
                maxSeverity = notif.severity;
            }
        }
        
        UserChoice choice = ShowNotification(
            "Skyrim Crash Guard - Multiple Events",
            message,
            maxSeverity
        );
        
        s_batchedNotifications.clear();
        
        return choice;
    }

    std::string UserNotificationManager::FormatBatchedNotifications() {
        std::ostringstream oss;
        
        oss << "Skyrim Crash Guard has detected " << s_batchedNotifications.size() 
            << " events:\n\n";
        
        // Group by severity
        std::map<NotificationSeverity, std::vector<std::string>> grouped;
        for (const auto& notif : s_batchedNotifications) {
            grouped[notif.severity].push_back(notif.message);
        }
        
        // Format by severity
        for (const auto& [severity, messages] : grouped) {
            std::string severityStr;
            switch (severity) {
                case NotificationSeverity::Info: severityStr = "INFO"; break;
                case NotificationSeverity::Warning: severityStr = "WARNING"; break;
                case NotificationSeverity::Error: severityStr = "ERROR"; break;
                case NotificationSeverity::Critical: severityStr = "CRITICAL"; break;
            }
            
            oss << severityStr << " (" << messages.size() << "):\n";
            for (size_t i = 0; i < std::min(messages.size(), size_t(5)); ++i) {
                oss << "  - " << messages[i] << "\n";
            }
            if (messages.size() > 5) {
                oss << "  ... and " << (messages.size() - 5) << " more\n";
            }
            oss << "\n";
        }
        
        return oss.str();
    }


    bool UserNotificationManager::ShouldBatch(
        const std::string& message,
        NotificationSeverity severity) {
        
        // Don't batch critical notifications
        if (severity == NotificationSeverity::Critical) {
            return false;
        }
        
        // Don't batch if too much time has passed since last notification
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastNotif = std::chrono::duration_cast<std::chrono::seconds>(
            now - s_lastNotificationTime).count();
        
        if (timeSinceLastNotif > 60) {
            return false;
        }
        
        // Batch if we already have notifications queued
        return !s_batchedNotifications.empty();
    }

    bool UserNotificationManager::IsSpamPrevented(const std::string& messageHash) {
        auto now = std::chrono::steady_clock::now();
        
        auto it = s_recentNotifications.find(messageHash);
        if (it != s_recentNotifications.end()) {
            auto timeSince = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second).count();
            
            // Prevent same notification within 10 seconds
            if (timeSince < 10) {
                return true;
            }
        }
        
        return false;
    }

    void UserNotificationManager::RecordNotification(const std::string& messageHash) {
        s_recentNotifications[messageHash] = std::chrono::steady_clock::now();
        s_lastNotificationTime = std::chrono::steady_clock::now();
        
        // Clean up old entries (older than 60 seconds)
        auto now = std::chrono::steady_clock::now();
        for (auto it = s_recentNotifications.begin(); it != s_recentNotifications.end();) {
            auto timeSince = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second).count();
            if (timeSince > 60) {
                it = s_recentNotifications.erase(it);
            } else {
                ++it;
            }
        }
    }

    UserChoice UserNotificationManager::ShowNotification(
        const std::string& title,
        const std::string& message,
        NotificationSeverity severity,
        const std::vector<std::string>& choices) {
        
        if (!s_initialized || !s_enabled) {
            return UserChoice::Continue;
        }

        // Check spam prevention
        std::hash<std::string> hasher;
        std::string messageHash = std::to_string(hasher(message));
        
        if (IsSpamPrevented(messageHash)) {
            spdlog::debug("UserNotificationManager: Notification spam prevented");
            return UserChoice::Continue;
        }
        
        RecordNotification(messageHash);
        
        s_notificationCount++;
        
        // Determine MessageBox type based on severity
        UINT type = MB_SYSTEMMODAL | MB_SETFOREGROUND;
        switch (severity) {
            case NotificationSeverity::Info:
                type |= MB_ICONINFORMATION;
                break;
            case NotificationSeverity::Warning:
                type |= MB_ICONWARNING;
                break;
            case NotificationSeverity::Error:
            case NotificationSeverity::Critical:
                type |= MB_ICONERROR;
                break;
        }
        
        // Add buttons
        if (choices.empty()) {
            type |= MB_OK;
        } else if (choices.size() == 1) {
            type |= MB_OK;
        } else if (choices.size() == 2) {
            type |= MB_YESNO;
        } else {
            type |= MB_ABORTRETRYIGNORE;
        }
        
        int result = ShowMessageBox(title, message, choices, s_defaultOptions.timeoutSeconds);
        
        UserChoice choice = ConvertToUserChoice(result, choices);
        
        s_userInteractionCount++;
        s_userChoiceHistory.push_back(std::to_string(static_cast<int>(choice)));
        
        return choice;
    }


    bool UserNotificationManager::ConfirmSaveWithCorruption(
        const std::string& corruptionDetails) {
        
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        
        if (!s_initialized || !s_enabled) {
            return false;
        }

        std::ostringstream oss;
        oss << "WARNING: Your game session may be corrupted!\n\n";
        oss << corruptionDetails << "\n\n";
        oss << "Saving now may corrupt your save file.\n";
        oss << "Do you want to save anyway?";
        
        UINT type = MB_YESNO | MB_ICONWARNING | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_DEFBUTTON2;
        
        int result = MessageBoxA(nullptr, oss.str().c_str(), 
                                "Skyrim Crash Guard - Save Warning", type);
        
        bool confirmed = (result == IDYES);
        
        spdlog::info("UserNotificationManager: Save with corruption {}", 
                    confirmed ? "confirmed" : "cancelled");
        
        return confirmed;
    }

    void UserNotificationManager::ShowRecoverySuccess(
        const std::string& recoveryDetails,
        const std::vector<std::string>& actionsPerformed) {
        
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        
        if (!s_initialized || !s_enabled) {
            return;
        }

        std::ostringstream oss;
        oss << "Crash successfully recovered!\n\n";
        oss << recoveryDetails << "\n\n";
        
        if (!actionsPerformed.empty()) {
            oss << "Actions performed:\n";
            for (size_t i = 0; i < std::min(actionsPerformed.size(), size_t(3)); ++i) {
                oss << "  - " << actionsPerformed[i] << "\n";
            }
            if (actionsPerformed.size() > 3) {
                oss << "  ... and " << (actionsPerformed.size() - 3) << " more\n";
            }
        }
        
        BatchNotification(oss.str(), NotificationSeverity::Info, "recovery_success");
    }

    void UserNotificationManager::ShowPreventionNotification(
        const std::string& preventedIssue,
        const std::string& suspectedMod) {
        
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        
        if (!s_initialized || !s_enabled) {
            return;
        }

        std::ostringstream oss;
        oss << "Crash prevented: " << preventedIssue << "\n";
        if (!suspectedMod.empty()) {
            oss << "Suspected mod: " << suspectedMod;
        }
        
        BatchNotification(oss.str(), NotificationSeverity::Info, "crash_prevention");
    }

    void UserNotificationManager::ShowFileConflictAnalysis(
        const std::vector<std::string>& conflictingMods,
        const std::string& analysisDetails) {
        
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        
        if (!s_initialized || !s_enabled) {
            return;
        }

        std::ostringstream oss;
        oss << "File conflict detected:\n\n";
        oss << analysisDetails << "\n\n";
        oss << "Conflicting mods:\n";
        for (const auto& mod : conflictingMods) {
            oss << "  - " << mod << "\n";
        }
        
        ShowNotification("Skyrim Crash Guard - Conflict Detected", 
                        oss.str(), 
                        NotificationSeverity::Warning);
    }


    void UserNotificationManager::ShowPatternLearningUpdate(
        const std::string& patternName,
        uint32_t occurrences,
        const std::string& recommendedAction) {
        
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        
        if (!s_initialized || !s_enabled) {
            return;
        }

        std::ostringstream oss;
        oss << "Pattern learned: " << patternName << "\n";
        oss << "Occurrences: " << occurrences << "\n";
        if (!recommendedAction.empty()) {
            oss << "Recommended: " << recommendedAction;
        }
        
        BatchNotification(oss.str(), NotificationSeverity::Info, "pattern_learning");
    }

    void UserNotificationManager::SetDefaultOptions(const NotificationOptions& options) {
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        s_defaultOptions = options;
    }

    void UserNotificationManager::SetEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        s_enabled = enabled;
        spdlog::info("UserNotificationManager: {}", enabled ? "Enabled" : "Disabled");
    }

    void UserNotificationManager::SetAutoRecoverSafe(bool autoRecover) {
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        s_autoRecoverSafe = autoRecover;
    }

    void UserNotificationManager::SetAutoRecoverWarning(bool autoRecover) {
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        s_autoRecoverWarning = autoRecover;
    }

    uint32_t UserNotificationManager::GetNotificationCount() {
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        return s_notificationCount;
    }

    uint32_t UserNotificationManager::GetUserInteractionCount() {
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        return s_userInteractionCount;
    }

    std::vector<std::string> UserNotificationManager::GetMostCommonChoices() {
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        
        // Count occurrences of each choice
        std::map<std::string, uint32_t> choiceCounts;
        for (const auto& choice : s_userChoiceHistory) {
            choiceCounts[choice]++;
        }
        
        // Sort by count
        std::vector<std::pair<std::string, uint32_t>> sorted(choiceCounts.begin(), choiceCounts.end());
        std::sort(sorted.begin(), sorted.end(), 
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        // Return top choices
        std::vector<std::string> result;
        for (const auto& [choice, count] : sorted) {
            result.push_back(choice + " (" + std::to_string(count) + ")");
        }
        
        return result;
    }

    void UserNotificationManager::OpenCrashLog(const std::string& reportId) {
        // Construct log file path
        std::string logPath = "Data/SKSE/Plugins/CrashGuard/Logs/crash_" + reportId + ".log";
        
        // Open with default application
        ShellExecuteA(nullptr, "open", logPath.c_str(), nullptr, nullptr, SW_SHOW);
        
        spdlog::info("UserNotificationManager: Opened crash log: {}", reportId);
    }

    void UserNotificationManager::TeleportToSafeCell() {
        spdlog::info("UserNotificationManager: Teleport to safe cell requested");
        
        try {
            // Get player reference
            auto player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                spdlog::error("UserNotificationManager: Failed to get player reference");
                return;
            }
            
            // Use CellManager to teleport to safe cell
            CellValidation::CellManager::TeleportToSafeCell(player);
            
            spdlog::info("UserNotificationManager: Player teleported to safe cell");
            
        } catch (const std::exception& e) {
            spdlog::error("UserNotificationManager: Exception during teleport: {}", e.what());
        }
    }

    // Convenience functions
    namespace QuickNotify {
        void CrashPrevented(const std::string& issue, const std::string& mod) {
            UserNotificationManager::ShowPreventionNotification(issue, mod);
        }

        void RecoverySuccessful(const std::string& strategy) {
            std::vector<std::string> actions = { strategy };
            UserNotificationManager::ShowRecoverySuccess("Recovery completed", actions);
        }

        void ModConflictDetected(const std::vector<std::string>& mods) {
            UserNotificationManager::ShowFileConflictAnalysis(mods, "Multiple mods modifying same resources");
        }

        void PerformanceWarning(const std::string& issue, const std::string& suggestion) {
            std::string message = "Performance issue: " + issue + "\nSuggestion: " + suggestion;
            UserNotificationManager::BatchNotification(message, NotificationSeverity::Warning, "performance");
        }

        void PatternLearned(const std::string& pattern, const std::string& solution) {
            UserNotificationManager::ShowPatternLearningUpdate(pattern, 1, solution);
        }
    }

    // ========================================================================
    // Enhanced Dialog System
    // ========================================================================

    std::string UserNotificationManager::FormatDetailedCrashDialog(
        const CrashGuard::SeverityAnalysis& analysis,
        const Diagnostics::CrashReport& report,
        const NotificationOptions& options) {
        
        std::ostringstream oss;
        
        // Helper function to get severity icon
        auto getSeverityIcon = [](VEH::SeverityLevel level) -> std::string {
            switch (level) {
                case VEH::SeverityLevel::Safe: return "[OK]";
                case VEH::SeverityLevel::Warning: return "[WARN]";
                case VEH::SeverityLevel::Critical: return "[CRIT]";
                case VEH::SeverityLevel::Fatal: return "[FATAL]";
                default: return "[?]";
            }
        };
        
        // Helper function to get severity name
        auto getSeverityName = [](VEH::SeverityLevel level) -> std::string {
            switch (level) {
                case VEH::SeverityLevel::Safe: return "SAFE";
                case VEH::SeverityLevel::Warning: return "WARNING";
                case VEH::SeverityLevel::Critical: return "CRITICAL";
                case VEH::SeverityLevel::Fatal: return "FATAL";
                default: return "UNKNOWN";
            }
        };
        
        // Header with severity
        oss << getSeverityIcon(analysis.level) << " ";
        oss << getSeverityName(analysis.level) << " CRASH ";
        oss << (report.recoverySuccessful ? "RECOVERED" : "DETECTED") << "\n\n";
        
        // Severity explanation section
        oss << "Severity: " << getSeverityName(analysis.level);
        if (!analysis.technicalReason.empty()) {
            oss << " (" << analysis.technicalReason << ")";
        }
        oss << "\n";
        oss << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
        
        // Why is this dangerous?
        oss << "Why is this " << getSeverityName(analysis.level) << "?\n";
        oss << analysis.userExplanation << "\n\n";
        
        oss << "Detection Method: " << analysis.detectionMethod << "\n";
        oss << "Confidence: " << static_cast<int>(analysis.confidenceScore * 100) << "%\n\n";
        
        oss << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
        
        // Crash details
        oss << "Crash Details:\n";
        if (!report.gameState.playerLocation.empty()) {
            oss << "• Location: " << report.gameState.playerLocation << "\n";
        }
        if (!report.gameState.currentActivity.empty()) {
            oss << "• Activity: " << report.gameState.currentActivity << "\n";
        }
        oss << "• Suspected Cause: " << report.rootCause << "\n";
        if (!report.suspectedMods.empty()) {
            oss << "• Involved Mod: " << report.suspectedMods[0] << "\n";
        }
        oss << "\n";
        
        // Recovery information
        if (report.recoverySuccessful) {
            oss << "Recovery Applied:\n";
            oss << "• Strategy: " << report.recoveryStrategy << "\n";
            if (!report.recoveryActions.empty()) {
                oss << "• Actions: " << report.recoveryActions[0];
                if (report.recoveryActions.size() > 1) {
                    oss << " (+" << (report.recoveryActions.size() - 1) << " more)";
                }
                oss << "\n";
            }
            oss << "\n";
        }
        
        // Risks
        if (!analysis.risks.empty()) {
            oss << "Risks if you continue:\n";
            for (const auto& risk : analysis.risks) {
                oss << "  " << risk << "\n";
            }
            oss << "\n";
        }
        
        // Recommendation
        if (!analysis.recommendation.empty()) {
            oss << "Recommendation: " << analysis.recommendation << "\n\n";
        }
        
        // Technical details (if enabled)
        if (options.showTechnicalDetails) {
            oss << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
            oss << "TECHNICAL DETAILS:\n";
            oss << "  Exception: 0x" << std::hex << std::uppercase << report.exceptionCode << std::dec << "\n";
            oss << "  Address: " << report.crashAddress << "\n";
            oss << "  Report ID: " << report.reportId << "\n";
        }
        
        return oss.str();
    }

    std::vector<std::string> UserNotificationManager::GenerateChoiceButtons(
        const CrashGuard::SeverityAnalysis& analysis,
        bool recoverySuccessful,
        const NotificationOptions& options) {
        
        std::vector<std::string> choices;
        
        if (recoverySuccessful) {
            // Recovery was successful - offer continue option
            choices.push_back("Continue Playing");
            
            // Always offer load save for Critical/Fatal
            if (analysis.level == VEH::SeverityLevel::Critical || 
                analysis.level == VEH::SeverityLevel::Fatal) {
                choices.push_back("Load Last Save ⭐ RECOMMENDED");
            } else {
                choices.push_back("Load Last Save");
            }
            
            choices.push_back("Teleport to Safe Location");
            choices.push_back("View Full Technical Log");
            
            // Only show "Let It Crash" if configured
            if (options.allowCrashAnyway) {
                choices.push_back("Let It Crash");
            }
        } else {
            // Recovery failed - different options
            choices.push_back("Load Last Save ⭐ RECOMMENDED");
            choices.push_back("Teleport to Safe Location");
            choices.push_back("View Full Technical Log");
            
            if (options.allowCrashAnyway) {
                choices.push_back("Let It Crash");
            }
        }
        
        return choices;
    }

    // ========================================================================
    // User Choice Action Handlers
    // ========================================================================

    void UserNotificationManager::HandleUserChoice(
        UserChoice choice,
        const Diagnostics::CrashReport& report) {
        
        std::lock_guard<std::mutex> lock(s_notificationMutex);
        
        if (!s_initialized) {
            spdlog::error("UserNotificationManager: Cannot handle choice - not initialized");
            return;
        }

        spdlog::info("UserNotificationManager: Handling user choice: {}", static_cast<int>(choice));
        
        switch (choice) {
            case UserChoice::Continue:
                HandleContinue(report);
                break;
                
            case UserChoice::CrashAnyway:
                HandleCrashAnyway(report);
                break;
                
            case UserChoice::LoadLastSave:
                HandleLoadLastSave(report);
                break;
                
            case UserChoice::TeleportToSafe:
                HandleTeleportToSafe(report);
                break;
                
            case UserChoice::ViewLog:
                HandleViewLog(report);
                break;
                
            case UserChoice::RestartGame:
                HandleRestartGame(report);
                break;
                
            case UserChoice::Timeout:
                HandleTimeout(report);
                break;
                
            default:
                spdlog::warn("UserNotificationManager: Unknown choice: {}", static_cast<int>(choice));
                HandleContinue(report);  // Default to continue
                break;
        }
    }

    void UserNotificationManager::HandleContinue(const Diagnostics::CrashReport& report) {
        spdlog::info("UserNotificationManager: User chose to continue playing");
        
        // Log the decision
        spdlog::info("  Recovery strategy: {}", report.recoveryStrategy);
        spdlog::info("  Recovery successful: {}", report.recoverySuccessful);
        
        // Record user choice
        s_userChoiceHistory.push_back("Continue");
        
        // If recovery was successful, just continue
        if (report.recoverySuccessful) {
            spdlog::info("UserNotificationManager: Continuing with successful recovery");
        } else {
            spdlog::warn("UserNotificationManager: Continuing despite failed recovery - game may be unstable");
        }
    }

    void UserNotificationManager::HandleCrashAnyway(const Diagnostics::CrashReport& report) {
        spdlog::info("UserNotificationManager: User chose to crash anyway (debugging mode)");
        
        // Record user choice
        s_userChoiceHistory.push_back("CrashAnyway");
        
        // Log the crash details for debugging
        spdlog::info("  Exception code: 0x{:08X}", report.exceptionCode);
        spdlog::info("  Crash address: {}", report.crashAddress);
        spdlog::info("  Root cause: {}", report.rootCause);
        
        // In a full implementation, this would:
        // 1. Disable crash recovery temporarily
        // 2. Re-trigger the exception
        // 3. Allow normal crash to occur for debugging
        
        spdlog::warn("UserNotificationManager: Crash Anyway not fully implemented - continuing instead");
    }

    void UserNotificationManager::HandleLoadLastSave(const Diagnostics::CrashReport& report) {
        spdlog::info("[Notification] User chose to load last save");

        s_userChoiceHistory.push_back("LoadLastSave");

        try {
            // Verify BGSSaveLoadManager is accessible before queuing (fast check on
            // calling thread; the actual load must happen on the main game thread).
            auto* saveLoadManager = RE::BGSSaveLoadManager::GetSingleton();
            if (!saveLoadManager) {
                spdlog::error("[Notification] BGSSaveLoadManager singleton not available");
                return;
            }

            // BGSSaveLoadManager is not thread-safe — queue via SKSE task interface so
            // LoadMostRecentSaveGame() executes on the main thread during the next frame.
            auto* taskInterface = SKSE::GetTaskInterface();
            if (!taskInterface) {
                spdlog::error("[Notification] SKSE TaskInterface not available — cannot queue load");
                return;
            }

            taskInterface->AddTask([]() {
                auto* slm = RE::BGSSaveLoadManager::GetSingleton();
                if (!slm) {
                    spdlog::error("[Notification] BGSSaveLoadManager unavailable on main thread");
                    return;
                }
                spdlog::info("[Notification] Calling BGSSaveLoadManager::LoadMostRecentSaveGame()");
                const bool ok = slm->LoadMostRecentSaveGame();
                if (ok) {
                    spdlog::info("[Notification] LoadMostRecentSaveGame() returned true — load initiated");
                } else {
                    spdlog::warn("[Notification] LoadMostRecentSaveGame() returned false — no save found or load failed");
                }
            });

            spdlog::info("[Notification] Load-last-save task queued on main thread");

        } catch (const std::exception& e) {
            spdlog::error("[Notification] Exception while queuing load: {}", e.what());
        }
    }

    void UserNotificationManager::HandleTeleportToSafe(const Diagnostics::CrashReport& report) {
        spdlog::info("UserNotificationManager: User chose to teleport to safe cell");
        
        // Record user choice
        s_userChoiceHistory.push_back("TeleportToSafe");
        
        // Use the TeleportToSafeCell function
        TeleportToSafeCell();
        
        spdlog::info("UserNotificationManager: Teleport completed");
    }

    void UserNotificationManager::HandleViewLog(const Diagnostics::CrashReport& report) {
        spdlog::info("UserNotificationManager: User chose to view crash log");
        
        // Record user choice
        s_userChoiceHistory.push_back("ViewLog");
        
        // Open the crash log
        OpenCrashLog(report.reportId);
        
        spdlog::info("UserNotificationManager: Crash log opened");
    }

    void UserNotificationManager::HandleRestartGame(const Diagnostics::CrashReport& report) {
        spdlog::info("UserNotificationManager: User chose to restart game");
        
        // Record user choice
        s_userChoiceHistory.push_back("RestartGame");
        
        try {
            // In a full implementation, this would:
            // 1. Save current game state if safe
            // 2. Close Skyrim gracefully
            // 3. Restart the game executable
            
            spdlog::info("UserNotificationManager: Restart game requested");
            spdlog::warn("UserNotificationManager: Restart game not fully implemented");
            
        } catch (const std::exception& e) {
            spdlog::error("UserNotificationManager: Exception during restart: {}", e.what());
        }
    }

    void UserNotificationManager::HandleTimeout(const Diagnostics::CrashReport& report) {
        spdlog::info("UserNotificationManager: User choice timed out - using safe default");
        
        // Record user choice
        s_userChoiceHistory.push_back("Timeout");
        
        // Default to Continue for safe crashes, Load Last Save for critical/fatal
        if (report.category == "Critical" || report.category == "Fatal") {
            spdlog::info("UserNotificationManager: Timeout on critical crash - defaulting to Load Last Save");
            HandleLoadLastSave(report);
        } else {
            spdlog::info("UserNotificationManager: Timeout on non-critical crash - defaulting to Continue");
            HandleContinue(report);
        }
    }

}  // namespace UserNotifications
