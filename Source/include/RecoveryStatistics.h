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
#include <atomic>
#include <cstdint>
#include "VEH.h"
#include "UserNotificationManager.h"

namespace CrashGuard {

/**
 * @brief Tracks recovery statistics for the current session
 * 
 * This class maintains atomic counters for crash recoveries by severity level
 * and user choices made in response to crash notifications. Statistics are
 * displayed in the F11 menu Recovery Statistics tab.
 */
class RecoveryStatistics {
public:
    static RecoveryStatistics& GetInstance() {
        static RecoveryStatistics instance;
        return instance;
    }

    /**
     * @brief Record a crash recovery event
     * @param level Severity level of the crash
     * @param userPrompted Whether the user was prompted for a choice
     */
    void RecordRecovery(VEH::SeverityLevel level, bool userPrompted);

    /**
     * @brief Record a user choice made in response to a crash notification
     * @param choice The choice the user made
     */
    void RecordUserChoice(UserNotifications::UserChoice choice);

    /**
     * @brief Reset all statistics to zero
     */
    void Reset();

    // Getters for severity counts
    uint32_t GetSafeCount() const { return m_safeCount.load(); }
    uint32_t GetWarningCount() const { return m_warningCount.load(); }
    uint32_t GetCriticalCount() const { return m_criticalCount.load(); }
    uint32_t GetFatalCount() const { return m_fatalCount.load(); }

    // Getters for user choice counts
    uint32_t GetUserChoiceContinue() const { return m_userChoiceContinue.load(); }
    uint32_t GetUserChoiceLoadSave() const { return m_userChoiceLoadSave.load(); }
    uint32_t GetUserChoiceTeleport() const { return m_userChoiceTeleport.load(); }
    uint32_t GetUserChoiceViewLog() const { return m_userChoiceViewLog.load(); }
    uint32_t GetUserChoiceCrashAnyway() const { return m_userChoiceCrashAnyway.load(); }
    uint32_t GetUserChoiceTimeout() const { return m_userChoiceTimeout.load(); }

private:
    RecoveryStatistics() = default;
    ~RecoveryStatistics() = default;
    RecoveryStatistics(const RecoveryStatistics&) = delete;
    RecoveryStatistics& operator=(const RecoveryStatistics&) = delete;

    // Severity level counters
    std::atomic<uint32_t> m_safeCount{0};
    std::atomic<uint32_t> m_warningCount{0};
    std::atomic<uint32_t> m_criticalCount{0};
    std::atomic<uint32_t> m_fatalCount{0};

    // User choice counters
    std::atomic<uint32_t> m_userChoiceContinue{0};
    std::atomic<uint32_t> m_userChoiceLoadSave{0};
    std::atomic<uint32_t> m_userChoiceTeleport{0};
    std::atomic<uint32_t> m_userChoiceViewLog{0};
    std::atomic<uint32_t> m_userChoiceCrashAnyway{0};
    std::atomic<uint32_t> m_userChoiceTimeout{0};
};

} // namespace CrashGuard
