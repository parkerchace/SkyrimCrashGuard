// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "Config.h"

#include <toml.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

namespace Config {

    static Settings s_settings;

    template<typename T>
    T GetValue(const toml::value& config, const std::string& section, const std::string& key, T defaultValue)
    {
        try {
            if (config.contains(section)) {
                const auto& sec = toml::find(config, section);
                if (sec.contains(key)) {
                    return toml::find<T>(sec, key);
                }
            }
        } catch (...) {
            // Return default on any error
        }
        return defaultValue;
    }

    void Load(const std::string& tomlPath)
    {
        s_settings = {};  // Reset to defaults

        try {
            auto config = toml::parse(tomlPath);

            // [General]
            s_settings.enabled = GetValue(config, "General", "enabled", true);
            s_settings.safeMode = GetValue(config, "General", "safeMode", false);
            s_settings.logLevel = GetValue(config, "General", "logLevel", 1);

            // [VEH]
            s_settings.vehEnabled = GetValue(config, "VEH", "enabled", true);
            s_settings.cascadeLimit = GetValue(config, "VEH", "cascadeLimit", 3);

            // [Patches]
            s_settings.patchesEnabled = GetValue(config, "Patches", "enabled", true);

            // [ProactiveValidation]
            s_settings.enableMeshValidation = GetValue(config, "ProactiveValidation", "enableMeshValidation", true);
            s_settings.enableAnimationValidation = GetValue(config, "ProactiveValidation", "enableAnimationValidation", true);
            s_settings.enableScriptMonitoring = GetValue(config, "ProactiveValidation", "enableScriptMonitoring", true);
            s_settings.enableCellValidation = GetValue(config, "ProactiveValidation", "enableCellValidation", true);

            // [SafetyChecks]
            s_settings.enableNullChecks = GetValue(config, "SafetyChecks", "enableNullChecks", true);
            s_settings.enableBoundsChecks = GetValue(config, "SafetyChecks", "enableBoundsChecks", true);
            s_settings.enableFormIDChecks = GetValue(config, "SafetyChecks", "enableFormIDChecks", true);

            // [StateManagement]
            s_settings.enableStateSnapshots = GetValue(config, "StateManagement", "enableStateSnapshots", true);
            s_settings.maxSnapshotsPerSession = GetValue(config, "StateManagement", "maxSnapshotsPerSession", 100);

            // [Learning]
            s_settings.enableLearning = GetValue(config, "Learning", "enableLearning", true);
            s_settings.patternDatabasePath = GetValue(config, "Learning", "patternDatabasePath", 
                std::string("Data/SKSE/Plugins/CrashGuard/patterns.json"));

            // [Notifications]
            s_settings.showNotifications = GetValue(config, "Notifications", "showNotifications", true);
            s_settings.autoRecoverSafe = GetValue(config, "Notifications", "autoRecoverSafe", true);
            s_settings.autoRecoverWarning = GetValue(config, "Notifications", "autoRecoverWarning", false);
            s_settings.notificationTimeoutSeconds = GetValue(config, "Notifications", "notificationTimeoutSeconds", 30);

            // [UserNotifications]
            s_settings.notifyOnSafe = GetValue(config, "UserNotifications", "NotifyOnSafe", false);
            s_settings.notifyOnWarning = GetValue(config, "UserNotifications", "NotifyOnWarning", false);
            s_settings.notifyOnCritical = GetValue(config, "UserNotifications", "NotifyOnCritical", true);
            s_settings.notifyOnFatal = GetValue(config, "UserNotifications", "NotifyOnFatal", true);
            s_settings.showToastForAutoRecovery = GetValue(config, "UserNotifications", "ShowToastForAutoRecovery", true);
            s_settings.toastDurationSeconds = GetValue(config, "UserNotifications", "ToastDurationSeconds", 5);
            s_settings.dialogTimeoutSeconds = GetValue(config, "UserNotifications", "DialogTimeoutSeconds", 30);
            s_settings.timeoutDefaultAction = GetValue(config, "UserNotifications", "TimeoutDefaultAction", std::string("Continue"));
            s_settings.showTechnicalDetails = GetValue(config, "UserNotifications", "ShowTechnicalDetails", false);
            s_settings.allowCrashAnywayOption = GetValue(config, "UserNotifications", "AllowCrashAnywayOption", true);
            s_settings.batchSimilarCrashes = GetValue(config, "UserNotifications", "BatchSimilarCrashes", true);
            s_settings.logAllRecoveries = GetValue(config, "UserNotifications", "LogAllRecoveries", true);
            s_settings.logSilentRecoveries = GetValue(config, "UserNotifications", "LogSilentRecoveries", false);

            // [Performance]
            s_settings.scriptTimeoutMs = GetValue(config, "Performance", "scriptTimeoutMs", 5000);
            s_settings.maxRecoveryAttempts = GetValue(config, "Performance", "maxRecoveryAttempts", 3);

            // [Logging]
            s_settings.enableDetailedLogging = GetValue(config, "Logging", "enableDetailedLogging", false);
            s_settings.logOnlyFailures = GetValue(config, "Logging", "logOnlyFailures", true);
            s_settings.logSuccessfulRecoveries = GetValue(config, "Logging", "logSuccessfulRecoveries", false);
            s_settings.aggregatePatterns = GetValue(config, "Logging", "aggregatePatterns", true);
            s_settings.maxLogSizeMB = GetValue(config, "Logging", "maxLogSizeMB", 10);
            s_settings.maxLogFiles = GetValue(config, "Logging", "maxLogFiles", 3);

            // Per-subsystem debug toggles
            s_settings.enableInputDebugLogging = GetValue(config, "Logging", "enableInputDebugLogging", false);
            s_settings.enableVehDebugLogging = GetValue(config, "Logging", "enableVehDebugLogging", false);
            s_settings.enablePatchDebugLogging = GetValue(config, "Logging", "enablePatchDebugLogging", false);
            s_settings.enablePapyrusDebugLogging = GetValue(config, "Logging", "enablePapyrusDebugLogging", false);
            s_settings.enablePerfTracing = GetValue(config, "Logging", "enablePerfTracing", false);

            // [InputDiagnostics]
            s_settings.enableInputDiagnostics = GetValue(config, "InputDiagnostics", "enableInputDiagnostics", false);

            // [Compatibility]
            // Compatibility modes are now auto-detected. No TOML loading needed.

            // [PapyrusValidation]
            s_settings.papyrusValidationEnabled = GetValue(config, "PapyrusValidation", "enabled", true);
            s_settings.papyrusValidationLogFailures = GetValue(config, "PapyrusValidation", "logFailures", true);
            s_settings.papyrusValidationStrictMode = GetValue(config, "PapyrusValidation", "strictMode", false);

            // [InputConflictPrevention]
            s_settings.enableInputConflictPrevention = GetValue(config, "InputConflictPrevention", "enabled", true);
            s_settings.blockCameraZoomInMenus = GetValue(config, "InputConflictPrevention", "blockCameraZoom", true);
            s_settings.blockFavoritesInMenus = GetValue(config, "InputConflictPrevention", "blockFavorites", true);
            s_settings.autoDetectModdedMenus = GetValue(config, "InputConflictPrevention", "autoDetectModdedMenus", true);
            s_settings.enableInputTracking = GetValue(config, "InputConflictPrevention", "enableInputTracking", true);
            
            // Load custom scrollable menus array
            s_settings.customScrollableMenus.clear();
            try {
                if (config.contains("InputConflictPrevention")) {
                    const auto& section = toml::find(config, "InputConflictPrevention");
                    if (section.contains("customScrollableMenus")) {
                        const auto& menuArray = toml::find<std::vector<std::string>>(section, "customScrollableMenus");
                        s_settings.customScrollableMenus = menuArray;
                        spdlog::info("[Config] Loaded {} custom scrollable menus", menuArray.size());
                        for (const auto& menu : menuArray) {
                            spdlog::debug("[Config]   - {}", menu);
                        }
                    }
                }
            } catch (...) {
                spdlog::warn("[Config] Failed to load customScrollableMenus array, using defaults");
            }

            // [ImGui]
            s_settings.disableImGuiMenu = GetValue(config, "ImGui", "disableMenu", false);
            s_settings.enableImGuiDebugLogs = GetValue(config, "ImGui", "enableDebugLogging", false);
            s_settings.allowImGuiInVR = GetValue(config, "ImGui", "allowInVR", true);
            if (s_settings.disableImGuiMenu) {
                spdlog::info("[Config] ImGui F11 menu is DISABLED - using TOML-only configuration");
            }
            
            // [PerformanceOverlay]
            s_settings.overlayEnabled = GetValue(config, "PerformanceOverlay", "enabled", false);
            s_settings.overlayShowFPS = GetValue(config, "PerformanceOverlay", "showFPS", true);
            s_settings.overlayShowFrameTime = GetValue(config, "PerformanceOverlay", "showFrameTime", true);
            s_settings.overlayShowMemory = GetValue(config, "PerformanceOverlay", "showMemory", true);
            s_settings.overlayShowCrashStats = GetValue(config, "PerformanceOverlay", "showCrashStats", true);
            s_settings.overlayShowRecoveryStats = GetValue(config, "PerformanceOverlay", "showRecoveryStats", true);
            s_settings.overlayShowPatternStats = GetValue(config, "PerformanceOverlay", "showPatternStats", false);
            s_settings.overlayPosition = GetValue(config, "PerformanceOverlay", "position", 1);
            s_settings.overlayBackgroundAlpha = GetValue(config, "PerformanceOverlay", "backgroundAlpha", 0.35f);
            s_settings.overlayTextAlpha = GetValue(config, "PerformanceOverlay", "textAlpha", 1.0f);
            s_settings.overlayScale = GetValue(config, "PerformanceOverlay", "scale", 1.0f);
            
            // [ResourceLimiter]
            s_settings.maxActors = GetValue(config, "ResourceLimiter", "maxActors", 50);
            s_settings.maxReferences = GetValue(config, "ResourceLimiter", "maxReferences", 20000);
            s_settings.maxParticles = GetValue(config, "ResourceLimiter", "maxParticles", 5000);
            s_settings.autoCullingEnabled = GetValue(config, "ResourceLimiter", "autoCullingEnabled", true);
            s_settings.dynamicLimitAdjustment = GetValue(config, "ResourceLimiter", "dynamicLimitAdjustment", true);
            s_settings.cullPerTick = GetValue(config, "ResourceLimiter", "cullPerTick", 10);
            s_settings.autoManageNPCs = GetValue(config, "ResourceLimiter", "autoManageNPCs", true);
            
            // [NPCManagement]
            s_settings.npcToolsToasts = GetValue(config, "NPCManagement", "npcToolsToasts", true);
            s_settings.npcReleaseRate = GetValue(config, "NPCManagement", "npcReleaseRate", 5);
            s_settings.npcCleanupRate = GetValue(config, "NPCManagement", "npcCleanupRate", 10);
            s_settings.maxDeadBodies = GetValue(config, "NPCManagement", "maxDeadBodies", 20);
            s_settings.usePerCellBaseline = GetValue(config, "NPCManagement", "usePerCellBaseline", true);
            s_settings.cellNPCDelta = GetValue(config, "NPCManagement", "cellNPCDelta", 20);
            s_settings.disableInsteadOfDelete = GetValue(config, "NPCManagement", "disableInsteadOfDelete", true);
            s_settings.maxDisabledNPCs = GetValue(config, "NPCManagement", "maxDisabledNPCs", 50);
            s_settings.useSmartPrioritization = GetValue(config, "NPCManagement", "useSmartPrioritization", true);
            s_settings.npcRestoreRate = GetValue(config, "NPCManagement", "npcRestoreRate", 5);
            s_settings.restoreBehindPlayer = GetValue(config, "NPCManagement", "restoreBehindPlayer", true);
            s_settings.npcWhitelistKeywords = GetValue(config, "NPCManagement", "npcWhitelistKeywords", 
                std::string("Jarl,Steward,Housecarl,Companion,Merchant,Vendor,Trainer,Innkeeper,Blacksmith"));
            s_settings.npcBlacklistKeywords = GetValue(config, "NPCManagement", "npcBlacklistKeywords", 
                std::string("Summon,Conjure,Reanimate,Duplicate,Clone,Test,Debug"));
            s_settings.burdenInCombat = GetValue(config, "NPCManagement", "burdenInCombat", 30);
            s_settings.burdenHasMagicEffects = GetValue(config, "NPCManagement", "burdenHasMagicEffects", 15);
            s_settings.burdenComplexAI = GetValue(config, "NPCManagement", "burdenComplexAI", 15);
            s_settings.burdenSummoned = GetValue(config, "NPCManagement", "burdenSummoned", 50);
            s_settings.burdenDuplicate = GetValue(config, "NPCManagement", "burdenDuplicate", 100);
            
            // [Hotkeys]
            s_settings.menuToggleKey = GetValue(config, "Hotkeys", "menuToggleKey", std::string("F11"));
            
            // [ActorLOD]
            s_settings.actorLODEnabled = GetValue(config, "ActorLOD", "enabled", false);
            s_settings.actorLODUpdateInterval = GetValue(config, "ActorLOD", "updateInterval", 2.0f);
            s_settings.actorLODFullDetailDistance = GetValue(config, "ActorLOD", "fullDetailDistance", 3000.0f);
            s_settings.actorLODMediumDetailDistance = GetValue(config, "ActorLOD", "mediumDetailDistance", 6000.0f);
            s_settings.actorLODLlowDetailDistance = GetValue(config, "ActorLOD", "lowDetailDistance", 10000.0f);
            s_settings.actorLODHibernateDistance = GetValue(config, "ActorLOD", "hibernateDistance", 15000.0f);

            // [ActorLODDiagnostics] - opt-in diagnostic records for ActorLOD
            s_settings.actorLODDiagnosticsLevel = GetValue(config, "ActorLODDiagnostics", "actorLODDiagnosticsLevel", 1);
            s_settings.actorLODDiagnosticsMaxSizeMB = GetValue(config, "ActorLODDiagnostics", "actorLODDiagnosticsMaxSizeMB", 5);
            s_settings.actorLODDiagnosticsPerActorCooldownSeconds = GetValue(config, "ActorLODDiagnostics", "perActorCooldownSeconds", 30);
            s_settings.actorLODDiagnosticsGlobalRatePerSecond = GetValue(config, "ActorLODDiagnostics", "globalRatePerSecond", 20);

            // [Benchmark]
            s_settings.allowBuiltinActions = GetValue(config, "Benchmark", "allowBuiltinActions", true);

            // [MemoryLeakTracking] - REMOVED
            // Memory allocation hooks system has been completely removed.

        } catch (const std::exception& e) {
            auto log = spdlog::default_logger();
            if (log) {
                log->warn("Failed to parse TOML config: {}. Using defaults.", e.what());
            }
        }
    }

    const Settings& Get()
    {
        return s_settings;
    }

    Settings& GetMutable()
    {
        return s_settings;
    }

    bool Save(const std::string& tomlPath)
    {
        try {
            std::ofstream ofs(tomlPath);
            if (!ofs) {
                auto log = spdlog::default_logger();
                if (log) {
                    log->error("Failed to open TOML file for writing: {}", tomlPath);
                }
                return false;
            }

            // Write TOML file with all sections
            ofs << "# ═══════════════════════════════════════════════════════════════════════\n";
            ofs << "# Skyrim Crash Guard Configuration\n";
            ofs << "# Engine-Level Crash Recovery System\n";
            ofs << "# ═══════════════════════════════════════════════════════════════════════\n\n";

            ofs << "[General]\n";
            ofs << "enabled = " << (s_settings.enabled ? "true" : "false") << "\n";
            ofs << "safeMode = " << (s_settings.safeMode ? "true" : "false") << "  # When true, skip VEH/Hooks/ActorSpawn (escape hatch for troubleshooting)\n";
            ofs << "logLevel = " << s_settings.logLevel << "  # 0=off, 1=errors/warnings only, 2=info, 3=debug, 4=trace\n\n";

            ofs << "[VEH]\n";
            ofs << "enabled = " << (s_settings.vehEnabled ? "true" : "false") << "\n";
            ofs << "cascadeLimit = " << s_settings.cascadeLimit << "  # Max recovery attempts during cascade\n\n";

            ofs << "[Patches]\n";
            ofs << "enabled = " << (s_settings.patchesEnabled ? "true" : "false") << "\n\n";

            ofs << "[ProactiveValidation]\n";
            ofs << "enableMeshValidation = " << (s_settings.enableMeshValidation ? "true" : "false") << "\n";
            ofs << "enableAnimationValidation = " << (s_settings.enableAnimationValidation ? "true" : "false") << "\n";
            ofs << "enableScriptMonitoring = " << (s_settings.enableScriptMonitoring ? "true" : "false") << "\n";
            ofs << "enableCellValidation = " << (s_settings.enableCellValidation ? "true" : "false") << "\n\n";

            ofs << "[SafetyChecks]\n";
            ofs << "enableNullChecks = " << (s_settings.enableNullChecks ? "true" : "false") << "\n";
            ofs << "enableBoundsChecks = " << (s_settings.enableBoundsChecks ? "true" : "false") << "\n";
            ofs << "enableFormIDChecks = " << (s_settings.enableFormIDChecks ? "true" : "false") << "\n\n";

            ofs << "[StateManagement]\n";
            ofs << "enableStateSnapshots = " << (s_settings.enableStateSnapshots ? "true" : "false") << "\n";
            ofs << "maxSnapshotsPerSession = " << s_settings.maxSnapshotsPerSession << "\n\n";

            ofs << "[Learning]\n";
            ofs << "enableLearning = " << (s_settings.enableLearning ? "true" : "false") << "\n";
            ofs << "patternDatabasePath = \"" << s_settings.patternDatabasePath << "\"\n\n";

            ofs << "[Notifications]\n";
            ofs << "showNotifications = " << (s_settings.showNotifications ? "true" : "false") << "\n";
            ofs << "autoRecoverSafe = " << (s_settings.autoRecoverSafe ? "true" : "false") << "\n";
            ofs << "autoRecoverWarning = " << (s_settings.autoRecoverWarning ? "true" : "false") << "\n";
            ofs << "notificationTimeoutSeconds = " << s_settings.notificationTimeoutSeconds << "\n\n";

            ofs << "[UserNotifications]\n";
            ofs << "NotifyOnSafe = " << (s_settings.notifyOnSafe ? "true" : "false") << "\n";
            ofs << "NotifyOnWarning = " << (s_settings.notifyOnWarning ? "true" : "false") << "\n";
            ofs << "NotifyOnCritical = " << (s_settings.notifyOnCritical ? "true" : "false") << "\n";
            ofs << "NotifyOnFatal = " << (s_settings.notifyOnFatal ? "true" : "false") << "\n";
            ofs << "ShowToastForAutoRecovery = " << (s_settings.showToastForAutoRecovery ? "true" : "false") << "\n";
            ofs << "ToastDurationSeconds = " << s_settings.toastDurationSeconds << "\n";
            ofs << "DialogTimeoutSeconds = " << s_settings.dialogTimeoutSeconds << "\n";
            ofs << "TimeoutDefaultAction = \"" << s_settings.timeoutDefaultAction << "\"\n";
            ofs << "ShowTechnicalDetails = " << (s_settings.showTechnicalDetails ? "true" : "false") << "\n";
            ofs << "AllowCrashAnywayOption = " << (s_settings.allowCrashAnywayOption ? "true" : "false") << "\n";
            ofs << "BatchSimilarCrashes = " << (s_settings.batchSimilarCrashes ? "true" : "false") << "\n";
            ofs << "LogAllRecoveries = " << (s_settings.logAllRecoveries ? "true" : "false") << "\n";
            ofs << "LogSilentRecoveries = " << (s_settings.logSilentRecoveries ? "true" : "false") << "\n\n";

            ofs << "[Performance]\n";
            ofs << "scriptTimeoutMs = " << s_settings.scriptTimeoutMs << "\n";
            ofs << "maxRecoveryAttempts = " << s_settings.maxRecoveryAttempts << "\n\n";

            ofs << "[Logging]\n";
            ofs << "enableDetailedLogging = " << (s_settings.enableDetailedLogging ? "true" : "false") << "  # Set to true only for debugging\n";
            ofs << "logOnlyFailures = " << (s_settings.logOnlyFailures ? "true" : "false") << "  # Only log validation failures\n";
            ofs << "logSuccessfulRecoveries = " << (s_settings.logSuccessfulRecoveries ? "true" : "false") << "  # Don't log successful recoveries\n";
            ofs << "aggregatePatterns = " << (s_settings.aggregatePatterns ? "true" : "false") << "\n";
            ofs << "maxLogSizeMB = " << s_settings.maxLogSizeMB << "  # Rotate logs at this size\n";
            ofs << "maxLogFiles = " << s_settings.maxLogFiles << "  # Keep this many log files\n\n";

            // Per-subsystem debug toggles
            ofs << "enableInputDebugLogging = " << (s_settings.enableInputDebugLogging ? "true" : "false") << "\n";
            ofs << "enableVehDebugLogging = " << (s_settings.enableVehDebugLogging ? "true" : "false") << "\n";
            ofs << "enablePatchDebugLogging = " << (s_settings.enablePatchDebugLogging ? "true" : "false") << "\n";
            ofs << "enablePapyrusDebugLogging = " << (s_settings.enablePapyrusDebugLogging ? "true" : "false") << "\n";
            ofs << "enablePerfTracing = " << (s_settings.enablePerfTracing ? "true" : "false") << "\n\n";

            ofs << "[InputDiagnostics]\n";
            ofs << "enableInputDiagnostics = " << (s_settings.enableInputDiagnostics ? "true" : "false") << "  # Enable diagnostic logging for F11 menu input\n\n";

            ofs << "[Compatibility]\n";
            ofs << "# Compatibility modes are automatically enabled when external tools are detected.\n";
            ofs << "# CrashLogger: Auto-detected via DLL presence\n";
            ofs << "# S.L.A.C.K.: Auto-detected via DLL presence\n";
            ofs << "# No manual configuration needed.\n\n";

            ofs << "[PapyrusValidation]\n";
            ofs << "enabled = " << (s_settings.papyrusValidationEnabled ? "true" : "false") << "\n";
            ofs << "logFailures = " << (s_settings.papyrusValidationLogFailures ? "true" : "false") << "\n";
            ofs << "strictMode = " << (s_settings.papyrusValidationStrictMode ? "true" : "false") << "  # Block function calls that fail validation\n\n";

            ofs << "[InputConflictPrevention]\n";
            ofs << "enabled = " << (s_settings.enableInputConflictPrevention ? "true" : "false") << "\n";
            ofs << "blockCameraZoom = " << (s_settings.blockCameraZoomInMenus ? "true" : "false") << "\n";
            ofs << "blockFavorites = " << (s_settings.blockFavoritesInMenus ? "true" : "false") << "\n";
            ofs << "autoDetectModdedMenus = " << (s_settings.autoDetectModdedMenus ? "true" : "false") << "\n";
            ofs << "enableInputTracking = " << (s_settings.enableInputTracking ? "true" : "false") << "\n";
            
            // Write custom scrollable menus array
            ofs << "customScrollableMenus = [";
            for (size_t i = 0; i < s_settings.customScrollableMenus.size(); ++i) {
                if (i > 0) ofs << ", ";
                ofs << "\"" << s_settings.customScrollableMenus[i] << "\"";
            }
            ofs << "]\n\n";

            ofs << "[ImGui]\n";
            ofs << "disableMenu = " << (s_settings.disableImGuiMenu ? "true" : "false") << "  # Set to true to completely disable the F11 menu\n";
            ofs << "enableDebugLogging = " << (s_settings.enableImGuiDebugLogs ? "true" : "false") << "\n\n";
            ofs << "allowInVR = " << (s_settings.allowImGuiInVR ? "true" : "false") << "  # Allow ImGui/F11 menu in VR (may not work without IVRCompositor hook)\n\n";
            
            ofs << "[PerformanceOverlay]\n";
            ofs << "enabled = " << (s_settings.overlayEnabled ? "true" : "false") << "\n";
            ofs << "showFPS = " << (s_settings.overlayShowFPS ? "true" : "false") << "\n";
            ofs << "showFrameTime = " << (s_settings.overlayShowFrameTime ? "true" : "false") << "\n";
            ofs << "showMemory = " << (s_settings.overlayShowMemory ? "true" : "false") << "\n";
            ofs << "showCrashStats = " << (s_settings.overlayShowCrashStats ? "true" : "false") << "\n";
            ofs << "showRecoveryStats = " << (s_settings.overlayShowRecoveryStats ? "true" : "false") << "\n";
            ofs << "showPatternStats = " << (s_settings.overlayShowPatternStats ? "true" : "false") << "\n";
            ofs << "position = " << s_settings.overlayPosition << "  # 0=TopLeft, 1=TopRight, 2=BottomLeft, 3=BottomRight\n";
            ofs << "backgroundAlpha = " << s_settings.overlayBackgroundAlpha << "\n";
            ofs << "textAlpha = " << s_settings.overlayTextAlpha << "\n";
            ofs << "scale = " << s_settings.overlayScale << "\n\n";
            
            ofs << "[ResourceLimiter]\n";
            ofs << "maxActors = " << s_settings.maxActors << "\n";
            ofs << "maxReferences = " << s_settings.maxReferences << "\n";
            ofs << "maxParticles = " << s_settings.maxParticles << "\n";
            ofs << "autoCullingEnabled = " << (s_settings.autoCullingEnabled ? "true" : "false") << "\n";
            ofs << "dynamicLimitAdjustment = " << (s_settings.dynamicLimitAdjustment ? "true" : "false") << "\n\n";
            ofs << "cullPerTick = " << s_settings.cullPerTick << "  # How many actors to process per tick during incremental cull\n\n";
            ofs << "autoManageNPCs = " << (s_settings.autoManageNPCs ? "true" : "false") << "  # When true, automatically manage NPC population (throttle/spawn/cull)\n\n";
            
            ofs << "[NPCManagement]\n";
            ofs << "# ═══════════════════════════════════════════════════════════════════════\n";
            ofs << "# NPC Management System - Reactive approach with smart prioritization\n";
            ofs << "# ═══════════════════════════════════════════════════════════════════════\n";
            ofs << "# This system monitors NPC count every 2 seconds and immediately removes\n";
            ofs << "# excess NPCs when over threshold. Uses smart burden-based prioritization\n";
            ofs << "# to remove the most performance-heavy NPCs first.\n\n";
            
            ofs << "npcToolsToasts = " << (s_settings.npcToolsToasts ? "true" : "false") << "  # Show toast notifications for NPC actions\n";
            ofs << "npcReleaseRate = " << s_settings.npcReleaseRate << "  # How many queued NPCs to release at once (1-20)\n";
            ofs << "npcCleanupRate = " << s_settings.npcCleanupRate << "  # How many dead bodies to clean at once (1-50)\n";
            ofs << "maxDeadBodies = " << s_settings.maxDeadBodies << "  # Max dead bodies before auto-cleanup (5-100)\n\n";
            
            ofs << "# Per-Cell Baseline System\n";
            ofs << "# Learns the normal NPC count for each cell (e.g., modded cities with 200 NPCs)\n";
            ofs << "# and only removes NPCs when you exceed baseline + delta\n";
            ofs << "usePerCellBaseline = " << (s_settings.usePerCellBaseline ? "true" : "false") << "\n";
            ofs << "cellNPCDelta = " << s_settings.cellNPCDelta << "  # How many NPCs above baseline to allow (5-100)\n\n";
            
            ofs << "# Disable vs Delete\n";
            ofs << "# When true, NPCs are disabled (hidden) instead of deleted, allowing restoration\n";
            ofs << "disableInsteadOfDelete = " << (s_settings.disableInsteadOfDelete ? "true" : "false") << "\n";
            ofs << "maxDisabledNPCs = " << s_settings.maxDisabledNPCs << "  # Max NPCs to keep disabled (10-200)\n";
            ofs << "npcRestoreRate = " << s_settings.npcRestoreRate << "  # How many disabled NPCs to restore at once (1-20)\n";
            ofs << "restoreBehindPlayer = " << (s_settings.restoreBehindPlayer ? "true" : "false") << "  # Only restore NPCs behind player (out of view)\n\n";
            
            ofs << "# Smart Prioritization\n";
            ofs << "# Uses burden scoring to remove most performance-heavy NPCs first\n";
            ofs << "useSmartPrioritization = " << (s_settings.useSmartPrioritization ? "true" : "false") << "\n\n";
            
            ofs << "# Whitelist/Blacklist Keywords (comma-separated)\n";
            ofs << "# Whitelist: NPCs with these keywords will NEVER be disabled\n";
            ofs << "# Add your custom keywords: \"MyImportantNPC,QuestGiver,DoNotRemove\"\n";
            ofs << "npcWhitelistKeywords = \"" << s_settings.npcWhitelistKeywords << "\"\n\n";
            
            ofs << "# Blacklist: NPCs with these keywords will be disabled FIRST\n";
            ofs << "# Add your custom keywords: \"Spam,Duplicate,Test\"\n";
            ofs << "npcBlacklistKeywords = \"" << s_settings.npcBlacklistKeywords << "\"\n\n";
            
            ofs << "# Burden Weights - Higher = more likely to be disabled\n";
            ofs << "# Adjust these to fine-tune which NPCs are considered \"heavy\"\n";
            ofs << "burdenInCombat = " << s_settings.burdenInCombat << "  # NPC is actively fighting\n";
            ofs << "burdenHasMagicEffects = " << s_settings.burdenHasMagicEffects << "  # NPC has active spell effects\n";
            ofs << "burdenComplexAI = " << s_settings.burdenComplexAI << "  # NPC is running AI packages\n";
            ofs << "burdenSummoned = " << s_settings.burdenSummoned << "  # Summoned/conjured creatures\n";
            ofs << "burdenDuplicate = " << s_settings.burdenDuplicate << "  # Duplicate NPCs (2nd+ copy)\n\n";
            
            ofs << "[Hotkeys]\n";
            ofs << "menuToggleKey = \"" << s_settings.menuToggleKey << "\"  # Hotkey to toggle F11 menu\n\n";

            ofs << "[Benchmark]\n";
            ofs << "allowBuiltinActions = " << (s_settings.allowBuiltinActions ? "true" : "false") << "  # Allow built-in automated actions (spawn/hide/restore)\n\n";

            ofs.close();

            auto log = spdlog::default_logger();
            if (log) {
                log->info("Configuration saved to {}", tomlPath);
            }
            
            return true;

        } catch (const std::exception& e) {
            auto log = spdlog::default_logger();
            if (log) {
                log->error("Failed to save TOML config: {}", e.what());
            }
            return false;
        }
    }

}  // namespace Config
