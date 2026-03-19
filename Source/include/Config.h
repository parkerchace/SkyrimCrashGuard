// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <string>
#include <vector>

/// TOML-based configuration loaded from SkyrimCrashGuard.toml.
namespace Config {

    struct Settings {
        // [General]
        bool        enabled       = true;
        bool        safeMode      = false; // When true, skip VEH/Hooks/ActorSpawn (escape hatch)
        int         logLevel      = 1;     // 0=off, 1=info, 2=debug, 3=trace

        // [VEH]
        bool        vehEnabled    = true;
        int         cascadeLimit  = 3;     // Max recovery attempts during cascade
        
        // High-frequency crash throttling
        bool        enableModuleThrottling = true;  // Enable per-module crash frequency throttling
        int         moduleThrottleThreshold = 20;   // Crashes before entering silent mode
        int         moduleThrottleWindowMs = 2000;  // Tracking window in milliseconds
        int         moduleSilentDurationMs = 30000; // Duration of silent recovery mode
        int         moduleRelogIntervalMs = 60000;  // Re-log interval while in silent mode

        // [Patches]
        bool        patchesEnabled = true;

        // [ProactiveValidation]
        bool        enableMeshValidation      = true;
        bool        enableAnimationValidation = true;
        bool        enableScriptMonitoring    = true;
        bool        enableCellValidation      = true;

        // [SafetyChecks]
        bool        enableNullChecks    = true;
        bool        enableBoundsChecks  = true;
        bool        enableFormIDChecks  = true;

        // [StateManagement]
        bool        enableStateSnapshots = true;
        int         maxSnapshotsPerSession = 100;

        // [Learning]
        bool        enableLearning = true;
        std::string patternDatabasePath = "Data/SKSE/Plugins/CrashGuard/patterns.json";

        // [Notifications]
        bool        showNotifications = true;
        bool        autoRecoverSafe = true;
        bool        autoRecoverWarning = false;
        int         notificationTimeoutSeconds = 30;

        // [UserNotifications]
        bool        notifyOnSafe = false;
        bool        notifyOnWarning = false;
        bool        notifyOnCritical = true;
        bool        notifyOnFatal = true;
        bool        showToastForAutoRecovery = true;
        int         toastDurationSeconds = 5;
        int         dialogTimeoutSeconds = 30;
        std::string timeoutDefaultAction = "Continue";
        bool        showTechnicalDetails = false;
        bool        allowCrashAnywayOption = true;
        bool        batchSimilarCrashes = true;
        bool        logAllRecoveries = true;
        bool        logSilentRecoveries = false;

        // [Performance]
        int         scriptTimeoutMs = 5000;
        int         maxRecoveryAttempts = 3;

        // [Logging]
        bool        enableDetailedLogging = false;  // Default to false to reduce log bloat
        bool        logOnlyFailures = true;  // Only log failures, not successes
        bool        logSuccessfulRecoveries = false;  // Don't log successful recoveries
        bool        aggregatePatterns = true;
        int         maxLogSizeMB = 10;  // Rotate logs at this size
        int         maxLogFiles = 3;  // Keep this many log files

        // Per-subsystem debug toggles (disabled by default to avoid log spam)
        bool        enableInputDebugLogging = false;   // ImGui/input per-frame diagnostics
        bool        enableVehDebugLogging = false;     // VEH/exception handling internals
        bool        enablePatchDebugLogging = false;   // PatchEngine and patch application
        bool        enablePapyrusDebugLogging = false; // Papyrus validation and hook logs
        bool        enablePerfTracing = false;         // Lightweight performance tracing
        // [InputDiagnostics]
        bool        enableInputDiagnostics = false;  // Enable diagnostic logging for F11 menu input

        // [Compatibility]
        // Compatibility modes are now auto-detected. No manual configuration needed.
        // CrashLogger: Auto-detected via CrashLoggerDetector
        // S.L.A.C.K.: Auto-detected via CoSaveManager

        // [PapyrusValidation]
        bool        papyrusValidationEnabled = true;
        bool        papyrusValidationLogFailures = true;
        bool        papyrusValidationStrictMode = false;

        // [InputConflictPrevention]
        bool        enableInputConflictPrevention = true;
        bool        blockCameraZoomInMenus = true;
        bool        blockFavoritesInMenus = true;
        bool        autoDetectModdedMenus = true;
        bool        enableInputTracking = true;  // Track which inputs each menu uses
        std::vector<std::string> customScrollableMenus;  // User-defined modded menus

        // [ImGui]
        bool        disableImGuiMenu = false;  // Disable F11 menu entirely (TOML-only config)
        bool        enableImGuiDebugLogs = false; // Enable ImGui input debug logging (debug level)
        bool        allowImGuiInVR = true;   // Allow ImGui/F11 menu in VR when true

        // [PerformanceOverlay]
        bool        overlayEnabled = false;
        bool        overlayShowFPS = true;
        bool        overlayShowFrameTime = true;
        bool        overlayShowMemory = true;
        bool        overlayShowCrashStats = true;
        bool        overlayShowRecoveryStats = true;
        bool        overlayShowPatternStats = false;
        int         overlayPosition = 1; // 0=TopLeft, 1=TopRight, 2=BottomLeft, 3=BottomRight
        float       overlayBackgroundAlpha = 0.35f;
        float       overlayTextAlpha = 1.0f;
        float       overlayScale = 1.0f;

        // [ResourceLimiter]
        int         maxActors = 50;
        int         maxReferences = 20000;
        int         maxParticles = 5000;
        bool        autoCullingEnabled = true;
        bool        dynamicLimitAdjustment = true;

        // [Hotkeys]
        std::string menuToggleKey = "F11";

        // [ActorLOD]
        bool        actorLODEnabled = false;    // Actor LOD manager initialization (disabled by default in code)
        float       actorLODUpdateInterval = 2.0f;
        float       actorLODFullDetailDistance = 3000.0f;
        float       actorLODMediumDetailDistance = 6000.0f;
        float       actorLODLlowDetailDistance = 10000.0f;
        float       actorLODHibernateDistance = 15000.0f;
        // [ActorLODDiagnostics]
        // Diagnostic control for ActorLOD subsystem
        // 0 = Off, 1 = TransitionsOnly, 2 = Verbose (consider/optimal/skip/transition)
        int         actorLODDiagnosticsLevel = 1; // Default to TransitionsOnly
        int         actorLODDiagnosticsMaxSizeMB = 5; // Rotate at this size
        int         actorLODDiagnosticsPerActorCooldownSeconds = 30; // Per-actor minimum seconds between verbose records
        int         actorLODDiagnosticsGlobalRatePerSecond = 20; // Global cap for diagnostic records per second
        // [Benchmark]
        // Allow built-in automated benchmark actions (spawn/hide/restore).
        // These actions may modify actors in the loaded cell; enabled by default
        // but can be turned off via TOML if desired.
        bool        allowBuiltinActions = true;
        float       maxFreezeDistance = 8192.0f;
        float       updateInterval = 0.5f;

        // Spawn queue / deferred spawn tuning
        bool        enableDeferredSpawning = true;
        int         deferredSpawnThreshold = 50; // If a single spawn request requests more than this, defer
        int         deferredSpawnPerTick = 10;   // How many spawn requests to process per tick
        int         cullPerTick = 10;            // How many actors to cull per tick for incremental cull
        
        // NPC Tools
        bool        npcToolsToasts = true; // Show toast notifications for NPC Tools actions
        bool        autoManageNPCs = true; // When true, auto-manage NPC population (throttle spawns / aggressive cull)
        int         npcReleaseRate = 5;    // How many queued NPCs to release at once (1-20)
        int         npcCleanupRate = 10;   // How many dead bodies to clean at once (1-50)
        int         maxDeadBodies = 20;    // Max dead bodies before auto-cleanup triggers (5-100)
        
        // NPC Management Strategy
        bool        usePerCellBaseline = true;  // Use per-cell baseline instead of global threshold
        int         cellNPCDelta = 20;          // How many NPCs above baseline to allow (5-100)
        bool        disableInsteadOfDelete = true; // Disable NPCs instead of deleting (allows restoration)
        int         maxDisabledNPCs = 50;       // Max NPCs to keep disabled (10-200)
        bool        useSmartPrioritization = true; // Prioritize which NPCs to remove based on burden
        int         npcRestoreRate = 5;         // How many disabled NPCs to restore at once (1-20)
        bool        restoreBehindPlayer = true; // Only restore NPCs that are behind player (out of view)
        
        // NPC Whitelist/Blacklist (comma-separated keywords)
        // Whitelist: NPCs with these keywords in their name will NEVER be disabled
        // Examples: "Jarl" protects all Jarls, "Lydia" protects your housecarl
        // Add custom keywords here: "MyCustomNPC,ImportantQuest,DoNotRemove"
        std::string npcWhitelistKeywords = "Jarl,Steward,Housecarl,Companion,Merchant,Vendor,Trainer,Innkeeper,Blacksmith";
        
        // Blacklist: NPCs with these keywords will be disabled FIRST when over threshold
        // Examples: "Summon" removes conjured creatures first, "Bandit" removes generic enemies
        // Add custom keywords here: "Spam,Duplicate,Test"
        std::string npcBlacklistKeywords = "Summon,Conjure,Reanimate,Duplicate,Clone,Test,Debug";
        
        // Burden Weights - Higher = more likely to be disabled
        // Adjust these to fine-tune which NPCs are considered "heavy"
        // Combat NPCs are prioritized for removal to reduce performance impact
        int         burdenInCombat = 30;        // NPC is actively fighting
        int         burdenHasMagicEffects = 15; // NPC has active spell effects
        int         burdenComplexAI = 15;       // NPC is running AI packages (patrol, travel, etc.)
        int         burdenSummoned = 50;        // Summoned/conjured creatures (temporary by design)
        int         burdenDuplicate = 100;      // Duplicate NPCs (2nd+ copy of same NPC)

        // [MemoryLeakTracking] - REMOVED
        // Memory allocation hooks system has been completely removed.
        // See ALLOCATION_HOOK_ANALYSIS.md for why this approach was abandoned.
    };

    /// Load settings from TOML. Call during plugin init.
    void Load(const std::string& tomlPath);

    /// Save settings to TOML. Returns true on success, false on failure.
    bool Save(const std::string& tomlPath);

    /// Get current settings (read-only).
    const Settings& Get();

    /// Get mutable settings (for MCM).
    Settings& GetMutable();

}  // namespace Config
