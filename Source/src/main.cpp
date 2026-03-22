// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

// main.cpp
// SKSE Plugin Entry Point
//
// Skyrim Crash Guard
// Engine-Level Crash Recovery System
// Six-layer defense: L1 (Proactive) to L6 (Learning)
// Uses CommonLibSSE for game introspection and SKSE for function hooking
//
// Log location: Documents/My Games/Skyrim Special Edition/SKSE/SkyrimCrashGuard.log
// ═══════════════════════════════════════════════════════════════════════

#include <SKSE/SKSE.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <filesystem>
#include <fstream>
#include <Windows.h>
#include <ShlObj.h>

#include "Plugin.h"
#include "GameDetect.h"
#include "Config.h"
#include "VEH.h"
#include "Hooks.h"
#include "PatchEngine.h"
#include "Patches.h"
#include "AddressResolver.h"
#include "CrashCollector.h"
#include "TrainwreckBridge.h"
#include "FunctionHookManager.h"
#include "MeshValidator.h"
#include "ScriptMonitor.h"
#include "PhaseTracker.h"
#include "FormIDValidator.h"
#include "CoSaveManager.h"
#include "MemoryManager.h"
#include "PapyrusNativeFunctions.h"
#include "PapyrusNativeFunctionHook.h"
#include "PapyrusValidator.h"
#include "PresentHook.h"
// ResourceLimiter and ActorLODManager are disabled during refactor; headers retained in backups.
#include "MemoryPressureDetector.h"
#include "DeadlockDetector.h"

namespace {

    // Store the chosen log directory for other modules to use (e.g., VEH recovery reports)
    static std::filesystem::path s_pluginLogDirectory;

} // anonymous namespace

namespace Plugin {
    void SetLogDirectory(const std::filesystem::path& dir) {
        s_pluginLogDirectory = dir;
    }

    const std::filesystem::path& GetLogDirectory() {
        return s_pluginLogDirectory;
    }
}

namespace {

    // ── SEH-safe wrappers ──
    // These must be in separate functions because __try/__except cannot coexist
    // with C++ objects that have destructors (MSVC C2712)

    static int TryInstallHooks() {
        // Returns: 1=success, 0=SEH caught
        __try {
            Hooks::InstallHooks();
            return 1;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    static int TryInstallVEH() {
        // Returns: 1=success, 0=SEH caught
        __try {
            VEH::Install();
            return 1;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    // Helper to check REL/Address DB readiness
    bool IsRelocationDatabaseReady() {
        try {
            // Force REL's IDDatabase singleton to initialize and load the
            // address library file. If the address library is missing or
            // incompatible, this may throw or call report_and_fail.
            (void)REL::IDDatabase::get();
            if (auto log = spdlog::default_logger()) log->info("REL: IDDatabase initialized (module AE={}, VR={})", REL::Module::IsAE(), REL::Module::IsVR());
            return true;
        } catch (const std::exception& e) {
            if (auto log = spdlog::default_logger()) log->warn("REL: IDDatabase initialization failed: {}", e.what());
            return false;
        } catch (...) {
            if (auto log = spdlog::default_logger()) log->warn("REL: IDDatabase initialization failed (unknown error)");
            return false;
        }
    }


    void SetupLogging()
    {
        // Prefer the canonical Documents/My Games/<Game>/SKSE location as primary.
        std::filesystem::path logPath;
        std::filesystem::path canonicalLogDir;

        const auto& gameInfo = GameDetect::Detect();
        PWSTR documentsPathPtr = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documentsPathPtr))) {
            std::filesystem::path documentsPath = documentsPathPtr;
            CoTaskMemFree(documentsPathPtr);
            std::string docsDir = gameInfo.docsDir ? std::string(gameInfo.docsDir) : std::string("Skyrim Special Edition");
            canonicalLogDir = documentsPath / "My Games" / docsDir / "SKSE";
            std::error_code ec;
            std::filesystem::create_directories(canonicalLogDir, ec);
            logPath = canonicalLogDir / "SkyrimCrashGuard.log";
        }

        // If canonical failed, fall back to SKSE-provided log directory or current dir
        auto skseLogDir = SKSE::log::log_directory();
        if (logPath.empty() && skseLogDir && !skseLogDir->empty()) {
            canonicalLogDir = *skseLogDir;
            logPath = canonicalLogDir / "SkyrimCrashGuard.log";
        }
        if (logPath.empty()) {
            logPath = "SkyrimCrashGuard.log";
            canonicalLogDir = std::filesystem::current_path();
        }

        // Store canonical log dir for other modules
        Plugin::SetLogDirectory(canonicalLogDir);

        // Touch the primary file to ensure creation and detect permission issues
        std::error_code ec;
        std::filesystem::create_directories(canonicalLogDir, ec);
        bool touched = false;
        try {
            std::ofstream touch(logPath.string(), std::ios::app);
            if (touch) { touch.flush(); touch.close(); touched = true; }
        } catch (...) { touched = false; }
        {
            std::string dbg = std::format("CrashGuard: primary logPath='{}' touched={}\n", logPath.string(), touched ? "yes" : "no");
            OutputDebugStringA(dbg.c_str());
            fprintf(stderr, "%s", dbg.c_str());
        }

        // Build spdlog sinks: use a single canonical primary sink to ensure
        // strictly sequential writes into the main plugin log file.
        spdlog::sink_ptr primarySink;
        try {
            primarySink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
        } catch (const std::exception& e) {
            std::string err = std::format("CrashGuard: Failed to create primary sink: {}\n", e.what());
            OutputDebugStringA(err.c_str()); fprintf(stderr, "%s", err.c_str());
        }

        if (!primarySink) {
            std::string err = std::format("CrashGuard: No sinks available for '{}'.\n", logPath.string());
            OutputDebugStringA(err.c_str()); fprintf(stderr, "%s", err.c_str());
            return;
        }

        auto logger = std::make_shared<spdlog::logger>("SkyrimCrashGuard", primarySink);
        // Explicit pattern: timestamp, logger name, level and message. This
        // keeps log lines human-readable and consistent when inspecting
        // the single canonical log file.
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
        logger->set_level(spdlog::level::debug);
        // Flush on info to keep the file up-to-date while avoiding extreme
        // flush overhead from trace-level flushing.
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(logger));
        spdlog::info("═══════════════════════════════════════════════════════════════");
        spdlog::info("  SkyrimCrashGuard Log Started");
        if (auto l = spdlog::default_logger()) l->flush();
    }

    // SKSE messaging listener for phase detection and renderer-ready events
    void OnDataLoaded(SKSE::MessagingInterface::Message* msg)
    {
        auto log = spdlog::default_logger();
        if (!msg) return;

        if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
            // Install Present hook for ImGui now that renderer is available
            if (CrashGuard::PresentHook::Install()) {
                if (log) log->info("[ImGui] Overlay system initialized - Press F11 or hold L3+R3 to toggle menu");
            } else {
                if (log) log->warn("[ImGui] Failed to install overlay - F11 menu will not be available");
            }
        }
        else if (msg->type == SKSE::MessagingInterface::kNewGame || 
                 msg->type == SKSE::MessagingInterface::kPreLoadGame) {
            if (log) log->info("[PhaseDetection] Save loading started");
            PhaseTracking::PhaseTracker::TransitionTo(PhaseTracking::GamePhase::LoadingSave);
            // ActorLOD scheduler stop skipped (ActorLOD disabled)
            if (log) log->info("[ActorLOD] Scheduler stop skipped (ActorLOD disabled)");
        }
        else if (msg->type == SKSE::MessagingInterface::kPostLoadGame) {
            if (log) log->info("[PhaseDetection] Save loaded - transitioning to Gameplay");
            PhaseTracking::PhaseTracker::TransitionTo(PhaseTracking::GamePhase::Gameplay);
            PhaseTracking::PhaseTracker::StartHealthChecks();
            // Start ActorLOD scheduler now that gameplay is active. This
            // defers background ActorLOD work until the game is ready
            // to avoid interfering with main menu/load screens.
            // ActorLOD scheduler start skipped (ActorLOD disabled)
            if (log) log->info("[ActorLOD] Scheduler start skipped (ActorLOD disabled)");
        }
    }
    

    std::string FindConfigFile()
    {
        // Look for TOML config in SKSE/Plugins directory
        std::vector<std::filesystem::path> searchPaths = {
            "Data/SKSE/Plugins/SkyrimCrashGuard.toml",
            "SKSE/Plugins/SkyrimCrashGuard.toml",
            "SkyrimCrashGuard.toml"
        };

        for (const auto& path : searchPaths) {
            std::error_code ec;
            if (std::filesystem::exists(path, ec)) {
                return path.string();
            }
        }

        return "";
    }

    // Separate function to avoid C2712: __try in function with C++ unwind objects
    [[maybe_unused]] static bool TryInitActorLODManager() {
        spdlog::info("[ActorLOD] Initialization skipped (ActorLOD disabled)");
        return true;
    }

    void InitializePlugin()
    {
        // Initialize phase tracker
        PhaseTracking::PhaseTracker::Initialize();
        
        // Track component status
        PhaseTracking::ComponentStatus status = {};
        
        auto log = spdlog::default_logger();

        // ── Load config ──
        std::string configPath = FindConfigFile();
        if (!configPath.empty()) {
            Config::Load(configPath);
            status.configLoaded = true;
            if (log) log->info("Config loaded from {}", configPath);
        } else {
            if (log) log->warn("TOML config not found, using defaults");
        }

        if (!Config::Get().enabled) {
            if (log) log->critical("Plugin disabled in config, exiting");
            return;
        }

        // If address library validation failed, avoid initializing
        // relocation-dependent subsystems. Additionally ensure REL's
        // IDDatabase is initialized and usable. We keep a minimal set of
        // components so the plugin remains inert but doesn't crash.
        bool addressLibOk = AddressLib::IsValid();
        bool relOk = IsRelocationDatabaseReady();

        if (!addressLibOk || !relOk) {
            if (log) log->warn("Address/REL readiness: addressLib={}, rel={}. Skipping most initializations to avoid crashes.", AddressLib::Reason(), relOk);

            // Minimal safe initialization
            CrashCollector::Init();
            TrainwreckBridge::UnifiedCrashReporter::Initialize();

            status = {};
            status.configLoaded = true;
            PhaseTracking::PhaseTracker::SetComponentStatus(status);
            PhaseTracking::PhaseTracker::LogStartupSummary();

            if (log) log->info("Plugin running in limited-safe mode due to missing/invalid address library or REL IDDatabase");
            return;
        }

        // ── Initialize crash collector ──
        CrashCollector::Init();

        // ── Initialize Trainwreck integration (if available) ──
        TrainwreckBridge::UnifiedCrashReporter::Initialize();

        // ── Initialize mesh validator ──
        if (MeshValidation::MeshValidator::Initialize()) {
            status.meshValidatorReady = true;
        }

        // ── Initialize script monitor ──
        if (ScriptValidation::ScriptMonitor::Initialize()) {
            status.scriptMonitorReady = true;
        }

        // ── Initialize CoSave manager (S.L.A.C.K. compatibility) ──
        if (CrashGuard::CoSaveManager::GetInstance().Initialize()) {
            if (log) log->info("CoSave manager initialized");
        }

        // ── Initialize Papyrus validation system ──
        if (Config::Get().papyrusValidationEnabled) {
            if (PapyrusValidation::NativeFunctionHook::Initialize()) {
                if (log) log->info("Papyrus validation system initialized");
            } else {
                if (log) log->warn("Papyrus validation system failed to initialize");
            }
        }

        // ── Initialize memory manager (Task 27) ──
        if (CrashGuard::MemoryManager::GetInstance().Initialize()) {
            if (log) log->info("Memory manager initialized");
        }

        // ── Memory allocation hooks system removed ──
        // The allocation hooks approach was abandoned because it caused more problems
        // than it solved (VCRUNTIME140 crashes, reentrancy issues, extreme overhead).
        // See ALLOCATION_HOOK_ANALYSIS.md and PERFORMANCE_MEMORY_STRATEGY.md for details.

        // ── Resource limiter initialization disabled (gut for restart)
        // NOTE: Backed up under docs/backups. ResourceLimiter code remains in
        // the repo but is intentionally not initialized to simplify behavior
        // while we rework actor/AI throttling. See docs/backups for originals.
        if (log) log->info("Resource limiter initialization SKIPPED (disabled)");

        // ── Initialize memory pressure detector ──
        // Safe: Only reads Windows memory APIs, no game state access
        CrashGuard::MemoryPressureDetector::GetSingleton().Initialize();
        if (log) log->info("Memory pressure detector initialized");

        // ── Actor LOD manager initialization intentionally disabled
        // ActorLOD sources are preserved under docs/backups but the in-game
        // Actor LOD subsystem is disabled to remove F11 menu clutter while
        // we rebuild a simpler, save-safe AI threshold + deferred spawn system.
        if (log) log->info("Actor LOD manager initialization SKIPPED (disabled)");

        // ── Register Papyrus native functions for MCM ──
        CrashGuard::PapyrusNatives::Register();
        if (log) log->info("Papyrus native functions registered for MCM");

        // Initialize function hook manager
        if (Config::Get().patchesEnabled) {
            if (!AddressLib::IsValid()) {
                if (log) log->warn("Address library invalid: {}. Skipping function hooks and related proactive features.", AddressLib::Reason());
            } else {
                if (FunctionHooks::FunctionHookManager::Initialize()) {
                    if (log) log->info("[StartupTrace] Before FunctionHookManager::InstallAllHooks (re-enabled)");
                    // Re-enable function hook installation for targeted isolation
                    FunctionHooks::FunctionHookManager::InstallAllHooks();
                    if (log) log->info("[StartupTrace] After FunctionHookManager::InstallAllHooks (re-enabled)");
                    auto stats = FunctionHooks::FunctionHookManager::GetStats();
                    status.hooksInstalledCount = static_cast<int>(stats.installedHooks);
                    status.hooksTotalCount = static_cast<int>(stats.installedHooks + stats.failedHooks);
                    status.hooksInstalled = (stats.installedHooks > 0);
                    // FormID validator might fail during init (game data not ready yet)
                    // This is expected and will be retried at main menu
                    status.formIDValidatorReady = (stats.failedHooks == 0);
                } else {
                    if (log) log->warn("Function hook manager initialization failed");
                }
            }
        }

        // ── Install proactive patches ──
        // Re-enabled: These are targeted null-check patches
        if (Config::Get().patchesEnabled) {
            if (!AddressLib::IsValid()) {
                if (log) log->warn("Address library invalid: {}. Skipping proactive patches.", AddressLib::Reason());
            } else {
                if (log) log->info("[StartupTrace] Before PatchEngine::Init");
                PatchEngine::Init();
                if (log) log->info("[StartupTrace] After PatchEngine::Init");
                Patches::RegisterAll();
                auto patchCount = PatchEngine::ApplyAll();
                if (log) {
                    log->info("Applied {}/{} proactive engine patches",
                              patchCount, PatchEngine::GetPatches().size());
                }
            }
        }

        // Install proactive hooks
        if (Config::Get().safeMode) {
            if (log) log->warn("[SafeMode] Skipping Hooks::InstallHooks (safeMode=true)");
        } else if (!AddressLib::IsValid()) {
            if (log) log->warn("Address library invalid: {}. Skipping legacy hook installations.", AddressLib::Reason());
        } else {
            if (log) log->info("[StartupTrace] Before Hooks::InstallHooks");
            if (TryInstallHooks()) {
                if (log) log->info("[StartupTrace] After Hooks::InstallHooks -> success");
            } else {
                if (log) log->error("[SEH] Hooks::InstallHooks caused an access violation — skipping. Set [General] safeMode=true if this persists.");
            }
        }

        // Install VEH safety net
        if (Config::Get().safeMode) {
            if (log) log->warn("[SafeMode] Skipping VEH::Install (safeMode=true)");
            status.vehActive = false;
        } else if (Config::Get().vehEnabled) {
            if (log) log->info("[StartupTrace] Before VEH::Install");
            if (TryInstallVEH()) {
                status.vehActive = true;
                if (log) log->info("[StartupTrace] After VEH::Install -> success (crash recovery ACTIVE)");
            } else {
                status.vehActive = false;
                if (log) log->error("[SEH] VEH::Install caused an access violation — crash recovery disabled. Set [General] safeMode=true if this persists.");
            }
        } else {
            if (log) log->info("VEH disabled in config");
            status.vehActive = false;
        }

        // ── Start deadlock watchdog thread ──
        if (!Config::Get().safeMode && Config::Get().vehEnabled) {
            ThreadSafety::DeadlockDetector::Initialize();
            if (log) log->info("Deadlock watchdog thread started (checks every 2s)");
        }

        // Note: ImGui Present hook will be installed at kDataLoaded event
        // when the renderer and swap chain are guaranteed to be available

        // Update component status and log startup summary
        PhaseTracking::PhaseTracker::SetComponentStatus(status);
        PhaseTracking::PhaseTracker::LogStartupSummary();

        if (log) {
            log->flush();
        }
    }

}  // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════
// SKSE Plugin Interface
// ═══════════════════════════════════════════════════════════════════════

// Forward declare Handler for VEH
static LONG CALLBACK Handler(PEXCEPTION_POINTERS info);

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SetupLogging();
    
    auto log = spdlog::default_logger();
    if (log) {
        log->info("SkyrimCrashGuard loading");
    }
    
    // Detect game version before initializing SKSE
    auto gameInfo = GameDetect::Detect();
    if (log) {
        log->info("Detected game: {} ({})", gameInfo.gameName, gameInfo.exeName);
        auto runtimeVer = skse->RuntimeVersion();
        log->info("Runtime version: {}.{}.{}", 
            runtimeVer.major(), runtimeVer.minor(), runtimeVer.patch());
    }
    
    // Ensure address library stubs / bundled files are prepared before REL/CommonLibSSE uses them
    // Note: EnsureAddressLibraryStub removed - address library is now managed by vcpkg

    // Allocate trampoline space for hooks (1KB should be plenty)
    SKSE::AllocTrampoline(1 << 10);
    
    // Initialize SKSE normally
    // Version independence is handled by SKSEPlugin_Version flags in SKSEExports.cpp
    SKSE::Init(skse);
    
    // Probe REL/IDDatabase and log results before initializing plugin
    try {
        if (auto l = spdlog::default_logger()) {
            l->info("AddressLib valid: {}", AddressLib::IsValid());
            l->info("REL Module: AE={}, VR={}", REL::Module::IsAE(), REL::Module::IsVR());
        }
    } catch (...) {}

    InitializePlugin();
    
    // Register for SKSE messaging to detect phase transitions
    auto* messaging = SKSE::GetMessagingInterface();
    if (messaging) {
        messaging->RegisterListener(OnDataLoaded);
        if (log) log->info("Phase detection registered - will track game state transitions");
    } else {
        if (log) log->warn("SKSE messaging interface not available - phase detection disabled");
    }
    
    return true;
}

