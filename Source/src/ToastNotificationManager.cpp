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

#include "ToastNotificationManager.h"
#include "SeverityAnalyzer.h"
#include "VEH.h"
#include "RecoveryNotifications.h"
#include <spdlog/spdlog.h>
#include <sstream>

namespace CrashGuard {

    void ToastNotificationManager::ShowCrashPreventedToast(
        const SeverityAnalysis& analysis,
        const std::string& recoveryStrategy) {
        
        std::string message = FormatToastMessage(analysis, recoveryStrategy);
        
        // Use Skyrim's debug notification system
        RE::SendHUDMessage::ShowHUDMessage(message.c_str());
        
        spdlog::info("ToastNotificationManager: Displayed toast notification");
        spdlog::debug("  Severity: {}", static_cast<int>(analysis.level));
        spdlog::debug("  Message: {}", message);
        
        // Also log to F11 menu history
        auto getSeverityName = [](VEH::SeverityLevel level) -> std::string {
            switch (level) {
                case VEH::SeverityLevel::Safe: return "Safe";
                case VEH::SeverityLevel::Warning: return "Warning";
                case VEH::SeverityLevel::Critical: return "Critical";
                case VEH::SeverityLevel::Fatal: return "Fatal";
                default: return "Unknown";
            }
        };
        
        RecoveryNotifications::GetSingleton().AddRecovery(
            getSeverityName(analysis.level),
            analysis.technicalReason,
            recoveryStrategy,
            {},  // No detailed actions for toast
            {},  // No mods for toast
            true // Recovery successful
        );
    }

    void ToastNotificationManager::ShowRecoveryToast(
        const std::string& message,
        uint32_t durationSeconds) {
        
        // Use Skyrim's debug notification system
        RE::SendHUDMessage::ShowHUDMessage(message.c_str());
        
        spdlog::info("ToastNotificationManager: Displayed generic toast");
        spdlog::debug("  Message: {}", message);
        spdlog::debug("  Duration: {} seconds", durationSeconds);
    }

    std::string ToastNotificationManager::FormatToastMessage(
        const SeverityAnalysis& analysis,
        const std::string& recoveryStrategy) {
        
        std::ostringstream oss;
        
        // Helper function to get severity name
        auto getSeverityName = [](VEH::SeverityLevel level) -> std::string {
            switch (level) {
                case VEH::SeverityLevel::Safe: return "Safe";
                case VEH::SeverityLevel::Warning: return "Warning";
                case VEH::SeverityLevel::Critical: return "Critical";
                case VEH::SeverityLevel::Fatal: return "Fatal";
                default: return "Unknown";
            }
        };
        
        oss << "Crash Prevented (" << getSeverityName(analysis.level) << ")\n";
        oss << analysis.technicalReason << "\n";
        oss << "Recovery: " << recoveryStrategy << "\n";
        oss << "Press F11 for details";
        
        return oss.str();
    }

}  // namespace CrashGuard
