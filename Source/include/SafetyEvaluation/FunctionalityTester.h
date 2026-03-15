// Copyright (C) 2024-2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "DataStructures.h"
#include <memory>
#include <functional>

namespace SafetyEvaluation {

/**
 * @brief Tests component functionality to verify correct operation
 * 
 * The FunctionalityTester validates that each CrashGuard component performs
 * its intended function correctly. It generates appropriate test cases based
 * on component type, executes tests with valid and invalid inputs, and
 * produces detailed functionality reports.
 * 
 * Requirements: 3.1-3.10
 */
class FunctionalityTester {
public:
    /**
     * @brief Test a component's functionality
     * @param component Component to test
     * @return Functionality report with test results
     * 
     * Requirements: 3.1, 3.2, 3.3, 3.10
     */
    static FunctionalityReport TestComponent(const ComponentInfo& component);

    /**
     * @brief Generate test cases for a component based on its type
     * @param component Component to generate tests for
     * @return Vector of test cases
     * 
     * Requirements: 3.4, 3.5
     */
    static std::vector<TestCase> GenerateTestCases(const ComponentInfo& component);

    /**
     * @brief Execute a single test case
     * @param testCase Test case to execute
     * @param component Component being tested
     * @return Test result
     * 
     * Requirements: 3.5
     */
    static TestResult ExecuteTest(const TestCase& testCase, const ComponentInfo& component);

    /**
     * @brief Test a validation component (Layer 1)
     * @param component Validation component to test
     * @return True if validation component works correctly
     * 
     * Tests that the component correctly identifies invalid data and
     * accepts valid data.
     * 
     * Requirements: 3.1, 3.2
     */
    static bool TestValidationComponent(const ComponentInfo& component);

    /**
     * @brief Test a recovery component (Layers 3-4)
     * @param component Recovery component to test
     * @return True if recovery component works correctly
     * 
     * Tests that the component successfully handles crash scenarios
     * and applies appropriate fixes.
     * 
     * Requirements: 3.3
     */
    static bool TestRecoveryComponent(const ComponentInfo& component);

    /**
     * @brief Test MeshValidator component specifically
     * @param component MeshValidator component
     * @return True if mesh validation works correctly
     * 
     * Requirements: 3.6
     */
    static bool TestMeshValidator(const ComponentInfo& component);

    /**
     * @brief Test AnimationHandler component specifically
     * @param component AnimationHandler component
     * @return True if animation handling works correctly
     * 
     * Requirements: 3.7
     */
    static bool TestAnimationHandler(const ComponentInfo& component);

    /**
     * @brief Test ScriptMonitor component specifically
     * @param component ScriptMonitor component
     * @return True if script monitoring works correctly
     * 
     * Requirements: 3.8
     */
    static bool TestScriptMonitor(const ComponentInfo& component);

    /**
     * @brief Test CellManager component specifically
     * @param component CellManager component
     * @return True if cell management works correctly
     * 
     * Requirements: 3.9
     */
    static bool TestCellManager(const ComponentInfo& component);

private:
    /**
     * @brief Generate test cases for validation components
     * @param component Validation component
     * @return Vector of test cases
     */
    static std::vector<TestCase> GenerateValidationTests(const ComponentInfo& component);

    /**
     * @brief Generate test cases for recovery components
     * @param component Recovery component
     * @return Vector of test cases
     */
    static std::vector<TestCase> GenerateRecoveryTests(const ComponentInfo& component);

    /**
     * @brief Generate test cases for state management components
     * @param component State management component
     * @return Vector of test cases
     */
    static std::vector<TestCase> GenerateStateManagementTests(const ComponentInfo& component);

    /**
     * @brief Generate test cases for pattern learning components
     * @param component Pattern learning component
     * @return Vector of test cases
     */
    static std::vector<TestCase> GeneratePatternLearningTests(const ComponentInfo& component);

    /**
     * @brief Create a test case for valid input
     * @param name Test name
     * @param description Test description
     * @return Test case
     */
    static TestCase CreateValidInputTest(const std::string& name, const std::string& description);

    /**
     * @brief Create a test case for invalid input
     * @param name Test name
     * @param description Test description
     * @return Test case
     */
    static TestCase CreateInvalidInputTest(const std::string& name, const std::string& description);

    /**
     * @brief Create a test case for edge case input
     * @param name Test name
     * @param description Test description
     * @return Test case
     */
    static TestCase CreateEdgeCaseTest(const std::string& name, const std::string& description);

    /**
     * @brief Determine functionality status from test results
     * @param results Vector of test results
     * @return Functionality status
     */
    static FunctionalityStatus DetermineFunctionalityStatus(const std::vector<TestResult>& results);

    /**
     * @brief Check if component name matches a pattern
     * @param componentName Component name to check
     * @param pattern Pattern to match (case-insensitive)
     * @return True if name matches pattern
     */
    static bool ComponentNameMatches(const std::string& componentName, const std::string& pattern);
};

} // namespace SafetyEvaluation
