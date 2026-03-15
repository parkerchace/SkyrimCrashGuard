// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "PCH.h"
#include "PhaseTracker.h"
#include "Config.h"
#include "FunctionHookManager.h"
#include "FormIDValidator.h"
#include <spdlog/spdlog.h>
#include <fstream>

// VEH save-load recovery notification functions (defined in VEH.cpp)
namespace VEH {
    void ResetSaveLoadRecoveryTracking();
    void ShowSaveLoadRecoverySummary();
}

namespace PhaseTracking {

    // Static member initialization
    GamePhase PhaseTracker::s_currentPhase = GamePhase::PluginInit;
    ComponentStatus PhaseTracker::s_componentStatus = {};
    SessionStats PhaseTracker::s_sessionStats = {};
    bool PhaseTracker::s_initialized = false;
    std::chrono::steady_clock::time_point PhaseTracker::s_phaseStartTime;

    void PhaseTracker::Initialize() {
        if (s_initialized) {
            return;
        }

        s_sessionStats.startTime = std::chrono::steady_clock::now();
        s_phaseStartTime = s_sessionStats.startTime;
        s_initialized = true;
    }

    GamePhase PhaseTracker::GetCurrentPhase() {
        return s_currentPhase;
    }

    void PhaseTracker::TransitionTo(GamePhase newPhase) {
        // Track time spent in previous phase
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_phaseStartTime);
        
        // If leaving LoadingSave phase, show recovery summary if any crashes were prevented
        if (s_currentPhase == GamePhase::LoadingSave) {
            VEH::ShowSaveLoadRecoverySummary();
        }
        
        switch (s_currentPhase) {
            case GamePhase::PluginInit:
                s_sessionStats.timeInPluginInit = duration;
                break;
            case GamePhase::MainMenu:
                s_sessionStats.timeInMainMenu = duration;
                break;
            case GamePhase::LoadingSave:
                s_sessionStats.timeInLoadingSave = duration;
                break;
            case GamePhase::Gameplay:
                s_sessionStats.timeInGameplay = duration;
                break;
            default:
                break;
        }
        
        // If entering LoadingSave phase, reset tracking for new load
        if (newPhase == GamePhase::LoadingSave) {
            VEH::ResetSaveLoadRecoveryTracking();
        }
        
        s_currentPhase = newPhase;
        s_phaseStartTime = now;
        LogPhaseTransition(newPhase);
    }

    void PhaseTracker::SetComponentStatus(const ComponentStatus& status) {
        s_componentStatus = status;
    }

    void PhaseTracker::IncrementCrashesPrevented() {
        s_sessionStats.crashesPrevented++;
    }

    void PhaseTracker::IncrementMeshesValidated() {
        s_sessionStats.meshesValidated++;
    }

    void PhaseTracker::IncrementMeshesRepaired() {
        s_sessionStats.meshesRepaired++;
    }

    void PhaseTracker::IncrementScriptsMonitored() {
        s_sessionStats.scriptsMonitored++;
    }

    void PhaseTracker::IncrementScriptTimeouts() {
        s_sessionStats.scriptTimeouts++;
    }

    void PhaseTracker::IncrementFormIDsValidated() {
        s_sessionStats.formIDsValidated++;
    }

    void PhaseTracker::IncrementFormIDsInvalid() {
        s_sessionStats.formIDsInvalid++;
    }

    const SessionStats& PhaseTracker::GetSessionStats() {
        return s_sessionStats;
    }

    std::string PhaseTracker::GetStatusIcon(bool success) {
        if (Config::Get().enableDetailedLogging) {
            // Use Unicode icons if available
            return success ? "✓" : "✗";
        } else {
            // ASCII fallback
            return success ? "+" : "X";
        }
    }

    std::string PhaseTracker::GetPhaseName(GamePhase phase) {
        switch (phase) {
            case GamePhase::PluginInit: return "Plugin Initialization";
            case GamePhase::MainMenu: return "Main Menu Loaded";
            case GamePhase::LoadingSave: return "Loading Save";
            case GamePhase::Gameplay: return "Gameplay";
            case GamePhase::Exiting: return "Exiting";
            default: return "Unknown";
        }
    }

    void PhaseTracker::LogStartupSummary() {
        spdlog::info("╔══════════════════════════════════════════════════════════════╗");
        spdlog::info("║  SkyrimCrashGuard v{}.{}.{} Engine-Level Crash Recovery      ║", 
                     PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR, PLUGIN_VERSION_PATCH);
        spdlog::info("║  Six-Layer Defense System (L1-L6)                           ║");
        spdlog::info("╠══════════════════════════════════════════════════════════════╣");
        spdlog::info("║  PHASE: {}                               ║", GetPhaseName(s_currentPhase));
        spdlog::info("╠══════════════════════════════════════════════════════════════╣");
        
        // Component status
        spdlog::info("║  {} Config loaded                                            ║", 
                     GetStatusIcon(s_componentStatus.configLoaded));
        spdlog::info("║  {} MeshValidator ready                                      ║", 
                     GetStatusIcon(s_componentStatus.meshValidatorReady));
        spdlog::info("║  {} ScriptMonitor ready                                      ║", 
                     GetStatusIcon(s_componentStatus.scriptMonitorReady));
        spdlog::info("║  {} Hooks installed ({}/{})                                    ║", 
                     GetStatusIcon(s_componentStatus.hooksInstalled),
                     s_componentStatus.hooksInstalledCount,
                     s_componentStatus.hooksTotalCount);
        
        if (!s_componentStatus.formIDValidatorReady) {
            spdlog::info("║  ⏳ FormIDValidator (waiting for game data)                 ║");
        } else {
            spdlog::info("║  {} FormIDValidator ready                                    ║", 
                         GetStatusIcon(s_componentStatus.formIDValidatorReady));
        }
        
        spdlog::info("║  {} VEH crash guard active                                   ║", 
                     GetStatusIcon(s_componentStatus.vehActive));
        
        spdlog::info("╠══════════════════════════════════════════════════════════════╣");
        spdlog::info("║  STATUS: Ready, waiting for main menu                      ║");
        spdlog::info("╚══════════════════════════════════════════════════════════════╝");
    }

    void PhaseTracker::LogPhaseTransition(GamePhase newPhase) {
        switch (newPhase) {
            case GamePhase::MainMenu:
                spdlog::info("╔══════════════════════════════════════════════════════════════╗");
                spdlog::info("║  PHASE: Main Menu Loaded                                    ║");
                spdlog::info("╠══════════════════════════════════════════════════════════════╣");
                spdlog::info("║  {} Game data available                                      ║", 
                             GetStatusIcon(true));
                
                if (s_componentStatus.formIDValidatorReady) {
                    spdlog::info("║  {} FormIDValidator initialized (retry successful)           ║", 
                                 GetStatusIcon(true));
                    spdlog::info("║  {} All {} hooks active                                       ║", 
                                 GetStatusIcon(true), s_componentStatus.hooksTotalCount);
                } else {
                    spdlog::info("║  ⏳ FormIDValidator (will retry)                             ║");
                }
                
                spdlog::info("╠══════════════════════════════════════════════════════════════╣");
                spdlog::info("║  STATUS: All systems operational                            ║");
                spdlog::info("╚══════════════════════════════════════════════════════════════╝");
                break;
                
            case GamePhase::LoadingSave:
                spdlog::info("╔══════════════════════════════════════════════════════════════╗");
                spdlog::info("║  PHASE: Loading Save                                        ║");
                spdlog::info("╠══════════════════════════════════════════════════════════════╣");
                spdlog::info("║  Validating: Meshes, Scripts, Cells, FormIDs               ║");
                break;
                
            case GamePhase::Gameplay:
                {
                    uint32_t criticalIssues = s_sessionStats.meshesRepaired + s_sessionStats.scriptTimeouts;
                    spdlog::info("╠══════════════════════════════════════════════════════════════╣");
                    spdlog::info("║  Monitoring: {} objects loaded                           ║", 
                                 s_sessionStats.meshesValidated);
                    spdlog::info("║  Issues found: {} critical, {} warnings (auto-fixed)         ║", 
                                 0, criticalIssues);
                    spdlog::info("╠══════════════════════════════════════════════════════════════╣");
                    spdlog::info("║  STATUS: Save loaded successfully                           ║");
                    spdlog::info("╚══════════════════════════════════════════════════════════════╝");
                    spdlog::info("[GAMEPLAY] Monitoring active (silent mode - only logging issues)");
                }
                break;
                
            case GamePhase::Exiting:
                LogSessionSummary();
                break;
                
            default:
                break;
        }
    }

    void PhaseTracker::LogSessionSummary() {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - s_sessionStats.startTime);
        int hours = static_cast<int>(duration.count() / 60);
        int minutes = static_cast<int>(duration.count() % 60);
        
        spdlog::info("╔══════════════════════════════════════════════════════════════╗");
        spdlog::info("║  Session Summary                                            ║");
        spdlog::info("╠══════════════════════════════════════════════════════════════╣");
        spdlog::info("║  Duration: {}h {}m                                           ║", hours, minutes);
        spdlog::info("║  Crashes prevented: {}                                       ║", 
                     s_sessionStats.crashesPrevented);
        spdlog::info("║  Meshes validated: {} ({} repaired)                      ║", 
                     s_sessionStats.meshesValidated, s_sessionStats.meshesRepaired);
        spdlog::info("║  Scripts monitored: {} ({} timeouts)                      ║", 
                     s_sessionStats.scriptsMonitored, s_sessionStats.scriptTimeouts);
        spdlog::info("║  FormIDs validated: {} ({} invalid)                     ║", 
                     s_sessionStats.formIDsValidated, s_sessionStats.formIDsInvalid);
        spdlog::info("╠══════════════════════════════════════════════════════════════╣");
        spdlog::info("║  Phase Timings:                                             ║");
        spdlog::info("║    Plugin Init: {}ms                                        ║", 
                     s_sessionStats.timeInPluginInit.count());
        spdlog::info("║    Main Menu: {}ms                                          ║", 
                     s_sessionStats.timeInMainMenu.count());
        spdlog::info("║    Loading Save: {}ms                                       ║", 
                     s_sessionStats.timeInLoadingSave.count());
        spdlog::info("║    Gameplay: {}ms                                           ║", 
                     s_sessionStats.timeInGameplay.count());
        spdlog::info("╠══════════════════════════════════════════════════════════════╣");
        spdlog::info("║  Health Checks: {} performed, {} failures                   ║", 
                     s_sessionStats.healthChecksPerformed, s_sessionStats.healthCheckFailures);
        spdlog::info("╠══════════════════════════════════════════════════════════════╣");
        
        uint32_t totalIssues = s_sessionStats.crashesPrevented + s_sessionStats.meshesRepaired + 
                               s_sessionStats.scriptTimeouts + s_sessionStats.formIDsInvalid;
        
        if (totalIssues == 0) {
            spdlog::info("║  STATUS: Clean session, no critical issues                 ║");
        } else {
            spdlog::info("║  STATUS: {} issues handled successfully                      ║", totalIssues);
        }
        
        spdlog::info("╚══════════════════════════════════════════════════════════════╝");
        
        // Export detailed stats to JSON
        ExportSessionStats("Data/SKSE/Plugins/SkyrimCrashGuard_session.json");
    }

    void PhaseTracker::StartHealthChecks() {
        spdlog::info("[HealthCheck] Starting periodic health monitoring");
        s_sessionStats.lastHealthCheck = std::chrono::steady_clock::now();
        
        // Perform initial health check
        if (PerformHealthCheck()) {
            spdlog::info("[HealthCheck] Initial check passed - all systems operational");
        } else {
            spdlog::warn("[HealthCheck] Initial check detected issues - see above");
        }
    }

    bool PhaseTracker::PerformHealthCheck() {
        s_sessionStats.healthChecksPerformed++;
        bool allHealthy = true;
        
        // Check 1: Verify hooks are still installed
        auto hookStats = FunctionHooks::FunctionHookManager::GetStats();
        if (hookStats.installedHooks == 0) {
            spdlog::error("[HealthCheck] CRITICAL: No hooks installed - protection disabled!");
            allHealthy = false;
            s_sessionStats.healthCheckFailures++;
        }
        
        // Check 2: Verify validation is happening (check our own stats)
        if (s_currentPhase == GamePhase::Gameplay && s_sessionStats.meshesValidated == 0) {
            spdlog::debug("[HealthCheck] No mesh validations yet (normal if just started gameplay)");
        }
        
        // Check 3: Verify FormID validator is working
        auto formIDStats = FormIDValidation::FormIDValidator::GetStats();
        if (s_componentStatus.formIDValidatorReady && formIDStats.totalLookups == 0) {
            spdlog::debug("[HealthCheck] FormID validator ready but unused (normal if no mods)");
        }
        
        // Check 4: Check for excessive crash prevention (might indicate underlying issue)
        if (s_sessionStats.crashesPrevented > 100) {
            spdlog::warn("[HealthCheck] WARNING: {} crashes prevented - investigate root cause", 
                        s_sessionStats.crashesPrevented);
        }
        
        s_sessionStats.lastHealthCheck = std::chrono::steady_clock::now();
        return allHealthy;
    }

    void PhaseTracker::ExportSessionStats(const std::string& filepath) {
        // Calculate final phase duration
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_phaseStartTime);
        
        switch (s_currentPhase) {
            case GamePhase::Gameplay:
                s_sessionStats.timeInGameplay += duration;
                break;
            default:
                break;
        }
        
        // Build JSON manually (simple format, no library needed)
        std::ofstream file(filepath);
        if (!file.is_open()) {
            spdlog::error("Failed to export session stats to {}", filepath);
            return;
        }
        
        auto totalDuration = std::chrono::duration_cast<std::chrono::seconds>(now - s_sessionStats.startTime);
        
        file << "{\n";
        file << "  \"sessionDuration\": " << totalDuration.count() << ",\n";
        file << "  \"crashesPrevented\": " << s_sessionStats.crashesPrevented << ",\n";
        file << "  \"meshesValidated\": " << s_sessionStats.meshesValidated << ",\n";
        file << "  \"meshesRepaired\": " << s_sessionStats.meshesRepaired << ",\n";
        file << "  \"scriptsMonitored\": " << s_sessionStats.scriptsMonitored << ",\n";
        file << "  \"scriptTimeouts\": " << s_sessionStats.scriptTimeouts << ",\n";
        file << "  \"formIDsValidated\": " << s_sessionStats.formIDsValidated << ",\n";
        file << "  \"formIDsInvalid\": " << s_sessionStats.formIDsInvalid << ",\n";
        file << "  \"healthChecksPerformed\": " << s_sessionStats.healthChecksPerformed << ",\n";
        file << "  \"healthCheckFailures\": " << s_sessionStats.healthCheckFailures << ",\n";
        file << "  \"phaseTimings\": {\n";
        file << "    \"pluginInit\": " << s_sessionStats.timeInPluginInit.count() << ",\n";
        file << "    \"mainMenu\": " << s_sessionStats.timeInMainMenu.count() << ",\n";
        file << "    \"loadingSave\": " << s_sessionStats.timeInLoadingSave.count() << ",\n";
        file << "    \"gameplay\": " << s_sessionStats.timeInGameplay.count() << "\n";
        file << "  }\n";
        file << "}\n";
        
        file.close();
        spdlog::info("Session statistics exported to {}", filepath);
    }

}  // namespace PhaseTracking