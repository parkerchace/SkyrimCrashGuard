// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "RecoveryStatistics.h"

namespace CrashGuard {

void RecoveryStatistics::RecordRecovery(VEH::SeverityLevel level, bool userPrompted) {
    switch (level) {
        case VEH::SeverityLevel::Safe:
            m_safeCount.fetch_add(1, std::memory_order_relaxed);
            break;
        case VEH::SeverityLevel::Warning:
            m_warningCount.fetch_add(1, std::memory_order_relaxed);
            break;
        case VEH::SeverityLevel::Critical:
            m_criticalCount.fetch_add(1, std::memory_order_relaxed);
            break;
        case VEH::SeverityLevel::Fatal:
            m_fatalCount.fetch_add(1, std::memory_order_relaxed);
            break;
    }
}

void RecoveryStatistics::RecordUserChoice(UserNotifications::UserChoice choice) {
    switch (choice) {
        case UserNotifications::UserChoice::Continue:
            m_userChoiceContinue.fetch_add(1, std::memory_order_relaxed);
            break;
        case UserNotifications::UserChoice::LoadLastSave:
            m_userChoiceLoadSave.fetch_add(1, std::memory_order_relaxed);
            break;
        case UserNotifications::UserChoice::TeleportToSafe:
            m_userChoiceTeleport.fetch_add(1, std::memory_order_relaxed);
            break;
        case UserNotifications::UserChoice::ViewLog:
            m_userChoiceViewLog.fetch_add(1, std::memory_order_relaxed);
            break;
        case UserNotifications::UserChoice::CrashAnyway:
            m_userChoiceCrashAnyway.fetch_add(1, std::memory_order_relaxed);
            break;
        case UserNotifications::UserChoice::Timeout:
            m_userChoiceTimeout.fetch_add(1, std::memory_order_relaxed);
            break;
    }
}

void RecoveryStatistics::Reset() {
    m_safeCount.store(0, std::memory_order_relaxed);
    m_warningCount.store(0, std::memory_order_relaxed);
    m_criticalCount.store(0, std::memory_order_relaxed);
    m_fatalCount.store(0, std::memory_order_relaxed);
    
    m_userChoiceContinue.store(0, std::memory_order_relaxed);
    m_userChoiceLoadSave.store(0, std::memory_order_relaxed);
    m_userChoiceTeleport.store(0, std::memory_order_relaxed);
    m_userChoiceViewLog.store(0, std::memory_order_relaxed);
    m_userChoiceCrashAnyway.store(0, std::memory_order_relaxed);
    m_userChoiceTimeout.store(0, std::memory_order_relaxed);
}

} // namespace CrashGuard
