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

        // [Learning]
        bool        enableLearning = true;
        std::string patternDatabasePath = "Data/SKSE/Plugins/CrashGuard/patterns.json";

        // [Notifications]
        bool        showNotifications = true;
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

        // [MemoryLeakTracking] - REMOVED
        // Memory allocation hooks system has been completely removed.
        // See ALLOCATION_HOOK_ANALYSIS.md for why this approach was abandoned.

        /// Hash every field in this struct into a single 64-bit number.
        /// Used by the UI to answer "has anything changed since we last saved?"
        /// without needing a separate comparison line for every field.
        ///
        /// How this works: FNV-1a is a simple non-cryptographic hash.
        /// We start with a fixed "seed" number and repeatedly XOR each byte of each
        /// field into it, then multiply by a large prime. At the end, even a single
        /// changed bit anywhere produces a completely different result.
        uint64_t ComputeHash() const {
            constexpr uint64_t FNV_BASIS = 14695981039346656037ULL;
            constexpr uint64_t FNV_PRIME = 1099511628211ULL;
            uint64_t h = FNV_BASIS;

            // Fold a POD value (int, bool, float, enum) byte-by-byte into h
            auto mix = [&h](auto val) {
                const auto* b = reinterpret_cast<const uint8_t*>(&val);
                for (size_t i = 0; i < sizeof(val); ++i) {
                    h ^= b[i];
                    h *= FNV_PRIME;
                }
            };
            // Fold a std::string character-by-character
            auto mixStr = [&h](const std::string& s) {
                for (unsigned char c : s) { h ^= c; h *= FNV_PRIME; }
            };

            mix(enabled);           mix(safeMode);          mix(logLevel);
            mix(vehEnabled);        mix(cascadeLimit);
            mix(enableModuleThrottling);    mix(moduleThrottleThreshold);
            mix(moduleThrottleWindowMs);    mix(moduleSilentDurationMs);
            mix(moduleRelogIntervalMs);     mix(patchesEnabled);
            mix(enableMeshValidation);      mix(enableAnimationValidation);
            mix(enableScriptMonitoring);    mix(enableCellValidation);
            mix(enableLearning);            mixStr(patternDatabasePath);
            mix(showNotifications);         mix(notificationTimeoutSeconds);
            mix(notifyOnSafe);              mix(notifyOnWarning);
            mix(notifyOnCritical);          mix(notifyOnFatal);
            mix(showToastForAutoRecovery);  mix(toastDurationSeconds);
            mix(dialogTimeoutSeconds);      mixStr(timeoutDefaultAction);
            mix(showTechnicalDetails);      mix(allowCrashAnywayOption);
            mix(scriptTimeoutMs);           mix(maxRecoveryAttempts);
            mix(enableDetailedLogging);     mix(logOnlyFailures);
            mix(logSuccessfulRecoveries);   mix(aggregatePatterns);
            mix(maxLogSizeMB);              mix(maxLogFiles);
            mix(enableInputDiagnostics);
            mix(papyrusValidationEnabled);  mix(papyrusValidationLogFailures);
            mix(papyrusValidationStrictMode);
            mix(enableInputConflictPrevention); mix(blockCameraZoomInMenus);
            mix(blockFavoritesInMenus);     mix(autoDetectModdedMenus);
            mix(enableInputTracking);
            for (const auto& s : customScrollableMenus) mixStr(s);
            mix(disableImGuiMenu);          mix(enableImGuiDebugLogs);
            mix(allowImGuiInVR);
            mix(overlayEnabled);            mix(overlayShowFPS);
            mix(overlayShowFrameTime);      mix(overlayShowMemory);
            mix(overlayShowCrashStats);     mix(overlayShowRecoveryStats);
            mix(overlayShowPatternStats);   mix(overlayPosition);
            mix(overlayBackgroundAlpha);    mix(overlayTextAlpha);
            mix(overlayScale);

            return h;
        }
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
