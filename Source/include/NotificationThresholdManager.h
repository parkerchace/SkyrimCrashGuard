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
