// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "NotificationThresholdManager.h"
#include "Config.h"
#include "VEH.h"

namespace CrashGuard {

// Initialize static fields with safe defaults
bool NotificationThresholdManager::s_notifyOnSafe = false;
bool NotificationThresholdManager::s_notifyOnWarning = false;
bool NotificationThresholdManager::s_notifyOnCritical = true;
bool NotificationThresholdManager::s_notifyOnFatal = true;
bool NotificationThresholdManager::s_showToastForAutoRecovery = true;
uint32_t NotificationThresholdManager::s_dialogTimeout = 30;
std::string NotificationThresholdManager::s_timeoutAction = "Continue";
bool NotificationThresholdManager::s_showTechnicalDetails = false;
bool NotificationThresholdManager::s_allowCrashAnyway = true;

void NotificationThresholdManager::Initialize() {
    // Load configuration from TOML
    LoadConfig();
}

void NotificationThresholdManager::LoadConfig() {
    // Load from Config class
    const auto& config = Config::Get();
    
    s_notifyOnSafe = config.notifyOnSafe;
    s_notifyOnWarning = config.notifyOnWarning;
    s_notifyOnCritical = config.notifyOnCritical;
    s_notifyOnFatal = config.notifyOnFatal;
    s_showToastForAutoRecovery = config.showToastForAutoRecovery;
    s_dialogTimeout = config.dialogTimeoutSeconds;
    s_timeoutAction = config.timeoutDefaultAction;
    s_showTechnicalDetails = config.showTechnicalDetails;
    s_allowCrashAnyway = config.allowCrashAnywayOption;
    
    // Validate timeout value (5-120 seconds)
    if (s_dialogTimeout < 5) {
        s_dialogTimeout = 5;
    } else if (s_dialogTimeout > 120) {
        s_dialogTimeout = 120;
    }
    
    // Validate timeout action
    if (s_timeoutAction != "Continue" && 
        s_timeoutAction != "LoadLastSave" && 
        s_timeoutAction != "MainMenu") {
        s_timeoutAction = "Continue";  // Default to safe option
    }
}

NotificationDecision NotificationThresholdManager::DecideNotification(
    const SeverityAnalysis& analysis,
    bool recoverySuccessful) {
    
    // Always prompt for Critical/Fatal if configured (default behavior)
    if (analysis.level == VEH::SeverityLevel::Critical && s_notifyOnCritical) {
        return NotificationDecision::ShowDialog;
    }
    
    if (analysis.level == VEH::SeverityLevel::Fatal && s_notifyOnFatal) {
        return NotificationDecision::ShowDialog;
    }
    
    // Check if user wants to be notified for Warning crashes
    if (analysis.level == VEH::SeverityLevel::Warning) {
        if (s_notifyOnWarning) {
            // User wants dialog for Warning crashes
            return NotificationDecision::ShowDialog;
        } else if (s_showToastForAutoRecovery) {
            // Auto-recover with toast notification
            return NotificationDecision::ShowToast;
        } else {
            // Silent recovery
            return NotificationDecision::SilentRecover;
        }
    }
    
    // Check if user wants to be notified for Safe crashes
    if (analysis.level == VEH::SeverityLevel::Safe) {
        if (s_notifyOnSafe) {
            // User wants dialog for Safe crashes
            return NotificationDecision::ShowDialog;
        } else if (s_showToastForAutoRecovery) {
            // Auto-recover with toast notification (optional)
            return NotificationDecision::ShowToast;
        } else {
            // Silent recovery (default for Safe)
            return NotificationDecision::SilentRecover;
        }
    }
    
    // Unknown severity - default to showing dialog to be safe
    return NotificationDecision::ShowDialog;
}

// Configuration getters
bool NotificationThresholdManager::ShouldNotifyOnSafe() {
    return s_notifyOnSafe;
}

bool NotificationThresholdManager::ShouldNotifyOnWarning() {
    return s_notifyOnWarning;
}

bool NotificationThresholdManager::ShouldNotifyOnCritical() {
    return s_notifyOnCritical;
}

bool NotificationThresholdManager::ShouldNotifyOnFatal() {
    return s_notifyOnFatal;
}

bool NotificationThresholdManager::ShowToastForAutoRecovery() {
    return s_showToastForAutoRecovery;
}

uint32_t NotificationThresholdManager::GetDialogTimeout() {
    return s_dialogTimeout;
}

std::string NotificationThresholdManager::GetTimeoutAction() {
    return s_timeoutAction;
}

bool NotificationThresholdManager::ShowTechnicalDetails() {
    return s_showTechnicalDetails;
}

bool NotificationThresholdManager::AllowCrashAnywayOption() {
    return s_allowCrashAnyway;
}

}  // namespace CrashGuard
