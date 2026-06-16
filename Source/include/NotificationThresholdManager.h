// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "SeverityAnalyzer.h"
#include <string>
#include <cstdint>

/// Notification Threshold Manager - Determines when to show user dialogs vs auto-recover
/// Implements requirements from user-controlled-crash-notifications spec
namespace CrashGuard {

    /// Notification decision types
    enum class NotificationDecision {
        SilentRecover,    // No notification, log only
        ShowToast,        // Brief toast notification
        ShowDialog        // Full dialog with user choice
    };

    /// Manages notification thresholds and decides when to prompt users
    class NotificationThresholdManager {
    public:
        /// Initialize the notification threshold manager
        static void Initialize();
        
        /// Load configuration from TOML file
        static void LoadConfig();
        
        /// Decide what type of notification to show based on severity and config
        /// @param analysis The severity analysis result
        /// @param recoverySuccessful Whether recovery was successful
        /// @return The notification decision (Silent, Toast, or Dialog)
        static NotificationDecision DecideNotification(
            const SeverityAnalysis& analysis,
            bool recoverySuccessful);
        
        // Configuration getters
        static bool ShouldNotifyOnSafe();
        static bool ShouldNotifyOnWarning();
        static bool ShouldNotifyOnCritical();
        static bool ShouldNotifyOnFatal();
        static bool ShowToastForAutoRecovery();
        static uint32_t GetDialogTimeout();
        static std::string GetTimeoutAction();
        static bool ShowTechnicalDetails();
        static bool AllowCrashAnywayOption();
        
    private:
        // Configuration fields
        static bool s_notifyOnSafe;
        static bool s_notifyOnWarning;
        static bool s_notifyOnCritical;
        static bool s_notifyOnFatal;
        static bool s_showToastForAutoRecovery;
        static uint32_t s_dialogTimeout;
        static std::string s_timeoutAction;
        static bool s_showTechnicalDetails;
        static bool s_allowCrashAnyway;
    };

}  // namespace CrashGuard
