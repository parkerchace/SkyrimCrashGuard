// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <mutex>

/// User Notification Manager for In-Game Crash Notifications
/// Displays rich, contextual MessageBox notifications with crash details
/// Integrates with DiagnosticLogger for comprehensive crash information
namespace UserNotifications {

    /// User choice from crash notification
    enum class UserChoice {
        Continue,           // Apply fixes and continue playing
        CrashAnyway,       // Allow normal crash (for debugging)
        LoadLastSave,      // Reload most recent save
        TeleportToSafe,    // Move to safe cell (Whiterun)
        ViewLog,           // Open crash log file
        RestartGame,       // Close and restart Skyrim
        Timeout            // User didn't respond in 30s
    };

    /// Notification severity levels
    enum class NotificationSeverity {
        Info,       // Informational (successful recovery)
        Warning,    // Warning (potential issue)
        Error,      // Error (recovery failed)
        Critical    // Critical (save corruption risk)
    };

    /// Notification display options
    struct NotificationOptions {
        bool showFileDetails = true;        // Show suspected files
        bool showQuickFixes = true;         // Show fix suggestions
        bool showTechnicalDetails = false;  // Show technical info
        bool allowCrashAnyway = false;      // Allow "Crash Anyway" option
        uint32_t timeoutSeconds = 30;       // Auto-timeout
        bool batchSimilar = true;           // Batch similar notifications
    };

    /// Batched notification entry
    struct BatchedNotification {
        std::string message;
        NotificationSeverity severity;
        std::chrono::steady_clock::time_point timestamp;
        std::string correlationId;
    };

    // Forward declaration for crash report from global Diagnostics namespace
    // (defined in DiagnosticLogger.h)
}

namespace Diagnostics {
    struct CrashReport;
}

namespace CrashGuard {
    struct SeverityAnalysis;
}

namespace UserNotifications {

    /// Main user notification manager
    class UserNotificationManager {
    public:
        /// Initialize the notification manager
        static bool Initialize();
        
        /// Shutdown and cleanup
        static void Shutdown();
        
        /// Display crash notification with rich context from DiagnosticLogger
        static UserChoice ShowCrashNotification(const ::Diagnostics::CrashReport& report,
                                               const NotificationOptions& options = {});
        
        /// Handle user choice and execute corresponding action
        static void HandleUserChoice(UserChoice choice, const ::Diagnostics::CrashReport& report);
        
        /// Format detailed crash dialog with severity analysis
        static std::string FormatDetailedCrashDialog(
            const ::CrashGuard::SeverityAnalysis& analysis,
            const ::Diagnostics::CrashReport& report,
            const NotificationOptions& options = {});
        
        /// Generate choice buttons based on severity and recovery status
        static std::vector<std::string> GenerateChoiceButtons(
            const ::CrashGuard::SeverityAnalysis& analysis,
            bool recoverySuccessful,
            const NotificationOptions& options = {});
        
        /// Display simple notification message
        static UserChoice ShowNotification(const std::string& title,
                                         const std::string& message,
                                         NotificationSeverity severity,
                                         const std::vector<std::string>& choices = {});
        
        /// Display warning before saving corrupted state
        static bool ConfirmSaveWithCorruption(const std::string& corruptionDetails);
        
        /// Display recovery success notification
        static void ShowRecoverySuccess(const std::string& recoveryDetails,
                                      const std::vector<std::string>& actionsPerformed);
        
        /// Display prevention notification (when crash was prevented)
        static void ShowPreventionNotification(const std::string& preventedIssue,
                                             const std::string& suspectedMod);
        
        /// Batch multiple notifications to avoid spam
        static void BatchNotification(const std::string& message,
                                    NotificationSeverity severity,
                                    const std::string& correlationId = "");
        
        /// Flush batched notifications
        static UserChoice FlushBatchedNotifications();
        
        /// Show file relationship analysis
        static void ShowFileConflictAnalysis(const std::vector<std::string>& conflictingMods,
                                           const std::string& analysisDetails);
        
        /// Show pattern learning notification
        static void ShowPatternLearningUpdate(const std::string& patternName,
                                            uint32_t occurrences,
                                            const std::string& recommendedAction);
        
        /// Configuration
        static void SetDefaultOptions(const NotificationOptions& options);
        static void SetEnabled(bool enabled);
        static void SetAutoRecoverSafe(bool autoRecover);
        static void SetAutoRecoverWarning(bool autoRecover);
        
        /// Statistics
        static uint32_t GetNotificationCount();
        static uint32_t GetUserInteractionCount();
        static std::vector<std::string> GetMostCommonChoices();

    private:
        /// Format crash notification message with rich context
        static std::string FormatCrashMessage(const ::Diagnostics::CrashReport& report,
                                            const NotificationOptions& options);
        
        /// Generate user choices based on severity and context
        static std::vector<std::string> GenerateChoices(const ::Diagnostics::CrashReport& report,
                                                       const NotificationOptions& options);
        
        /// Format file details for display
        static std::string FormatFileDetails(const std::vector<std::string>& suspectedMods,
                                           const std::string& category);
        
        /// Format quick fixes for display
        static std::string FormatQuickFixes(const std::vector<std::string>& fixes);
        
        /// Format technical details for advanced users
        static std::string FormatTechnicalDetails(const ::Diagnostics::CrashReport& report);
        
        /// Show Windows MessageBox with custom styling
        static int ShowMessageBox(const std::string& title,
                                 const std::string& message,
                                 const std::vector<std::string>& buttons,
                                 uint32_t timeoutSeconds = 30);
        
        /// Convert MessageBox result to UserChoice
        static UserChoice ConvertToUserChoice(int result, const std::vector<std::string>& choices);
        
        /// Check if notification should be batched
        static bool ShouldBatch(const std::string& message, NotificationSeverity severity);
        
        /// Format batched notifications
        static std::string FormatBatchedNotifications();
        
        /// Open crash log file in default viewer
        static void OpenCrashLog(const std::string& reportId);
        
        /// Teleport player to safe cell
        static void TeleportToSafeCell();
        
        /// Prevent notification spam
        static bool IsSpamPrevented(const std::string& messageHash);
        static void RecordNotification(const std::string& messageHash);
        
        // User choice action handlers
        static void HandleContinue(const ::Diagnostics::CrashReport& report);
        static void HandleCrashAnyway(const ::Diagnostics::CrashReport& report);
        static void HandleLoadLastSave(const ::Diagnostics::CrashReport& report);
        static void HandleTeleportToSafe(const ::Diagnostics::CrashReport& report);
        static void HandleViewLog(const ::Diagnostics::CrashReport& report);
        static void HandleRestartGame(const ::Diagnostics::CrashReport& report);
        static void HandleTimeout(const ::Diagnostics::CrashReport& report);
        
        // State management
        static bool s_initialized;
        static bool s_enabled;
        static NotificationOptions s_defaultOptions;
        static bool s_autoRecoverSafe;
        static bool s_autoRecoverWarning;
        
        static std::vector<BatchedNotification> s_batchedNotifications;
        static std::chrono::steady_clock::time_point s_lastNotificationTime;
        static std::unordered_map<std::string, std::chrono::steady_clock::time_point> s_recentNotifications;
        
        static uint32_t s_notificationCount;
        static uint32_t s_userInteractionCount;
        static std::vector<std::string> s_userChoiceHistory;
        
        static std::mutex s_notificationMutex;
    };

    /// Convenience functions for common notifications
    namespace QuickNotify {
        /// Show crash prevented notification
        void CrashPrevented(const std::string& issue, const std::string& mod);
        
        /// Show recovery successful notification
        void RecoverySuccessful(const std::string& strategy);
        
        /// Show mod conflict detected
        void ModConflictDetected(const std::vector<std::string>& mods);
        
        /// Show performance warning
        void PerformanceWarning(const std::string& issue, const std::string& suggestion);
        
        /// Show pattern learned notification
        void PatternLearned(const std::string& pattern, const std::string& solution);
    }

}  // namespace UserNotifications