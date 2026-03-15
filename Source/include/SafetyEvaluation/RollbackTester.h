// Copyright (C) 2024-2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "DataStructures.h"
#include <string>
#include <vector>
#include <memory>

namespace SafetyEvaluation {

/**
 * @brief Tests rollback functionality comprehensively
 * 
 * The RollbackTester validates that the StateManager's rollback capability
 * works correctly across various scenarios, ensuring state can be safely
 * restored after modifications or failures.
 * 
 * Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8, 7.9
 */
class RollbackTester {
public:
    /**
     * @brief Test rollback functionality comprehensively
     * @return Rollback report with all test results
     * 
     * Requirement 7.9: Generate rollback test report
     */
    static RollbackReport TestRollback();

    /**
     * @brief Test complete snapshot and rollback cycle
     * @return True if snapshot-rollback cycle works correctly
     * 
     * Requirement 7.1: Test StateManager snapshot, modify state, and verify rollback
     */
    static bool TestSnapshotRollbackCycle();

    /**
     * @brief Test rollback with various types of state modifications
     * @param type Type of state modification to test
     * @return True if rollback works for this modification type
     * 
     * Requirement 7.2: Test rollback with various state modifications
     */
    static bool TestStateModificationRollback(StateModificationType type);

    /**
     * @brief Test that rollback does not leak memory
     * @return True if no memory leaks detected during rollback
     * 
     * Requirement 7.3: Verify rollback does not leak memory
     */
    static bool TestRollbackMemoryLeaks();

    /**
     * @brief Test rollback after partial operation failures
     * @return True if rollback works after partial failures
     * 
     * Requirement 7.4: Test rollback after partial operation failures
     */
    static bool TestPartialFailureRollback();

    /**
     * @brief Test that pointers and references are correctly restored
     * @return True if all pointers/references are valid after rollback
     * 
     * Requirement 7.5: Verify pointers and references are correctly restored
     */
    static bool TestPointerRestoration();

    /**
     * @brief Test multiple sequential rollbacks
     * @return True if sequential rollbacks maintain consistency
     * 
     * Requirement 7.6: Test multiple sequential rollbacks
     */
    static bool TestSequentialRollbacks();

    /**
     * @brief Test rollback performance
     * @return Average rollback time in milliseconds
     * 
     * Requirement 7.7: Verify rollback completes within acceptable time
     */
    static double TestRollbackPerformance();

    /**
     * @brief Test rollback under low memory conditions
     * @return True if rollback works under memory constraints
     * 
     * Requirement 7.8: Test rollback under low memory conditions
     */
    static bool TestLowMemoryRollback();

private:
    /**
     * @brief Simulate capturing a state snapshot
     * @return True if snapshot was captured successfully
     */
    static bool SimulateSnapshotCapture();

    /**
     * @brief Simulate modifying game state
     * @param type Type of modification to simulate
     * @return True if modification was simulated successfully
     */
    static bool SimulateStateModification(StateModificationType type);

    /**
     * @brief Simulate rolling back to a snapshot
     * @return True if rollback was simulated successfully
     */
    static bool SimulateRollback();

    /**
     * @brief Verify that state was restored correctly
     * @param type Type of modification that was rolled back
     * @return True if state matches pre-modification state
     */
    static bool VerifyStateRestored(StateModificationType type);

    /**
     * @brief Track memory usage before and after rollback
     * @param beforeBytes Memory usage before rollback
     * @param afterBytes Memory usage after rollback
     * @return True if no memory leak detected
     */
    static bool CheckMemoryLeak(size_t beforeBytes, size_t afterBytes);

    /**
     * @brief Get current memory usage
     * @return Current memory usage in bytes
     */
    static size_t GetCurrentMemoryUsage();

    /**
     * @brief Simulate a partial operation failure
     * @return True if failure was simulated successfully
     */
    static bool SimulatePartialFailure();

    /**
     * @brief Create test pointers and references
     * @return True if test data was created successfully
     */
    static bool CreateTestPointers();

    /**
     * @brief Verify test pointers are still valid
     * @return True if all pointers are valid
     */
    static bool VerifyTestPointers();

    /**
     * @brief Clean up test pointers
     */
    static void CleanupTestPointers();

    /**
     * @brief Measure time taken for a rollback operation
     * @return Time in milliseconds
     */
    static double MeasureRollbackTime();

    /**
     * @brief Simulate low memory conditions
     * @return True if low memory was simulated successfully
     */
    static bool SimulateLowMemory();

    /**
     * @brief Restore normal memory conditions
     */
    static void RestoreNormalMemory();

    /**
     * @brief Create a rollback scenario for reporting
     * @param name Scenario name
     * @param type Modification type
     * @param success Whether rollback succeeded
     * @param memoryLeak Whether memory leak was detected
     * @param timeMs Rollback time in milliseconds
     * @param failureReason Reason for failure (if any)
     * @return Populated RollbackScenario
     */
    static RollbackScenario CreateScenario(
        const std::string& name,
        StateModificationType type,
        bool success,
        bool memoryLeak,
        double timeMs,
        const std::string& failureReason = ""
    );

    // Test data storage
    struct TestState {
        int objectValue;
        std::vector<int> inventoryItems;
        int questState;
        void* referencePtr;
    };

    static TestState s_originalState;
    static TestState s_currentState;
    static std::vector<void*> s_testPointers;
};

} // namespace SafetyEvaluation
