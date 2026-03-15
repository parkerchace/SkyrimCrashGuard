// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <mutex>

namespace CrashGuard {

    /// Brief recovery notification for ImGui overlay
    struct RecoveryToast {
        std::string severity;           // "Safe", "Warning", "Critical"
        std::string summary;            // Brief one-line description
        std::string strategy;           // Recovery strategy used
        std::chrono::steady_clock::time_point timestamp;
        float displayTime;              // Seconds to display (3-5s)
        bool visible;
        
        RecoveryToast() : displayTime(0.0f), visible(false) {}
    };

    /// Detailed recovery entry for F11 menu
    struct RecoveryEntry {
        std::string severity;
        std::string timestamp;          // Formatted time string
        std::string rootCause;
        std::string strategy;
        std::vector<std::string> actions;
        std::vector<std::string> suspectedMods;
        bool successful;
        
        RecoveryEntry() : successful(false) {}
    };

    /// Manages real-time recovery notifications
    class RecoveryNotifications {
    public:
        static RecoveryNotifications& GetSingleton() {
            static RecoveryNotifications instance;
            return instance;
        }

        /// Add a new recovery notification (called from VEH)
        void AddRecovery(
            const std::string& severity,
            const std::string& rootCause,
            const std::string& strategy,
            const std::vector<std::string>& actions,
            const std::vector<std::string>& suspectedMods,
            bool successful
        );
        
        /// Add a resource warning notification (auto-opens menu if critical)
        void AddResourceWarning(
            const std::string& resourceType,
            const std::string& message,
            bool isCritical
        );

        /// Render toast notifications (top-right overlay)
        void RenderToasts();

        /// Get recent recoveries for F11 menu
        std::vector<RecoveryEntry> GetRecentRecoveries(size_t maxCount = 50) const;

        /// Clear all recovery history
        void ClearHistory();

        /// Get statistics
        size_t GetTotalRecoveries() const;
        size_t GetSuccessfulRecoveries() const;
        size_t GetFailedRecoveries() const;

    private:
        RecoveryNotifications() = default;
        ~RecoveryNotifications() = default;
        RecoveryNotifications(const RecoveryNotifications&) = delete;
        RecoveryNotifications& operator=(const RecoveryNotifications&) = delete;

        void UpdateToasts();
        std::string FormatTimestamp(const std::chrono::steady_clock::time_point& time) const;
        std::string CreateSummary(const std::string& rootCause, const std::string& strategy) const;

        mutable std::mutex m_mutex;
        std::vector<RecoveryToast> m_activeToasts;
        std::vector<RecoveryEntry> m_history;
        
        size_t m_totalRecoveries = 0;
        size_t m_successfulRecoveries = 0;
        size_t m_failedRecoveries = 0;
        
        static constexpr size_t MAX_ACTIVE_TOASTS = 3;
        static constexpr size_t MAX_HISTORY = 100;
        static constexpr float TOAST_DURATION_SAFE = 3.0f;
        static constexpr float TOAST_DURATION_WARNING = 4.0f;
        static constexpr float TOAST_DURATION_CRITICAL = 5.0f;
    };

}
