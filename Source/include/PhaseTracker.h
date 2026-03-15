// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <string>
#include <chrono>

namespace PhaseTracking {

    /// Game phase enumeration
    enum class GamePhase {
        PluginInit,      // SKSE loading plugins
        MainMenu,        // Main menu displayed
        LoadingSave,     // Loading a save file
        Gameplay,        // Playing the game
        Exiting          // Game shutting down
    };

    /// Session statistics
    struct SessionStats {
        std::chrono::steady_clock::time_point startTime;
        uint32_t crashesPrevented = 0;
        uint32_t meshesValidated = 0;
        uint32_t meshesRepaired = 0;
        uint32_t scriptsMonitored = 0;
        uint32_t scriptTimeouts = 0;
        uint32_t formIDsValidated = 0;
        uint32_t formIDsInvalid = 0;
        
        // Performance metrics per phase
        std::chrono::milliseconds timeInPluginInit{0};
        std::chrono::milliseconds timeInMainMenu{0};
        std::chrono::milliseconds timeInLoadingSave{0};
        std::chrono::milliseconds timeInGameplay{0};
        
        // Health check metrics
        uint32_t healthChecksPerformed = 0;
        uint32_t healthCheckFailures = 0;
        std::chrono::steady_clock::time_point lastHealthCheck;
    };

    /// Component initialization status
    struct ComponentStatus {
        bool configLoaded = false;
        bool meshValidatorReady = false;
        bool scriptMonitorReady = false;
        bool hooksInstalled = false;
        bool formIDValidatorReady = false;
        bool vehActive = false;
        int hooksInstalledCount = 0;
        int hooksTotalCount = 0;
    };

    class PhaseTracker {
    public:
        /// Initialize the phase tracker
        static void Initialize();

        /// Get current game phase
        static GamePhase GetCurrentPhase();

        /// Transition to a new phase
        static void TransitionTo(GamePhase newPhase);

        /// Update component status
        static void SetComponentStatus(const ComponentStatus& status);

        /// Update session statistics
        static void IncrementCrashesPrevented();
        static void IncrementMeshesValidated();
        static void IncrementMeshesRepaired();
        static void IncrementScriptsMonitored();
        static void IncrementScriptTimeouts();
        static void IncrementFormIDsValidated();
        static void IncrementFormIDsInvalid();

        /// Get session statistics
        static const SessionStats& GetSessionStats();

        /// Log startup summary (consolidated)
        static void LogStartupSummary();

        /// Log phase transition
        static void LogPhaseTransition(GamePhase newPhase);

        /// Log session summary (on exit)
        static void LogSessionSummary();

        /// Helper to get status icon
        static std::string GetStatusIcon(bool success);

        /// Helper to get phase name
        static std::string GetPhaseName(GamePhase phase);

        /// Start periodic health checks (called when gameplay starts)
        static void StartHealthChecks();

        /// Perform a health check (verify hooks are still working)
        static bool PerformHealthCheck();

        /// Export session statistics to JSON file
        static void ExportSessionStats(const std::string& filepath);

    private:
        static GamePhase s_currentPhase;
        static ComponentStatus s_componentStatus;
        static SessionStats s_sessionStats;
        static bool s_initialized;
        static std::chrono::steady_clock::time_point s_phaseStartTime;
    };

}  // namespace PhaseTracking
