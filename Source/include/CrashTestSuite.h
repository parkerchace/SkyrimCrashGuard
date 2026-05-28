// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "LayerTrace.h"

namespace CrashGuard {

/// Which testing mode a VEH test ran under.
enum class TestTier {
    /// t_testMode on, __try backup, crash instruction inside CrashGuard.dll.
    /// Guaranteed not to crash the game.  Tests the mechanism, not real conditions.
    Demo,

    /// VirtualAlloc stubs (RIP outside CrashGuard.dll), real cascade / cooldown,
    /// __try safety net retained.  Honest test of what VEH does in-game; game
    /// will not crash if VEH fails (SEH catches it and records a failure).
    RealConditions,

    /// VirtualAlloc stubs, real conditions, NO __try safety net.
    /// If VEH fails to handle the crash the game crashes to desktop.
    /// Save your game before running this tier.
    Live
};

/// Result of a single diagnostic test.
struct TestResult {
    // Identity
    std::string name;
    std::string description;       ///< Plain-English description of what the test does
    std::string exceptionType;     ///< e.g. "Access Violation (read)", "Null function call"
    std::string recoveryLayer;     ///< e.g. "Universal – zeroed register", "L5 function return"

    // Run state
    bool    ran               = false;
    bool    passed            = false;

    // VEH interception details
    bool    vehIntercepted    = false;   ///< VEH handler fired before SEH
    bool    executionResumed  = false;   ///< Code after the fault continued (s_execResumed)
    size_t  crashCountBefore  = 0;
    size_t  crashCountAfter   = 0;

    // Timing
    float   elapsedMs         = 0.0f;

    // Which tier this result came from
    TestTier tierUsed         = TestTier::Demo;

    // Human-readable outcome
    std::string resultMessage;

    // Recovery chain trace (for animated diagram)
    CrashGuard::LayerTrace layerTrace;
};

/// In-process diagnostic test suite.
/// All VEH crash-recovery tests run on a dedicated thread to keep
/// the game's main thread clean.  System health tests run inline.
class CrashTestSuite {
public:
    static constexpr int NUM_TESTS         = 14;
    static constexpr int SYSTEM_TEST_START = 10;  ///< First index of system-health tests

    CrashTestSuite();

    /// Run a single test by index (0-based).
    TestResult RunTest(int index);

    /// Run all tests in order (disabled in Live tier — too risky).
    void RunAllTests();

    /// Access results (read-only after RunTest / RunAllTests).
    const TestResult& GetResult(int index) const;
    TestResult&       GetResult(int index);

    /// Select which testing tier subsequent RunTest calls will use.
    static void     SetTestTier(TestTier tier);
    static TestTier GetTestTier();

    // ── VEH Crash Recovery (0–9) ──────────────────────────────────────────
    static TestResult Test_NullPointerRead();         ///< 0
    static TestResult Test_NullPointerWrite();        ///< 1
    static TestResult Test_NullFunctionCall();        ///< 2
    static TestResult Test_InteriorLightingPattern(); ///< 3
    static TestResult Test_VtableNullAccess();        ///< 4
    static TestResult Test_BadPointerWrite();         ///< 5
    static TestResult Test_DeepStructAccess();        ///< 6
    static TestResult Test_SequentialStress();        ///< 7
    static TestResult Test_RapidCascade();            ///< 8
    static TestResult Test_ShadowVisibilityTESTPattern(); ///< 9

    // ── System Health (10–13) ─────────────────────────────────────────────
    static TestResult Test_SaveFileIntegrity();       ///< 10
    static TestResult Test_AddressLibValidity();      ///< 11
    static TestResult Test_PatternLearningSystem();   ///< 12
    static TestResult Test_MemoryPressureMonitor();   ///< 13

private:
    TestResult m_results[NUM_TESTS];
};

}  // namespace CrashGuard
