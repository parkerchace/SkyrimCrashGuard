// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

// VEH.cpp
// Vectored Exception Handler with 6-Level Recovery Chain
//
// Philosophy: Recover crashes whenever possible; cooperate with other tools.
// The handler tries six progressively more aggressive strategies.
// Each one is safe on its own. The worst case is a visual glitch,
// a missing sound, or a skipped animation. All are preferable to CTD.
//
// Recovery chain (tried in order):
//   L1  Known Site         - pre-analysed crash sites, instant fix
//   L1b Instruction Pattern- version-independent pattern match (Zydis)
//   L2  Learned Site       - previously decoded at runtime, cached fix
//   L3  Register Fixup     - redirect faulting base register to safety buf
//   L4  Instruction Skip   - decode, zero dest register, advance RIP
//   L5  Function Return    - pop ret-addr, RAX=0, jump to caller
//   L6  Deep Stack Walk    - scan stack for valid executable ret-addr
//
// CrashLogger cooperation:
//   CrashGuard runs BEFORE CrashLogger's VEH handler. When recovery
//   succeeds, CrashLogger never sees the exception. To compensate:
//   - CrashGuard writes its own recovery reports to the SKSE log dir
//   - These reports document EVERY recovered crash with full context
//   - CrashLogger only fires for truly unrecoverable crashes
//   - Users should cross-reference both sets of logs for full picture
//
// We return CONTINUE_SEARCH when recovery is exhausted, allowing
// CrashLogger (or other crash loggers) to analyze unrecoverable crashes.
// ═══════════════════════════════════════════════════════════════════════

#include "VEH.h"
#include "CrashCollector.h"
#include "CrashLoggerDetector.h"
#include "GameDetect.h"
#include "TrainwreckBridge.h"
#include "Plugin.h"
#include "GameObjectIntrospector.h"
#include "RootCauseAnalyzer.h"
#include "CoSaveManager.h"
#include "DiagnosticLogger.h"
#include "PhaseTracker.h"
#include "PerformanceMetrics.h"
#include "StateManager.h"
#include "DynamicFixApplicator.h"
#include "UserNotificationManager.h"
#include "PatternLearningSystem.h"
#include "RecoveryNotifications.h"
#include "SeverityAnalyzer.h"
#include "NotificationThresholdManager.h"
#include "ToastNotificationManager.h"
#include "RecoveryStatistics.h"

#include <spdlog/spdlog.h>
#include "Config.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <Zydis/Zydis.h>
#include <REL/Relocation.h>

#include <atomic>
#include <array>
#include <cstring>
#include <chrono>
#include <thread>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

#pragma comment(lib, "dbghelp.lib")

// Define STATUS_HEAP_CORRUPTION if not already defined
#ifndef STATUS_HEAP_CORRUPTION
#define STATUS_HEAP_CORRUPTION 0xC0000374
#endif

// NT types for thread query (may already be defined in winternl.h or ntdef.h)
#ifndef NTSTATUS
typedef LONG NTSTATUS;
#endif

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

// Thread info class enum
typedef enum _THREAD_INFO_CLASS {
    kThreadBasicInformation = 0,
    kThreadQuerySetWin32StartAddress = 9
} THREAD_INFO_CLASS;

// NtQueryInformationThread function pointer type
typedef NTSTATUS (NTAPI *NtQueryInformationThreadFn)(
    HANDLE ThreadHandle,
    THREAD_INFO_CLASS ThreadInformationClass,
    PVOID ThreadInformation,
    ULONG ThreadInformationLength,
    PULONG ReturnLength
);

namespace VEH {

// ═══════════════════════════════════════════════════════════════════════
// § 1  Known Crash Sites
// ═══════════════════════════════════════════════════════════════════════
// Each entry is a crash instruction we have reverse-engineered.
// When VEH catches a fault at one of these offsets it applies the
// exact correct recovery with zero decoding overhead.

// CONTEXT field offsets for GP registers (Windows x64 CONTEXT)
static constexpr int kRAX = static_cast<int>(offsetof(CONTEXT, Rax));
static constexpr int kRCX = static_cast<int>(offsetof(CONTEXT, Rcx));
static constexpr int kRDX = static_cast<int>(offsetof(CONTEXT, Rdx));
static constexpr int kRBX = static_cast<int>(offsetof(CONTEXT, Rbx));
static constexpr int kRSI = static_cast<int>(offsetof(CONTEXT, Rsi));
static constexpr int kRDI = static_cast<int>(offsetof(CONTEXT, Rdi));
static constexpr int kR8  = static_cast<int>(offsetof(CONTEXT, R8));
static constexpr int kR9  = static_cast<int>(offsetof(CONTEXT, R9));
static constexpr int kR10 = static_cast<int>(offsetof(CONTEXT, R10));
static constexpr int kR11 = static_cast<int>(offsetof(CONTEXT, R11));
static constexpr int kR12 = static_cast<int>(offsetof(CONTEXT, R12));
static constexpr int kR13 = static_cast<int>(offsetof(CONTEXT, R13));
static constexpr int kR14 = static_cast<int>(offsetof(CONTEXT, R14));
static constexpr int kR15 = static_cast<int>(offsetof(CONTEXT, R15));
static constexpr int kNONE = -1;

// XMM register pseudo-constants for KnownSite (not real CONTEXT offsets)
// Used by L1_KnownSite to zero XMM registers via ZeroXMMRegister().
static constexpr int kXMM0  = -100;
static constexpr int kXMM1  = -101;
static constexpr int kXMM2  = -102;
static constexpr int kXMM3  = -103;
static constexpr int kXMM4  = -104;
static constexpr int kXMM5  = -105;
static constexpr int kXMM6  = -106;
static constexpr int kXMM7  = -107;
static constexpr int kXMM8  = -108;
static constexpr int kXMM9  = -109;
static constexpr int kXMM10 = -110;
static constexpr int kXMM11 = -111;
static constexpr int kXMM12 = -112;
static constexpr int kXMM13 = -113;
static constexpr int kXMM14 = -114;
static constexpr int kXMM15 = -115;

// Helper: Convert register name string to CONTEXT offset
static int ParseRegisterName(const std::string& name) {
    if (name.empty() || name == "NONE" || name == "none") return kNONE;
    if (name == "RAX" || name == "rax") return kRAX;
    if (name == "RCX" || name == "rcx") return kRCX;
    if (name == "RDX" || name == "rdx") return kRDX;
    if (name == "RBX" || name == "rbx") return kRBX;
    if (name == "RSI" || name == "rsi") return kRSI;
    if (name == "RDI" || name == "rdi") return kRDI;
    if (name == "R8"  || name == "r8")  return kR8;
    if (name == "R9"  || name == "r9")  return kR9;
    if (name == "R10" || name == "r10") return kR10;
    if (name == "R11" || name == "r11") return kR11;
    if (name == "R12" || name == "r12") return kR12;
    if (name == "R13" || name == "r13") return kR13;
    if (name == "R14" || name == "r14") return kR14;
    if (name == "R15" || name == "r15") return kR15;
    // XMM register names for SIMD crash sites
    if (name == "XMM0"  || name == "xmm0")  return kXMM0;
    if (name == "XMM1"  || name == "xmm1")  return kXMM1;
    if (name == "XMM2"  || name == "xmm2")  return kXMM2;
    if (name == "XMM3"  || name == "xmm3")  return kXMM3;
    if (name == "XMM4"  || name == "xmm4")  return kXMM4;
    if (name == "XMM5"  || name == "xmm5")  return kXMM5;
    if (name == "XMM6"  || name == "xmm6")  return kXMM6;
    if (name == "XMM7"  || name == "xmm7")  return kXMM7;
    if (name == "XMM8"  || name == "xmm8")  return kXMM8;
    if (name == "XMM9"  || name == "xmm9")  return kXMM9;
    if (name == "XMM10" || name == "xmm10") return kXMM10;
    if (name == "XMM11" || name == "xmm11") return kXMM11;
    if (name == "XMM12" || name == "xmm12") return kXMM12;
    if (name == "XMM13" || name == "xmm13") return kXMM13;
    if (name == "XMM14" || name == "xmm14") return kXMM14;
    if (name == "XMM15" || name == "xmm15") return kXMM15;
    return kNONE;
}

struct KnownSite {
    uintptr_t   offset;    // from SkyrimVR.exe base
    uint8_t     instrLen;  // byte length of the faulting instruction
    int         destCtx;   // CONTEXT offset of destination register, or kNONE
    const char* name;
    bool        bailout;   // true = return from function instead of skip instruction
};

// ── Table of known crash sites (game executable) ────────────────────
// Feel free to add rows as new crash logs come in.
// offset / instrLen / destReg / description / bailout
static std::vector<KnownSite> s_knownSites;

// ── Known crash sites in mod DLLs ───────────────────────────────────
// These are indexed by DLL name (case-insensitive) + offset.
// When a crash occurs in a mod DLL at a known offset, we apply the
// exact recovery without generic decoding overhead.
struct ModKnownSite {
    const char* dllName;   // e.g. "skee64.dll"
    uintptr_t   offset;    // offset from DLL base
    uint8_t     instrLen;  // byte length of the faulting instruction
    int         destCtx;   // CONTEXT offset of destination register, or kNONE  
    const char* name;      // human-readable description
    bool        bailout;   // true = return from function instead of skip instruction
};

static std::vector<ModKnownSite> s_modKnownSites;

// Storage for dynamically loaded string data from JSON
// These must persist for the lifetime of the crash sites
static std::vector<std::string> s_jsonStringStorage;

// ── JSON Crash Sites Loader ─────────────────────────────────────────
// Loads crash sites from JSON files in Data/SKSE/Plugins/CrashGuard/
// This allows users and mod authors to add custom crash sites without
// recompiling the DLL.
//
// JSON format:
// {
//   "game_sites": [
//     { "offset": "0x2D32A5", "instrLen": 4, "destReg": "RAX", "name": "...", "runtime": "VR" }
//   ],
//   "mod_sites": [
//     { "dll": "skee64.dll", "offset": "0x367A3", "instrLen": 4, "destReg": "RDX", "name": "..." }
//   ]
// }
static void LoadCrashSitesFromJSON() {
    auto log = spdlog::default_logger();
    
    // Build path to JSON crash sites directory
    std::filesystem::path jsonDir;
    
    // Try SKSE plugin directory first
    if (auto skseDir = std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "CrashGuard";
        std::filesystem::exists(skseDir)) {
        jsonDir = skseDir;
    } else {
        // Fallback: relative to executable
        jsonDir = std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "CrashGuard";
    }
    
    // Create directory if it doesn't exist
    std::error_code ec;
    std::filesystem::create_directories(jsonDir, ec);
    
    // Determine current runtime for filtering
    bool isVR = REL::Module::IsVR();
    bool isAE = false;
    bool isSE = false;
    if (!isVR) {
        // Check for AE vs SE based on version
        auto version = REL::Module::get().version();
        isAE = version >= SKSE::RUNTIME_SSE_1_6_317;
        isSE = !isAE;
    }
    
    size_t gameSitesLoaded = 0;
    size_t modSitesLoaded = 0;
    
    // Load all JSON files in the directory
    for (const auto& entry : std::filesystem::directory_iterator(jsonDir, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;
        
        try {
            std::ifstream file(entry.path());
            if (!file.is_open()) continue;
            
            nlohmann::json j;
            file >> j;
            
            // Parse game_sites array
            if (j.contains("game_sites") && j["game_sites"].is_array()) {
                for (const auto& site : j["game_sites"]) {
                    // Check runtime filter
                    if (site.contains("runtime")) {
                        std::string runtime = site["runtime"].get<std::string>();
                        if (runtime == "VR" && !isVR) continue;
                        if (runtime == "SE" && !isSE) continue;
                        if (runtime == "AE" && !isAE) continue;
                        // "ALL" or empty matches any
                    }
                    
                    // Parse offset (hex string or number)
                    uintptr_t offset = 0;
                    if (site["offset"].is_string()) {
                        offset = std::stoull(site["offset"].get<std::string>(), nullptr, 0);
                    } else {
                        offset = site["offset"].get<uintptr_t>();
                    }
                    
                    uint8_t instrLen = static_cast<uint8_t>(site.value("instrLen", 4));
                    int destCtx = ParseRegisterName(site.value("destReg", "NONE"));
                    
                    // Store name string persistently
                    s_jsonStringStorage.push_back(site.value("name", "JSON crash site"));
                    const char* name = s_jsonStringStorage.back().c_str();
                    
                    // Parse bailout mode (default false for backward compatibility)
                    bool bailout = site.value("bailout", false);
                    
                    s_knownSites.push_back({ offset, instrLen, destCtx, name, bailout });
                    ++gameSitesLoaded;
                }
            }
            
            // Parse mod_sites array
            if (j.contains("mod_sites") && j["mod_sites"].is_array()) {
                for (const auto& site : j["mod_sites"]) {
                    // Parse DLL name
                    std::string dll = site.value("dll", "");
                    if (dll.empty()) continue;
                    s_jsonStringStorage.push_back(dll);
                    const char* dllName = s_jsonStringStorage.back().c_str();
                    
                    // Parse offset
                    uintptr_t offset = 0;
                    if (site["offset"].is_string()) {
                        offset = std::stoull(site["offset"].get<std::string>(), nullptr, 0);
                    } else {
                        offset = site["offset"].get<uintptr_t>();
                    }
                    
                    uint8_t instrLen = static_cast<uint8_t>(site.value("instrLen", 4));
                    int destCtx = ParseRegisterName(site.value("destReg", "NONE"));
                    
                    // Store name string persistently
                    s_jsonStringStorage.push_back(site.value("name", "JSON mod crash site"));
                    const char* name = s_jsonStringStorage.back().c_str();
                    
                    // Parse bailout mode (default false for backward compatibility)
                    bool bailout = site.value("bailout", false);
                    
                    s_modKnownSites.push_back({ dllName, offset, instrLen, destCtx, name, bailout });
                    ++modSitesLoaded;
                }
            }
            
            if (log) {
                log->info("[VEH] Loaded crash sites from {}", entry.path().filename().string());
            }
            
        } catch (const std::exception& e) {
            if (log) {
                log->warn("[VEH] Failed to parse {}: {}", entry.path().filename().string(), e.what());
            }
        }
    }
    
    if (log && (gameSitesLoaded > 0 || modSitesLoaded > 0)) {
        log->info("[VEH] Loaded {} game sites and {} mod sites from JSON", 
                  gameSitesLoaded, modSitesLoaded);
    }
}

// ═══════════════════════════════════════════════════════════════════════
// § 2  Runtime-Learned Sites
// ═══════════════════════════════════════════════════════════════════════
// When we successfully recover from an *unknown* crash via L3/L4,
// we cache the decoded info so repeat hits are instant (L2).

struct LearnedSite {
    std::atomic<uintptr_t> rip{0};   // atomic: publish guard for concurrent reads
    uint8_t   instrLen;
    int       destCtx;    // CONTEXT offset or kNONE
    uint32_t  hits;
};

static constexpr size_t MAX_LEARNED = 512;
static LearnedSite          s_learned[MAX_LEARNED] = {};
static std::atomic<size_t>  s_learnedCount{0};

// ═══════════════════════════════════════════════════════════════════════
// § 3  Static State
// ═══════════════════════════════════════════════════════════════════════

static PVOID     s_handler     = nullptr;
static uintptr_t s_gameBase    = 0;
static uintptr_t s_gameEnd     = 0;

// CrashGuard's own module bounds — used to reject self-crashes
static uintptr_t s_selfBase    = 0;
static uintptr_t s_selfEnd     = 0;

// Safety buffer: 64 KB of committed, zeroed, RW memory.
// Register-fixup redirects the faulting base register to point into
// the middle of this buffer so any displacement still lands inside.
static void*                s_safetyBuf = nullptr;
static constexpr size_t     SAFETY_SZ   = 0x10000;   // 64 KB

static ZydisDecoder         s_decoder;
static bool                 s_decoderOK = false;

// Per-thread reentrancy guard
static thread_local int     t_depth = 0;
static constexpr int        MAX_DEPTH = 3;

// Recovery statistics
struct Stats {
    std::atomic<uint64_t> total          {0};
    std::atomic<uint64_t> knownSite      {0};
    std::atomic<uint64_t> instrPattern   {0};  // L1b hits
    std::atomic<uint64_t> learnedSite    {0};
    std::atomic<uint64_t> regFixup       {0};
    std::atomic<uint64_t> instrSkip      {0};
    std::atomic<uint64_t> funcReturn     {0};
    std::atomic<uint64_t> deepWalk       {0};
    std::atomic<uint64_t> unrecoverable  {0};
};
static Stats s_stats;

// Per-address hit tracking (rate-limited logging)
static constexpr size_t TRACK_SLOTS = 64;
struct TrackSlot { uintptr_t rip; uint32_t count; DWORD tick; };
static TrackSlot s_track[TRACK_SLOTS] = {};

// L3 cascade detection: if L3 fires too many times in a short window
// (different addresses, same burst), suppress further L3 to stop
// safety-buffer zeros from cascading through pointer chains into
// null function calls.
static constexpr uint32_t L3_CASCADE_MAX    = 4;   // max L3 fires per window
static constexpr DWORD    L3_CASCADE_WINDOW = 100;  // ms
static std::atomic<uint32_t> s_l3CascadeCount{0};
static DWORD                 s_l3CascadeStart = 0;

// ═══════════════════════════════════════════════════════════════════════
// § 3b  GLOBAL CASCADE PROTECTION - Prevent CrashGuard from causing crashes
// ═══════════════════════════════════════════════════════════════════════
// Problem: Per-RIP tracking fails when crashes cascade through different
// addresses (e.g., +0x76, +0x7f, +0x85 in same function). Each gets a fresh
// counter, so the limit never triggers, and CrashGuard keeps "recovering"
// until the game state is completely destroyed.
//
// Solution: Three layers of self-protection:
// 1. Global crash rate limiter - max N crashes in X seconds total
// 2. Function-level grouping - crashes within 256 bytes share a counter
// 3. Recovery cooldown - require healthy execution time between recoveries

// Global rate limiter: max crashes across ALL addresses in time window
// AGGRESSIVE MODE: Allow many recoveries since we're using intelligent Zydis-based recovery
static constexpr uint32_t GLOBAL_CASCADE_MAX    = 100;    // max total recoveries (was 5)
static constexpr DWORD    GLOBAL_CASCADE_WINDOW = 5000;   // in 5 seconds (was 2)
static std::atomic<uint32_t> s_globalCrashCount{0};
static DWORD                 s_globalCrashWindowStart = 0;
static std::atomic<bool>     s_globalCascadeTripped{false};

// Function-level tracking: group crashes within same 256-byte block
// This catches cascades like +0x76 -> +0x7f -> +0x85 (all in same function)
static constexpr size_t FUNC_TRACK_SLOTS = 32;
static constexpr uintptr_t FUNC_BLOCK_MASK = ~static_cast<uintptr_t>(0xFF); // Round to 256-byte boundary
struct FuncTrackSlot { uintptr_t funcBase; uint32_t count; DWORD tick; };
static FuncTrackSlot s_funcTrack[FUNC_TRACK_SLOTS] = {};
static constexpr uint32_t FUNC_CASCADE_MAX = 30; // Max crashes per function block (was 3)

// Recovery cooldown: require minimum healthy execution time
static DWORD s_lastRecoveryTick = 0;
static constexpr DWORD RECOVERY_COOLDOWN_MS = 5; // 5ms between recoveries (was 50)
static bool s_lastRecoveryWasWriteSkip = false;  // Track if last recovery skipped a write

// ═══════════════════════════════════════════════════════════════════════
// § 4a  Save-Load Recovery Tracking
// ═══════════════════════════════════════════════════════════════════════
// Track crashes recovered during save load so we can inform the user
// what went wrong without actually crashing the game.

struct SaveLoadRecoveryInfo {
    std::atomic<uint32_t> totalRecoveries{0};
    std::atomic<uint32_t> nullVtableCalls{0};      // call [rax+disp] with null
    std::atomic<uint32_t> nullPointerReads{0};     // read from null
    std::atomic<uint32_t> nullPointerWrites{0};    // write to null
    std::atomic<bool>     notificationShown{false};
    
    void Reset() {
        totalRecoveries.store(0, std::memory_order_relaxed);
        nullVtableCalls.store(0, std::memory_order_relaxed);
        nullPointerReads.store(0, std::memory_order_relaxed);
        nullPointerWrites.store(0, std::memory_order_relaxed);
        notificationShown.store(false, std::memory_order_relaxed);
    }
};

static SaveLoadRecoveryInfo s_saveLoadRecoveries;

// Record a recovery during save load for later notification
[[maybe_unused]] static void RecordSaveLoadRecovery(ULONG_PTR accessType) {
    s_saveLoadRecoveries.totalRecoveries.fetch_add(1, std::memory_order_relaxed);
    
    // Categorize by type
    if (accessType == 8) {
        // Execute - vtable call through null
        s_saveLoadRecoveries.nullVtableCalls.fetch_add(1, std::memory_order_relaxed);
    } else if (accessType == 0) {
        // Read
        s_saveLoadRecoveries.nullPointerReads.fetch_add(1, std::memory_order_relaxed);
    } else if (accessType == 1) {
        // Write
        s_saveLoadRecoveries.nullPointerWrites.fetch_add(1, std::memory_order_relaxed);
    }
}

// Show notification after save load completes (called from phase tracker)
void ShowSaveLoadRecoverySummary() {
    auto log = spdlog::default_logger();
    
    uint32_t total = s_saveLoadRecoveries.totalRecoveries.load(std::memory_order_relaxed);
    if (total == 0) return;
    
    // Only show notification once per save load
    bool expected = false;
    if (!s_saveLoadRecoveries.notificationShown.compare_exchange_strong(
            expected, true, std::memory_order_relaxed)) {
        return;
    }
    
    uint32_t vtable = s_saveLoadRecoveries.nullVtableCalls.load(std::memory_order_relaxed);
    uint32_t reads = s_saveLoadRecoveries.nullPointerReads.load(std::memory_order_relaxed);
    uint32_t writes = s_saveLoadRecoveries.nullPointerWrites.load(std::memory_order_relaxed);
    
    // Log detailed info
    if (log) {
        log->warn("╔══════════════════════════════════════════════════════════════╗");
        log->warn("║  CrashGuard: Save Load Issues Detected                       ║");
        log->warn("╠══════════════════════════════════════════════════════════════╣");
        log->warn("║  {} crashes were PREVENTED during save load               ║", total);
        if (vtable > 0)
            log->warn("║    - {} null vtable calls (missing mod/shader?)           ║", vtable);
        if (reads > 0) 
            log->warn("║    - {} null pointer reads (uninitialized data?)          ║", reads);
        if (writes > 0)
            log->warn("║    - {} null pointer writes                               ║", writes);
        log->warn("╠══════════════════════════════════════════════════════════════╣");
        log->warn("║  POSSIBLE CAUSES:                                            ║");
        log->warn("║    - Missing mod dependency (Community Shaders, etc.)        ║");
        log->warn("║    - Disabled mod that other mods depend on                  ║");
        log->warn("║    - Mod load order issues                                   ║");
        log->warn("║    - Corrupted mod files                                     ║");
        log->warn("╠══════════════════════════════════════════════════════════════╣");
        log->warn("║  The game continued but you may see visual glitches.         ║");
        log->warn("║  Check your mod dependencies and load order.                 ║");
        log->warn("╚══════════════════════════════════════════════════════════════╝");
    }
    
    // Show in-game toast notification
    std::string toastMsg = fmt::format("CrashGuard: {} issues fixed during load. Check mod dependencies.", total);
    RE::DebugNotification(toastMsg.c_str());
}

// Reset save load tracking when entering LoadingSave phase
void ResetSaveLoadRecoveryTracking() {
    s_saveLoadRecoveries.Reset();
}

// ═══════════════════════════════════════════════════════════════════════
// § 4  Helpers
// ═══════════════════════════════════════════════════════════════════════

static bool IsGameAddr(uintptr_t a) { return a >= s_gameBase && a < s_gameEnd; }

// Check if address is inside CrashGuard's own DLL
static bool IsSelfAddr(uintptr_t a) { return a >= s_selfBase && a < s_selfEnd; }

// Check if address is inside a system/OS DLL (ntdll, kernel32, etc.)
// These should never be recovered — they indicate OS-level issues or
// are part of normal exception dispatch.
static bool IsSystemDLL(uintptr_t a) {
    HMODULE h = nullptr;
    constexpr DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                          | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (!GetModuleHandleExA(flags, reinterpret_cast<LPCSTR>(a), &h) || !h)
        return true;  // unknown module → treat as system

    char buf[MAX_PATH];
    if (!GetModuleFileNameA(h, buf, MAX_PATH))
        return true;

    std::string path(buf);
    auto slash = path.find_last_of("\\/");
    std::string name = (slash != std::string::npos) ? path.substr(slash + 1) : path;

    // Convert to lowercase for comparison
    for (auto& c : name) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

    // System DLLs we should never try to recover
    static const char* systemDLLs[] = {
        "ntdll.dll", "kernel32.dll", "kernelbase.dll",
        "ucrtbase.dll", "msvcrt.dll", "vcruntime140.dll",
        "vcruntime140d.dll", "msvcp140.dll", "msvcp140d.dll",
        "user32.dll", "gdi32.dll", "gdi32full.dll", "win32u.dll",
        "advapi32.dll", "sechost.dll", "rpcrt4.dll",
        "combase.dll", "ole32.dll", "oleaut32.dll",
        "ws2_32.dll", "bcrypt.dll", "crypt32.dll",
        "shell32.dll", "shlwapi.dll", "setupapi.dll",
        "comdlg32.dll", "imm32.dll", "psapi.dll",
        "dbghelp.dll", "dbgcore.dll",
        // Crash loggers must be excluded from recovery. They use intentional
        // SEH-protected memory probing (reading pointers that may be invalid)
        // to analyze crashes. VEH runs BEFORE SEH — if we intercept their AVs
        // and "recover" by zeroing registers / skipping instructions, we 
        // corrupt their analysis and cause secondary crashes.
        // This exclusion is COOPERATIVE: it lets CrashLogger do its job
        // when it's analyzing crashes that CrashGuard couldn't recover.
        "crashlogger.dll", "crashloggersse.dll",
        "trainwreck.dll",
        nullptr
    };

    for (int i = 0; systemDLLs[i]; ++i) {
        if (name == systemDLLs[i]) return true;
    }

    // Also exclude driver DLLs (GPU, etc.)
    // These have known paths in system directories
    auto sysDir = path.find("\\Windows\\");
    auto sysDirAlt = path.find("\\windows\\");
    if (sysDir != std::string::npos || sysDirAlt != std::string::npos) {
        return true;
    }

    return false;
}

// Check if address belongs to a recoverable module (game exe or mod DLL)
// Excludes CrashGuard's own module but ALLOWS system DLLs to reach recovery layers
// where L1b can decide the appropriate recovery strategy (function return vs write-skip)
static bool IsRecoverableAddr(uintptr_t a) {
    if (IsSelfAddr(a))   return false;  // Never recover our own crashes
    // System DLLs are now allowed through - L1b will handle them appropriately
    return true;  // Game exe, mod DLL, or system DLL → try recovery
}

static void GenerateMinidump(PEXCEPTION_POINTERS exceptionInfo) {
    // Get SKSE log directory path
    char logPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, 0, logPath))) {
        std::string dumpPath = std::string(logPath) + "\\My Games\\Skyrim Special Edition\\SKSE\\";
        
        // Check for VR
        if (REL::Module::IsVR()) {
            dumpPath = std::string(logPath) + "\\My Games\\Skyrim VR\\SKSE\\";
        }
        
        // Create timestamped filename
        SYSTEMTIME st;
        GetLocalTime(&st);
        char filename[256];
        snprintf(filename, sizeof(filename), "CrashGuard_%04d-%02d-%02d_%02d-%02d-%02d.dmp",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        dumpPath += filename;
        
        // Create the dump file
        HANDLE hFile = CreateFileA(dumpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION mdei;
            mdei.ThreadId = GetCurrentThreadId();
            mdei.ExceptionPointers = exceptionInfo;
            mdei.ClientPointers = FALSE;
            
            // Use MiniDumpWithDataSegs for more useful debugging info
            // but not MiniDumpWithFullMemory to keep file size reasonable
            MINIDUMP_TYPE mdType = static_cast<MINIDUMP_TYPE>(
                MiniDumpNormal | MiniDumpWithDataSegs | MiniDumpWithHandleData |
                MiniDumpWithUnloadedModules | MiniDumpWithThreadInfo);
            
            BOOL success = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                                             hFile, mdType, &mdei, nullptr, nullptr);
            CloseHandle(hFile);
            
            auto log = spdlog::default_logger();
            if (log) {
                if (success) {
                    log->info("[VEH] Minidump written to: {}", dumpPath);
                } else {
                    log->error("[VEH] Failed to write minidump: {}", GetLastError());
                }
            }
        }
    }
}

static bool IsReadable(const void* p, size_t len = 1) {
    MEMORY_BASIC_INFORMATION m{};
    if (!VirtualQuery(p, &m, sizeof(m)))      return false;
    if (m.State != MEM_COMMIT)                return false;
    constexpr DWORD ok = PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ
                       | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
    return (m.Protect & ok) != 0;
}

static bool IsExec(const void* p) {
    MEMORY_BASIC_INFORMATION m{};
    if (!VirtualQuery(p, &m, sizeof(m)))      return false;
    if (m.State != MEM_COMMIT)                return false;
    constexpr DWORD ok = PAGE_EXECUTE | PAGE_EXECUTE_READ
                       | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (m.Protect & ok) != 0;
}

static std::string ModName(uintptr_t a) {
    HMODULE h = nullptr;
    constexpr DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                          | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (GetModuleHandleExA(flags, reinterpret_cast<LPCSTR>(a), &h) && h) {
        char buf[MAX_PATH];
        if (GetModuleFileNameA(h, buf, MAX_PATH)) {
            std::string s(buf);
            auto p = s.find_last_of("\\/");
            return (p != std::string::npos) ? s.substr(p + 1) : s;
        }
    }
    return "unknown";
}

static uintptr_t ModOff(uintptr_t a) {
    HMODULE h = nullptr;
    constexpr DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                          | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (GetModuleHandleExA(flags, reinterpret_cast<LPCSTR>(a), &h) && h)
        return a - reinterpret_cast<uintptr_t>(h);
    return a;
}

// Track hit count for a given RIP; returns total hits.
static uint32_t TrackHit(uintptr_t rip) {
    DWORD now = GetTickCount();
    // Find existing
    for (auto& s : s_track) {
        if (s.rip == rip) {
            if (now - s.tick > 10000) s.count = 0;   // reset after 10 s idle
            s.count++;
            s.tick = now;
            return s.count;
        }
    }
    // Find free or oldest slot
    size_t best = 0; DWORD oldest = now;
    for (size_t i = 0; i < TRACK_SLOTS; ++i) {
        if (s_track[i].rip == 0) { best = i; break; }
        if (s_track[i].tick < oldest) { oldest = s_track[i].tick; best = i; }
    }
    s_track[best] = { rip, 1, now };
    return 1;
}

// ═══════════════════════════════════════════════════════════════════════
// § 4b  Global Cascade Protection Helpers
// ═══════════════════════════════════════════════════════════════════════

// Track crashes at function level (256-byte blocks)
// Returns hit count for the function block containing this RIP
static uint32_t TrackFunctionHit(uintptr_t rip) {
    DWORD now = GetTickCount();
    uintptr_t funcBase = rip & FUNC_BLOCK_MASK;
    
    // Find existing function block
    for (auto& s : s_funcTrack) {
        if (s.funcBase == funcBase) {
            if (now - s.tick > 5000) s.count = 0;  // reset after 5s idle
            s.count++;
            s.tick = now;
            return s.count;
        }
    }
    // Find free or oldest slot
    size_t best = 0; DWORD oldest = now;
    for (size_t i = 0; i < FUNC_TRACK_SLOTS; ++i) {
        if (s_funcTrack[i].funcBase == 0) { best = i; break; }
        if (s_funcTrack[i].tick < oldest) { oldest = s_funcTrack[i].tick; best = i; }
    }
    s_funcTrack[best] = { funcBase, 1, now };
    return 1;
}

// Check if a RIP is in the Moon rendering function (0D1BF70-0D1BFFF range)
// or Sky::Update function (0405000-041FFFF range - includes +0406FBE movss crash)
// These crash frequently during save load with SkyrimSoulsRE and need elevated
// cascade limits even if not at an exact known site offset.
static bool IsInMoonOrSkyFunction(uintptr_t rip) {
    if (!IsGameAddr(rip)) return false;
    uintptr_t off = rip - s_gameBase;
    // Moon::UpdateImpl function spans roughly +0D1BF70 to +0D1BFFF
    if (off >= 0x0D1BF70 && off <= 0x0D1BFFF) return true;
    // Sky rendering functions span roughly +0405000 to +041FFFF (expanded to cover +0406FBE)
    if (off >= 0x0405000 && off <= 0x041FFFF) return true;
    return false;
}

// Check if a RIP is a known bailout site (higher cascade limits during save load)
// Returns true if this is a bailout site that merits extended tolerance.
static bool IsKnownBailoutSite(uintptr_t rip) {
    // First check if in Moon/Sky render function range (always bailout during save load)
    if (IsInMoonOrSkyFunction(rip)) return true;
    
    if (!IsGameAddr(rip)) return false;
    uintptr_t off = rip - s_gameBase;
    for (const auto& site : s_knownSites) {
        if (site.offset == off && site.bailout) {
            return true;
        }
    }
    return false;
}

// Cascade limit for known bailout sites during save load
// Moon/Sky rendering crashes frequently during async save load with SkyrimSoulsRE
// because the rendering continues while Sky objects are partially loaded.
// This is expected behavior - we need to recover many times until loading completes.
static constexpr uint32_t BAILOUT_CASCADE_MAX = 200;

// Check global cascade - returns true if we should STOP recovering
static bool CheckGlobalCascadeTripped(uintptr_t rip) {
    // If already tripped, stay tripped until window expires
    if (s_globalCascadeTripped.load(std::memory_order_relaxed)) {
        DWORD now = GetTickCount();
        if (now - s_globalCrashWindowStart < GLOBAL_CASCADE_WINDOW * 2) {
            return true;  // Still in cooldown
        }
        // Window expired, reset
        s_globalCascadeTripped.store(false, std::memory_order_relaxed);
        s_globalCrashCount.store(0, std::memory_order_relaxed);
    }
    
    DWORD now = GetTickCount();
    
    // Reset window if expired
    if (now - s_globalCrashWindowStart > GLOBAL_CASCADE_WINDOW) {
        s_globalCrashWindowStart = now;
        s_globalCrashCount.store(1, std::memory_order_relaxed);
        return false;
    }
    
    // Increment and check
    uint32_t count = s_globalCrashCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (count > GLOBAL_CASCADE_MAX) {
        s_globalCascadeTripped.store(true, std::memory_order_relaxed);
        auto log = spdlog::default_logger();
        if (log) {
            log->critical("[VEH] GLOBAL CASCADE BREAKER: {} crashes in {}ms at/near {:#x} — "
                          "CrashGuard is making things worse, giving up",
                          count, GLOBAL_CASCADE_WINDOW, rip);
            log->flush();
        }
        
        // Allocation hooks system has been removed
        // (Previously auto-disabled allocation hooks during cascade crashes)
        
        return true;
    }
    return false;
}

// Check if enough time has passed since last recovery
static bool CheckRecoveryCooldown() {
    DWORD now = GetTickCount();
    if (s_lastRecoveryTick != 0 && (now - s_lastRecoveryTick) < RECOVERY_COOLDOWN_MS) {
        // Allow one immediate cascade if the previous recovery skipped a write
        // (write-skip can leave corrupted state that triggers immediate follow-up crash)
        if (s_lastRecoveryWasWriteSkip) {
            s_lastRecoveryWasWriteSkip = false;  // Only allow one cascade
            return true;
        }
        return false;  // Too soon, deny recovery
    }
    return true;
}

// Record that a recovery happened
static void RecordRecovery(bool wasWriteSkip = false) {
    s_lastRecoveryTick = GetTickCount();
    s_lastRecoveryWasWriteSkip = wasWriteSkip;
}

// Map Zydis register enum → CONTEXT field offset
// Returns kNONE for unsupported registers
static int RegToCtx(ZydisRegister r) {
    switch (r) {
    case ZYDIS_REGISTER_RAX: case ZYDIS_REGISTER_EAX: return kRAX;
    case ZYDIS_REGISTER_RCX: case ZYDIS_REGISTER_ECX: return kRCX;
    case ZYDIS_REGISTER_RDX: case ZYDIS_REGISTER_EDX: return kRDX;
    case ZYDIS_REGISTER_RBX: case ZYDIS_REGISTER_EBX: return kRBX;
    case ZYDIS_REGISTER_RSI: case ZYDIS_REGISTER_ESI: return kRSI;
    case ZYDIS_REGISTER_RDI: case ZYDIS_REGISTER_EDI: return kRDI;
    case ZYDIS_REGISTER_R8:  case ZYDIS_REGISTER_R8D:  return kR8;
    case ZYDIS_REGISTER_R9:  case ZYDIS_REGISTER_R9D:  return kR9;
    case ZYDIS_REGISTER_R10: case ZYDIS_REGISTER_R10D: return kR10;
    case ZYDIS_REGISTER_R11: case ZYDIS_REGISTER_R11D: return kR11;
    case ZYDIS_REGISTER_R12: case ZYDIS_REGISTER_R12D: return kR12;
    case ZYDIS_REGISTER_R13: case ZYDIS_REGISTER_R13D: return kR13;
    case ZYDIS_REGISTER_R14: case ZYDIS_REGISTER_R14D: return kR14;
    case ZYDIS_REGISTER_R15: case ZYDIS_REGISTER_R15D: return kR15;
    default: return kNONE;
    }
}

// Check if a Zydis register is an XMM register
static bool IsXMMRegister(ZydisRegister r) {
    return (r >= ZYDIS_REGISTER_XMM0 && r <= ZYDIS_REGISTER_XMM15);
}

// Zero an XMM register in the CONTEXT
// XMM registers are stored in CONTEXT::Xmm0 through Xmm15 as M128A structs
static void ZeroXMMRegister(PCONTEXT ctx, ZydisRegister r) {
    if (!IsXMMRegister(r)) return;
    int index = r - ZYDIS_REGISTER_XMM0;
    if (index < 0 || index > 15) return;
    
    // CONTEXT has Xmm0-Xmm15 as an array in FltSave.XmmRegisters
    // Or we can access them via the Header + XmmRegisters union
    // For safety, use the direct Xmm fields
    M128A* xmmRegs = &ctx->Xmm0;
    xmmRegs[index].Low = 0;
    xmmRegs[index].High = 0;
}

// ═══════════════════════════════════════════════════════════════════════
// § 5  Recovery Strategies
// ═══════════════════════════════════════════════════════════════════════

// ── Forward declaration for bailout recovery ────────────────────────
static bool L5_FuncReturn(PCONTEXT ctx);

// ── L1: Known Site ──────────────────────────────────────────────────
static bool L1_KnownSite(PCONTEXT ctx, uintptr_t rip) {
    auto log = spdlog::default_logger();
    
    // Helper lambda: apply recovery for a known site (GP or XMM register)
    // If site.bailout is true, use L5_FuncReturn to exit the entire function
    // instead of just skipping one instruction (prevents cascade crashes).
    auto applyKnownSiteRecovery = [&](PCONTEXT c, const auto& site) -> bool {
        // BAILOUT MODE: For crash sites where skipping instructions causes
        // cascading failures (e.g., Moon rendering with null vtable), we
        // return from the entire function instead of continuing.
        if (site.bailout) {
            if (log) {
                log->info("[VEH]   -> L1 known-site BAILOUT: {} - returning from function", site.name);
            }
            // Zero any register that might hold a return value
            c->Rax = 0;
            
            // Try L5 function return to exit cleanly
            if (L5_FuncReturn(c)) {
                s_stats.knownSite++;
                return true;
            }
            
            // If L5 fails, try direct RSP read as fallback
            if (IsReadable(reinterpret_cast<void*>(c->Rsp), 8)) {
                uint64_t ret = *reinterpret_cast<uint64_t*>(c->Rsp);
                if (IsExec(reinterpret_cast<void*>(ret))) {
                    c->Rsp += 8;
                    c->Rip = ret;
                    s_stats.knownSite++;
                    if (log) {
                        log->info("[VEH]   -> Bailout fallback: returning to {:#x}", ret);
                    }
                    return true;
                }
            }
            
            // IMPORTANT: If bailout failed, return FALSE so L1b can try
            // with accurate instruction decoding. The known site's instrLen/destCtx
            // may be wrong for the actual instruction at this RIP (e.g., multiple
            // different instructions can crash at nearby offsets in the same function).
            if (log) {
                log->warn("[VEH]   -> Bailout failed, deferring to L1b for instruction-accurate recovery");
            }
            return false;
        }
        
        // Normal recovery: zero register and skip instruction
        if (site.destCtx >= kXMM15 && site.destCtx <= kXMM0) {
            // XMM register range: kXMM0=-100 .. kXMM15=-115
            int xmmIndex = -(site.destCtx + 100);  // kXMM0→0, kXMM13→13, etc.
            if (xmmIndex >= 0 && xmmIndex <= 15) {
                M128A* xmmRegs = &c->Xmm0;
                xmmRegs[xmmIndex].Low = 0;
                xmmRegs[xmmIndex].High = 0;
            }
        } else if (site.destCtx != kNONE) {
            auto* reg = reinterpret_cast<DWORD64*>(
                reinterpret_cast<char*>(c) + site.destCtx);
            *reg = 0;
        }
        c->Rip += site.instrLen;
        s_stats.knownSite++;
        return true;
    };

    // Check game executable known sites
    if (IsGameAddr(rip)) {
        uintptr_t off = rip - s_gameBase;
        for (const auto& site : s_knownSites) {
            if (site.offset == off) {
                return applyKnownSiteRecovery(ctx, site);
            }
        }
    }

    // Check mod DLL known sites
    HMODULE h = nullptr;
    constexpr DWORD gflags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                           | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (GetModuleHandleExA(gflags, reinterpret_cast<LPCSTR>(rip), &h) && h) {
        uintptr_t modBase = reinterpret_cast<uintptr_t>(h);
        uintptr_t off = rip - modBase;
        
        char buf[MAX_PATH];
        if (GetModuleFileNameA(h, buf, MAX_PATH)) {
            std::string path(buf);
            auto slash = path.find_last_of("\\/");
            std::string dllName = (slash != std::string::npos) ? path.substr(slash + 1) : path;
            
            for (auto& c : dllName) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            
            for (const auto& site : s_modKnownSites) {
                std::string siteDll(site.dllName);
                for (auto& c : siteDll) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
                
                if (dllName == siteDll && site.offset == off) {
                    applyKnownSiteRecovery(ctx, site);
                    
                    if (log) log->info("[VEH]   -> L1 mod-known-site: {} ({})", site.name, site.dllName);
                    return true;
                }
            }
        }
    }

    return false;
}

// ── L1b: Instruction-Pattern Match (version-independent) ────────────
// Instead of matching on fixed RIP offsets that shift between game/mod
// versions, L1b decodes the faulting instruction with Zydis and matches
// on its semantic pattern + register heuristics.  This covers:
//   P1  call [reg+disp] with bad base   → zero RAX, skip instruction
//   P2  jmp  [reg+disp] with bad base   → function return
//   P3  read from null/invalid pointer   → zero dest register, skip
//   P4  write to null/invalid pointer    → skip instruction
//
// Typical crash nodes: NiParticleSystem vtable corruption
//   (pFireballCore06-Emitter, MPSFireBoltImpact01, NiPSysData, etc.),
//   BSFadeNode skeleton, hkaRagdollInstance, BSAnimationGraphManager.
static bool L1b_InstructionPattern(PEXCEPTION_POINTERS info) {
    if (!s_decoderOK) return false;
    auto* ctx = info->ContextRecord;
    uintptr_t rip = ctx->Rip;

    // Only handle access violations
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return false;

    if (!IsReadable(reinterpret_cast<void*>(rip), 15)) return false;

    ZydisDecodedInstruction instr;
    ZydisDecodedOperand     ops[ZYDIS_MAX_OPERAND_COUNT];
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&s_decoder,
            reinterpret_cast<const void*>(rip), 15, &instr, ops)))
        return false;

    ULONG_PTR accessType = info->ExceptionRecord->ExceptionInformation[0];
    uintptr_t accessAddr = info->ExceptionRecord->ExceptionInformation[1];
    auto log = spdlog::default_logger();

    // ═══════════════════════════════════════════════════════════════════════
    // § EARLY STARTUP BAILOUT MODE
    // ═══════════════════════════════════════════════════════════════════════
    // During early startup (first 10 seconds), the game state is unstable and
    // instruction-skip/write-skip recovery corrupts objects that span multiple
    // instructions (strings, vtables, etc.), causing cascade crashes into
    // system DLLs (VCRUNTIME140.dll, ucrtbase.dll).
    //
    // SOLUTION: During early startup, ALWAYS use function return (bailout)
    // instead of instruction skip for ALL crashes. This prevents cascade crashes
    // by cleanly exiting problematic functions instead of leaving corrupted state.
    //
    // WHY 10 SECONDS: This covers plugin init + main menu load. After that,
    // the game is stable enough for normal recovery strategies.
    static DWORD s_startTick = GetTickCount();
    DWORD elapsed = GetTickCount() - s_startTick;
    bool isEarlyStartup = (elapsed < 10000);  // First 10 seconds
    
    if (isEarlyStartup) {
        // Early startup - use function return for ALL crashes
        uintptr_t rsp = ctx->Rsp;
        if (IsReadable(reinterpret_cast<void*>(rsp), 8)) {
            uint64_t ret = *reinterpret_cast<uint64_t*>(rsp);
            if (IsExec(reinterpret_cast<void*>(ret))) {
                ctx->Rsp += 8;
                ctx->Rip = ret;
                ctx->Rax = 0;
                s_stats.instrPattern++;
                if (log) {
                    const char* op = (accessType == 0) ? "READ" : (accessType == 1) ? "WRITE" : "EXEC";
                    log->info("[VEH]   -> L1b EARLY STARTUP BAILOUT: {} AV at {:#x}, "
                              "RETURNING to caller {:#x} ({}ms elapsed, prevents cascade)",
                              op, accessAddr, ret, elapsed);
                }
                return true;
            }
        }
        // Fallback: if function return fails during early startup, skip as last resort
        if (log) {
            log->warn("[VEH]   -> L1b EARLY STARTUP: func return failed, "
                      "skipping {} bytes (CASCADE RISK)", instr.length);
        }
        ctx->Rip += instr.length;
        s_stats.instrPattern++;
        return true;
    }

    // ── P1: call [reg+disp] with bad base ──
    // The faulting instruction is an indirect call through a memory operand
    // whose base register contains an invalid pointer (corrupted vtable,
    // float data interpreted as pointer, freed memory, etc.).
    // CRITICAL FIX: When the base is null/bad, DON'T just skip the call!
    // Skipping causes cascade crashes because downstream code will hit
    // more calls through the same null vtable. Instead, RETURN from the
    // entire function (like P2 does for JMPs). This cleanly exits the
    // problematic function rather than continuing with corrupted state.
    if (instr.mnemonic == ZYDIS_MNEMONIC_CALL) {
        for (ZyanU8 i = 0; i < instr.operand_count; ++i) {
            if (ops[i].type != ZYDIS_OPERAND_TYPE_MEMORY) continue;
            auto base = ops[i].mem.base;
            if (base == ZYDIS_REGISTER_NONE || base == ZYDIS_REGISTER_RSP ||
                base == ZYDIS_REGISTER_RBP  || base == ZYDIS_REGISTER_RIP)
                continue;

            int off = RegToCtx(base);
            if (off == kNONE) continue;

            uintptr_t baseVal = *reinterpret_cast<DWORD64*>(
                reinterpret_cast<char*>(ctx) + off);

            // Check if the base register looks bad (null, very low,
            // or points to non-readable memory)
            bool badBase = (baseVal < 0x10000) ||
                           !IsReadable(reinterpret_cast<void*>(baseVal), 8);
            if (!badBase) continue;

            // USE FUNCTION RETURN instead of instruction skip!
            // This prevents cascade crashes from null vtable calls.
            uintptr_t rsp = ctx->Rsp;
            if (IsReadable(reinterpret_cast<void*>(rsp), 8)) {
                uint64_t ret = *reinterpret_cast<uint64_t*>(rsp);
                if (IsExec(reinterpret_cast<void*>(ret))) {
                    ctx->Rsp += 8;
                    ctx->Rip = ret;
                    ctx->Rax = 0;
                    s_stats.instrPattern++;
                    if (log)
                        log->info("[VEH]   -> L1b P1: call [{}+{:#x}] bad base {:#x}, "
                                  "RETURNING to caller {:#x} (prevents cascade)",
                                  ZydisRegisterGetString(base),
                                  ops[i].mem.disp.has_displacement ? ops[i].mem.disp.value : 0,
                                  baseVal, ret);
                    return true;
                }
            }
            
            // Fallback: if function return fails, skip instruction as last resort
            ctx->Rax = 0;                    // null return value
            ctx->Rip += instr.length;        // skip past the call
            s_stats.instrPattern++;
            if (log)
                log->warn("[VEH]   -> L1b P1: call [{}+{:#x}] bad base {:#x}, "
                          "skipped {} bytes (func return failed, CASCADE RISK)",
                          ZydisRegisterGetString(base),
                          ops[i].mem.disp.has_displacement ? ops[i].mem.disp.value : 0,
                          baseVal, instr.length);
            return true;
        }
    }

    // ── P2: jmp [reg+disp] with bad base ──
    // Indirect jump through corrupted vtable / function pointer table.
    // Can't skip (no meaningful "next instruction"), so return from
    // the current function instead.
    if (instr.mnemonic == ZYDIS_MNEMONIC_JMP) {
        for (ZyanU8 i = 0; i < instr.operand_count; ++i) {
            if (ops[i].type != ZYDIS_OPERAND_TYPE_MEMORY) continue;
            auto base = ops[i].mem.base;
            if (base == ZYDIS_REGISTER_NONE || base == ZYDIS_REGISTER_RSP ||
                base == ZYDIS_REGISTER_RBP  || base == ZYDIS_REGISTER_RIP)
                continue;

            int off = RegToCtx(base);
            if (off == kNONE) continue;

            uintptr_t baseVal = *reinterpret_cast<DWORD64*>(
                reinterpret_cast<char*>(ctx) + off);

            bool badBase = (baseVal < 0x10000) ||
                           !IsReadable(reinterpret_cast<void*>(baseVal), 8);
            if (!badBase) continue;

            // Function return: pop ret-addr, zero RAX
            uintptr_t rsp = ctx->Rsp;
            if (!IsReadable(reinterpret_cast<void*>(rsp), 8)) continue;
            uint64_t ret = *reinterpret_cast<uint64_t*>(rsp);
            if (!IsExec(reinterpret_cast<void*>(ret))) continue;

            ctx->Rsp += 8;
            ctx->Rip  = ret;
            ctx->Rax  = 0;
            s_stats.instrPattern++;
            if (log)
                log->info("[VEH]   -> L1b P2: jmp [{}+{:#x}] bad base {:#x}, "
                          "returned to caller {:#x}",
                          ZydisRegisterGetString(base),
                          ops[i].mem.disp.has_displacement ? ops[i].mem.disp.value : 0,
                          baseVal, ret);
            return true;
        }
    }

    // ── P3: read from null / invalid pointer ──
    // mov reg, [reg+disp]  or  and/or/test/cmp reg, [mem]  etc.
    // where the memory operand base points to null or freed memory (READ AV).
    // Recovery: zero the destination register, skip the instruction.
    if (accessType == 0 /* READ */) {
        // Verify at least one memory operand is a read with a bad base
        bool hasBadRead = false;
        for (ZyanU8 i = 0; i < instr.operand_count; ++i) {
            if (ops[i].type != ZYDIS_OPERAND_TYPE_MEMORY) continue;
            if (!(ops[i].actions & ZYDIS_OPERAND_ACTION_READ)) continue;
            auto base = ops[i].mem.base;
            if (base == ZYDIS_REGISTER_RSP || base == ZYDIS_REGISTER_RBP ||
                base == ZYDIS_REGISTER_RIP)
                continue;

            if (base == ZYDIS_REGISTER_NONE) {
                // Absolute address access — check if it's a low/null address
                if (accessAddr < 0x10000) { hasBadRead = true; break; }
            } else {
                int off = RegToCtx(base);
                if (off == kNONE) continue;
                uintptr_t baseVal = *reinterpret_cast<DWORD64*>(
                    reinterpret_cast<char*>(ctx) + off);
                if (baseVal < 0x10000 ||
                    !IsReadable(reinterpret_cast<void*>(baseVal), 1)) {
                    hasBadRead = true;
                    break;
                }
            }
        }
        if (!hasBadRead) return false;

        // Don't handle CALL/JMP here — P1/P2 above are for those
        if (instr.mnemonic == ZYDIS_MNEMONIC_CALL ||
            instr.mnemonic == ZYDIS_MNEMONIC_JMP)
            return false;

        // Zero the first written register (the destination of the read)
        // Handle both GP registers and XMM registers (for SIMD reads like movss/movsd)
        int destOff = kNONE;
        bool zeroedXMM = false;
        for (ZyanU8 i = 0; i < instr.operand_count; ++i) {
            if (ops[i].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                (ops[i].actions & ZYDIS_OPERAND_ACTION_WRITE)) {
                // Try GP register first
                destOff = RegToCtx(ops[i].reg.value);
                if (destOff != kNONE) break;
                
                // Try XMM register (for SIMD instructions like movss, movsd, etc.)
                if (IsXMMRegister(ops[i].reg.value)) {
                    ZeroXMMRegister(ctx, ops[i].reg.value);
                    zeroedXMM = true;
                    if (log)
                        log->trace("[VEH] L1b P3: zeroed XMM register {}",
                                   ZydisRegisterGetString(ops[i].reg.value));
                    break;
                }
            }
        }
        if (destOff != kNONE) {
            *reinterpret_cast<DWORD64*>(
                reinterpret_cast<char*>(ctx) + destOff) = 0;
        }

        ctx->Rip += instr.length;
        s_stats.instrPattern++;
        if (log)
            log->info("[VEH]   -> L1b P3: read AV at {:#x}, zeroed dest{}, "
                      "skipped {} bytes", accessAddr, 
                      zeroedXMM ? " (XMM)" : "", instr.length);
        return true;
    }

    // ── P4: write to null / invalid pointer ──
    // mov [reg+disp], src  where the destination address is null/invalid
    // (WRITE AV).
    // CRITICAL: System DLLs (VCRUNTIME140.dll, ucrtbase.dll, etc.) need
    // function return instead of write-skip to prevent cascade crashes.
    // Game code can tolerate write-skip.
    if (accessType == 1 /* WRITE */) {
        // Don't handle CALL/JMP
        if (instr.mnemonic == ZYDIS_MNEMONIC_CALL ||
            instr.mnemonic == ZYDIS_MNEMONIC_JMP)
            return false;

        // Verify the access address is genuinely bad
        if (accessAddr < 0x10000 ||
            !IsReadable(reinterpret_cast<void*>(accessAddr), 1)) {
            
            // Check if this is a system DLL - use function return instead of write-skip
            if (IsSystemDLL(rip)) {
                // System DLL - use function return to prevent cascade
                uintptr_t rsp = ctx->Rsp;
                if (IsReadable(reinterpret_cast<void*>(rsp), 8)) {
                    uint64_t ret = *reinterpret_cast<uint64_t*>(rsp);
                    if (IsExec(reinterpret_cast<void*>(ret))) {
                        ctx->Rsp += 8;
                        ctx->Rip = ret;
                        ctx->Rax = 0;
                        s_stats.instrPattern++;
                        if (log)
                            log->info("[VEH]   -> L1b P4: write AV at {:#x} (SYSTEM DLL), "
                                      "RETURNING to caller {:#x} (prevents cascade)",
                                      accessAddr, ret);
                        return true;
                    }
                }
                // Fallback: if function return fails, skip as last resort
                if (log)
                    log->warn("[VEH]   -> L1b P4: write AV at {:#x} (SYSTEM DLL), "
                              "func return failed, skipping {} bytes (CASCADE RISK)",
                              accessAddr, instr.length);
            }
            
            // Game code - skip the write (safe for game code)
            ctx->Rip += instr.length;
            s_stats.instrPattern++;
            if (log)
                log->info("[VEH]   -> L1b P4: write AV at {:#x}, "
                          "skipped {} bytes", accessAddr, instr.length);
            return true;
        }
    }

    return false;
}

// ── L2: Learned Site ────────────────────────────────────────────────
static bool L2_LearnedSite(PCONTEXT ctx, uintptr_t rip) {
    size_t n = s_learnedCount.load(std::memory_order_acquire);
    for (size_t i = 0; i < n && i < MAX_LEARNED; ++i) {
        // Acquire load on rip acts as a publication barrier —
        // if we see the rip value, instrLen/destCtx are fully written.
        uintptr_t storedRip = s_learned[i].rip.load(std::memory_order_acquire);
        if (storedRip == rip) {
            if (s_learned[i].destCtx != kNONE) {
                auto* reg = reinterpret_cast<DWORD64*>(
                    reinterpret_cast<char*>(ctx) + s_learned[i].destCtx);
                *reg = 0;
            }
            ctx->Rip += s_learned[i].instrLen;
            s_learned[i].hits++;
            s_stats.learnedSite++;
            return true;
        }
    }
    return false;
}

// ── L3: Register Fixup ─────────────────────────────────────────────
// Redirect the faulting base register to point into the safety buffer
// so the instruction re-executes successfully (reads zero, writes to
// safe memory).  Works for both READ and WRITE AVs.
//
// IMPORTANT: L3 does NOT advance RIP — it re-executes the same
// instruction.  If the underlying pointer is still bad on the next
// iteration (e.g., a loop over a null list), L3 will fire again and
// again, freezing the game.  To prevent this, we track per-RIP hits
// and escalate to L4/L5 after a small threshold.
static constexpr uint32_t L3_MAX_HITS = 3;

static bool L3_RegFixup(PEXCEPTION_POINTERS info) {
    if (!s_decoderOK || !s_safetyBuf) return false;
    auto* ctx = info->ContextRecord;
    uintptr_t rip = ctx->Rip;

    // Check per-address hit count — if we've already done L3 here
    // multiple times, give up and let L4/L5 handle it instead.
    for (const auto& s : s_track) {
        if (s.rip == rip && s.count >= L3_MAX_HITS)
            return false;
    }

    // Cascade detection — if L3 has fired too many times in a short
    // burst (different addresses), the safety-buffer zeros are likely
    // propagating through a pointer chain.  Stop L3 and let L4/L5
    // skip the instruction or return from the function instead.
    DWORD now = GetTickCount();
    if (now - s_l3CascadeStart > L3_CASCADE_WINDOW) {
        // New window — reset
        s_l3CascadeCount.store(1, std::memory_order_relaxed);
        s_l3CascadeStart = now;
    } else {
        uint32_t c = s_l3CascadeCount.fetch_add(1, std::memory_order_relaxed) + 1;
        if (c > L3_CASCADE_MAX) {
            auto log = spdlog::default_logger();
            if (log)
                log->warn("[VEH] L3 cascade suppressed ({} fires in {}ms window)",
                          c, L3_CASCADE_WINDOW);
            return false;
        }
    }

    if (!IsReadable(reinterpret_cast<void*>(rip), 15)) return false;

    ZydisDecodedInstruction instr;
    ZydisDecodedOperand     ops[ZYDIS_MAX_OPERAND_COUNT];
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&s_decoder,
            reinterpret_cast<const void*>(rip), 15, &instr, ops)))
        return false;

    for (ZyanU8 i = 0; i < instr.operand_count; ++i) {
        if (ops[i].type != ZYDIS_OPERAND_TYPE_MEMORY) continue;
        auto base = ops[i].mem.base;
        if (base == ZYDIS_REGISTER_NONE ||
            base == ZYDIS_REGISTER_RSP  ||
            base == ZYDIS_REGISTER_RBP  ||
            base == ZYDIS_REGISTER_RIP)
            continue;

        // GUARD: Never redirect the base register for CALL or JMP
        // instructions.  Redirecting to the safety buffer would cause
        // execution of zeroed memory, which triggers another AV.
        // These are handled properly by L1b (pattern) or L4 (skip).
        if (instr.mnemonic == ZYDIS_MNEMONIC_CALL ||
            instr.mnemonic == ZYDIS_MNEMONIC_JMP) {
            return false;
        }

        int off = RegToCtx(base);
        if (off == kNONE) continue;

        auto* reg = reinterpret_cast<DWORD64*>(
            reinterpret_cast<char*>(ctx) + off);

        int64_t disp = ops[i].mem.disp.has_displacement
                     ? ops[i].mem.disp.value : 0;
        uintptr_t bufMid = reinterpret_cast<uintptr_t>(s_safetyBuf) + SAFETY_SZ / 2;
        uintptr_t newBase = static_cast<uintptr_t>(
            static_cast<int64_t>(bufMid) - disp);
        uintptr_t effective = static_cast<uintptr_t>(
            static_cast<int64_t>(newBase) + disp);

        uintptr_t bufStart = reinterpret_cast<uintptr_t>(s_safetyBuf);
        if (effective >= bufStart && effective < bufStart + SAFETY_SZ) {
            *reg = static_cast<DWORD64>(newBase);
            s_stats.regFixup++;
            return true;
        }
    }
    return false;
}

// ── L4: Instruction Skip ────────────────────────────────────────────
// Decode the faulting instruction, zero its destination register (if
// it writes to one), and advance RIP past the instruction.  Then
// cache the result in the learned-sites table for future L2 fast-path.
static bool L4_InstrSkip(PCONTEXT ctx) {
    if (!s_decoderOK) return false;
    uintptr_t rip = ctx->Rip;
    if (!IsReadable(reinterpret_cast<void*>(rip), 15)) return false;

    ZydisDecodedInstruction instr;
    ZydisDecodedOperand     ops[ZYDIS_MAX_OPERAND_COUNT];
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&s_decoder,
            reinterpret_cast<const void*>(rip), 15, &instr, ops)))
        return false;

    // CRITICAL: For CALL and JMP instructions, use function return instead
    // of instruction skip! Skipping causes cascade crashes.
    if (instr.mnemonic == ZYDIS_MNEMONIC_CALL || instr.mnemonic == ZYDIS_MNEMONIC_JMP) {
        uintptr_t rsp = ctx->Rsp;
        if (IsReadable(reinterpret_cast<void*>(rsp), 8)) {
            uint64_t ret = *reinterpret_cast<uint64_t*>(rsp);
            if (IsExec(reinterpret_cast<void*>(ret))) {
                ctx->Rsp += 8;
                ctx->Rip = ret;
                ctx->Rax = 0;
                s_stats.instrSkip++;
                auto log = spdlog::default_logger();
                if (log)
                    log->info("[VEH]   -> L4: {} at {:#x}, RETURNING to caller {:#x}",
                              instr.mnemonic == ZYDIS_MNEMONIC_CALL ? "CALL" : "JMP",
                              rip, ret);
                return true;
            }
        }
        // Function return failed - don't skip CALL/JMP, let higher layers handle it
        return false;
    }

    // Find the first register that is written by this instruction
    int destOff = kNONE;
    for (ZyanU8 i = 0; i < instr.operand_count; ++i) {
        if (ops[i].type == ZYDIS_OPERAND_TYPE_REGISTER &&
            (ops[i].actions & ZYDIS_OPERAND_ACTION_WRITE)) {
            destOff = RegToCtx(ops[i].reg.value);
            if (destOff != kNONE) break;
        }
    }

    // Zero the dest register (safe: the value would have been garbage anyway)
    if (destOff != kNONE) {
        auto* reg = reinterpret_cast<DWORD64*>(
            reinterpret_cast<char*>(ctx) + destOff);
        *reg = 0;
    }

    ctx->Rip += instr.length;
    s_stats.instrSkip++;

    // Cache in learned-sites for L2 fast-path on repeats.
    // Thread safety: Write instrLen/destCtx/hits BEFORE publishing rip
    // with a release store.  Readers use acquire load on rip, so they
    // see the fully-initialized non-atomic fields.
    size_t idx = s_learnedCount.load(std::memory_order_acquire);
    // Check not already cached
    bool found = false;
    for (size_t i = 0; i < idx && i < MAX_LEARNED; ++i) {
        if (s_learned[i].rip.load(std::memory_order_relaxed) == rip) { found = true; break; }
    }
    if (!found) {
        size_t slot = s_learnedCount.fetch_add(1, std::memory_order_acq_rel);
        if (slot < MAX_LEARNED) {
            // Write non-atomic fields first (not yet visible to readers)
            s_learned[slot].instrLen = static_cast<uint8_t>(instr.length);
            s_learned[slot].destCtx  = destOff;
            s_learned[slot].hits     = 1;
            // Release store on rip publishes all preceding writes
            s_learned[slot].rip.store(rip, std::memory_order_release);
        }
    }
    return true;
}

// ── L5: Function Return ─────────────────────────────────────────────
// Pop the return address from the stack, set RAX = 0, jump to it.
// Equivalent to the current function returning nullptr / 0.
static bool L5_FuncReturn(PCONTEXT ctx) {
    uintptr_t rsp = ctx->Rsp;
    if (!IsReadable(reinterpret_cast<void*>(rsp), 8)) return false;
    uint64_t ret = *reinterpret_cast<uint64_t*>(rsp);
    if (!IsExec(reinterpret_cast<void*>(ret)))         return false;

    ctx->Rsp += 8;
    ctx->Rip  = ret;
    ctx->Rax  = 0;
    s_stats.funcReturn++;
    return true;
}

// ── Execute-AV Recovery ─────────────────────────────────────────────
// Handle the case where RIP itself is at a bad (non-readable) address.
// This means the CPU jumped to an invalid address via a corrupted vtable,
// wild function pointer, or freed memory.  The return address (caller)
// is on the stack at RSP.
//
// Strategy:
//   1. Read the return address from RSP (this is where the CALL came from)
//   2. Verify the caller is in executable memory
//   3. Optionally verify the byte before the return address is a CALL opcode
//   4. Set RAX=0 (null return value), pop RSP, jump to caller
//
// This effectively makes the corrupted call return nullptr, which is
// far safer than crashing.  The calling code typically null-checks the
// result or ignores it (e.g., vtable calls for rendering, animation).
static bool ExecuteAV_Recovery(PCONTEXT ctx, bool shouldLog = true) {
    auto log = shouldLog ? spdlog::default_logger() : nullptr;
    uintptr_t rsp = ctx->Rsp;

    // Validate stack pointer
    if (!IsReadable(reinterpret_cast<void*>(rsp), 8)) {
        if (log) log->warn("[VEH] Execute-AV: RSP {:#x} not readable", rsp);
        return false;
    }

    // The return address was pushed by the CALL that jumped to the bad RIP
    uint64_t retAddr = *reinterpret_cast<uint64_t*>(rsp);

    // Verify return address is in executable memory
    if (!IsExec(reinterpret_cast<void*>(retAddr))) {
        if (log) log->warn("[VEH] Execute-AV: return addr {:#x} not executable", retAddr);
        // Try deeper stack scan — the call may have been through a thunk
        // that pushed additional data
        for (size_t off = 8; off < 64; off += 8) {
            if (!IsReadable(reinterpret_cast<void*>(rsp + off), 8)) break;
            uint64_t candidate = *reinterpret_cast<uint64_t*>(rsp + off);
            if (candidate < 0x10000) continue;
            if (!IsExec(reinterpret_cast<void*>(candidate))) continue;

            // Verify the byte(s) before this address look like a CALL
            if (IsReadable(reinterpret_cast<void*>(candidate - 5), 5)) {
                uint8_t b = *reinterpret_cast<uint8_t*>(candidate - 5);
                if (b == 0xE8) {  // CALL rel32
                    ctx->Rsp = rsp + off + 8;
                    ctx->Rip = candidate;
                    ctx->Rax = 0;
                    s_stats.funcReturn++;
                    if (log) log->info("[VEH]   -> Execute-AV: deep return to {:#x} "
                                       "(found CALL at {:#x}), RSP adjusted +{}",
                                       candidate, candidate - 5, off + 8);
                    return true;
                }
            }
            // Also check for indirect call FF 15 (6 bytes) or FF /2 reg (2-3 bytes)
            if (IsReadable(reinterpret_cast<void*>(candidate - 6), 6)) {
                uint8_t b0 = *reinterpret_cast<uint8_t*>(candidate - 6);
                uint8_t b1 = *reinterpret_cast<uint8_t*>(candidate - 5);
                if (b0 == 0xFF && (b1 == 0x15 || (b1 & 0x38) == 0x10)) {  // CALL [rip+disp32] or CALL [reg]
                    ctx->Rsp = rsp + off + 8;
                    ctx->Rip = candidate;
                    ctx->Rax = 0;
                    s_stats.funcReturn++;
                    if (log) log->info("[VEH]   -> Execute-AV: deep return to {:#x} "
                                       "(found indirect CALL), RSP adjusted +{}",
                                       candidate, off + 8);
                    return true;
                }
            }
        }
        return false;
    }

    // Heuristic: verify the instruction before the return address is a CALL.
    // This reduces false positives from random executable pointers on the stack.
    bool callerVerified = false;
    // Check for E8 xx xx xx xx (CALL rel32, 5 bytes)
    if (IsReadable(reinterpret_cast<void*>(retAddr - 5), 5)) {
        uint8_t b = *reinterpret_cast<uint8_t*>(retAddr - 5);
        if (b == 0xE8) callerVerified = true;
    }
    // Check for FF 15 xx xx xx xx (CALL [rip+disp32], 6 bytes)
    if (!callerVerified && IsReadable(reinterpret_cast<void*>(retAddr - 6), 6)) {
        uint8_t b0 = *reinterpret_cast<uint8_t*>(retAddr - 6);
        uint8_t b1 = *reinterpret_cast<uint8_t*>(retAddr - 5);
        if (b0 == 0xFF && b1 == 0x15) callerVerified = true;
    }
    // Check for FF /2 forms (2-3 byte indirect CALL through register)
    if (!callerVerified && IsReadable(reinterpret_cast<void*>(retAddr - 2), 2)) {
        uint8_t b0 = *reinterpret_cast<uint8_t*>(retAddr - 2);
        uint8_t b1 = *reinterpret_cast<uint8_t*>(retAddr - 1);
        // FF D0-FF D7 = call rax..call rdi, FF 10-FF 17 = call [rax]..[rdi]
        if (b0 == 0xFF && ((b1 & 0xF8) == 0xD0 || (b1 & 0x38) == 0x10))
            callerVerified = true;
    }
    // Check for REX.W + FF /2 (3 byte, e.g., 48 FF D0 = call rax with REX)
    if (!callerVerified && IsReadable(reinterpret_cast<void*>(retAddr - 3), 3)) {
        uint8_t b0 = *reinterpret_cast<uint8_t*>(retAddr - 3);
        uint8_t b1 = *reinterpret_cast<uint8_t*>(retAddr - 2);
        uint8_t b2 = *reinterpret_cast<uint8_t*>(retAddr - 1);
        if ((b0 & 0xF0) == 0x40 && b1 == 0xFF && ((b2 & 0xF8) == 0xD0 || (b2 & 0x38) == 0x10))
            callerVerified = true;
    }

    if (!callerVerified) {
        // Still try if the return address is in game code — better than CTD
        if (!IsRecoverableAddr(retAddr)) {
            if (log) log->warn("[VEH] Execute-AV: return addr {:#x} not verified as CALL target", retAddr);
            return false;
        }
        if (log) log->warn("[VEH] Execute-AV: return addr {:#x} not verified as CALL, "
                           "but in game/mod code — attempting recovery anyway", retAddr);
    }

    // Capture the bad RIP for logging BEFORE we modify the context
    uintptr_t badRip = ctx->Rip;

    // Recover: pop return address, zero RAX, jump to caller
    ctx->Rsp = rsp + 8;   // pop the return address
    ctx->Rip = retAddr;   // jump back to caller
    ctx->Rax = 0;         // null return value
    s_stats.funcReturn++;

    if (log) {
        log->info("[VEH]   -> Execute-AV recovery: bad RIP={:#x}, returning to caller {:#x} "
                  "({}+{:#x}), RAX=0{}",
                  badRip, retAddr, ModName(retAddr), ModOff(retAddr),
                  callerVerified ? " [CALL verified]" : " [unverified]");
    }
    return true;
}

// ── L6: Deep Stack Walk ─────────────────────────────────────────────
// Check if the bytes before a return address look like a CALL instruction
// This is a heuristic to reduce false positives in L6_DeepWalk
// Returns true if likely a real call site, false if probably not
static bool IsCallSite(uintptr_t returnAddr) {
    // CALL instructions that could lead to this return address:
    // - E8 xx xx xx xx      : CALL rel32 (5 bytes)
    // - FF 15 xx xx xx xx   : CALL [RIP+disp32] (6 bytes)
    // - FF 10-17            : CALL [reg] / CALL [reg+disp8/32] (2+ bytes)
    // - FF 50-57 xx         : CALL [reg+disp8] (3 bytes)
    // - FF D0-D7            : CALL reg (2 bytes)
    // - 41 FF Dx            : CALL r8-r15 (3 bytes)
    
    // Check CALL rel32 (most common)
    if (IsReadable(reinterpret_cast<void*>(returnAddr - 5), 5)) {
        uint8_t opcode = *reinterpret_cast<uint8_t*>(returnAddr - 5);
        if (opcode == 0xE8) return true;
    }
    
    // Check CALL [RIP+disp32] (FF 15)
    if (IsReadable(reinterpret_cast<void*>(returnAddr - 6), 6)) {
        uint8_t* p = reinterpret_cast<uint8_t*>(returnAddr - 6);
        if (p[0] == 0xFF && p[1] == 0x15) return true;
    }
    
    // Check CALL reg (FF D0-D7) - 2 bytes
    if (IsReadable(reinterpret_cast<void*>(returnAddr - 2), 2)) {
        uint8_t* p = reinterpret_cast<uint8_t*>(returnAddr - 2);
        if (p[0] == 0xFF && (p[1] >= 0xD0 && p[1] <= 0xD7)) return true;
    }
    
    // Check CALL [reg+disp8] (FF 50-57 xx) - 3 bytes
    if (IsReadable(reinterpret_cast<void*>(returnAddr - 3), 3)) {
        uint8_t* p = reinterpret_cast<uint8_t*>(returnAddr - 3);
        if (p[0] == 0xFF && (p[1] >= 0x50 && p[1] <= 0x57)) return true;
    }
    
    // Check REX.W CALL r8-r15 (41 FF Dx) - 3 bytes
    if (IsReadable(reinterpret_cast<void*>(returnAddr - 3), 3)) {
        uint8_t* p = reinterpret_cast<uint8_t*>(returnAddr - 3);
        if (p[0] == 0x41 && p[1] == 0xFF && (p[2] >= 0xD0 && p[2] <= 0xD7)) return true;
    }
    
    // Check CALL [reg+disp32] (FF 90-97 xx xx xx xx) - 6 bytes
    if (IsReadable(reinterpret_cast<void*>(returnAddr - 6), 6)) {
        uint8_t* p = reinterpret_cast<uint8_t*>(returnAddr - 6);
        if (p[0] == 0xFF && (p[1] >= 0x90 && p[1] <= 0x97)) return true;
    }
    
    return false;
}

// Scan up to 8 KB of stack for any executable pointer that could be a
// valid return address.  Skip the first slot (that's L5's territory).
// Uses CALL heuristic to reduce false positives.
static bool L6_DeepWalk(PCONTEXT ctx) {
    uintptr_t rsp = ctx->Rsp;
    auto log = spdlog::default_logger();
    
    for (size_t off = 8; off < 8192; off += 8) {
        uintptr_t slot = rsp + off;
        if (!IsReadable(reinterpret_cast<void*>(slot), 8)) break;
        uint64_t candidate = *reinterpret_cast<uint64_t*>(slot);
        if (candidate < 0x10000) continue;
        if (!IsExec(reinterpret_cast<void*>(candidate))) continue;
        
        if (!IsCallSite(candidate)) {
            // Log for debugging but continue searching
            if (log) log->trace("[VEH] L6 skipping non-call-site candidate at {:#x}", candidate);
            continue;
        }

        ctx->Rsp = slot + 8;
        ctx->Rip = candidate;
        ctx->Rax = 0;
        s_stats.deepWalk++;
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════
// § 5.5  CrashLogger Log Ingestion
// ═══════════════════════════════════════════════════════════════════════
// Scans the SKSE log directory for CrashLogger crash logs from previous
// sessions. Extracts crash RIPs and module offsets to pre-populate the
// pattern learning system, so CrashGuard has awareness of crash patterns
// the user has historically experienced.
//
// This is a best-effort parse — CrashLogger's format is human-readable
// text, not structured data. We extract the essential crash location
// info and log a summary.
// ---------------------------------------------------------------------------
// CrashLogger log helpers — used by IngestCrashLoggerLogs AND the background
// injection thread.  Must be plain C-style (no C++ destructors) where they
// are called from SEH-guarded paths.
// ---------------------------------------------------------------------------

static std::string GetSKSELogDir_Safe() {
    char doc[MAX_PATH];
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, 0, doc))) return {};
    std::string base = std::string(doc) + "\\My Games\\";
    base += REL::Module::IsVR() ? "Skyrim VR" : "Skyrim Special Edition";
    return base + "\\SKSE\\";
}

static const char* kCrashGuardLogMarker = "SkyrimCrashGuard (experimental)";

static bool FileHasCrashGuardMarker(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    char line[256];
    int n = 0; bool found = false;
    while (n++ < 6 && fgets(line, sizeof(line), f))
        if (strstr(line, kCrashGuardLogMarker)) { found = true; break; }
    fclose(f);
    return found;
}

static void PrependStringToFile(const std::string& path, const std::string& text) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    std::string existing(sz, '\0');
    if (sz > 0) fread(&existing[0], 1, sz, f);
    fclose(f);
    f = fopen(path.c_str(), "wb");
    if (!f) return;
    fwrite(text.c_str(), 1, text.size(), f);
    fwrite(existing.c_str(), 1, existing.size(), f);
    fclose(f);
}

// Synchronous injection function - executes immediately in VEH handler
// Returns true if injection succeeded, false otherwise
static bool InjectIntoCrashLoggerLog_Sync() {
    auto log = spdlog::default_logger();
    
    std::string logDir = GetSKSELogDir_Safe();
    if (logDir.empty()) {
        if (log) log->error("[VEH] CrashLogger injection failed: could not determine SKSE log directory");
        return false;
    }
    
    auto nowFS = std::filesystem::file_time_type::clock::now();
    std::string newestPath;
    std::chrono::nanoseconds newestAge{std::chrono::seconds(999)};
    
    try {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(logDir, ec)) {
            if (ec || !entry.is_regular_file()) continue;
            std::string fn = entry.path().filename().string();
            if (fn.size() < 10 || fn.compare(0, 6, "crash-") != 0) continue;
            if (fn.size() < 4  || fn.compare(fn.size()-4, 4, ".log") != 0) continue;
            if (fn.find("CrashGuard") != std::string::npos) continue;
            auto wt = entry.last_write_time(ec); if (ec) continue;
            auto age = std::chrono::duration_cast<std::chrono::nanoseconds>(nowFS - wt);
            // Reduced from 60s to 5s - CrashLogger writes immediately
            if (age < std::chrono::seconds(5) && age < newestAge) {
                newestAge = age;
                newestPath = entry.path().string();
            }
        }
    } catch (const std::exception& e) {
        if (log) log->error("[VEH] CrashLogger injection failed: directory iteration error: {}", e.what());
        return false;
    } catch (...) {
        if (log) log->error("[VEH] CrashLogger injection failed: unknown directory iteration error");
        return false;
    }
    
    if (newestPath.empty()) {
        if (log) log->warn("[VEH] CrashLogger log not found within 5 seconds");
        return false;
    }
    
    // Check for CrashGuard marker to prevent duplicate injection
    if (FileHasCrashGuardMarker(newestPath)) {
        if (log) log->info("[VEH] CrashGuard marker already present in {}, skipping injection", newestPath);
        return true;  // Already injected, consider this success
    }
    
    std::string notice =
        "================================================================================\n"
        "NOTE: SkyrimCrashGuard (experimental) is also installed.\n"
        "CrashGuard attempted to recover this crash via its 7-layer VEH chain (L1-L6+L1b).\n"
        "All recovery layers were exhausted. CrashGuard may have modified game state\n"
        "during those attempts. Read this crash log with a grain of salt and\n"
        "cross-reference SkyrimCrashGuard.log (same SKSE folder) for full details.\n"
        "================================================================================\n\n";
    
    try {
        PrependStringToFile(newestPath, notice);
        if (log) log->info("[VEH] Injection successful: {}", newestPath);
        return true;
    } catch (const std::exception& e) {
        if (log) log->error("[VEH] Injection failed: file write error: {}", e.what());
        return false;
    } catch (...) {
        if (log) log->error("[VEH] Injection failed: unknown file write error");
        return false;
    }
}

// Separate SEH-safe helper: dump raw stack pointers without C++ objects in scope
// Pure-C SEH helper — MUST NOT contain any C++ objects with destructors
// because MSVC forbids mixing __try with C++ stack-unwind objects (C2712).
static int ReadRawStackPointers_SEH(const uintptr_t* sp, uintptr_t* out, int count) {
    int n = 0;
    for (int i = 0; i < count; ++i) {
        __try { out[n++] = sp[i]; }
        __except(EXCEPTION_EXECUTE_HANDLER) { break; }
    }
    return n;
}

static void IngestCrashLoggerLogs() {
    auto log = spdlog::default_logger();

    // Use canonical log directory - fallback to Documents path
    std::string logDir;
    char docPath[MAX_PATH];
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, 0, docPath)))
        return;

    logDir = std::string(docPath) + "\\My Games\\Skyrim Special Edition\\SKSE\\";
    if (REL::Module::IsVR()) {
        logDir = std::string(docPath) + "\\My Games\\Skyrim VR\\SKSE\\";
    }

    // Scan for CrashLogger log files (crash-YYYY-MM-DD-HH-MM-SS.log)
    std::error_code ec;
    if (!std::filesystem::exists(logDir, ec)) return;

    uint32_t logsFound = 0;
    uint32_t crashSitesLearned = 0;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(logDir, ec)) {
            if (ec || !entry.is_regular_file()) continue;

            std::string filename = entry.path().filename().string();

            // Match CrashLogger naming convention: crash-YYYY-MM-DD-*.log
            if (filename.size() < 10 || filename.substr(0, 6) != "crash-" ||
                filename.substr(filename.size() - 4) != ".log")
                continue;

            // Don't ingest our own recovery reports
            if (filename.find("CrashGuard") != std::string::npos) continue;

            logsFound++;

            // Read first 200 lines (crash info is at the top)
            std::ifstream file(entry.path());
            if (!file.is_open()) continue;

            std::string line;
            uint32_t lineNum = 0;
            bool foundUnhandled = false;

            while (std::getline(file, line) && lineNum < 200) {
                lineNum++;

                // Look for "Unhandled exception" line which contains the crash RIP
                // Format varies but typically: "Unhandled exception ... at 0xNNNN"
                // or module+offset patterns like "SkyrimSE.exe+0xNNNNN"
                if (line.find("Unhandled exception") != std::string::npos ||
                    line.find("EXCEPTION_ACCESS_VIOLATION") != std::string::npos) {
                    foundUnhandled = true;
                }

                // Look for the crash address line — typically has the module+offset
                if (foundUnhandled) {
                    // Look for patterns like "SkyrimSE.exe+0x406FBE" or "skee64.dll+0x367A3"
                    auto plusPos = line.find("+0x");
                    if (plusPos != std::string::npos && plusPos > 0) {
                        // Extract module name
                        auto modStart = line.rfind(' ', plusPos);
                        if (modStart == std::string::npos) modStart = 0;
                        else modStart++;

                        std::string modName = line.substr(modStart, plusPos - modStart);
                        std::string offsetStr = line.substr(plusPos + 3);

                        // Trim offset to first non-hex character
                        size_t hexEnd = 0;
                        while (hexEnd < offsetStr.size() &&
                               ((offsetStr[hexEnd] >= '0' && offsetStr[hexEnd] <= '9') ||
                                (offsetStr[hexEnd] >= 'a' && offsetStr[hexEnd] <= 'f') ||
                                (offsetStr[hexEnd] >= 'A' && offsetStr[hexEnd] <= 'F')))
                            hexEnd++;
                        offsetStr = offsetStr.substr(0, hexEnd);

                        if (!offsetStr.empty() && !modName.empty()) {
                            crashSitesLearned++;
                            if (log && crashSitesLearned <= 20) {
                                log->trace("[VEH] CrashLogger ingestion: {}+0x{} from {}",
                                           modName, offsetStr, filename);
                            }
                        }

                        foundUnhandled = false;  // Only capture the first crash per file
                        break;
                    }
                }
            }
        }
    } catch (...) {
        // Best-effort — don't crash on directory traversal errors
    }

    if (log) {
        if (logsFound > 0) {
            log->info("[VEH] CrashLogger log ingestion: scanned {} crash logs, "
                      "identified {} crash sites from previous sessions",
                      logsFound, crashSitesLearned);
        } else {
            log->trace("[VEH] CrashLogger log ingestion: no previous crash logs found");
        }
    }

    // ── Inject CrashGuard notice into previous-session crash logs ──
    // Any crash-*.log that doesn't already have the marker gets a notice prepended
    // so users reading old logs know CrashGuard was active.
    if (CrashLoggerDetector::Detector::IsCrashLoggerPresent()) {
        int injected = 0;
        try {
            std::error_code ec2;
            for (const auto& entry : std::filesystem::directory_iterator(logDir, ec2)) {
                if (ec2 || !entry.is_regular_file()) continue;
                std::string fname = entry.path().filename().string();
                if (fname.size() < 10 || fname.compare(0, 6, "crash-") != 0) continue;
                if (fname.size() < 4 || fname.compare(fname.size() - 4, 4, ".log") != 0) continue;
                if (fname.find("CrashGuard") != std::string::npos) continue;
                std::string fpath = entry.path().string();
                if (FileHasCrashGuardMarker(fpath)) continue;
                std::string notice =
                    "================================================================================\n"
                    "NOTE: SkyrimCrashGuard (experimental) was also installed during this session.\n"
                    "CrashGuard may have modified game state via VEH recovery attempts before this\n"
                    "crash reached CrashLogger. Read this analysis with a grain of salt and\n"
                    "cross-reference SkyrimCrashGuard.log for CrashGuard's own perspective.\n"
                    "================================================================================\n\n";
                PrependStringToFile(fpath, notice);
                injected++;
            }
        } catch (...) {}
        if (log && injected > 0)
            log->info("[VEH] Injected CrashGuard notice into {} previous crash log(s)", injected);
    }
}

// ═══════════════════════════════════════════════════════════════════════
// § 6  VEHExceptionHandler Class Implementation
// ═══════════════════════════════════════════════════════════════════════

PVOID VEHExceptionHandler::s_handler = nullptr;
std::vector<HANDLE> VEHExceptionHandler::s_pausedThreads;

bool VEHExceptionHandler::Initialize() {
    auto log = spdlog::default_logger();
    
    // ── Game module bounds (auto-detect SE/AE/VR) ──
    const auto& game = GameDetect::Detect();
    if (game.hModule) {
        s_gameBase = game.base;
        s_gameEnd  = game.end;
    }

    // ── Detect CrashGuard's own module bounds ──
    {
        HMODULE hSelf = nullptr;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&VEHExceptionHandler::Initialize), &hSelf);
        if (hSelf) {
            MODULEINFO selfInfo{};
            if (GetModuleInformation(GetCurrentProcess(), hSelf, &selfInfo, sizeof(selfInfo))) {
                s_selfBase = reinterpret_cast<uintptr_t>(selfInfo.lpBaseOfDll);
                s_selfEnd  = s_selfBase + selfInfo.SizeOfImage;
                if (log) log->info("[VEH] Self-module bounds: {:#x} - {:#x}", s_selfBase, s_selfEnd);
            }
        }
    }

    // ── Populate known crash sites based on runtime ──
    if (REL::Module::IsVR()) {
        // BGSImpactManager footstep pipeline: mov rax,[rcx+0x20]
        // RCX = dangling ptr, caller at +05A9A8E does `test rax,rax` → null-safe
        s_knownSites.push_back({ 0x02D32A5, 4, kRAX, "ImpactManager footstep null deref", false });
        
        // BGSImpactManager material lookup crash at +0x2d4c47
        // This is the "Reindeer crash" - cmp [rax], rsi where RAX is garbage pointer
        // Recovery: zero RAX, skip instruction
        s_knownSites.push_back({ 0x02D4C47, 3, kRAX, "ImpactManager material lookup null (Reindeer crash)", false });
        
        // VR-specific: NiPSysData particle system crashes
        // Common when particle effects reference deleted nodes
        s_knownSites.push_back({ 0x0D09E23, 4, kRAX, "VR NiPSysData particle update null", false });
        s_knownSites.push_back({ 0x0D0A1B5, 3, kRCX, "VR NiPSysData emitter null", false });
        
        // VR-specific: BSFadeNode skeleton crashes
        // Occurs when skeleton becomes invalid during LOD transition
        s_knownSites.push_back({ 0x0C8F2A7, 4, kRAX, "VR BSFadeNode skeleton null", false });
        
        // VR-specific: hkaRagdollInstance crashes
        // Animation ragdoll physics with invalid bones
        s_knownSites.push_back({ 0x0B7E3C1, 4, kRAX, "VR hkaRagdollInstance bone null", false });
        
        // VR-specific: OpenVR compositor crashes (openvr_api.dll)
        // These manifest when VR headset disconnects or has tracking issues
        // Note: These are mod DLL sites, added below
        
        // ── Save-load stability: Sky/Moon null texture pointer ──
        // Moon phase update dereferences a texture data pointer that can be null
        // during save loading (sky system not fully initialized).
        // Instruction: movss xmmN, dword ptr [rax+0x18] where RAX=0
        // VR offsets pending user crash reports — L1b pattern matching covers generically
        // s_knownSites.push_back({ 0xVR_MOON_OFFSET, 6, kXMM13, "VR Moon null texture deref during save load" });
        
        // ── Save-load stability: TESWaterReflections null this ──
        // Water reflection update with null 'this' pointer during save load.
        // Instruction: movss xmm0, dword ptr [rdi+0xF0] where RDI=0
        // VR offsets pending user crash reports — L1b pattern matching covers generically
        // s_knownSites.push_back({ 0xVR_WATER_OFFSET, 8, kXMM0, "VR WaterReflections null this" });
        
        if (log) log->info("[VEH] Registered {} VR-specific game crash sites (Moon/Water covered by L1b)", 6);
    } else {
        // SE/AE specific crash sites
        // These are common crash locations found in crash logger reports
        // Using relative offsets from module base for version independence
        
        // BGSImpactManager::PlayImpactEffect - common on projectile impacts
        // SE: ~0x5A8E10, AE: varies
        // Crash when impact data is null during effect spawn
        s_knownSites.push_back({ 0x05A8E10, 4, kRAX, "SE ImpactManager PlayImpactEffect null", false });
        
        // ConsoleLog::VPrint - happens when formatting bad strings from mods
        // Common with poorly scripted mods that print invalid data
        s_knownSites.push_back({ 0x08662A0, 4, kRDX, "SE ConsoleLog printf format crash", false });
        
        // Character::AttachBodyPart - mesh attachment issues
        // Occurs when loading actors with missing body parts
        s_knownSites.push_back({ 0x06092B0, 4, kRAX, "SE Character AttachBodyPart null", false });
        
        // BSFadeNode::GetVisibleRefCount - LOD transition crash
        // Common when rapidly traveling between cells
        s_knownSites.push_back({ 0x0C8E1A0, 4, kRAX, "SE BSFadeNode LOD transition crash", false });
        
        // ScriptFunction::Call - Papyrus native call crash
        // Happens when script calls native function with bad parameters
        s_knownSites.push_back({ 0x12D0550, 4, kRCX, "SE ScriptFunction::Call null this", false });
        
        // TESObjectARMO vtable crash - armor form corruption
        // Seen when armor has invalid enchantment reference
        s_knownSites.push_back({ 0x021DAF0, 4, kRAX, "SE TESObjectARMO vtable corruption", false });
        
        // BGSSaveLoadManager::ProcessEvents - save corruption cascade
        // Critical: prevents save corruption from cascading
        s_knownSites.push_back({ 0x058E2B0, 4, kRAX, "SE SaveLoadManager event null", false });
        
        // ── Moon/Sky rendering crashes during save load with SkyrimSoulsRE ──
        // When game continues rendering during async save load, Moon/Sky objects
        // may have null or partially-loaded textures/vtables.
        
        // Moon::UpdateImpl - call [rax+0x160] with RAX=0 (null vtable)
        // SkyrimSE.exe+0D1BF7F (function 70251+0xF)
        // BAILOUT MODE: Skipping individual instructions causes cascade crashes.
        // Instead, return from the entire Moon rendering function to cleanly
        // skip this frame's Moon update. Next frame the vtable may be valid.
        s_knownSites.push_back({ 0x0D1BF7F, 6, kRAX, "Moon vtable null call during save load (SkyrimSoulsRE)", true });
        
        // Additional Moon crash points in same function (70251) - these are CALLs through null vtables
        // +0D1BF76 appears to be a CALL variant; +0D1BF85 another
        // All use bailout mode to return from the function instead of skip
        s_knownSites.push_back({ 0x0D1BF76, 6, kRAX, "Moon rendering null vtable call A", true });
        // +0D1BF85 is "mov rcx, [rbx+0x30]" (48 8B 4B 30): len=4, dest=RCX
        s_knownSites.push_back({ 0x0D1BF85, 4, kRCX, "Moon rendering null ptr read (mov rcx,[rbx+0x30])", true });
        
        // Moon function entry points - call [rax+disp] variants seen in crash logs
        // Function 70251 (Moon::UpdateImpl) has multiple vtable call sites
        s_knownSites.push_back({ 0x0D1BF70, 6, kRAX, "Moon vtable call (func entry)", true });
        s_knownSites.push_back({ 0x0D1BF8C, 6, kRAX, "Moon vtable call C", true });
        s_knownSites.push_back({ 0x0D1BF93, 6, kRAX, "Moon vtable call D", true });
        
        // Sky::Update crash sites (function 26240 at +040E0A2, called from Moon)
        // and dword ptr [rbx+0x1DC], 0xFFFFFFBF - may crash if rbx is null
        s_knownSites.push_back({ 0x040E0A2, 7, kRBX, "Sky::Update null rbx during save load", true });
        
        // Sky helper functions that may crash during async load
        // Note: These are covered by L1b pattern matching for null-dereference recovery
        s_knownSites.push_back({ 0x040AF23, 3, kRDI, "Sky helper mov rcx,rdi null", true });
        
        // Moon/Sky movss xmm13,[rax+0x18] at +0406FBE during async save load
        // Hard fail-safe handles this directly, but add known site for cascade tolerance
        s_knownSites.push_back({ 0x0406FBE, 6, kRAX, "Sky movss xmm13,[rax+0x18] null rax during load", true });
        
        // ── SKSE Plugin Init String Construction Crash ──
        // SkyrimSE.exe+0XCEDE89 - WRITE AV during string construction ("HUDCamData")
        // This happens during SKSE plugin initialization before game fully loads.
        // CRITICAL: Write-skip corrupts the string object, causing cascade crash
        // in VCRUNTIME140.dll+0x1208c. Use BAILOUT to return from function instead.
        // Instruction: mov [rax], r8 where RAX=invalid pointer (corrupted string state)
        s_knownSites.push_back({ 0x0CEDE89, 3, kRAX, "SKSE plugin init string construction crash (HUDCamData)", true });
        
        // ── SKSE Plugin Init InstalledContent Crash ──
        // SkyrimSE.exe+05FA08F - READ AV during plugin initialization
        // Instruction: cmp qword ptr [rsi], 0x00 where RSI=0x9 (invalid pointer)
        // This happens during SKSE plugin init ("init complete" message).
        // Use BAILOUT to return from function - skipping would leave corrupted state.
        s_knownSites.push_back({ 0x05FA08F, 4, kRSI, "SKSE plugin init InstalledContent null check", true });
        
        if (log) log->info("[VEH] Registered {} SE/AE-specific crash sites (including Moon/Sky rendering)", s_knownSites.size());
    }

    // ── Populate known crash sites for game executable ──
    // NiParticleSystem vtable corruption: call [rax+0x28] at SkyrimSE.exe+0D032C6
    // RAX contains float data (corrupted vtable), involved object: NiPSysData/NiParticleSystem
    // Recovery: zero RAX (null vtable call → skip), advance RIP past the call
    // The caller at +0C56D8F checks the next node and continues the loop
    // NOTE: instrLen = 3 (FF 50 28), not 2
    s_knownSites.push_back({ 0x0D032C6, 3, kRAX, "NiParticleSystem vtable corruption (call [rax+0x28])", false });
    
    // ShadowSceneNode related crash (BSParticleSystemManager update loop)
    // Common in particle-heavy scenes with ShadowSceneNode parent chain
    // call [rax+0x28] variant in particle controller update
    s_knownSites.push_back({ 0x0C56D8F, 4, kNONE, "BSParticleSystemManager controller update null", false });

    // ── Populate known crash sites for mod DLLs ──
    // skee64.dll (RaceMenu): mov rdx, [rcx+0x20] with RCX=0
    // Null pointer in overlay/body morph code, called from OBody
    // Recovery: zero RDX (the read result), skip instruction
    s_modKnownSites.push_back({ "skee64.dll", 0x00367A3, 4, kRDX, "RaceMenu overlay null (mov rdx,[rcx+0x20])", false });
    
    // skee64.dll: and rax, [rdi+0x20] with RDI=0  
    // Null pointer in NiTransformInterface during cosave loading
    // Recovery: zero RAX, skip instruction  
    s_modKnownSites.push_back({ "skee64.dll", 0x0075292, 3, kRAX, "RaceMenu NiTransformInterface null (and rax,[rdi+0x20])", false });
    
    // skee64.dll: additional crash sites seen in cosave/morph chains
    s_modKnownSites.push_back({ "skee64.dll", 0x0007D3E, 4, kRAX, "RaceMenu morph handler null", false });
    s_modKnownSites.push_back({ "skee64.dll", 0x00083E5, 4, kRAX, "RaceMenu morph apply null", false });
    s_modKnownSites.push_back({ "skee64.dll", 0x00084F0, 4, kRAX, "RaceMenu morph callback null", false });
    s_modKnownSites.push_back({ "skee64.dll", 0x0018385, 4, kRAX, "RaceMenu skin instance null", false });
    
    // OpenVR: Common crash sites when VR headset tracking fails
    // These occur in openvr_api.dll when SteamVR loses tracking
    if (REL::Module::IsVR()) {
        s_modKnownSites.push_back({ "openvr_api.dll", 0x0006E160, 3, kRAX, "OpenVR compositor GetPose null", false });
        s_modKnownSites.push_back({ "openvr_api.dll", 0x0006E163, 3, kRAX, "OpenVR compositor GetPose+3", false });
        s_modKnownSites.push_back({ "openvr_api.dll", 0x0006E166, 3, kRAX, "OpenVR compositor pose buffer", false });
        s_modKnownSites.push_back({ "openvr_api.dll", 0x0006E320, 3, kRAX, "OpenVR WaitGetPoses null", false });
        if (log) log->info("[VEH] Registered 4 OpenVR crash sites");
    }
    
    if (log) log->info("[VEH] Registered {} mod DLL known crash sites", s_modKnownSites.size());

    // ── Load additional crash sites from JSON files ──
    LoadCrashSitesFromJSON();
    if (log) {
        log->info("[VEH] Total crash sites: {} game, {} mod", 
                  s_knownSites.size(), s_modKnownSites.size());
    }

    if (!GameObjectIntrospection::GameObjectIntrospector::Initialize()) {
        if (log) log->warn("[VEH] GameObjectIntrospector initialization failed");
    }

    if (!PatternLearning::PatternLearningSystem::Initialize()) {
        if (log) log->warn("[VEH] PatternLearningSystem initialization failed");
    }

    // ── Initialize UserNotificationManager ──
    if (!UserNotifications::UserNotificationManager::Initialize()) {
        if (log) log->warn("[VEH] UserNotificationManager initialization failed");
    }

    // ── Safety buffer ──
    s_safetyBuf = VirtualAlloc(nullptr, SAFETY_SZ,
                               MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (s_safetyBuf) memset(s_safetyBuf, 0, SAFETY_SZ);

    // ── Zydis decoder ──
    s_decoderOK = ZYAN_SUCCESS(ZydisDecoderInit(&s_decoder,
        ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64));

    // ── Initialize DbgHelp for symbol resolution ──
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    if (!SymInitialize(process, nullptr, TRUE)) {
        if (log) log->warn("[VEH] SymInitialize failed: {}", GetLastError());
    }

    // ── Clear runtime tables ──
    for (size_t i = 0; i < MAX_LEARNED; ++i) {
        s_learned[i].rip.store(0, std::memory_order_relaxed);
        s_learned[i].instrLen = 0;
        s_learned[i].destCtx  = 0;
        s_learned[i].hits     = 0;
    }
    s_learnedCount.store(0, std::memory_order_relaxed);
    memset(s_track, 0, sizeof(s_track));
    
    // Reset global cascade protection state
    memset(s_funcTrack, 0, sizeof(s_funcTrack));
    s_globalCrashCount.store(0, std::memory_order_relaxed);
    s_globalCrashWindowStart = 0;
    s_globalCascadeTripped.store(false, std::memory_order_relaxed);
    s_lastRecoveryTick = 0;

    // ── Install handler with highest priority ──
    s_handler = AddVectoredExceptionHandler(1, ExceptionFilter);

    if (log) {
        log->info("[VEH] 6-Layer Recovery System initialized");
        log->info("[VEH] Game base: {:#x}, Known sites: {} (game) + {} (mods), Zydis: {}, Safety buffer: {} KB",
                  s_gameBase, s_knownSites.size(), s_modKnownSites.size(), 
                  s_decoderOK ? "ready" : "FAILED", 
                  s_safetyBuf ? SAFETY_SZ / 1024 : 0);
        log->info("[VEH] Module recovery scope: game exe + mod DLLs (system DLLs excluded)");
        log->info("[VEH] L1b pattern matching: version-independent SIMD/GP null-deref coverage (SE/AE/VR)");
        log->info("[VEH] Global cascade protection: max {} crashes in {}ms, {}ms cooldown, 256-byte function grouping",
                  GLOBAL_CASCADE_MAX, GLOBAL_CASCADE_WINDOW, RECOVERY_COOLDOWN_MS);
        log->info("[VEH] Recovery reports written to SKSE log directory (CrashGuard-recovery-*.log)");
    }

    // ── CrashLogger cooperation advisory ──
    if (CrashLoggerDetector::Detector::IsCrashLoggerPresent()) {
        if (log) {
            log->info("[VEH] CrashLogger detected — writing recovery reports to complement its crash logs");
            log->info("[VEH] CrashLogger crash logs = unrecovered crashes (game crashed)");
            log->info("[VEH] CrashGuard recovery logs = recovered crashes (game kept running)");
        }
    }

    // ── Ingest CrashLogger logs from previous sessions ──
    // Scan the SKSE log directory for CrashLogger crash logs and learn from them.
    // This lets CrashGuard build awareness of crash patterns the user has experienced,
    // even if CrashGuard wasn't installed (or failed to recover) when they happened.
    IngestCrashLoggerLogs();

    return s_handler != nullptr;
}

void VEHExceptionHandler::Shutdown() {
    if (s_handler) {
        RemoveVectoredExceptionHandler(s_handler);
        s_handler = nullptr;
    }
    
    LogStats();
    
    // Shutdown all components
    PatternLearning::PatternLearningSystem::Shutdown();
    UserNotifications::UserNotificationManager::Shutdown();
    
    if (s_safetyBuf) {
        VirtualFree(s_safetyBuf, 0, MEM_RELEASE);
        s_safetyBuf = nullptr;
    }

    // Cleanup DbgHelp
    HANDLE process = GetCurrentProcess();
    SymCleanup(process);
}

size_t VEHExceptionHandler::GetCrashCount() {
    return static_cast<size_t>(s_stats.total.load());
}

void VEHExceptionHandler::LogStats() {
    auto log = spdlog::default_logger();
    if (!log) return;
    log->info("[VEH] Recovery Stats - Total: {}, L1: {}, L2: {}, L3: {}, L4: {}, L5: {}, L6: {}, Failed: {}, Learned: {}/{}",
              s_stats.total.load(), s_stats.knownSite.load(), s_stats.learnedSite.load(),
              s_stats.regFixup.load(), s_stats.instrSkip.load(), s_stats.funcReturn.load(),
              s_stats.deepWalk.load(), s_stats.unrecoverable.load(), 
              s_learnedCount.load(), MAX_LEARNED);
}

void VEHExceptionHandler::PauseGameThreads() {
    // Suspend game threads only - filter out system threads to avoid deadlocks
    // System threads (thread pool, etc.) may hold locks needed for recovery
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;

    THREADENTRY32 entry;
    entry.dwSize = sizeof(entry);
    
    DWORD currentThreadId = GetCurrentThreadId();
    DWORD currentProcessId = GetCurrentProcessId();
    
    // Get handle to main module (SkyrimVR.exe or SkyrimSE.exe)
    HMODULE mainModule = GetModuleHandle(nullptr);
    MODULEINFO mainModuleInfo = {0};
    GetModuleInformation(GetCurrentProcess(), mainModule, &mainModuleInfo, sizeof(mainModuleInfo));
    uintptr_t mainBase = reinterpret_cast<uintptr_t>(mainModuleInfo.lpBaseOfDll);
    uintptr_t mainEnd = mainBase + mainModuleInfo.SizeOfImage;
    
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID == currentProcessId &&
                entry.th32ThreadID != currentThreadId) {
                
                // Open thread with query access to check start address
                HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_QUERY_LIMITED_INFORMATION, 
                                          FALSE, entry.th32ThreadID);
                if (!thread) continue;
                
                // Try to determine if this is a game thread vs system thread
                // by checking if thread start address is in game module
                bool isGameThread = true;
                
                NtQueryInformationThreadFn ntQueryInfo = reinterpret_cast<NtQueryInformationThreadFn>(
                    GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread"));
                
                if (ntQueryInfo) {
                    PVOID startAddress = nullptr;
                    ULONG returnLength = 0;
                    // kThreadQuerySetWin32StartAddress = 9
                    NTSTATUS status = ntQueryInfo(thread, kThreadQuerySetWin32StartAddress, 
                                                  &startAddress, sizeof(startAddress), &returnLength);
                    if (NT_SUCCESS(status) && startAddress) {
                        uintptr_t addr = reinterpret_cast<uintptr_t>(startAddress);
                        
                        // Check if start address is in a system module
                        HMODULE hMod = nullptr;
                        constexpr DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                              | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
                        if (GetModuleHandleExA(flags, reinterpret_cast<LPCSTR>(startAddress), &hMod) && hMod) {
                            char modName[64];
                            if (GetModuleFileNameA(hMod, modName, sizeof(modName))) {
                                std::string modPath(modName);
                                auto pos = modPath.find_last_of("\\/");
                                std::string baseName = (pos != std::string::npos) ? modPath.substr(pos + 1) : modPath;
                                
                                // Convert to lowercase for comparison
                                for (auto& c : baseName) c = static_cast<char>(::tolower(c));
                                
                                // Skip system threads
                                if (baseName.find("ntdll") != std::string::npos ||
                                    baseName.find("kernel32") != std::string::npos ||
                                    baseName.find("kernelbase") != std::string::npos ||
                                    baseName.find("msvcrt") != std::string::npos ||
                                    baseName.find("ucrtbase") != std::string::npos) {
                                    isGameThread = false;
                                }
                            }
                        }
                    }
                }
                
                if (isGameThread) {
                    SuspendThread(thread);
                    s_pausedThreads.push_back(thread);
                } else {
                    CloseHandle(thread);
                }
            }
        } while (Thread32Next(snapshot, &entry));
    }
    
    CloseHandle(snapshot);
}

void VEHExceptionHandler::ResumeGameThreads() {
    for (HANDLE thread : s_pausedThreads) {
        ResumeThread(thread);
        CloseHandle(thread);
    }
    s_pausedThreads.clear();
}

void VEHExceptionHandler::ExtractExceptionInfo(EXCEPTION_POINTERS* exceptionInfo,
                                               DWORD& outCode, void*& outAddress) {
    outCode = exceptionInfo->ExceptionRecord->ExceptionCode;
    
    // For access violations, get the faulting address
    if (outCode == EXCEPTION_ACCESS_VIOLATION && 
        exceptionInfo->ExceptionRecord->NumberParameters >= 2) {
        outAddress = reinterpret_cast<void*>(
            exceptionInfo->ExceptionRecord->ExceptionInformation[1]);
    } else {
        // For other exceptions, use the instruction pointer
        outAddress = reinterpret_cast<void*>(exceptionInfo->ContextRecord->Rip);
    }
}

void VEHExceptionHandler::CaptureCPURegisters(CONTEXT* source, CONTEXT& dest) {
    // Copy the entire CONTEXT structure
    memcpy(&dest, source, sizeof(CONTEXT));
}

bool VEHExceptionHandler::ResolveSymbol(void* address, StackFrame& frame) {
    HANDLE process = GetCurrentProcess();
    DWORD64 addr = reinterpret_cast<DWORD64>(address);
    
    frame.address = address;
    
    // Get module name
    HMODULE hModule = nullptr;
    constexpr DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                          | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (GetModuleHandleExA(flags, reinterpret_cast<LPCSTR>(address), &hModule) && hModule) {
        char modulePath[MAX_PATH];
        if (GetModuleFileNameA(hModule, modulePath, MAX_PATH)) {
            std::string path(modulePath);
            auto pos = path.find_last_of("\\/");
            frame.moduleName = (pos != std::string::npos) ? path.substr(pos + 1) : path;
        }
        frame.offset = addr - reinterpret_cast<DWORD64>(hModule);
    }
    
    // Get symbol name
    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;
    
    DWORD64 displacement = 0;
    if (SymFromAddr(process, addr, &displacement, symbol)) {
        frame.functionName = symbol->Name;
    } else {
        frame.functionName = "unknown";
    }
    
    // Get source file and line number
    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    DWORD lineDisplacement = 0;
    if (SymGetLineFromAddr64(process, addr, &lineDisplacement, &line)) {
        frame.sourceFile = line.FileName;
        frame.lineNumber = line.LineNumber;
    } else {
        frame.sourceFile = "";
        frame.lineNumber = 0;
    }
    
    return !frame.functionName.empty();
}

std::vector<StackFrame> VEHExceptionHandler::BuildCallStack(CONTEXT* context) {
    std::vector<StackFrame> frames;
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    
    // Initialize stack frame for StackWalk64
    STACKFRAME64 stackFrame;
    memset(&stackFrame, 0, sizeof(STACKFRAME64));
    
    DWORD machineType;
#ifdef _M_X64
    machineType = IMAGE_FILE_MACHINE_AMD64;
    stackFrame.AddrPC.Offset = context->Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context->Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context->Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#else
    machineType = IMAGE_FILE_MACHINE_I386;
    stackFrame.AddrPC.Offset = context->Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context->Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context->Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#endif
    
    // Walk the stack
    constexpr int MAX_FRAMES = 128;
    for (int i = 0; i < MAX_FRAMES; ++i) {
        if (!StackWalk64(machineType, process, thread, &stackFrame,
                        context, nullptr, SymFunctionTableAccess64,
                        SymGetModuleBase64, nullptr)) {
            break;
        }
        
        if (stackFrame.AddrPC.Offset == 0) {
            break;
        }
        
        StackFrame frame;
        void* addr = reinterpret_cast<void*>(stackFrame.AddrPC.Offset);
        ResolveSymbol(addr, frame);
        frames.push_back(frame);
    }
    
    return frames;
}

SeverityLevel VEHExceptionHandler::ClassifySeverity(DWORD exceptionCode, void* crashAddress) {
    // ═══════════════════════════════════════════════════════════════════════
    // Severity Classification Logic
    // ═══════════════════════════════════════════════════════════════════════
    // 
    // Safe:     Visual glitches only (rendering, UI, cosmetic issues)
    //           - Breakpoints, single-step (debugging)
    //           - Rendering-related crashes in known safe areas
    //
    // Warning:  Missing resources, null pointers (recoverable, no data loss)
    //           - Null pointer dereferences (< 0x10000)
    //           - Divide by zero (arithmetic errors)
    //           - Illegal instructions (code corruption, but recoverable)
    //           - Missing mesh/texture/animation files
    //
    // Critical: Save data or persistent state affected
    //           - Access violations in save/load code
    //           - Access violations in quest/inventory systems
    //           - Crashes that could corrupt game state
    //
    // Fatal:    Stack corruption, unrecoverable
    //           - Stack overflow (cannot safely recover)
    //           - Heap corruption
    //           - Multiple cascading failures
    //
    // Unknown:  Cannot determine severity
    //           - Unrecognized exception codes
    //           - Insufficient context to classify
    // ═══════════════════════════════════════════════════════════════════════
    
    // ── Fatal: Stack overflow is always unrecoverable ──
    if (exceptionCode == EXCEPTION_STACK_OVERFLOW) {
        return SeverityLevel::Fatal;
    }
    
    // ── Safe: Debugging exceptions ──
    if (exceptionCode == EXCEPTION_BREAKPOINT || 
        exceptionCode == EXCEPTION_SINGLE_STEP) {
        return SeverityLevel::Safe;
    }
    
    // ── Warning: Arithmetic and instruction errors ──
    if (exceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        exceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION ||
        exceptionCode == EXCEPTION_FLT_DIVIDE_BY_ZERO ||
        exceptionCode == EXCEPTION_FLT_OVERFLOW ||
        exceptionCode == EXCEPTION_FLT_UNDERFLOW) {
        return SeverityLevel::Warning;
    }
    
    // ── Access Violation: Requires detailed analysis ──
    if (exceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(crashAddress);
        
        // Null or near-null pointer dereference → Warning
        // These are common and usually recoverable (missing resource, uninitialized pointer)
        if (addr < 0x10000) {
            return SeverityLevel::Warning;
        }
        
        // Very high addresses (kernel space) → Fatal
        // Indicates severe memory corruption or invalid pointer arithmetic
        if (addr >= 0x7FFFFFFFFFFF) {
            return SeverityLevel::Fatal;
        }
        
        // Check if crash address is in stack region
        // Stack corruption is typically Fatal
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(crashAddress, &mbi, sizeof(mbi))) {
            // If the crash is in a guard page, it's likely stack overflow → Fatal
            if (mbi.Protect & PAGE_GUARD) {
                return SeverityLevel::Fatal;
            }
            
            // If the crash is in an uncommitted region → Critical
            // Could indicate heap corruption or use-after-free
            if (mbi.State == MEM_FREE || mbi.State == MEM_RESERVE) {
                return SeverityLevel::Critical;
            }
        }
        
        // Default for access violations: Critical
        // Could affect game state, but potentially recoverable
        return SeverityLevel::Critical;
    }
    
    // ── Array bounds and invalid parameters → Warning ──
    if (exceptionCode == EXCEPTION_ARRAY_BOUNDS_EXCEEDED ||
        exceptionCode == EXCEPTION_INVALID_HANDLE ||
        exceptionCode == EXCEPTION_INVALID_DISPOSITION) {
        return SeverityLevel::Warning;
    }
    
    // ── Heap corruption → Fatal ──
    if (exceptionCode == STATUS_HEAP_CORRUPTION) {
        return SeverityLevel::Fatal;
    }
    
    // ── Unknown exception codes ──
    return SeverityLevel::Unknown;
}

CrashContext VEHExceptionHandler::AnalyzeException(EXCEPTION_POINTERS* exceptionInfo) {
    CrashContext context;
    
    // Extract exception code and crash address
    ExtractExceptionInfo(exceptionInfo, context.exceptionCode, context.crashAddress);
    
    // Capture CPU registers
    CaptureCPURegisters(exceptionInfo->ContextRecord, context.cpuContext);
    
    // Build call stack with symbol resolution
    context.callStack = BuildCallStack(exceptionInfo->ContextRecord);
    
    // Classify severity
    context.severity = ClassifySeverity(context.exceptionCode, context.crashAddress);
    
    // Capture timestamp
    auto now = std::chrono::system_clock::now();
    context.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    // Initialize other fields
    context.involvedObject = nullptr;
    context.rootCause = "Unknown";

    // NOTE: We intentionally do NOT call ScanRegisters/ScanStack here.
    // These functions probe arbitrary register values and stack memory as
    // potential TESForm pointers, calling virtual functions (GetFormType,
    // GetFormID, GetFormEditorID) on each candidate.  If any candidate is
    // a stale/corrupted pointer, the virtual call triggers another access
    // violation INSIDE the VEH handler, leading to recursive VEH entries,
    // cascade crashes, and the infinite recovery loops that caused the
    // game to hang.  The SEH wrappers we added help but cannot fully
    // prevent this — the sheer volume of pointer probing makes it
    // statistically likely to hit an unmapped vtable during a crash.
    //
    // Object identification can be done AFTER recovery succeeds, in a
    // deferred/safe context, not during the exception handler.

    // Use RootCauseAnalyzer for comprehensive crash analysis
    // This provides category classification, confidence scoring, and suggested fixes
    auto rootCauseResult = RootCauseAnalysis::RootCauseAnalyzer::AnalyzeCrash(context);
    
    // Update context with enhanced root cause information
    if (!rootCauseResult.description.empty()) {
        context.rootCause = rootCauseResult.description;
    }
    
    return context;
}

bool VEHExceptionHandler::IsRecoverable(const CrashContext& context) {
    // Stack overflow is generally not recoverable
    if (context.exceptionCode == EXCEPTION_STACK_OVERFLOW) {
        return false;
    }
    
    // If we have a valid call stack, there's a good chance we can recover
    if (!context.callStack.empty()) {
        return true;
    }
    
    // Null or near-null instruction pointer is hard to recover from
    uintptr_t rip = context.cpuContext.Rip;
    if (rip < 0x10000) {
        // But we can try if we have a valid stack
        return IsReadable(reinterpret_cast<void*>(context.cpuContext.Rsp), 8);
    }
    
    // If the crash is in game code or a mod DLL, we should try to recover
    return IsRecoverableAddr(rip);
}

// Forward declaration of legacy Handler function
static LONG CALLBACK Handler(PEXCEPTION_POINTERS info);

// Forward declaration of orchestrated recovery function
static LONG CALLBACK OrchestratedRecovery(PEXCEPTION_POINTERS info);

// Forward declarations for post-recovery worker thread helpers
static void PostRecoveryAnalysis_SEH(DWORD exceptionCode, uintptr_t crashRip,
                                     uintptr_t crashAddr, const CONTEXT* savedContext,
                                     uint32_t hitCount);
static void PostRecoveryAnalysis_Inner(DWORD exceptionCode, uintptr_t crashRip,
                                       uintptr_t crashAddr, const CONTEXT* savedContext,
                                       uint32_t hitCount);

LONG WINAPI VEHExceptionHandler::ExceptionFilter(EXCEPTION_POINTERS* info) {
    // Call the orchestrated recovery function
    return OrchestratedRecovery(info);
}

// ═══════════════════════════════════════════════════════════════════════
// § 6.4  Post-Recovery Worker (SEH-safe helper)
// ═══════════════════════════════════════════════════════════════════════
// This function runs on a detached worker thread AFTER a successful recovery.
// It MUST be a standalone function (not a lambda) because it uses __try/__except
// and MSVC C2712 forbids __try in functions that use C++ object unwinding.
//
// It performs:
//   - Root cause analysis (category, confidence)
//   - Object identification (ScanRegisters / ScanStack)
//   - Pattern learning (RecordSuccess)
//   - Logging a one-line summary
//
// All of this is safe because we're NOT inside the VEH handler:
//   - Mutexes can be acquired normally (no deadlock risk)
//   - A secondary AV is caught by SEH (no recursive VEH entry)
//   - The game thread is running normally (no stale CONTEXT)
static void PostRecoveryAnalysis_SEH(DWORD exceptionCode,
                                     uintptr_t crashRip,
                                     uintptr_t crashAddr,
                                     const CONTEXT* savedContext,
                                     uint32_t hitCount)
{
    __try {
        PostRecoveryAnalysis_Inner(exceptionCode, crashRip, crashAddr, savedContext, hitCount);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        // Silently ignore — game is already running fine after recovery.
        // Cannot log here: spdlog returns shared_ptr which triggers C2712.
    }
}

// ═══════════════════════════════════════════════════════════════════════
// § 6.5  Recovery Report Writer
// ═══════════════════════════════════════════════════════════════════════
// Writes a detailed recovery report to the SKSE log directory for every
// recovered crash.  This serves as the companion to CrashLogger's crash
// logs: CrashLogger only fires for unrecovered crashes, while these
// reports document everything CrashGuard handled silently.
//
// Users should cross-reference both:
//   - CrashGuard recovery reports: crashes that were caught and fixed
//   - CrashLogger crash logs:      crashes that brought the game down
//
// Format is human-readable text with timestamps, module info, register
// state, root cause analysis, and involved objects.
// Note: Inner function does the actual work (C++ objects allowed).
// Wrapper function provides SEH protection.
static void WriteRecoveryReport_Inner(DWORD exceptionCode,
                                      uintptr_t crashRip,
                                      uintptr_t crashAddr,
                                      const CONTEXT* savedContext,
                                      const std::string& rootCause,
                                      const std::vector<GameObjectIntrospection::GameObjectInfo>& objects)
{
    // Build path to SKSE log directory
    char docPath[MAX_PATH];
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, 0, docPath)))
        return;

    std::string logDir = std::string(docPath) + "\\My Games\\Skyrim Special Edition\\SKSE\\";
    if (REL::Module::IsVR()) {
        logDir = std::string(docPath) + "\\My Games\\Skyrim VR\\SKSE\\";
    }

    // Create timestamped filename matching CrashLogger's naming convention
    SYSTEMTIME st;
    GetLocalTime(&st);
    char filename[256];
    snprintf(filename, sizeof(filename),
             "CrashGuard-recovery-%04d-%02d-%02d-%02d-%02d-%02d.log",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    std::string fullPath = logDir + filename;

    // Write the report (append if file exists — multiple recoveries in same second)
    std::ofstream out(fullPath, std::ios::app);
    if (!out.is_open()) return;

    out << "════════════════════════════════════════════════════════════\n";
    out << "  Skyrim CrashGuard — Recovered Crash Report\n";
    out << "════════════════════════════════════════════════════════════\n";
    out << "NOTE: This crash was RECOVERED. The game continued running.\n";
    out << "If CrashLogger also wrote a log around this time, its analysis\n";
    out << "covers a DIFFERENT crash that CrashGuard could NOT recover.\n";
    out << "────────────────────────────────────────────────────────────\n\n";

    // Timestamp
    char timeBuf[64];
    snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    out << "Timestamp: " << timeBuf << "\n";

    // Exception info
    const char* exType = "Unknown";
    if (exceptionCode == EXCEPTION_ACCESS_VIOLATION) exType = "Access Violation";
    else if (exceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO) exType = "Integer Divide by Zero";
    else if (exceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) exType = "Illegal Instruction";
    out << "Exception: " << exType << " (0x" << std::hex << exceptionCode << ")\n";

    // Crash location
    out << "Crash RIP: 0x" << std::hex << crashRip
        << " (" << ModName(crashRip) << "+0x" << ModOff(crashRip) << ")\n";
    if (crashAddr != 0) {
        out << "Access Addr: 0x" << std::hex << crashAddr << "\n";
    }

    // Game phase
    auto phase = PhaseTracking::PhaseTracker::GetCurrentPhase();
    const char* phaseStr = "Unknown";
    if (phase == PhaseTracking::GamePhase::LoadingSave) phaseStr = "Loading Save";
    else if (phase == PhaseTracking::GamePhase::Gameplay) phaseStr = "Gameplay";
    else if (phase == PhaseTracking::GamePhase::Exiting) phaseStr = "Exiting";
    out << "Game Phase: " << phaseStr << "\n\n";

    // Register state
    out << "Register State (at time of crash):\n";
    out << std::hex;
    out << "  RAX=0x" << savedContext->Rax << "  RCX=0x" << savedContext->Rcx << "\n";
    out << "  RDX=0x" << savedContext->Rdx << "  RBX=0x" << savedContext->Rbx << "\n";
    out << "  RSP=0x" << savedContext->Rsp << "  RBP=0x" << savedContext->Rbp << "\n";
    out << "  RSI=0x" << savedContext->Rsi << "  RDI=0x" << savedContext->Rdi << "\n";
    out << "  R8=0x"  << savedContext->R8  << "  R9=0x"  << savedContext->R9  << "\n";
    out << "  R10=0x" << savedContext->R10 << "  R11=0x" << savedContext->R11 << "\n";
    out << "  R12=0x" << savedContext->R12 << "  R13=0x" << savedContext->R13 << "\n";
    out << "  R14=0x" << savedContext->R14 << "  R15=0x" << savedContext->R15 << "\n";
    out << std::dec << "\n";

    // Root cause
    if (!rootCause.empty() && rootCause != "Unknown") {
        out << "Root Cause: " << rootCause << "\n";
    }

    // Involved objects
    if (!objects.empty()) {
        out << "Involved Objects:\n";
        for (const auto& obj : objects) {
            out << "  - Type: " << obj.type
                << ", FormID: 0x" << std::hex << obj.formID << std::dec;
            if (!obj.editorID.empty()) {
                out << ", Name: " << obj.editorID;
            }
            out << "\n";
        }
    }

    out << "\nRecovery: SUCCESS — game execution resumed.\n";
    out << "════════════════════════════════════════════════════════════\n\n";

    out.close();

    auto slog = spdlog::default_logger();
    if (slog) {
        slog->trace("[VEH] Recovery report written to: {}", fullPath);
    }
}

// The actual analysis body (may use C++ objects freely — called from within
// the __try above, but this function itself doesn't have __try).
static void PostRecoveryAnalysis_Inner(DWORD exceptionCode,
                                       uintptr_t crashRip,
                                       uintptr_t crashAddr,
                                       const CONTEXT* savedContext,
                                       uint32_t hitCount)
{
    auto log = spdlog::default_logger();
    
    bool doFullAnalysis = (hitCount <= 5 || hitCount == 50 || 
                           hitCount == 100 || hitCount % 500 == 0);
    if (!doFullAnalysis) {
        return;  // Skip heavy analysis for intermediate hits
    }

    // ── Build a CrashContext for the analysis subsystems ──
    VEH::CrashContext context;
    context.exceptionCode = exceptionCode;
    context.crashAddress  = reinterpret_cast<void*>(crashAddr);
    context.severity      = VEH::SeverityLevel::Warning;  // recovered = non-fatal
    auto now = std::chrono::system_clock::now();
    context.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    context.involvedObject = nullptr;
    context.rootCause = "Unknown";

    // Capture registers from snapshot (direct copy since cpuContext IS a CONTEXT)
    context.cpuContext = *savedContext;

    // ── Root cause analysis (safe: runs on worker thread) ──
    auto rootCauseResult = RootCauseAnalysis::RootCauseAnalyzer::AnalyzeCrash(context);
    if (!rootCauseResult.description.empty()) {
        context.rootCause = rootCauseResult.description;
    }

    // ── Identify involved objects (re-enabled: safe on worker thread) ──
    // ScanRegisters/ScanStack use SEH wrappers.  On the worker thread,
    // a secondary AV is caught by SEH — it will NOT trigger recursive
    // VEH handler entry because we're no longer inside the VEH handler.
    auto objects = RootCauseAnalysis::RootCauseAnalyzer::IdentifyInvolvedObjects(context);

    // ── Write recovery report file (CrashLogger-style, for user cross-reference) ──
    // Only write for first few hits to avoid spamming the log directory.
    if (hitCount <= 3) {
        try {
            WriteRecoveryReport_Inner(exceptionCode, crashRip, crashAddr, savedContext,
                                      rootCauseResult.description, objects);
        } catch (...) {
            // Silently ignore — writing a report is best-effort
        }
    }

    // ── Log summary ──
    if (log) {
        log->info("[VEH-Worker] Post-recovery analysis for {}+{:#x}: "
                  "category={}, confidence={:.0f}%, objects={}",
                  ModName(crashRip), ModOff(crashRip),
                  rootCauseResult.description,
                  rootCauseResult.confidence * 100.0,
                  objects.size());

        for (const auto& obj : objects) {
            log->info("[VEH-Worker]   Object: type={}, formID={:#x}, name={}",
                      obj.type, obj.formID,
                      obj.editorID.empty() ? "(none)" : obj.editorID);
        }
    }

    // ── Pattern learning: record successful recovery ──
    DynamicFix::RecoveryStrategy strategy = DynamicFix::RecoveryStrategy::InstructionPatch;
    if (exceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        strategy = DynamicFix::RecoveryStrategy::NullPointerFix;
    }
    PatternLearning::PatternLearningSystem::RecordSuccess(context, strategy);
}

// ═══════════════════════════════════════════════════════════════════════
// § 6.5  Orchestrated Recovery Flow
// ═══════════════════════════════════════════════════════════════════════
// This function implements the complete 6-layer recovery orchestration:
// 1. Pause game threads
// 2. Capture crash context
// 3. Identify involved objects
// 4. Analyze root cause
// 5. Classify severity
// 6. Capture state snapshot (if Critical/Fatal)
// 7. Query learning system for strategy
// 8. Apply dynamic fix
// 9. Validate state after fix
// 10. Show user notification (if needed)
// 11. Log crash and recovery
// 12. Record pattern
// 13. Resume execution or escalate

static LONG CALLBACK OrchestratedRecovery(PEXCEPTION_POINTERS info) {
    auto profileStart = std::chrono::high_resolution_clock::now();
    static std::atomic<uint64_t> s_totalRecoveryTimeUs{0};
    static std::atomic<uint32_t> s_recoveryCount{0};
    
    auto log = spdlog::default_logger();
    
    // ── Fast reject: If game is exiting normally, don't handle ANY exceptions ──
    bool isNormalExit = (PhaseTracking::PhaseTracker::GetCurrentPhase() == PhaseTracking::GamePhase::Exiting);
    if (isNormalExit) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // ── Fast reject: only handle hardware faults ──
    DWORD code = info->ExceptionRecord->ExceptionCode;
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        break;  // handle these
    case EXCEPTION_STACK_OVERFLOW:
        s_stats.unrecoverable++;
        return EXCEPTION_CONTINUE_SEARCH;
    default:
        return EXCEPTION_CONTINUE_SEARCH;  // not ours
    }

    // ── Reentrancy guard ──
    if (t_depth >= MAX_DEPTH)
        return EXCEPTION_CONTINUE_SEARCH;
    ++t_depth;
    struct Guard { ~Guard() { --t_depth; } } guard;

    uintptr_t rip = info->ContextRecord->Rip;

    // ═══════════════════════════════════════════════════════════════════════
    // § UNIVERSAL INTELLIGENT RECOVERY
    // ═══════════════════════════════════════════════════════════════════════
    // Use Zydis to decode the faulting instruction and apply intelligent
    // recovery without corrupting game state.
    // ═══════════════════════════════════════════════════════════════════════
    
    if (code == EXCEPTION_ACCESS_VIOLATION &&
        info->ExceptionRecord->NumberParameters >= 2 &&
        !IsSelfAddr(rip)) {
        
        auto* ctx = info->ContextRecord;
        ULONG_PTR accessType = info->ExceptionRecord->ExceptionInformation[0];
        uintptr_t accessAddr = info->ExceptionRecord->ExceptionInformation[1];
        bool isRead = (accessType == 0);
        bool isWrite = (accessType == 1);
        // accessType == 8 is execute-AV, handled separately below
        
        // Only handle read/write AVs with intelligent recovery
        if (isRead || isWrite) {
            bool recovered = false;
            const char* recoveryMethod = nullptr;
            
            // ══════════════════════════════════════════════════════════════
            // INTELLIGENT RECOVERY: Use Zydis to understand the crash
            // ══════════════════════════════════════════════════════════════
            // ══════════════════════════════════════════════════════════════
            if (!recovered) {
                ZydisDecodedInstruction instr;
                ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
                if (IsReadable(reinterpret_cast<void*>(rip), 15) &&
                    ZYAN_SUCCESS(ZydisDecoderDecodeFull(&s_decoder, reinterpret_cast<void*>(rip), 15, &instr, operands))) {
                    
                    if (isRead) {
                        // ── READ AV: Find destination register and zero it ──
                        ZydisRegister destReg = ZYDIS_REGISTER_NONE;
                        for (int i = 0; i < instr.operand_count; ++i) {
                            if (operands[i].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                                (operands[i].actions & ZYDIS_OPERAND_ACTION_WRITE)) {
                                destReg = operands[i].reg.value;
                                break;
                            }
                        }
                        
                        // Handle XMM destination (movss, movaps, etc.)
                        if (IsXMMRegister(destReg)) {
                            ZeroXMMRegister(ctx, destReg);
                            ctx->Rip += instr.length;
                            recovered = true;
                            recoveryMethod = "zeroed XMM";
                            if (log) {
                                log->warn("[VEH] Universal Recovery: {}+{:#X} READ AV -> zeroed XMM{}, skip {} bytes",
                                          ModName(rip), ModOff(rip), destReg - ZYDIS_REGISTER_XMM0, instr.length);
                            }
                        }
                        // Handle GP register destination (mov, movzx, etc.)
                        else if (destReg != ZYDIS_REGISTER_NONE) {
                            int ctxIdx = RegToCtx(destReg);
                            if (ctxIdx != kNONE) {
                                *reinterpret_cast<uintptr_t*>(reinterpret_cast<uint8_t*>(ctx) + ctxIdx) = 0;
                                ctx->Rip += instr.length;
                                recovered = true;
                                recoveryMethod = "zeroed register";
                                if (log) {
                                    log->warn("[VEH] Universal Recovery: {}+{:#X} READ AV -> zeroed reg, skip {} bytes",
                                              ModName(rip), ModOff(rip), instr.length);
                                }
                            }
                        }
                        // No destination register - try function return
                        if (!recovered) {
                            if (L5_FuncReturn(ctx)) {
                                recovered = true;
                                recoveryMethod = "function return";
                                if (log) {
                                    log->warn("[VEH] Universal Recovery: {}+{:#X} READ AV -> function return",
                                              ModName(rip), ModOff(rip));
                                }
                            }
                        }
                    }
                    else if (isWrite) {
                        // ── WRITE AV: Skip the write or return from function ──
                        // For system DLLs (VCRUNTIME, ucrtbase, etc.), skipping writes is dangerous
                        // because it leaves corrupted state. Use function return instead.
                        bool isSystemDLL = IsSystemDLL(rip);
                        if (isSystemDLL) {
                            // System DLL - use function return instead of skip
                            if (L5_FuncReturn(ctx)) {
                                recovered = true;
                                recoveryMethod = "function return (system DLL)";
                                if (log) {
                                    log->warn("[VEH] Universal Recovery: {}+{:#X} WRITE AV (system DLL) -> function return",
                                              ModName(rip), ModOff(rip));
                                }
                            }
                        } else {
                            // Game code - skip the write
                            ctx->Rip += instr.length;
                            recovered = true;
                            recoveryMethod = "skipped write";
                            if (log) {
                                log->warn("[VEH] Universal Recovery: {}+{:#X} WRITE AV -> skip {} bytes",
                                          ModName(rip), ModOff(rip), instr.length);
                            }
                        }
                    }
                }
            }
            
            if (recovered) {
                s_stats.instrPattern++;
                CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();
                // Mark write-skip recoveries so we can allow one immediate cascade
                bool wasWriteSkip = (recoveryMethod && std::string(recoveryMethod) == "skipped write");
                RecordRecovery(wasWriteSkip);
                
                // ── In-game notification (minimal spam) ──
                // Only notify on first issue and significant milestones
                static std::atomic<uint32_t> s_notifyCount{0};
                uint32_t count = s_notifyCount.fetch_add(1, std::memory_order_relaxed) + 1;
                if (count == 1) {
                    RE::DebugNotification("CrashGuard: Preventing issues during load...");
                } else if (count == 10 || count == 50 || count == 100) {
                    std::string msg = fmt::format("CrashGuard: {} issues prevented", count);
                    RE::DebugNotification(msg.c_str());
                }
                // Also record recovery in F11 history and stats so the UI reflects prevented crashes
                try {
                    using namespace CrashGuard;
                    // Treat automatic recoveries here as Warning (non-fatal)
                    VEH::SeverityLevel sev = VEH::SeverityLevel::Warning;

                    // Update aggregated recovery statistics shown in F11
                    RecoveryStatistics::GetInstance().RecordRecovery(sev, false);

                    // Add a history entry for the F11 Crash History tab
                    auto severityStr = std::string("Warning");
                    std::string rootCause = (recoveryMethod && recoveryMethod[0]) ? std::string(recoveryMethod) : std::string("Automatic recovery");
                    RecoveryNotifications::GetSingleton().AddRecovery(
                        severityStr,
                        rootCause,
                        std::string("AutomaticRecovery"),
                        {}, {}, true
                    );
                } catch (...) {}

                // Silent for all other recoveries
                
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
    }

    // ── Execute-AV early detection ──
    // For execute-AVs (accessType==8), RIP is a garbage address the CPU
    // tried to execute.  It's never in any valid module.  We check the
    // RETURN ADDRESS on the stack instead to decide recoverability.
    bool isExecuteAV = false;
    if (code == EXCEPTION_ACCESS_VIOLATION &&
        info->ExceptionRecord->NumberParameters >= 2 &&
        info->ExceptionRecord->ExceptionInformation[0] == 8) {
        isExecuteAV = true;
        if (IsReadable(reinterpret_cast<void*>(info->ContextRecord->Rsp), 8)) {
            uintptr_t retAddr = *reinterpret_cast<uint64_t*>(info->ContextRecord->Rsp);
            if (IsSelfAddr(retAddr) || IsSystemDLL(retAddr) || !IsRecoverableAddr(retAddr)) {
                return EXCEPTION_CONTINUE_SEARCH;
            }
            // Caller is in game/mod code — let it through to Handler
        } else {
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }

    // ── Module filter: recover crashes in game exe AND mod DLLs ──
    // Skip system DLLs (ntdll, kernel32, etc.) and CrashGuard's own module.
    // (Skipped for execute-AVs which already passed the return-address check above)
    if (!isExecuteAV) {
        if (IsSelfAddr(rip)) {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        if (IsSystemDLL(rip)) {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        if (!IsRecoverableAddr(rip)) {
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }

    // ── Cascade breaker ──
    // Use the global cascade protection system (defined in §3b) instead of
    // local statics. This catches cascades across OrchestratedRecovery AND
    // Handler paths, and uses function-block grouping to catch crashes
    // at different RIPs within the same function.
    //
    // NOTE: Handler() checks these same limits, but we check here too to
    // avoid entering the analysis code path (mutex acquisitions, etc.)
    // when we're already in a cascade situation.
    if (CheckGlobalCascadeTripped(rip)) {
        if (log) {
            log->critical("[VEH] Global cascade breaker active in OrchestratedRecovery");
            log->flush();
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    s_stats.total++;

    // ═══════════════════════════════════════════════════════════════════════
    // FAST PATH: Try the 6-layer recovery FIRST.
    //
    // The previous flow did Steps 2-7 (AnalyzeException, RootCause,
    // severity analysis, state snapshots, pattern learning DB queries)
    // BEFORE trying recovery.  All of those steps acquire mutexes
    // (s_databaseMutex, StateManager::mutex_, CoSaveManager::mutex_),
    // walk the stack (StackWalk64), and do file I/O.  When recovery
    // fails (which it does for most crashes), all that work was wasted
    // and the mutex operations caused "resource deadlock would occur"
    // C++ exceptions on recursive/concurrent VEH entries.
    //
    // New flow: Try Handler first (zero mutex operations, pure register
    // manipulation).  If it fails, return immediately and let CrashLogger
    // handle it.  Only do heavy analysis after successful recovery.
    // ═══════════════════════════════════════════════════════════════════════

    LONG legacyResult = Handler(info);

    if (legacyResult != EXCEPTION_CONTINUE_EXECUTION) {
        // Recovery failed — hand off immediately to CrashLogger / other handlers.
        // CrashLogger will write its own crash log with full analysis.
        // NOTE: CrashGuard may have attempted L1-L6 recovery (all failed).
        // The CONTEXT was NOT modified (each strategy restores on failure),
        // so CrashLogger's analysis will be accurate.
        //
        // Do NOT:
        //   - Acquire any mutex (deadlock risk)
        //   - Walk the stack with StackWalk64 (recursive AV risk)
        //   - Show any MessageBox (blocks game thread)
        //   - Record pattern (acquires s_databaseMutex)
        //   - Touch CoSaveManager (acquires mutex_)
        // Just log and get out.
        s_stats.unrecoverable++;
        {
            // Inner scope so ctx2 lifetime covers all logging below
            auto* ctx2 = info->ContextRecord;
            if (log) {
                log->critical("[VEH] UNRECOVERED CRASH at {}+{:#x} — handing off to CrashLogger",
                              ModName(rip), ModOff(rip));
                // Log full register state so SkyrimCrashGuard.log has the same context CrashLogger has
                log->critical("[VEH]   RIP={:#018x}  RSP={:#018x}", ctx2->Rip, ctx2->Rsp);
                log->critical("[VEH]   RAX={:#018x}  RCX={:#018x}", ctx2->Rax, ctx2->Rcx);
                log->critical("[VEH]   RDX={:#018x}  RBX={:#018x}", ctx2->Rdx, ctx2->Rbx);
                log->critical("[VEH]   RBP={:#018x}  RSI={:#018x}", ctx2->Rbp, ctx2->Rsi);
                log->critical("[VEH]   RDI={:#018x}  R8 ={:#018x}", ctx2->Rdi, ctx2->R8);
                log->critical("[VEH]   R9 ={:#018x}  R10={:#018x}", ctx2->R9,  ctx2->R10);
                log->critical("[VEH]   R11={:#018x}  R12={:#018x}", ctx2->R11, ctx2->R12);
                log->critical("[VEH]   R13={:#018x}  R14={:#018x}", ctx2->R13, ctx2->R14);
                log->critical("[VEH]   R15={:#018x}", ctx2->R15);
                // Read raw stack pointers via pure-C SEH helper (C2712: cannot mix __try + C++ dtors)
                uintptr_t rawStack[16] = {};
                int nFrames = ReadRawStackPointers_SEH(
                    reinterpret_cast<const uintptr_t*>(ctx2->Rsp), rawStack, 16);
                log->critical("[VEH]   Stack trace (raw return addresses):");
                for (int i = 0; i < nFrames; ++i)
                    if (rawStack[i] > 0x10000)
                        log->critical("[VEH]     [{:2}] {:#018x}  {}+{:#x}",
                                      i, rawStack[i], ModName(rawStack[i]), ModOff(rawStack[i]));
                log->critical("[VEH] CrashGuard could not recover — CrashLogger's log will contain full analysis");
                log->critical("[VEH] See SkyrimCrashGuard.log for what CrashGuard attempted");
                log->flush();
            }
        }
        // Synchronous injection into CrashLogger's log (executes immediately before returning)
        if (CrashLoggerDetector::Detector::IsCrashLoggerPresent()) {
            bool injected = InjectIntoCrashLoggerLog_Sync();
            
            // Fallback: If injection failed, create separate CrashGuard log file
            if (!injected) {
                try {
                    std::string logDir = GetSKSELogDir_Safe();
                    if (!logDir.empty()) {
                        // Generate timestamp for fallback filename
                        auto now = std::chrono::system_clock::now();
                        auto time_t_now = std::chrono::system_clock::to_time_t(now);
                        std::tm tm_now;
                        localtime_s(&tm_now, &time_t_now);
                        
                        char timestamp[64];
                        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d-%H-%M-%S", &tm_now);
                        
                        std::string fallbackPath = logDir + "CrashGuard-unrecovered-" + timestamp + ".log";
                        
                        // Write fallback log with crash details
                        std::ofstream fallback(fallbackPath);
                        if (fallback.is_open()) {
                            fallback << "================================================================================\n";
                            fallback << "SkyrimCrashGuard - Unrecovered Crash Report\n";
                            fallback << "================================================================================\n\n";
                            fallback << "CrashGuard attempted to recover this crash via its 7-layer VEH chain (L1-L6+L1b).\n";
                            fallback << "All recovery layers were exhausted.\n\n";
                            fallback << "WARNING: CrashGuard could not inject this notice into CrashLogger's log file.\n";
                            fallback << "This may indicate CrashLogger wrote its log to a different location or\n";
                            fallback << "the crash occurred before CrashLogger could write its log.\n\n";
                            fallback << "Crash Location: " << ModName(rip) << "+0x" << std::hex << ModOff(rip) << std::dec << "\n";
                            fallback << "RIP: 0x" << std::hex << rip << std::dec << "\n\n";
                            fallback << "See SkyrimCrashGuard.log (same SKSE folder) for full recovery attempt details.\n";
                            fallback << "Cross-reference with CrashLogger's crash log if available.\n";
                            fallback << "================================================================================\n";
                            fallback.close();
                            
                            if (log) log->warn("[VEH] Created fallback log: {}", fallbackPath);
                        }
                    }
                } catch (const std::exception& e) {
                    if (log) log->error("[VEH] Failed to create fallback log: {}", e.what());
                } catch (...) {
                    if (log) log->error("[VEH] Failed to create fallback log: unknown error");
                }
            }
        }
        CrashGuard::PerformanceMonitor::GetSingleton().IncrementFailedRecovery();
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // RECOVERY SUCCEEDED — queue deferred analysis, then return IMMEDIATELY.
    //
    // CRITICAL: We are inside a VEH handler.  The CONTEXT has been modified
    // by Handler (L1-L6) to resume at a safe point.  But that resume only
    // happens when we return EXCEPTION_CONTINUE_EXECUTION to the OS.
    //
    // Every millisecond we spend here is time where:
    //  - The thread is NOT executing the recovery (it's still in the VEH)
    //  - Other threads ARE running, mutating game state
    //  - The modified CONTEXT becomes increasingly stale
    //
    // Strategy: Copy the minimal crash info to the heap, fire a detached
    // worker thread that does analysis + pattern learning, and return
    // EXCEPTION_CONTINUE_EXECUTION immediately.  The worker thread runs
    // OUTSIDE the VEH handler context where mutexes/StackWalk64/SEH are safe.
    // ═══════════════════════════════════════════════════════════════════════

    CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();

    // Use the per-RIP hit counter to throttle logging.
    // TrackHit was already called in Handler() so this is just a read.
    static std::atomic<uint32_t> s_successCount{0};
    uint32_t sc = s_successCount.fetch_add(1, std::memory_order_relaxed) + 1;
    {
        bool logThis = !isExecuteAV || (sc <= 5 || sc % 50 == 0);
        if (log && logThis) {
            log->info("[VEH] Recovery #{} succeeded at {}+{:#x} — resuming immediately",
                      sc, ModName(rip), ModOff(rip));
        }
        // No in-game notification here - handled by universal recovery path above
    }

    // ── Queue deferred post-recovery analysis on a worker thread ──
    // Copy immutable crash info to the heap so the thread owns it.
    // This is safe: we only read from the copies, never touch the CONTEXT again.
    //
    // CRITICAL: Skip post-recovery analysis during save load!
    // The worker thread probes game memory (IdentifyInvolvedObjects, etc.)
    // which is dangerous during save load when game state is in flux.
    // This was causing secondary crashes during SkyrimSoulsRE async loads.
    bool skipAnalysis = (PhaseTracking::PhaseTracker::GetCurrentPhase() == PhaseTracking::GamePhase::LoadingSave);
    if (!skipAnalysis) {
        struct DeferredCrashInfo {
            DWORD   exceptionCode;
            uintptr_t crashRip;
            uintptr_t crashAddr;
            CONTEXT savedContext;   // snapshot of pre-recovery context
            uint32_t hitCount;      // hit count for batch report throttling
        };

        auto info_copy = std::make_shared<DeferredCrashInfo>();
        info_copy->exceptionCode = code;
        info_copy->crashRip      = rip;
        info_copy->crashAddr     = 0;
        info_copy->hitCount      = sc;  // pass hit count for batch report throttling
        if (code == EXCEPTION_ACCESS_VIOLATION &&
            info->ExceptionRecord->NumberParameters >= 2) {
            info_copy->crashAddr = info->ExceptionRecord->ExceptionInformation[1];
        }
        // Save the original (pre-recovery-modified) register state
        // Note: ctx has been modified by Handler, but the crash address
        // and exception info are still valid for analysis purposes.
        info_copy->savedContext = *info->ContextRecord;

        std::thread([info_copy]() {
            PostRecoveryAnalysis_SEH(info_copy->exceptionCode,
                                    info_copy->crashRip,
                                    info_copy->crashAddr,
                                    &info_copy->savedContext,
                                    info_copy->hitCount);
        }).detach();
    }

    auto profileEnd = std::chrono::high_resolution_clock::now();
    auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(profileEnd - profileStart).count();
    s_totalRecoveryTimeUs.fetch_add(elapsedUs, std::memory_order_relaxed);
    uint32_t count = s_recoveryCount.fetch_add(1, std::memory_order_relaxed) + 1;
    
    // Log warning if recovery took >5ms (target threshold)
    if (elapsedUs > 5000) {
        if (log) {
            log->warn("[VEH] Recovery took {}us (>5ms threshold), avg={}us over {} recoveries",
                      elapsedUs, s_totalRecoveryTimeUs.load() / std::max(count, 1u), count);
        }
    }

    // Record this recovery for cooldown tracking
    RecordRecovery();
    
    return EXCEPTION_CONTINUE_EXECUTION;
}

// ═══════════════════════════════════════════════════════════════════════
// § 7  Main Handler (Legacy)
// ═══════════════════════════════════════════════════════════════════════

static LONG CALLBACK Handler(PEXCEPTION_POINTERS info) {
    DWORD code = info->ExceptionRecord->ExceptionCode;
    auto* ctx = info->ContextRecord;
    uintptr_t rip = ctx->Rip;

    // ── Execute-AV early detection ──
    // For execute-AVs (accessType==8), RIP is the garbage address the CPU
    // tried to execute — it's never in any valid module by definition.
    // The module filter below would reject it.  Instead, check the RETURN
    // ADDRESS on the stack to see if the CALLER is in recoverable code.
    bool isExecuteAV = false;
    if (code == EXCEPTION_ACCESS_VIOLATION &&
        info->ExceptionRecord->NumberParameters >= 2 &&
        info->ExceptionRecord->ExceptionInformation[0] == 8) {
        isExecuteAV = true;
        // For execute-AVs, validate via the return address (the CALL site)
        if (IsReadable(reinterpret_cast<void*>(ctx->Rsp), 8)) {
            uintptr_t retAddr = *reinterpret_cast<uint64_t*>(ctx->Rsp);
            if (IsSelfAddr(retAddr) || IsSystemDLL(retAddr) || !IsRecoverableAddr(retAddr)) {
                // Caller is in system/self code — don't recover
                return EXCEPTION_CONTINUE_SEARCH;
            }
            // Caller is in game/mod code — skip the RIP-based filter, let it through
        } else {
            // Can't even read the stack — unrecoverable
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }

    // ── Module filter (same as OrchestratedRecovery) ──
    // Handler can be called recursively via AnalyzeException triggering
    // AVs during pointer probing.  These secondary AVs are often in
    // system DLLs and must be rejected immediately.
    // (Skipped for execute-AVs which already passed the return-address check above)
    // CRITICAL: Don't reject system DLLs here - let them reach L1b which will
    // decide whether to use function return (system DLL) or write-skip (game code).
    // The IsSystemDLL check in IsRecoverableAddr was preventing VCRUNTIME140.dll
    // crashes from being recovered at all.
    if (!isExecuteAV && (IsSelfAddr(rip) || !IsRecoverableAddr(rip))) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    auto log = spdlog::default_logger();
    uint32_t hits = TrackHit(rip);
    // Log first 5 execute-AVs, then every 50th; log all regular AVs up to limit
    bool shouldLog = log && (isExecuteAV ? (hits <= 5 || hits % 50 == 0) : (hits <= 20));

    // ═══════════════════════════════════════════════════════════════
    // GLOBAL CASCADE PROTECTION - Check BEFORE attempting any recovery
    // ═══════════════════════════════════════════════════════════════
    
    // 1. Global rate limiter - too many crashes too fast across ANY address
    if (CheckGlobalCascadeTripped(rip)) {
        if (shouldLog) {
            log->critical("[VEH] Global cascade breaker active - refusing recovery for {:#x}", rip);
            log->flush();
        }
        s_stats.unrecoverable++;
        return EXCEPTION_CONTINUE_SEARCH;
    }
    
    // 2. Function-level cascade - crashes within same 256-byte block
    // Use higher limit for known bailout sites (e.g., Moon/Sky rendering during save load)
    // These are expected to crash repeatedly until the async load completes.
    uint32_t funcHits = TrackFunctionHit(rip);
    bool isBailoutSite = IsKnownBailoutSite(rip);
    bool isMoonSkySite = IsInMoonOrSkyFunction(rip);
    bool highToleranceSite = (isBailoutSite || isMoonSkySite);
    // Note: duringLoad check at top of Handler() already returned CONTINUE_SEARCH
    uint32_t funcCascadeLimit = highToleranceSite ? BAILOUT_CASCADE_MAX : FUNC_CASCADE_MAX;
    
    if (!isExecuteAV && funcHits > funcCascadeLimit) {
        if (log) {
            log->critical("[VEH] FUNCTION CASCADE: {:#x} (func block {:#x}) crashed {} times (limit {}) — "
                          "same function keeps crashing, giving up{}",
                          rip, rip & FUNC_BLOCK_MASK, funcHits, funcCascadeLimit,
                          highToleranceSite ? " [high-tolerance Moon/Sky site]" : "");
            log->flush();
        }
        s_stats.unrecoverable++;
        return EXCEPTION_CONTINUE_SEARCH;
    }
    
    // 3. Recovery cooldown - too many recoveries too fast
    // Relax cooldown for Moon/Sky and known bailout sites (phase tracking can lag)
    if (!highToleranceSite && !CheckRecoveryCooldown()) {
        if (shouldLog) {
            log->warn("[VEH] Recovery cooldown active - refusing recovery for {:#x}", rip);
        }
        s_stats.unrecoverable++;
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // ── Per-RIP cascade breaker ──
    // For execute-AVs: use a HIGH limit (200).  The recovery is deterministic
    // and safe (pop return address, zero RAX).  The game often iterates many
    // actors with the same corrupted vtable during InitTESThread — each is a
    // different actor hitting the same bad vtable entry.  We need to let the
    // game finish iterating.
    //
    // For known bailout sites during save load: use HIGH limit (BAILOUT_CASCADE_MAX).
    // Moon/Sky rendering crashes repeatedly when SkyrimSoulsRE continues rendering
    // during async save load. Each frame may try to render the Moon with a null
    // vtable until loading completes.
    //
    // For regular AVs (read/write): keep strict limit (3).  L1-L6 recovery
    // is more speculative and repeated failures indicate real instability.
    uint32_t cascadeLimit = isExecuteAV ? 200 : (highToleranceSite ? BAILOUT_CASCADE_MAX : 3);
    if (hits > cascadeLimit) {
        if (log) {
            log->critical("[VEH] CASCADE: RIP {:#x} ({}+{:#x}) crashed {} times (limit {}) — "
                          "giving up",
                          rip, ModName(rip), ModOff(rip), hits, cascadeLimit);
            log->flush();
        }
        s_stats.unrecoverable++;
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // ═══════════════════════════════════════════════════════════════
    // ACCESS VIOLATION
    // ═══════════════════════════════════════════════════════════════
    if (code == EXCEPTION_ACCESS_VIOLATION) {
        ULONG_PTR accessType = info->ExceptionRecord->ExceptionInformation[0];
        uintptr_t accessAddr = info->ExceptionRecord->ExceptionInformation[1];
        const char* op = (accessType == 0) ? "READ"
                       : (accessType == 1) ? "WRITE" : "EXEC";

        if (shouldLog) {
            log->warn("[VEH] AV {} {:#x}  at {}+{:#x}  [hit #{}]",
                      op, accessAddr, ModName(rip), ModOff(rip), hits);
        }

        // ── Execute-AV fast path ──
        // When accessType==8, the CPU tried to EXECUTE code at a bad address.
        // RIP itself is the bad address — L1-L4 all fail because they try to
        // decode the instruction at RIP which is not readable memory.
        // Instead, unwind to the caller (return address is on the stack).
        if (accessType == 8) {
            if (shouldLog) log->info("[VEH]   Execute-AV detected: RIP {:#x} is not executable code", rip);
            if (ExecuteAV_Recovery(ctx, shouldLog)) {
                if (shouldLog) log->info("[VEH]   -> Execute-AV recovery succeeded");
                CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            // If Execute-AV recovery failed, fall through to L5/L6
            if (shouldLog) log->warn("[VEH]   Execute-AV recovery failed, trying L5/L6");
            if (L5_FuncReturn(ctx)) {
                if (shouldLog) log->info("[VEH]   -> L5 function return (execute-AV fallback)");
                CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (L6_DeepWalk(ctx)) {
                if (shouldLog) log->info("[VEH]   -> L6 deep stack walk (execute-AV fallback)");
                CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (log) {
                log->critical("[VEH] UNRECOVERABLE Execute-AV: bad RIP={:#x} RSP={:#x}",
                              rip, ctx->Rsp);
                log->flush();
            }
            return EXCEPTION_CONTINUE_SEARCH;
        }

        // ── L1: Known site ──
        if (L1_KnownSite(ctx, rip)) {
            if (shouldLog) {
                log->info("[VEH]   -> L1 known-site recovery");
            }
            
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();
            
            RecordRecovery();
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        // ── L1b: Instruction-pattern match (version-independent) ──
        if (L1b_InstructionPattern(info)) {
            // L1b already logs its own detail line
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();
            RecordRecovery();
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        // ── L2: Learned site ──
        if (L2_LearnedSite(ctx, rip)) {
            if (shouldLog) log->info("[VEH]   -> L2 learned-site recovery");
            
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();
            
            RecordRecovery();
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        // ── L3: Register fixup (works for read AND write AVs) ──
        if (L3_RegFixup(info)) {
            if (shouldLog) log->info("[VEH]   -> L3 register fixup ({})", op);
            
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();
            
            RecordRecovery();
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        // ── L4: Instruction skip ──
        if (L4_InstrSkip(ctx)) {
            if (shouldLog) log->info("[VEH]   -> L4 instruction skip");
            
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();
            
            RecordRecovery();
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        // ── L5: Function return ──
        if (L5_FuncReturn(ctx)) {
            if (shouldLog) log->info("[VEH]   -> L5 function return");
            
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();
            
            RecordRecovery();
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        // ── L6: Deep stack walk ──
        if (L6_DeepWalk(ctx)) {
            if (shouldLog) log->info("[VEH]   -> L6 deep stack walk");
            
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();
            
            RecordRecovery();
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        // Every strategy exhausted - log detailed failure info
        if (log) {
            log->critical("╔══════════════════════════════════════════════════════════════╗");
            log->critical("║  CRASHGUARD: UNRECOVERABLE CRASH - ALL LAYERS FAILED         ║");
            log->critical("╠══════════════════════════════════════════════════════════════╣");
            log->critical("║  Location: {}+{:#x}", ModName(rip), ModOff(rip));
            log->critical("║  RIP: {:#018x}  RSP: {:#018x}", rip, ctx->Rsp);
            log->critical("║  RAX: {:#018x}  RCX: {:#018x}", ctx->Rax, ctx->Rcx);
            log->critical("║  RDX: {:#018x}  RBX: {:#018x}", ctx->Rdx, ctx->Rbx);
            log->critical("║  RBP: {:#018x}  RSI: {:#018x}", ctx->Rbp, ctx->Rsi);
            log->critical("║  RDI: {:#018x}  R8:  {:#018x}", ctx->Rdi, ctx->R8);
            log->critical("║  R9:  {:#018x}  R10: {:#018x}", ctx->R9,  ctx->R10);
            log->critical("║  R11: {:#018x}  R12: {:#018x}", ctx->R11, ctx->R12);
            log->critical("║  R13: {:#018x}  R14: {:#018x}", ctx->R13, ctx->R14);
            log->critical("║  R15: {:#018x}", ctx->R15);
            log->critical("╠══════════════════════════════════════════════════════════════╣");
            log->critical("║  Recovery Layers Attempted:                                  ║");
            log->critical("║    L1  (Known Sites)      - FAILED (no matching site)       ║");
            log->critical("║    L1b (Instruction Pat)  - FAILED (pattern not recognized) ║");
            log->critical("║    L2  (Learned Sites)    - FAILED (no learned pattern)     ║");
            log->critical("║    L3  (Register Fixup)   - FAILED (could not fix regs)     ║");
            log->critical("║    L4  (Instruction Skip) - FAILED (decode failed)          ║");
            log->critical("║    L5  (Function Return)  - FAILED (bad stack/ret addr)     ║");
            log->critical("║    L6  (Deep Stack Walk)  - FAILED (no safe return found)   ║");
            log->critical("╠══════════════════════════════════════════════════════════════╣");
            log->critical("║  This crash will be passed to CrashLogger for analysis.     ║");
            log->critical("╚══════════════════════════════════════════════════════════════╝");
            log->flush();
        }
        
        GenerateMinidump(info);
        
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // ═══════════════════════════════════════════════════════════════
    // DIVIDE BY ZERO
    // ═══════════════════════════════════════════════════════════════
    if (code == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (shouldLog)
            log->warn("[VEH] DIV/0 at {}+{:#x} [hit #{}]",
                      ModName(rip), ModOff(rip), hits);

        // Zero RAX/RDX (the dividend/remainder pair) and skip
        ctx->Rax = 0;
        ctx->Rdx = 0;
        if (L4_InstrSkip(ctx)) {
            if (shouldLog) log->info("[VEH]   -> instruction skip");
            
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();
            
            RecordRecovery();
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (L5_FuncReturn(ctx)) {
            if (shouldLog) log->info("[VEH]   -> function return");
            
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();
            
            RecordRecovery();
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // ═══════════════════════════════════════════════════════════════
    // ILLEGAL INSTRUCTION
    // ═══════════════════════════════════════════════════════════════
    if (code == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (shouldLog)
            log->warn("[VEH] Illegal instruction at {}+{:#x} [hit #{}]",
                      ModName(rip), ModOff(rip), hits);

        // Can't decode an illegal instruction, so jump to caller
        if (L5_FuncReturn(ctx)) {
            if (shouldLog) log->info("[VEH]   -> function return");
            
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();
            
            RecordRecovery();
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (L6_DeepWalk(ctx)) {
            if (shouldLog) log->info("[VEH]   -> deep stack walk");
            
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementCrashesPrevented();
            
            RecordRecovery();
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

// ═══════════════════════════════════════════════════════════════════════
// § 8  Legacy Public API (Backward Compatibility)
// ═══════════════════════════════════════════════════════════════════════

void Install() {
    VEHExceptionHandler::Initialize();
}

void Remove() {
    VEHExceptionHandler::Shutdown();
}

size_t GetCrashCount() {
    return VEHExceptionHandler::GetCrashCount();
}

void LogStats() {
    VEHExceptionHandler::LogStats();
}

}  // namespace VEH
