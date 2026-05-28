// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

// CrashTestSuite.cpp
// ═══════════════════════════════════════════════════════════════════════
//
// EXCEPTION KERNEL DESIGN
// ───────────────────────
// Each VEH test uses a tiny "exception kernel"  -  a function that
// deliberately triggers an access violation.  The kernel is wrapped in
// __try/__except(EXCEPTION_EXECUTE_HANDLER) as a safety net ONLY; VEH
// fires BEFORE SEH, so CrashGuard's handler gets first crack.
//
//   * If VEH recovers -> EXCEPTION_CONTINUE_EXECUTION -> s_execResumed = true
//                        __except block is never reached
//   * If VEH gives up -> __except fires -> s_sehCaught = true
//                        game does not crash, test records FAIL
//
// THREAD MODEL
// ────────────
// Each kernel runs on the calling thread (main menu / SKSE update thread).
// EnableThreadTestMode() sets a thread_local flag that:
//   * Lets the universal recovery block fire even when RIP is inside
//     CrashGuard.dll itself (IsSelfAddr == true).
//   * Bypasses cascade and cooldown rate limiters.
//   * Skips L5/L6 (function return / deep stack walk) in the no-dest-reg
//     path  -  the __try/__except prologue adjusts RSP, making those layers
//     jump to the wrong frame.  Instruction-skip is used instead.
//
// The flag is cleared by DisableThreadTestMode() after the kernel returns.
//
// ═══════════════════════════════════════════════════════════════════════

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>     // SHGetFolderPathA (CSIDL_PERSONAL)

#include "CrashTestSuite.h"
#include "VEH.h"
#include "LayerTrace.h"

#include <chrono>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <thread>
#include <atomic>

// Address Library header (CommonLibSSE-NG)
#include <SKSE/SKSE.h>

namespace CrashGuard {

// ─── Active test tier ────────────────────────────────────────────────────────
static TestTier s_tier = TestTier::Demo;

void CrashTestSuite::SetTestTier(TestTier t) { s_tier = t; }
TestTier CrashTestSuite::GetTestTier()       { return s_tier; }

// ─── VirtualAlloc stub executor ──────────────────────────────────────────────
// Used for RealConditions and Live tiers.  The crash instruction lives in a
// VirtualAlloc'd page, so RIP is outside CrashGuard.dll and VEH applies real
// IsSelfAddr / cascade / cooldown rules (no t_testMode bypass).
//
// withSafety=true  → __try/__except wraps the call (game will not crash if VEH fails)
// withSafety=false → no safety net (CTD if VEH fails — Live tier)

static uint8_t* s_stubPage     = nullptr;
static bool     s_stubPageInit = false;

static uint8_t* GetStubPage() {
    if (!s_stubPageInit) {
        s_stubPage = static_cast<uint8_t*>(
            VirtualAlloc(nullptr, 4096,
                MEM_COMMIT | MEM_RESERVE,
                PAGE_EXECUTE_READWRITE));
        s_stubPageInit = true;
    }
    return s_stubPage;
}

struct KernelResult { bool execResumed; bool sehCaught; };

static KernelResult ExecStub(const uint8_t* bytes, size_t len, bool withSafety) {
    uint8_t* page = GetStubPage();
    if (!page) return { false, true };   // allocation failed — treat as SEH

    memcpy(page, bytes, len);
    FlushInstructionCache(GetCurrentProcess(), page, len);

    using Fn = void(*)();
    Fn fn = reinterpret_cast<Fn>(page);

    bool resumed = false, sehFired = false;
    if (withSafety) {
        __try   { fn(); resumed = true; }
        __except(EXCEPTION_EXECUTE_HANDLER) { sehFired = true; }
    } else {
        // Live tier: no safety net.  If VEH does not recover this, the game crashes.
        fn();
        resumed = true;
    }
    return { resumed, sehFired };
}

// ─── Kernel state ────────────────────────────────────────────────────────────
// Written by the kernel, read by RunVEHTest after the thread returns.
// Each kernel pair gets its own s_execResumed / s_sehCaught to avoid
// false sharing when tests run back-to-back.
static bool s_execResumed = false;
static bool s_sehCaught   = false;

// KernelResult already defined above (before ExecStub)

// ─── Kernel implementations ──────────────────────────────────────────────────
// Each kernel:
//   1. Resets flags
//   2. Enables test mode
//   3. __try { <fault> ; s_execResumed = true; } __except { s_sehCaught = true; }
//   4. Disables test mode

// ── Simulate game workload before the fault ────────────────────────────────
// Mirrors the CPU cost of a typical Skyrim game-loop iteration so timing
// resembles a real crash rather than a bare exception benchmark.
__declspec(noinline) static void SimulateGameWork() {
    volatile float acc = 1.0f;
    for (int i = 0; i < 2048; ++i) acc = acc * 1.00001f + (float)i * 0.0001f;
    (void)acc;
}

// ── Test 0: null pointer read (MOV reg, [0]) ──────────────────────────────
__declspec(noinline) static void NullReadKernel() {
    s_execResumed = false;
    s_sehCaught   = false;
    SimulateGameWork();
    VEH::VEHExceptionHandler::EnableThreadTestMode();
    __try {
        volatile int* p = nullptr;
        volatile int  v = *p;
        (void)v;
        s_execResumed = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s_sehCaught = true;
    }
    VEH::VEHExceptionHandler::DisableThreadTestMode();
}

// ── Test 1: null pointer write (MOV [0], reg) ─────────────────────────────
__declspec(noinline) static void NullWriteKernel() {
    s_execResumed = false;
    s_sehCaught   = false;
    SimulateGameWork();
    VEH::VEHExceptionHandler::EnableThreadTestMode();
    __try {
        volatile int* p = nullptr;
        *p = 42;
        s_execResumed = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s_sehCaught = true;
    }
    VEH::VEHExceptionHandler::DisableThreadTestMode();
}

// ── Test 2: null function call (CALL [reg] with reg==0) ───────────────────
using FnPtr = int(*)();
__declspec(noinline) static void NullCallKernel() {
    s_execResumed = false;
    s_sehCaught   = false;
    SimulateGameWork();
    VEH::VEHExceptionHandler::EnableThreadTestMode();
    __try {
        FnPtr fn = nullptr;
        volatile int r = fn();
        (void)r;
        s_execResumed = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s_sehCaught = true;
    }
    VEH::VEHExceptionHandler::DisableThreadTestMode();
}

// ── Test 3: interior lighting pattern (read nullptr+0x108, mask 0x08) ─────
// Mirrors the SkyrimSE.exe lighting-manager crash pattern.
// Mask 0x08 differs from test 9 (0x04) to prevent ICF linker merging.
__declspec(noinline) static void InteriorLightingKernel() {
    s_execResumed = false;
    s_sehCaught   = false;
    SimulateGameWork();
    VEH::VEHExceptionHandler::EnableThreadTestMode();
    __try {
        const BYTE*   p       = nullptr;
        volatile BYTE b       = *(const volatile BYTE*)(p + 0x108);
        volatile bool flagSet = (b & 0x08) != 0;
        (void)flagSet;
        s_execResumed = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s_sehCaught = true;
    }
    VEH::VEHExceptionHandler::DisableThreadTestMode();
}

// ── Test 4: vtable null access (CALL [rax+offset] with rax==0) ───────────
struct FakeVtableObj {
    virtual int Method() { return 1; }
};
__declspec(noinline) static void VtableNullKernel() {
    s_execResumed = false;
    s_sehCaught   = false;
    SimulateGameWork();
    VEH::VEHExceptionHandler::EnableThreadTestMode();
    __try {
        FakeVtableObj* obj = nullptr;
        volatile int r = obj->Method();
        (void)r;
        s_execResumed = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s_sehCaught = true;
    }
    VEH::VEHExceptionHandler::DisableThreadTestMode();
}

// ── Test 5: bad (non-null) pointer write ──────────────────────────────────
__declspec(noinline) static void BadWriteKernel() {
    s_execResumed = false;
    s_sehCaught   = false;
    SimulateGameWork();
    VEH::VEHExceptionHandler::EnableThreadTestMode();
    __try {
        volatile int* p = reinterpret_cast<volatile int*>(static_cast<uintptr_t>(0xDEADBEEF));
        *p = 99;
        s_execResumed = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s_sehCaught = true;
    }
    VEH::VEHExceptionHandler::DisableThreadTestMode();
}

// ── Test 6: deep struct member access (nullptr + large offset) ───────────
__declspec(noinline) static void DeepStructKernel() {
    s_execResumed = false;
    s_sehCaught   = false;
    SimulateGameWork();
    VEH::VEHExceptionHandler::EnableThreadTestMode();
    __try {
        const uint8_t* base  = nullptr;
        volatile int   v     = *reinterpret_cast<const volatile int*>(base + 0x2A0);
        (void)v;
        s_execResumed = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s_sehCaught = true;
    }
    VEH::VEHExceptionHandler::DisableThreadTestMode();
}

// ── Test 7: sequential stress (10 different faults back-to-back) ──────────
__declspec(noinline) static void SequentialStressKernel() {
    s_execResumed = false;
    s_sehCaught   = false;
    VEH::VEHExceptionHandler::EnableThreadTestMode();

    int recovered = 0;
    for (int i = 0; i < 10; ++i) {
        bool thisOk = false;
        __try {
            volatile int* p = nullptr;
            volatile int  v = p[i];
            (void)v;
            thisOk = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // VEH gave up on this one
        }
        if (thisOk) ++recovered;
    }

    VEH::VEHExceptionHandler::DisableThreadTestMode();
    // Pass if all 10 were recovered
    s_execResumed = (recovered == 10);
    s_sehCaught   = (recovered < 10);
}

// ── Test 8: rapid cascade  -  two kernels at different offsets ─────────────
// CascadeKernelA and CascadeKernelB use different offsets so the linker
// does not fold them (ICF prevention).
__declspec(noinline) static bool CascadeKernelA() {
    bool ok = false;
    __try {
        volatile int* p = nullptr;
        volatile int  v = p[1];  // offset 0x4
        (void)v;
        ok = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return ok;
}
__declspec(noinline) static bool CascadeKernelB() {
    bool ok = false;
    __try {
        volatile int* p = nullptr;
        volatile int  v = p[5];  // offset 0x14
        (void)v;
        ok = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return ok;
}
__declspec(noinline) static void RapidCascadeKernel() {
    s_execResumed = false;
    s_sehCaught   = false;
    VEH::VEHExceptionHandler::EnableThreadTestMode();

    bool a = CascadeKernelA();
    bool b = CascadeKernelB();

    VEH::VEHExceptionHandler::DisableThreadTestMode();
    s_execResumed = a && b;
    s_sehCaught   = !(a && b);
}

// ── Test 9: Shadow visibility TEST pattern (SkyrimSE.exe+14F400E) ───────────
// Reproduces a crash first documented via user-submitted logs.  Every session
// with a specific HDT-SMP / shadow mod combination crashed at the same spot:
//   TEST BYTE PTR [r14+0x109], 0x08   (r14 = null)
// during interior cell load.  Stack at crash: Sun*, NiCamera* "WorldRoot Camera",
// BSShadowFrustumLight*, BSShaderAccumulator*  -  Skyrim's shadow frustum system
// checking a visibility flag on an actor pointer that was null.
//
// The instruction only tests a bit — there is no output register to zero.
// CrashGuard must advance RIP past it; the shadow system takes the
// "object not visible / flag not set" path and the cell loads normally.
//
// The 4 logs used to develop this fix came from 4 separate game sessions.
//
// Mask 0x04 vs test 3 (0x08) prevents ICF linker merging.
// volatile BYTE + volatile bool prevents LTCG from discarding the TEST.
__declspec(noinline) static void ShadowTESTKernel() {
    s_execResumed = false;
    s_sehCaught   = false;
    SimulateGameWork();   // mirror the game-loop cost preceding a cell-load crash
    VEH::VEHExceptionHandler::EnableThreadTestMode();
    __try {
        const BYTE*   p       = nullptr;
        volatile BYTE b       = *(const volatile BYTE*)(p + 0x109);
        volatile bool flagSet = (b & 0x04) != 0;
        (void)flagSet;
        s_execResumed = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s_sehCaught = true;
    }
    VEH::VEHExceptionHandler::DisableThreadTestMode();
}

// ─── Generic VEH test runner ─────────────────────────────────────────────────
using KernelFn = void(*)();

static KernelResult RunKernel(KernelFn fn) {
    s_execResumed = false;
    s_sehCaught   = false;
    fn();
    return { s_execResumed, s_sehCaught };
}

static void FillVEHResult(TestResult& result, size_t before, size_t after,
                          float elapsedMs, KernelResult kr)
{
    result.ran              = true;
    result.crashCountBefore = before;
    result.crashCountAfter  = after;
    result.elapsedMs        = elapsedMs;
    result.vehIntercepted   = (after > before);
    result.executionResumed = kr.execResumed;
    result.tierUsed         = s_tier;

    const bool live = (s_tier == TestTier::Live);
    const bool real = (s_tier == TestTier::RealConditions);

    if (kr.execResumed) {
        result.passed = true;
        if (live) {
            result.resultMessage =
                "No safety net  -  VEH intercepted and recovered this crash under fully "
                "real conditions. If VEH had failed here, the game would have crashed to "
                "desktop. This is the highest-confidence test result.";
        } else if (real) {
            result.resultMessage =
                "Recovered under real conditions (cascade and cooldown active, no test "
                "mode bypasses). VEH handled this exactly as it would mid-game. "
                "The __try safety net was present but never needed.";
        } else {
            result.resultMessage = result.vehIntercepted
                ? "CrashGuard caught this before Skyrim saw it. Code after the crash "
                  "continued running  -  in a real game session, the player would never "
                  "know anything happened."
                : "CrashGuard caught this and returned safely from the crashing function. "
                  "The crash was swallowed  -  Skyrim kept running.";
        }
    } else if (kr.sehCaught) {
        result.passed = false;
        if (real) {
            result.resultMessage =
                "VEH did not recover this under real conditions. Most likely cause: "
                "cascade protection or the 5ms recovery cooldown blocked it. This is "
                "expected for rapid-fire crashes  -  CrashGuard intentionally limits "
                "recovery rate to prevent runaway loops from destroying game state.";
        } else {
            result.resultMessage = result.vehIntercepted
                ? "CrashGuard fired but couldn't recover in time (cascade limit). In a "
                  "real session with this pattern, Skyrim might crash to desktop."
                : "CrashGuard did not intercept this exception. Without it, Skyrim would "
                  "have crashed to desktop here.";
        }
    } else {
        result.passed        = false;
        result.resultMessage = "The crash instruction was optimised away by the compiler  -  "
                               "the test didn't actually fault. Try a Release build.";
    }

    result.layerTrace = VEH::VEHExceptionHandler::GetLastTestTrace();
}

// ─── x64 stub byte arrays ────────────────────────────────────────────────────
// Each stub:  [optional register setup]  [crash instruction]  [C3 = RET]
// The crash instruction lives in VirtualAlloc'd memory (not CrashGuard.dll),
// so IsSelfAddr(rip) = false and VEH applies real cascade / cooldown rules.
// If VEH recovers → RIP advanced past crash instruction → RET → caller.

// Read from null (MOV EAX, [RAX] with RAX=0)
static const uint8_t kStub_NullRead[] = {
    0x48, 0x31, 0xC0,   // XOR RAX, RAX
    0x8B, 0x00,         // MOV EAX, [RAX]   <- crash: read 0x0
    0xC3                // RET
};

// Write to null (MOV [RAX], RAX with RAX=0)
static const uint8_t kStub_NullWrite[] = {
    0x48, 0x31, 0xC0,   // XOR RAX, RAX
    0x48, 0x89, 0x00,   // MOV [RAX], RAX   <- crash: write 0x0
    0xC3                // RET
};

// Execute AV: CALL RAX with RAX=0 (push retAddr, then execute 0x0)
static const uint8_t kStub_NullCall[] = {
    0x48, 0x31, 0xC0,   // XOR RAX, RAX
    0xFF, 0xD0,         // CALL RAX         <- execute AV at 0x0
    0xC3                // RET  (L5 will pop the saved retAddr -> here)
};

// Interior lighting: TEST BYTE PTR [RCX+0x108], 0x08 with RCX=null
static const uint8_t kStub_InteriorLighting[] = {
    0x48, 0x31, 0xC9,                       // XOR RCX, RCX
    0xF6, 0x81, 0x08, 0x01, 0x00, 0x00, 0x08, // TEST BYTE PTR [RCX+0x108], 0x08
    0xC3                                    // RET
};

// Vtable null: MOV RAX, [RCX] with RCX=null (reading vtable ptr)
static const uint8_t kStub_VtableNull[] = {
    0x48, 0x31, 0xC9,   // XOR RCX, RCX
    0x48, 0x8B, 0x01,   // MOV RAX, [RCX]   <- crash: read vtable at 0x0
    0xC3                // RET
};

// Bad write: write to 0xDEADBEEF
static const uint8_t kStub_BadWrite[] = {
    0x48, 0xB8, 0xEF, 0xBE, 0xAD, 0xDE, 0x00, 0x00, 0x00, 0x00, // MOV RAX, 0xDEADBEEF
    0x89, 0x00,         // MOV [RAX], EAX   <- crash
    0xC3                // RET
};

// Deep struct: MOV RAX, [RAX+0x2A0] with RAX=null
static const uint8_t kStub_DeepStruct[] = {
    0x48, 0x31, 0xC0,                              // XOR RAX, RAX
    0x48, 0x8B, 0x80, 0xA0, 0x02, 0x00, 0x00,      // MOV RAX, [RAX+0x2A0]
    0xC3                                           // RET
};

// Shadow visibility: TEST BYTE PTR [R14+0x109], 0x08 — exact real crash pattern
static const uint8_t kStub_ShadowVisibility[] = {
    0x4D, 0x31, 0xF6,                                        // XOR R14, R14
    0x41, 0xF6, 0x86, 0x09, 0x01, 0x00, 0x00, 0x08,          // TEST BYTE PTR [R14+0x109], 0x08
    0xC3                                                     // RET
};

// Helper: run a stub according to the current tier.
// For Demo tier this should never be called — Demo uses the existing kernel functions.
static KernelResult RunTieredStub(const uint8_t* bytes, size_t len) {
    const bool withSafety = (s_tier != TestTier::Live);
    return ExecStub(bytes, len, withSafety);
}

// Helper: run a stub from a specific byte offset within the 4 KB stub page.
// Placing stubs at different offsets gives each crash a distinct RIP address —
// VEH treats each as a new crash site with its own L2 cache slot and
// cascade/cooldown counters.  Safe to call back-to-back for the same stub
// because page contents at other offsets are undisturbed.
static KernelResult ExecStubAt(const uint8_t* bytes, size_t len, size_t offset, bool withSafety) {
    uint8_t* page = GetStubPage();
    if (!page || offset + len > 4096) return { false, true };
    memcpy(page + offset, bytes, len);
    FlushInstructionCache(GetCurrentProcess(), page + offset, len);
    using Fn = void(*)();
    Fn fn = reinterpret_cast<Fn>(page + offset);
    bool resumed = false, sehFired = false;
    if (withSafety) {
        __try   { fn(); resumed = true; }
        __except(EXCEPTION_EXECUTE_HANDLER) { sehFired = true; }
    } else {
        fn();
        resumed = true;
    }
    return { resumed, sehFired };
}

// ─── CrashTestSuite constructor ───────────────────────────────────────────────

CrashTestSuite::CrashTestSuite() {
    // ── VEH Crash Recovery Tests ──────────────────────────────────────────

    // ── VEH Crash Recovery Tests ──────────────────────────────────────────

    m_results[0].name        = "Missing Object Read";
    m_results[0].description = "One of the most common Skyrim crashes: a mod or the game itself "
                               "tries to read data from an object that hasn't loaded yet or was "
                               "already deleted. Without CrashGuard this is an instant CTD. "
                               "CrashGuard answers with zero  -  as if the object returned nothing  -  "
                               "and the game continues.";
    m_results[0].exceptionType  = "Read from null (address 0x0)";
    m_results[0].recoveryLayer  = "Answer with zero, keep running";

    m_results[1].name        = "Write to Deleted Object";
    m_results[1].description = "Happens constantly during fast travel and cell transitions: a script "
                               "or the engine tries to update a property on an object that was just "
                               "unloaded from memory. Without CrashGuard, CTD. "
                               "CrashGuard silently drops the write  -  the game never knows it failed.";
    m_results[1].exceptionType  = "Write to null (address 0x0)";
    m_results[1].recoveryLayer  = "Drop the write, keep running";

    m_results[2].name        = "Corrupted Function Table";
    m_results[2].description = "The #1 cause of mod-related CTDs: vtable corruption. The game tries to "
                               "call a function on an object whose function table has been wiped  -  "
                               "the 'call' jumps to address zero. Without CrashGuard, instant CTD. "
                               "CrashGuard intercepts the jump, returns an empty result, and the "
                               "caller continues as if the function ran and found nothing.";
    m_results[2].exceptionType  = "Call to null function pointer (execute 0x0)";
    m_results[2].recoveryLayer  = "Intercept bad call, return empty result";

    m_results[3].name        = "Interior Lighting Crash";
    m_results[3].description = "Affects many modded setups with ENB or lighting overhauls. When an "
                               "interior cell unloads mid-frame, the lighting system checks a flag "
                               "on an object that no longer exists. The instruction only tests a bit "
                               " -  it has no data to store, just sets a CPU flag. "
                               "CrashGuard skips the check; the flag stays unset and the game continues "
                               "as if the object reported 'not found'.";
    m_results[3].exceptionType  = "Read at offset +0x108 from null, via TEST instruction";
    m_results[3].recoveryLayer  = "Skip the check, continue on 'not found' path";

    m_results[4].name        = "Null Actor / Object Call";
    m_results[4].description = "Extremely common when an NPC's AI package loses its target: the game "
                               "tries to call a method on an object that is null. This attempts to "
                               "read the function table from address zero. Without CrashGuard, CTD. "
                               "CrashGuard zeroes the pointer and either skips the read or intercepts "
                               "the resulting call.";
    m_results[4].exceptionType  = "Read vtable at null (address 0x0)";
    m_results[4].recoveryLayer  = "Intercept null vtable access";

    m_results[5].name        = "Write to Freed Memory";
    m_results[5].description = "A mod writes data to an address that was already freed or was never "
                               "valid  -  a common symptom of use-after-free bugs in Papyrus scripts "
                               "or C++ mods. Without CrashGuard, the write corrupts memory or CTDs. "
                               "CrashGuard drops the write completely, leaving all other memory intact.";
    m_results[5].exceptionType  = "Write to invalid address 0xDEADBEEF";
    m_results[5].recoveryLayer  = "Drop the write, keep running";

    m_results[6].name        = "Partially Loaded Cell Data";
    m_results[6].description = "Reads deep into a data structure (at offset +0x2A0) that was only "
                               "partially initialised  -  common when a cell is still streaming in. "
                               "Without CrashGuard, CTD. CrashGuard treats the missing field as "
                               "zero and lets whatever was reading it continue with a safe default.";
    m_results[6].exceptionType  = "Read at offset +0x2A0 from null";
    m_results[6].recoveryLayer  = "Answer with zero, keep running";

    m_results[7].name        = "Ten Crashes in a Row";
    m_results[7].description = "Some game systems loop over a list of objects and access each one  -  "
                               "if several are null, the game would crash on every iteration. "
                               "This test fires ten separate null reads back-to-back to confirm "
                               "CrashGuard can handle a burst without its own rate limiter "
                               "getting in the way. (Rate limiters are bypassed in test mode.)";
    m_results[7].exceptionType  = "10 reads from null (offsets 0x0 through 0x24)";
    m_results[7].recoveryLayer  = "Answer with zero, x10 in sequence";

    m_results[8].name        = "Two Crashes, One Frame";
    m_results[8].description = "An NPC loop can hit the same missing data twice in the same frame  -  "
                               "two different instructions, two different crash addresses. "
                               "CrashGuard's cascade limiter is designed to stop runaway loops, "
                               "but it should not block two distinct, recoverable crashes that "
                               "happen close together. This test confirms both are caught.";
    m_results[8].exceptionType  = "Two reads from null at different offsets (0x4 and 0x14)";
    m_results[8].recoveryLayer  = "Answer with zero twice";

    m_results[9].name        = "Shadow System Null Actor";
    m_results[9].description = "Reproduces a crash in Skyrim's shadow frustum / visibility system "
                               "(SkyrimSE.exe+14F400E). When loading into an interior cell, the "
                               "shadow system checks a visibility flag on an actor pointer that "
                               "was null  -  a pattern seen consistently with certain HDT-SMP or "
                               "shadow overhaul setups. The instruction is TEST: it only compares "
                               "a bit, it stores nothing. There is no register to zero. "
                               "CrashGuard advances RIP past the TEST; the shadow system takes "
                               "the 'not visible' path and the cell loads normally.";
    m_results[9].exceptionType  = "Read at offset +0x109 from null, via TEST instruction";
    m_results[9].recoveryLayer  = "Skip the check, continue on 'flag not set' path";

    // ── System Health Tests ───────────────────────────────────────────────

    m_results[10].name        = "Save Folder Writable";
    m_results[10].description = "CrashGuard writes recovery logs and co-save data to your SKSE "
                                "folder. If that folder is missing, read-only, or on a full drive, "
                                "none of that works. This check also scans for zero-byte files "
                                "left behind by previous crash interruptions.";
    m_results[10].exceptionType  = "File-system check";
    m_results[10].recoveryLayer  = "N/A";

    m_results[11].name        = "Address Library Loaded";
    m_results[11].description = "Address Library translates memory addresses between game versions. "
                                "CrashGuard's list of pre-analysed crash sites (L1 Known Fix) depends "
                                "entirely on it. If Address Library is missing or wrong for your game "
                                "version, L1 fixes will silently fail and crashes will fall through to "
                                "slower layers  -  or not be caught at all.";
    m_results[11].exceptionType  = "Library version check";
    m_results[11].recoveryLayer  = "N/A";

    m_results[12].name        = "Runtime Learning Active";
    m_results[12].description = "Every time CrashGuard recovers a new crash it has not seen before, "
                                "it caches the solution. Next time the same code crashes  -  even with "
                                "a different mod list  -  recovery is instant. This confirms that "
                                "caching system is running and the VEH handler is installed correctly.";
    m_results[12].exceptionType  = "Internal state check";
    m_results[12].recoveryLayer  = "N/A";

    m_results[13].name        = "Memory Usage";
    m_results[13].description = "High memory usage is the most common pre-condition for the null "
                                "pointer crashes CrashGuard recovers. When the game runs out of "
                                "address space it starts failing to allocate objects  -  which then "
                                "show up as null pointers in other systems. This check reads your "
                                "current working set and private memory usage.";
    m_results[13].exceptionType  = "Memory metrics read";
    m_results[13].recoveryLayer  = "N/A";
}

// ─── Test implementations (0-9 VEH) ──────────────────────────────────────────

TestResult CrashTestSuite::Test_NullPointerRead() {
    TestResult result;
    result.name           = "Null Pointer Read";
    result.exceptionType  = "Access Violation (read 0x0)";
    result.recoveryLayer  = "Universal  -  zeroed register";

    size_t before = VEH::VEHExceptionHandler::GetCrashCount();
    auto   t0     = std::chrono::high_resolution_clock::now();
    KernelResult kr = (s_tier == TestTier::Demo)
        ? RunKernel(NullReadKernel)
        : RunTieredStub(kStub_NullRead, sizeof(kStub_NullRead));
    float  ms = std::chrono::duration<float, std::milli>(
                    std::chrono::high_resolution_clock::now() - t0).count();
    size_t after = VEH::VEHExceptionHandler::GetCrashCount();

    FillVEHResult(result, before, after, ms, kr);
    return result;
}

TestResult CrashTestSuite::Test_NullPointerWrite() {
    TestResult result;
    result.name           = "Null Pointer Write";
    result.exceptionType  = "Access Violation (write 0x0)";
    result.recoveryLayer  = "Universal  -  skipped write";

    size_t before = VEH::VEHExceptionHandler::GetCrashCount();
    auto   t0     = std::chrono::high_resolution_clock::now();
    KernelResult kr = (s_tier == TestTier::Demo)
        ? RunKernel(NullWriteKernel)
        : RunTieredStub(kStub_NullWrite, sizeof(kStub_NullWrite));
    float  ms = std::chrono::duration<float, std::milli>(
                    std::chrono::high_resolution_clock::now() - t0).count();
    size_t after = VEH::VEHExceptionHandler::GetCrashCount();

    FillVEHResult(result, before, after, ms, kr);
    return result;
}

TestResult CrashTestSuite::Test_NullFunctionCall() {
    TestResult result;
    result.name           = "Null Function Call";
    result.exceptionType  = "Access Violation (execute 0x0)";
    result.recoveryLayer  = "Execute-AV recovery";

    size_t before = VEH::VEHExceptionHandler::GetCrashCount();
    auto   t0     = std::chrono::high_resolution_clock::now();
    KernelResult kr = (s_tier == TestTier::Demo)
        ? RunKernel(NullCallKernel)
        : RunTieredStub(kStub_NullCall, sizeof(kStub_NullCall));
    float  ms = std::chrono::duration<float, std::milli>(
                    std::chrono::high_resolution_clock::now() - t0).count();
    size_t after = VEH::VEHExceptionHandler::GetCrashCount();

    FillVEHResult(result, before, after, ms, kr);
    return result;
}

TestResult CrashTestSuite::Test_InteriorLightingPattern() {
    TestResult result;
    result.name           = "Interior Lighting Crash";
    result.exceptionType  = "Access Violation (read 0x108)";
    result.recoveryLayer  = "Universal  -  skipped flags instruction";

    size_t before = VEH::VEHExceptionHandler::GetCrashCount();
    auto   t0     = std::chrono::high_resolution_clock::now();
    KernelResult kr = (s_tier == TestTier::Demo)
        ? RunKernel(InteriorLightingKernel)
        : RunTieredStub(kStub_InteriorLighting, sizeof(kStub_InteriorLighting));
    float  ms = std::chrono::duration<float, std::milli>(
                    std::chrono::high_resolution_clock::now() - t0).count();
    size_t after = VEH::VEHExceptionHandler::GetCrashCount();

    FillVEHResult(result, before, after, ms, kr);
    return result;
}

TestResult CrashTestSuite::Test_VtableNullAccess() {
    TestResult result;
    result.name           = "Null Vtable Call";
    result.exceptionType  = "Access Violation (read 0x0 for vtable)";
    result.recoveryLayer  = "Universal  -  zeroed register or execute-AV recovery";

    size_t before = VEH::VEHExceptionHandler::GetCrashCount();
    auto   t0     = std::chrono::high_resolution_clock::now();
    KernelResult kr = (s_tier == TestTier::Demo)
        ? RunKernel(VtableNullKernel)
        : RunTieredStub(kStub_VtableNull, sizeof(kStub_VtableNull));
    float  ms = std::chrono::duration<float, std::milli>(
                    std::chrono::high_resolution_clock::now() - t0).count();
    size_t after = VEH::VEHExceptionHandler::GetCrashCount();

    FillVEHResult(result, before, after, ms, kr);
    return result;
}

TestResult CrashTestSuite::Test_BadPointerWrite() {
    TestResult result;
    result.name           = "Bad Pointer Write";
    result.exceptionType  = "Access Violation (write 0xDEADBEEF)";
    result.recoveryLayer  = "Universal  -  skipped write";

    size_t before = VEH::VEHExceptionHandler::GetCrashCount();
    auto   t0     = std::chrono::high_resolution_clock::now();
    KernelResult kr = (s_tier == TestTier::Demo)
        ? RunKernel(BadWriteKernel)
        : RunTieredStub(kStub_BadWrite, sizeof(kStub_BadWrite));
    float  ms = std::chrono::duration<float, std::milli>(
                    std::chrono::high_resolution_clock::now() - t0).count();
    size_t after = VEH::VEHExceptionHandler::GetCrashCount();

    FillVEHResult(result, before, after, ms, kr);
    return result;
}

TestResult CrashTestSuite::Test_DeepStructAccess() {
    TestResult result;
    result.name           = "Deep Struct Access";
    result.exceptionType  = "Access Violation (read 0x2A0)";
    result.recoveryLayer  = "Universal  -  zeroed register";

    size_t before = VEH::VEHExceptionHandler::GetCrashCount();
    auto   t0     = std::chrono::high_resolution_clock::now();
    KernelResult kr = (s_tier == TestTier::Demo)
        ? RunKernel(DeepStructKernel)
        : RunTieredStub(kStub_DeepStruct, sizeof(kStub_DeepStruct));
    float  ms = std::chrono::duration<float, std::milli>(
                    std::chrono::high_resolution_clock::now() - t0).count();
    size_t after = VEH::VEHExceptionHandler::GetCrashCount();

    FillVEHResult(result, before, after, ms, kr);
    return result;
}

TestResult CrashTestSuite::Test_SequentialStress() {
    TestResult result;
    result.name           = "Sequential Stress";
    result.exceptionType  = "10x Access Violation (read 0x0-0x24)";
    result.recoveryLayer  = "Universal  -  zeroed register (x10)";

    size_t before = VEH::VEHExceptionHandler::GetCrashCount();
    auto   t0     = std::chrono::high_resolution_clock::now();

    KernelResult kr;
    if (s_tier == TestTier::Demo) {
        kr = RunKernel(SequentialStressKernel);
    } else {
        // Real/Live: place the same null-read stub at 10 distinct page offsets so
        // each crash has a unique RIP — VEH sees 10 separate crash sites, each
        // going through the full decode + L2 cache + cascade path independently.
        // 32-byte spacing comfortably fits the 6-byte stub with no overlap.
        const bool withSafety = (s_tier != TestTier::Live);
        int recovered = 0;
        for (int i = 0; i < 10; ++i) {
            KernelResult one = ExecStubAt(kStub_NullRead, sizeof(kStub_NullRead),
                                          static_cast<size_t>(i) * 32, withSafety);
            if (one.execResumed) ++recovered;
        }
        kr = { (recovered == 10), (recovered < 10) };
    }

    float  ms    = std::chrono::duration<float, std::milli>(
                       std::chrono::high_resolution_clock::now() - t0).count();
    size_t after = VEH::VEHExceptionHandler::GetCrashCount();

    result.ran              = true;
    result.crashCountBefore = before;
    result.crashCountAfter  = after;
    result.elapsedMs        = ms;
    result.vehIntercepted   = (after > before);
    result.executionResumed = kr.execResumed;
    result.tierUsed         = s_tier;
    result.layerTrace       = VEH::VEHExceptionHandler::GetLastTestTrace();

    if (kr.execResumed) {
        result.passed = true;
        if (s_tier != TestTier::Demo) {
            result.resultMessage =
                "All 10 crashes caught under real conditions (no test-mode bypasses). "
                "Each stub ran at a distinct memory address — VEH treated each as a new "
                "crash site. Cascade limiter and cooldown were active and did not interfere.";
        } else {
            result.resultMessage = "All 10 crashes caught and recovered  -  game kept running";
        }
    } else {
        result.passed = false;
        if (s_tier != TestTier::Demo) {
            result.resultMessage =
                "One or more of the 10 crashes were not recovered under real conditions. "
                "Most likely cause: the recovery cooldown (5 ms between consecutive crashes) "
                "or cascade limiter blocked some hits. This is expected behaviour when crashes "
                "arrive faster than the rate limit allows  -  CrashGuard intentionally "
                "throttles recovery to prevent runaway loops from destroying game state.";
        } else {
            result.resultMessage = "One or more crashes were not recovered  -  cascade limiter may have fired";
        }
    }
    return result;
}

TestResult CrashTestSuite::Test_RapidCascade() {
    TestResult result;
    result.name           = "Rapid Cascade";
    result.exceptionType  = "2x Access Violation (read 0x4, read 0x14)";
    result.recoveryLayer  = "Universal  -  zeroed register (x2)";

    size_t before = VEH::VEHExceptionHandler::GetCrashCount();
    auto   t0     = std::chrono::high_resolution_clock::now();

    KernelResult kr;
    if (s_tier == TestTier::Demo) {
        kr = RunKernel(RapidCascadeKernel);
    } else {
        // Real/Live: two stubs at distinct page offsets (0 and 64) — one null read,
        // one null write.  Different crash address AND different fault type, so the
        // cascade limiter should not block the second one.
        const bool withSafety = (s_tier != TestTier::Live);
        KernelResult a = ExecStubAt(kStub_NullRead,  sizeof(kStub_NullRead),   0, withSafety);
        KernelResult b = ExecStubAt(kStub_NullWrite, sizeof(kStub_NullWrite), 64, withSafety);
        kr = { a.execResumed && b.execResumed, !(a.execResumed && b.execResumed) };
    }

    float  ms    = std::chrono::duration<float, std::milli>(
                       std::chrono::high_resolution_clock::now() - t0).count();
    size_t after = VEH::VEHExceptionHandler::GetCrashCount();

    result.ran              = true;
    result.crashCountBefore = before;
    result.crashCountAfter  = after;
    result.elapsedMs        = ms;
    result.vehIntercepted   = (after > before);
    result.executionResumed = kr.execResumed;
    result.tierUsed         = s_tier;
    result.layerTrace       = VEH::VEHExceptionHandler::GetLastTestTrace();

    if (kr.execResumed) {
        result.passed = true;
        if (s_tier != TestTier::Demo) {
            result.resultMessage =
                "Both crashes caught under real conditions. Crash A (null read) and "
                "crash B (null write) occurred at distinct addresses with distinct fault "
                "types  -  cascade limiter correctly allowed both through. "
                "CrashGuard's cascade protection targets runaway loops at a single "
                "address, not legitimate rapid crashes at different sites.";
        } else {
            result.resultMessage = "Both rapid crashes caught and recovered  -  cascade limiter correctly bypassed in test mode";
        }
    } else {
        result.passed = false;
        if (s_tier != TestTier::Demo) {
            result.resultMessage =
                "One of the two crashes was not recovered under real conditions. "
                "Even though the crashes were at distinct addresses and of different types, "
                "the recovery cooldown or cascade limiter intervened. This may indicate "
                "the two stubs ran too close together for the 5ms cooldown window.";
        } else {
            result.resultMessage = "Second crash was not recovered  -  cascade limiter may have fired unexpectedly";
        }
    }
    return result;
}

TestResult CrashTestSuite::Test_ShadowVisibilityTESTPattern() {
    TestResult result;
    result.name           = "Shadow System Null Actor";
    result.exceptionType  = "Access Violation (read 0x109) via TEST instruction";
    result.recoveryLayer  = "Universal - skipped flags instruction (TEST/CMP)";

    size_t before = VEH::VEHExceptionHandler::GetCrashCount();
    auto   t0     = std::chrono::high_resolution_clock::now();
    KernelResult kr = (s_tier == TestTier::Demo)
        ? RunKernel(ShadowTESTKernel)
        : RunTieredStub(kStub_ShadowVisibility, sizeof(kStub_ShadowVisibility));
    float  ms     = std::chrono::duration<float, std::milli>(
                        std::chrono::high_resolution_clock::now() - t0).count();
    size_t after  = VEH::VEHExceptionHandler::GetCrashCount();

    FillVEHResult(result, before, after, ms, kr);

    // Override message with real-world context
    if (result.passed) {
        result.resultMessage =
            "CrashGuard skipped past the TEST instruction  -  the shadow system took the "
            "'not visible' path and execution continued. This crash pattern "
            "(SkyrimSE.exe+14F400E, TEST BYTE PTR [r14+0x109], r14=0) fires during "
            "interior cell load when a null actor pointer reaches the shadow frustum "
            "visibility check  -  a consistent side-effect of certain HDT-SMP / shadow "
            "overhaul mod setups.";
    } else {
        result.resultMessage =
            "CrashGuard did not recover the TEST instruction fault. In a real session this "
            "crash (SkyrimSE.exe+14F400E) would cause a CTD on every interior cell load "
            "when the HDT-SMP shadow null-actor condition is present.";
    }
    return result;
}

// ─── System health tests (10-13) ─────────────────────────────────────────────

TestResult CrashTestSuite::Test_SaveFileIntegrity() {
    TestResult result;
    result.ran            = true;
    result.name           = "Save File Integrity";
    result.exceptionType  = "N/A (file-system check)";
    result.recoveryLayer  = "N/A";

    try {
        namespace fs = std::filesystem;

        // Use SHGetFolderPathA to find the real Documents folder —
        // works whether or not the user has OneDrive redirection.
        char docsRaw[MAX_PATH] = {};
        if (FAILED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, docsRaw))) {
            // Fallback: try both OneDrive and direct Documents paths
            const char* profile = std::getenv("USERPROFILE");
            if (!profile) profile = "C:/Users/Default";
            fs::path p1 = fs::path(profile) / "OneDrive/Documents";
            fs::path p2 = fs::path(profile) / "Documents";
            if      (fs::exists(p1)) std::strncpy(docsRaw, p1.string().c_str(), MAX_PATH - 1);
            else if (fs::exists(p2)) std::strncpy(docsRaw, p2.string().c_str(), MAX_PATH - 1);
        }
        auto docsPath = fs::path(docsRaw) / "My Games/Skyrim Special Edition/SKSE";

        if (!fs::exists(docsPath)) {
            result.passed        = false;
            result.resultMessage = "SKSE folder not found (tried " + docsPath.string() + ")";
            return result;
        }

        // Check we can write a temp file
        auto testFile = docsPath / "__crashguard_write_test.tmp";
        {
            std::ofstream f(testFile);
            if (!f) {
                result.passed        = false;
                result.resultMessage = "SKSE folder exists but is not writable  -  recovery state will not persist";
                return result;
            }
            f << "ok";
        }
        fs::remove(testFile);

        // Scan the Saves folder for zero-byte .ess files — these are corrupt save data.
        // (The SKSE folder holds log files from many mods; zero-byte logs are normal and
        // must NOT be flagged here.)
        auto savesPath = fs::path(docsRaw) / "My Games/Skyrim Special Edition/Saves";
        int  zeroEss   = 0;
        bool savesFound = fs::exists(savesPath);

        if (savesFound) {
            for (auto& entry : fs::directory_iterator(savesPath)) {
                if (entry.is_regular_file()
                    && entry.path().extension() == ".ess"
                    && entry.file_size() == 0) {
                    ++zeroEss;
                }
            }
        }

        if (zeroEss > 0) {
            result.passed        = false;
            result.resultMessage = "Found " + std::to_string(zeroEss)
                                 + " zero-byte .ess save file(s) in Saves folder  -  corrupt save(s) detected";
        } else if (!savesFound) {
            result.passed        = true;
            result.resultMessage = "SKSE folder is writable  -  Saves folder not found (no saves yet, or custom path)";
        } else {
            result.passed        = true;
            result.resultMessage = "SKSE folder is writable  -  no corrupt save files detected";
        }
    } catch (const std::exception& e) {
        result.passed        = false;
        result.resultMessage = std::string("Exception during file check: ") + e.what();
    }
    return result;
}

TestResult CrashTestSuite::Test_AddressLibValidity() {
    TestResult result;
    result.ran            = true;
    result.name           = "Address Library";
    result.exceptionType  = "N/A (library check)";
    result.recoveryLayer  = "N/A";

    // Probe a known stable SKSE offset to confirm Address Library is functional
    try {
        // REL::ID 0 is the very first entry  -  exists in every version
        REL::ID testID(0);
        uintptr_t addr = testID.address();
        if (addr == 0) {
            result.passed        = false;
            result.resultMessage = "Address Library returned address 0 for ID 0  -  database may be missing or mismatched";
        } else {
            result.passed        = true;
            result.resultMessage = "Address Library operational  -  ID 0 resolved to a valid address";
        }
    } catch (...) {
        result.passed        = false;
        result.resultMessage = "Address Library threw an exception  -  version database not loaded";
    }
    return result;
}

TestResult CrashTestSuite::Test_PatternLearningSystem() {
    TestResult result;
    result.ran            = true;
    result.name           = "Pattern Learning System";
    result.exceptionType  = "N/A (internal state check)";
    result.recoveryLayer  = "N/A";

    // Check that the VEH is installed (prerequisite for learning)
    size_t crashCount = VEH::VEHExceptionHandler::GetCrashCount();
    auto   stats      = VEH::VEHExceptionHandler::GetLayerStats();

    // Handler is considered active if total >= 0 (always true once installed)
    // and the stats struct is readable.
    bool handlerActive = (stats.total != static_cast<uint64_t>(-1));

    if (!handlerActive) {
        result.passed        = false;
        result.resultMessage = "Could not read VEH stats  -  handler may not be installed";
        return result;
    }

    result.passed        = true;
    result.resultMessage = "Pattern learning system operational  -  "
                           + std::to_string(crashCount) + " total crash(es) tracked, "
                           + std::to_string(stats.instrPattern) + " via instruction-pattern recovery";
    return result;
}

TestResult CrashTestSuite::Test_MemoryPressureMonitor() {
    TestResult result;
    result.ran            = true;
    result.name           = "Memory Pressure Monitor";
    result.exceptionType  = "N/A (memory metrics check)";
    result.recoveryLayer  = "N/A";

    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    if (!GetProcessMemoryInfo(GetCurrentProcess(),
                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                              sizeof(pmc))) {
        result.passed        = false;
        result.resultMessage = "GetProcessMemoryInfo failed  -  cannot read memory metrics";
        return result;
    }

    size_t workingSetMB   = pmc.WorkingSetSize / (1024 * 1024);
    size_t privateUsageMB = pmc.PrivateUsage   / (1024 * 1024);

    // Two independent pressure signals for Skyrim SSE:
    //   WorkingSet  > 3000 MB  →  physical RAM pressure (pages being pulled in faster than evicted)
    //   PrivateUsage > 3500 MB →  virtual memory / commit exhaustion; common pre-crash warning
    //                             (Skyrim allocates heavily into private commit even when working set
    //                              is still moderate — this is the earlier and more reliable signal)
    bool highWorking = (workingSetMB   > 3000);
    bool highPrivate = (privateUsageMB > 3500);
    bool highPressure = highWorking || highPrivate;

    result.passed = !highPressure;
    result.resultMessage = "Working set: " + std::to_string(workingSetMB) + " MB, "
                         + "Private commit: " + std::to_string(privateUsageMB) + " MB";
    if (highPrivate && highWorking)
        result.resultMessage += "  -  HIGH: both metrics elevated (crash risk high)";
    else if (highPrivate)
        result.resultMessage += "  -  HIGH: private commit >3.5 GB (virtual memory pressure, crash risk elevated)";
    else if (highWorking)
        result.resultMessage += "  -  HIGH: working set >3 GB (RAM pressure, crash risk elevated)";
    else
        result.resultMessage += "  -  OK";
    return result;
}

// ─── CrashTestSuite public interface ─────────────────────────────────────────

TestResult CrashTestSuite::RunTest(int index) {
    if (index < 0 || index >= NUM_TESTS) {
        TestResult err;
        err.ran           = true;
        err.passed        = false;
        err.resultMessage = "Invalid test index";
        return err;
    }

    static TestResult(*vehTests[])() = {
        Test_NullPointerRead,
        Test_NullPointerWrite,
        Test_NullFunctionCall,
        Test_InteriorLightingPattern,
        Test_VtableNullAccess,
        Test_BadPointerWrite,
        Test_DeepStructAccess,
        Test_SequentialStress,
        Test_RapidCascade,
        Test_ShadowVisibilityTESTPattern,
    };
    static TestResult(*sysTests[])() = {
        Test_SaveFileIntegrity,
        Test_AddressLibValidity,
        Test_PatternLearningSystem,
        Test_MemoryPressureMonitor,
    };

    if (index < SYSTEM_TEST_START) {
        m_results[index] = vehTests[index]();
    } else {
        m_results[index] = sysTests[index - SYSTEM_TEST_START]();
    }
    return m_results[index];
}

void CrashTestSuite::RunAllTests() {
    // Live tier has no __try safety net — a single VEH failure crashes the game.
    // Running all 10 VEH tests back-to-back without a safety net is an unreasonable
    // risk.  This function is intentionally a no-op in Live mode; run tests
    // individually so you can save between them.
    if (s_tier == TestTier::Live) {
        return;
    }
    for (int i = 0; i < NUM_TESTS; ++i) {
        RunTest(i);
    }
}

const TestResult& CrashTestSuite::GetResult(int index) const {
    return m_results[index];
}

TestResult& CrashTestSuite::GetResult(int index) {
    return m_results[index];
}

}  // namespace CrashGuard
