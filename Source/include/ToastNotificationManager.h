// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <string>
#include <cstdint>

// Forward declarations
namespace CrashGuard {
    struct SeverityAnalysis;
}

/// Toast Notification Manager for Non-Intrusive In-Game Notifications
/// Displays brief notifications for auto-recovered crashes
/// Integrates with Skyrim's RE::DebugNotification system
namespace CrashGuard {

    /// Main toast notification manager class
    class ToastNotificationManager {
    public:
        /// Show crash prevented toast notification
        /// @param analysis The severity analysis result
        /// @param recoveryStrategy The recovery strategy used (e.g., "Instruction Skip (L4)")
        static void ShowCrashPreventedToast(
            const SeverityAnalysis& analysis,
            const std::string& recoveryStrategy);
        
        /// Show generic recovery toast notification
        /// @param message The message to display
        /// @param durationSeconds How long to show the toast (default 5 seconds)
        static void ShowRecoveryToast(
            const std::string& message,
            uint32_t durationSeconds = 5);
        
    private:
        /// Format toast message from severity analysis
        /// @param analysis The severity analysis result
        /// @param recoveryStrategy The recovery strategy used
        /// @return Formatted toast message
        static std::string FormatToastMessage(
            const SeverityAnalysis& analysis,
            const std::string& recoveryStrategy);
    };

}  // namespace CrashGuard
