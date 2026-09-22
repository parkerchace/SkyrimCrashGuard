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

#include <string>
#include <cstdint>

// Forward declarations
namespace CrashGuard {
    struct SeverityAnalysis;
}

/// Toast Notification Manager for Non-Intrusive In-Game Notifications
/// Displays brief notifications for auto-recovered crashes
/// Integrates with Skyrim's HUD notification system (RE::SendHUDMessage::ShowHUDMessage)
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
