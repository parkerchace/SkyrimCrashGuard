// Copyright (C) 2026-2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "DataStructures.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>

namespace SafetyEvaluation {

/**
 * @brief Tests components for memory leaks and corruption
 * 
 * The MemorySafetyTester validates that components properly manage memory,
 * detecting leaks, use-after-free, double-free, and buffer overflows.
 * 
 * Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8, 8.9
 */
class MemorySafetyTester {
public:
    /**
     * @brief Test component memory safety comprehensively
     * @param component Component to test
     * @return Memory safety report with leak and corruption details
     * 
     * Requirement 8.1: Run memory leak detection
     * Requirement 8.9: Generate memory safety report
     */
    static MemorySafetyReport TestMemorySafety(const ComponentInfo& component);

    /**
     * @brief Detect memory leaks with memory tracking
     * @param component Component to test
     * @return List of detected memory leaks
     * 
     * Requirement 8.2: Verify proper memory deallocation
     */
    static std::vector<MemoryLeak> DetectMemoryLeaks(const ComponentInfo& component);

    /**
     * @brief Test for use-after-free errors
     * @param component Component to test
     * @return True if no use-after-free detected
     * 
     * Requirement 8.3: Test for use-after-free
     */
    static bool TestUseAfterFree(const ComponentInfo& component);

    /**
     * @brief Test for double-free errors
     * @param component Component to test
     * @return True if no double-free detected
     * 
     * Requirement 8.4: Test for double-free
     */
    static bool TestDoubleFree(const ComponentInfo& component);

    /**
     * @brief Test allocation failure handling
     * @param component Component to test
     * @return True if component handles allocation failures gracefully
     * 
     * Requirement 8.5: Test allocation failure handling
     */
    static bool TestAllocationFailures(const ComponentInfo& component);

    /**
     * @brief Test for buffer overflows
     * @param component Component to test
     * @return True if no buffer overflows detected
     * 
     * Requirement 8.8: Test buffer overflow protection
     */
    static bool TestBufferOverflows(const ComponentInfo& component);

    /**
     * @brief Monitor memory usage over time under load
     * @param component Component to test
     * @return True if memory usage is stable
     * 
     * Requirement 8.6: Monitor memory usage over time under load
     */
    static bool MonitorMemoryUsageUnderLoad(const ComponentInfo& component);

    /**
     * @brief Test dangling pointer detection
     * @param component Component to test
     * @return True if dangling pointers are properly handled
     * 
     * Requirement 8.7: Test dangling pointer detection
     */
    static bool TestDanglingPointerDetection(const ComponentInfo& component);

private:
    /**
     * @brief Memory allocation tracker for leak detection
     */
    struct AllocationTracker {
        struct Allocation {
            void* address;
            size_t size;
            std::string location;
            bool freed;
        };

        std::unordered_map<void*, Allocation> allocations;
        size_t totalAllocated = 0;
        size_t totalFreed = 0;

        void RecordAllocation(void* ptr, size_t size, const std::string& location);
        void RecordDeallocation(void* ptr);
        std::vector<MemoryLeak> GetLeaks() const;
        size_t GetTotalLeaked() const;
    };

    /**
     * @brief Simulate component lifecycle for memory testing
     */
    static void SimulateComponentLifecycle(
        const ComponentInfo& component,
        AllocationTracker& tracker
    );

    /**
     * @brief Check for use-after-free by tracking freed pointers
     */
    static bool CheckUseAfterFree(
        const ComponentInfo& component,
        AllocationTracker& tracker
    );

    /**
     * @brief Check for double-free by tracking deallocation calls
     */
    static bool CheckDoubleFree(
        const ComponentInfo& component,
        AllocationTracker& tracker
    );
};

} // namespace SafetyEvaluation
