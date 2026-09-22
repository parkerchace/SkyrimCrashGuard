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

#include "Config.h"
#include "PapyrusNativeFunctionHook.h"
#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <spdlog/spdlog.h>

namespace CrashGuard {
namespace PapyrusNatives {

// Menu and Status
void ShowMenu(RE::StaticFunctionTag*) {
    RE::SendHUDMessage::ShowHUDMessage("Edit SkyrimCrashGuard.toml or use MCM menu");
    spdlog::info("ShowMenu called - directing user to TOML/MCM");
}

void ShowStatus(RE::StaticFunctionTag*) {
    const auto& config = Config::Get();
    
    spdlog::info("=== CrashGuard Status ===");
    spdlog::info("Enabled: {}", config.enabled);
    spdlog::info("VEH Enabled: {}", config.vehEnabled);
    spdlog::info("Mesh Validation: {}", config.enableMeshValidation);
    spdlog::info("Animation Validation: {}", config.enableAnimationValidation);
    spdlog::info("Script Monitoring: {}", config.enableScriptMonitoring);
    spdlog::info("Cell Validation: {}", config.enableCellValidation);
    spdlog::info("Pattern Learning: {}", config.enableLearning);
    spdlog::info("Notifications: {}", config.showNotifications);
    spdlog::info("========================");
    
    RE::SendHUDMessage::ShowHUDMessage("CrashGuard status logged - check SKSE log");
}

void ReloadConfig(RE::StaticFunctionTag*) {
    std::string configPath = "Data/SKSE/Plugins/SkyrimCrashGuard.toml";
    Config::Load(configPath);
    
    spdlog::info("Configuration reloaded from TOML");
    RE::SendHUDMessage::ShowHUDMessage("CrashGuard config reloaded");
}

void ResetToDefaults(RE::StaticFunctionTag*) {
    auto& config = Config::GetMutable();
    
    // Reset all settings to defaults
    config.enabled = true;
    config.logLevel = 1;
    config.vehEnabled = true;
    config.cascadeLimit = 3;
    config.patchesEnabled = true;
    config.enableMeshValidation = true;
    config.enableAnimationValidation = true;
    config.enableScriptMonitoring = true;
    config.enableCellValidation = true;
    config.enableLearning = true;
    config.showNotifications = true;
    config.notificationTimeoutSeconds = 30;
    config.scriptTimeoutMs = 5000;
    config.maxRecoveryAttempts = 3;
    
    std::string configPath = "Data/SKSE/Plugins/SkyrimCrashGuard.toml";
    Config::Save(configPath);
    
    spdlog::info("All settings reset to defaults");
    RE::SendHUDMessage::ShowHUDMessage("CrashGuard settings reset to defaults");
}

// Get/Set Enabled
bool GetEnabled(RE::StaticFunctionTag*) {
    return Config::Get().enabled;
}

void SetEnabled(RE::StaticFunctionTag*, bool value) {
    auto& config = Config::GetMutable();
    config.enabled = value;
    
    std::string configPath = "Data/SKSE/Plugins/SkyrimCrashGuard.toml";
    Config::Save(configPath);
    
    spdlog::info("CrashGuard enabled: {}", value);
    RE::SendHUDMessage::ShowHUDMessage(value ? "CrashGuard enabled" : "CrashGuard disabled");
}

// Get/Set Mesh Validation
bool GetMeshValidation(RE::StaticFunctionTag*) {
    return Config::Get().enableMeshValidation;
}

void SetMeshValidation(RE::StaticFunctionTag*, bool value) {
    auto& config = Config::GetMutable();
    config.enableMeshValidation = value;
    
    std::string configPath = "Data/SKSE/Plugins/SkyrimCrashGuard.toml";
    Config::Save(configPath);
    
    spdlog::info("Mesh validation: {}", value);
    RE::SendHUDMessage::ShowHUDMessage(value ? "Mesh validation enabled" : "Mesh validation disabled");
}

// Get/Set Animation Validation
bool GetAnimationValidation(RE::StaticFunctionTag*) {
    return Config::Get().enableAnimationValidation;
}

void SetAnimationValidation(RE::StaticFunctionTag*, bool value) {
    auto& config = Config::GetMutable();
    config.enableAnimationValidation = value;
    
    std::string configPath = "Data/SKSE/Plugins/SkyrimCrashGuard.toml";
    Config::Save(configPath);
    
    spdlog::info("Animation validation: {}", value);
    RE::SendHUDMessage::ShowHUDMessage(value ? "Animation validation enabled" : "Animation validation disabled");
}

// Get/Set Script Monitoring
bool GetScriptMonitoring(RE::StaticFunctionTag*) {
    return Config::Get().enableScriptMonitoring;
}

void SetScriptMonitoring(RE::StaticFunctionTag*, bool value) {
    auto& config = Config::GetMutable();
    config.enableScriptMonitoring = value;
    
    std::string configPath = "Data/SKSE/Plugins/SkyrimCrashGuard.toml";
    Config::Save(configPath);
    
    spdlog::info("Script monitoring: {}", value);
    RE::SendHUDMessage::ShowHUDMessage(value ? "Script monitoring enabled" : "Script monitoring disabled");
}

// Get/Set Cell Validation
bool GetCellValidation(RE::StaticFunctionTag*) {
    return Config::Get().enableCellValidation;
}

void SetCellValidation(RE::StaticFunctionTag*, bool value) {
    auto& config = Config::GetMutable();
    config.enableCellValidation = value;
    
    std::string configPath = "Data/SKSE/Plugins/SkyrimCrashGuard.toml";
    Config::Save(configPath);
    
    spdlog::info("Cell validation: {}", value);
    RE::SendHUDMessage::ShowHUDMessage(value ? "Cell validation enabled" : "Cell validation disabled");
}

// Get/Set Pattern Learning
bool GetPatternLearning(RE::StaticFunctionTag*) {
    return Config::Get().enableLearning;
}

void SetPatternLearning(RE::StaticFunctionTag*, bool value) {
    auto& config = Config::GetMutable();
    config.enableLearning = value;
    
    std::string configPath = "Data/SKSE/Plugins/SkyrimCrashGuard.toml";
    Config::Save(configPath);
    
    spdlog::info("Pattern learning: {}", value);
    RE::SendHUDMessage::ShowHUDMessage(value ? "Pattern learning enabled" : "Pattern learning disabled");
}

// Get/Set Notifications
bool GetNotifications(RE::StaticFunctionTag*) {
    return Config::Get().showNotifications;
}

void SetNotifications(RE::StaticFunctionTag*, bool value) {
    auto& config = Config::GetMutable();
    config.showNotifications = value;
    
    std::string configPath = "Data/SKSE/Plugins/SkyrimCrashGuard.toml";
    Config::Save(configPath);
    
    spdlog::info("Notifications: {}", value);
    RE::SendHUDMessage::ShowHUDMessage(value ? "Notifications enabled" : "Notifications disabled");
}

// Register all functions with Papyrus
bool RegisterFunctions(RE::BSScript::IVirtualMachine* vm) {
    vm->RegisterFunction("ShowMenu", "CrashGuardNative", ShowMenu);
    vm->RegisterFunction("ShowStatus", "CrashGuardNative", ShowStatus);
    vm->RegisterFunction("ReloadConfig", "CrashGuardNative", ReloadConfig);
    vm->RegisterFunction("ResetToDefaults", "CrashGuardNative", ResetToDefaults);
    
    vm->RegisterFunction("GetEnabled", "CrashGuardNative", GetEnabled);
    vm->RegisterFunction("SetEnabled", "CrashGuardNative", SetEnabled);
    
    vm->RegisterFunction("GetMeshValidation", "CrashGuardNative", GetMeshValidation);
    vm->RegisterFunction("SetMeshValidation", "CrashGuardNative", SetMeshValidation);
    
    vm->RegisterFunction("GetAnimationValidation", "CrashGuardNative", GetAnimationValidation);
    vm->RegisterFunction("SetAnimationValidation", "CrashGuardNative", SetAnimationValidation);
    
    vm->RegisterFunction("GetScriptMonitoring", "CrashGuardNative", GetScriptMonitoring);
    vm->RegisterFunction("SetScriptMonitoring", "CrashGuardNative", SetScriptMonitoring);
    
    vm->RegisterFunction("GetCellValidation", "CrashGuardNative", GetCellValidation);
    vm->RegisterFunction("SetCellValidation", "CrashGuardNative", SetCellValidation);
    
    vm->RegisterFunction("GetPatternLearning", "CrashGuardNative", GetPatternLearning);
    vm->RegisterFunction("SetPatternLearning", "CrashGuardNative", SetPatternLearning);
    
    vm->RegisterFunction("GetNotifications", "CrashGuardNative", GetNotifications);
    vm->RegisterFunction("SetNotifications", "CrashGuardNative", SetNotifications);
    
    spdlog::info("CrashGuard Papyrus native functions registered for MCM");
    return true;
}

void Register() {
    auto papyrus = SKSE::GetPapyrusInterface();
    if (papyrus) {
        papyrus->Register(RegisterFunctions);
        spdlog::info("CrashGuard Papyrus interface registered");
        
        // Register validation wrappers for problematic functions
        papyrus->Register(PapyrusValidation::RegisterValidationWrappers);
        spdlog::info("Papyrus validation wrappers registered");
    } else {
        spdlog::error("Failed to get Papyrus interface");
    }
}

} // namespace PapyrusNatives
} // namespace CrashGuard
