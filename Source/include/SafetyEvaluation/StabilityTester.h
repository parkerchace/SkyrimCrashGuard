// Copyright (C) 2024-2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "DataStructures.h"
#include <string>
#include <vector>
#include <chrono>
#include <functional>

namespace SafetyEvaluation {

/**
 * @brief Tests component stability under various conditions
 * 
 * The StabilityTester validates that components handle edge cases, memory failures,
 * thread safety, high load, and other stress conditions without crashing or failing.
 * 
 * Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 4.8, 4.9
 */
class StabilityTester {
public:
    /**
     * @brief Test component stability comprehensively
     * @param component Component to test
     * @return Stability report with rating and test results
     * 
     * Requirement 4.9: Generate stability rating
     */
    static StabilityReport TestStability(const ComponentInfo& component);

    /**
     * @brief Test component with edge case inputs
     * @param component Component to test
     * @return True if component handles all edge cases safely
     * 
     * Requirement 4.1: Test with edge case inputs
     * Requirement 4.2: Test with null, empty, boundary values
     */
    static bool TestEdgeCases(const ComponentInfo& component);

    /**
     * @brief Test component behavior when memory allocation fails
     * @param component Component to test
     * @return True if component handles allocation failures gracefully
     * 
     * Requirement 4.3: Test memory allocation failure handling
     */
    static bool TestMemoryFailures(const ComponentInfo& component);

    /**
     * @brief Test thread safety for multi-threaded components
     * @param component Component to test
     * @return True if component is thread-safe
     * 
     * Requirement 4.4: Test thread safety
     */
    static bool TestThreadSafety(const ComponentInfo& component);

    /**
     * @brief Test component under high load conditions
     * @param component Component to test
     * @return True if component remains stable under load
     * 
     * Requirement 4.5: Test under high load
     */
    static bool TestHighLoad(const ComponentInfo& component);

    /**
     * @brief Detect memory leaks during component operation
     * @param component Component to test
     * @return True if no memory leaks detected
     * 
     * Requirement 4.6: Detect memory leaks
     */
    static bool DetectMemoryLeaks(const ComponentInfo& component);

    /**
     * @brief Detect infinite loops with timeout mechanism
     * @param component Component to test
     * @return True if no infinite loops detected
     * 
     * Requirement 4.7: Detect infinite loops
     */
    static bool DetectInfiniteLoops(const ComponentInfo& component);

    /**
     * @brief Verify error logging and safe continuation
     * @param component Component to test
     * @return True if component logs errors and continues safely
     * 
     * Requirement 4.8: Verify error logging and safe continuation
     */
    static bool VerifyErrorHandling(const ComponentInfo& component);

private:
    /**
     * @brief Calculate stability rating based on test results
     */
    static StabilityRating CalculateStabilityRating(
        int edgeCasesPassed,
        int edgeCasesFailed,
        bool memoryFailuresHandled,
        bool threadSafe,
        bool highLoadStable,
        bool noMemoryLeaks,
        bool noInfiniteLoops,
        bool errorHandlingCorrect
    );

    /**
     * @brief Execute a test with timeout to detect infinite loops
     */
    static bool ExecuteWithTimeout(
        std::function<void()> testFunc,
        std::chrono::milliseconds timeout
    );

    /**
     * @brief Track memory allocations for leak detection
     */
    struct MemoryTracker {
        size_t allocatedBytes = 0;
        size_t freedBytes = 0;
        int allocationCount = 0;
        int freeCount = 0;

        bool HasLeaks() const {
            return allocatedBytes != freedBytes || allocationCount != freeCount;
        }

        size_t LeakedBytes() const {
            return allocatedBytes > freedBytes ? allocatedBytes - freedBytes : 0;
        }
    };

    /**
     * @brief Mock memory allocator for testing allocation failures
     */
    class MockAllocator {
    public:
        static void SetFailureMode(bool shouldFail) { s_shouldFail = shouldFail; }
        static bool IsFailureModeEnabled() { return s_shouldFail; }

    private:
        static inline bool s_shouldFail = false;
    };
};

} // namespace SafetyEvaluation
