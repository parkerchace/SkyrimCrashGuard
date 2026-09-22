// Copyright (C) 2024-2026 Parker Chace
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of Skyrim Crash Guard.
//
// Skyrim Crash Guard is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version.
//
// Skyrim Crash Guard is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "DataStructures.h"
#include <string>
#include <chrono>

namespace SafetyEvaluation {

/**
 * @brief Tests component performance impact
 * 
 * The PerformanceTester measures execution time, memory overhead, and validates
 * that components don't exceed acceptable performance thresholds (>1% overhead).
 * 
 * Requirements: 14.1, 14.2, 14.3, 14.4, 14.5, 14.6, 14.7, 14.8, 14.9, 14.10
 */
class PerformanceTester {
public:
    /**
     * @brief Test component performance comprehensively
     * @param component Component to test
     * @return Performance report with timing and overhead data
     * 
     * Requirement 14.10: Generate performance report
     */
    static PerformanceReport TestPerformance(const ComponentInfo& component);

    /**
     * @brief Measure component execution time
     * @param component Component to test
     * @return Execution time in milliseconds
     * 
     * Requirement 14.1: Measure execution time
     */
    static double MeasureExecutionTime(const ComponentInfo& component);

    /**
     * @brief Measure memory overhead introduced by component
     * @param component Component to test
     * @return Memory overhead in bytes
     * 
     * Requirement 14.2: Measure memory overhead
     */
    static size_t MeasureMemoryOverhead(const ComponentInfo& component);

    /**
     * @brief Measure validation overhead for validation components
     * @param component Component to test
     * @return Overhead percentage added to validated operations
     * 
     * Requirement 14.3: Measure validation overhead
     */
    static double MeasureValidationOverhead(const ComponentInfo& component);

    /**
     * @brief Compare component performance against baseline
     * @param component Component to test
     * @return Overhead percentage compared to game without CrashGuard
     * 
     * Requirement 14.7: Compare against baseline
     */
    static double CompareToBaseline(const ComponentInfo& component);

    /**
     * @brief Test component under various load conditions
     * @param component Component to test
     * @param load Load level to test
     * @return Performance metrics for the specified load
     * 
     * Requirement 14.9: Test under various load conditions
     */
    static PerformanceMetrics TestUnderLoad(const ComponentInfo& component, LoadLevel load);

    /**
     * @brief Identify if component exceeds acceptable threshold
     * @param overheadPercentage Measured overhead percentage
     * @return True if component exceeds 1% overhead threshold
     * 
     * Requirement 14.8: Identify components exceeding threshold
     */
    static bool ExceedsThreshold(double overheadPercentage);

private:
    /**
     * @brief Baseline performance (game without CrashGuard)
     */
    static constexpr double BASELINE_EXECUTION_TIME_MS = 16.67; // 60 FPS frame time

    /**
     * @brief Acceptable overhead threshold (1%)
     */
    static constexpr double OVERHEAD_THRESHOLD = 0.01;

    /**
     * @brief Number of iterations for accurate measurement
     */
    static constexpr int MEASUREMENT_ITERATIONS = 100;

    /**
     * @brief Simulate component operation for timing
     */
    static void SimulateComponentOperation(const ComponentInfo& component);

    /**
     * @brief Get load multiplier for different load levels
     */
    static int GetLoadMultiplier(LoadLevel load);

    /**
     * @brief Calculate overhead percentage
     */
    static double CalculateOverheadPercentage(double componentTime, double baselineTime);
};

} // namespace SafetyEvaluation
