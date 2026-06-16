// Copyright (C) 2026-2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "DataStructures.h"
#include <string>
#include <vector>

namespace SafetyEvaluation {

/**
 * @brief Generates automated test suites for CrashGuard components
 * 
 * The TestSuiteGenerator creates unit tests, integration tests, test fixtures,
 * and mock objects in the Catch2 testing framework. Generated tests include
 * documentation, positive and negative test cases, and are organized by
 * component and category.
 * 
 * Requirements: 13.1, 13.2, 13.3, 13.4, 13.5, 13.6, 13.7, 13.8
 */
class TestSuiteGenerator {
public:
    /**
     * @brief Generate a complete test suite for all components
     * 
     * Creates unit tests, integration tests, fixtures, and mocks for all
     * discovered components. Tests are organized by component and category.
     * 
     * @param components List of components to generate tests for
     * @return Complete test suite with all generated tests
     * 
     * Requirements: 13.1, 13.6
     */
    static TestSuite GenerateTestSuite(const std::vector<ComponentInfo>& components);

    /**
     * @brief Generate unit test for a specific component
     * 
     * Creates a Catch2 unit test that validates the component's core
     * functionality. Includes positive tests (correct behavior) and
     * negative tests (error handling).
     * 
     * @param component Component to generate test for
     * @return Generated unit test code
     * 
     * Requirements: 13.1, 13.4, 13.5
     */
    static std::string GenerateUnitTest(const ComponentInfo& component);

    /**
     * @brief Generate integration test for component interactions
     * 
     * Creates a Catch2 integration test that validates how components
     * work together. Tests layer interactions and complete workflows.
     * 
     * @param components Components involved in integration test
     * @return Generated integration test code
     * 
     * Requirements: 13.2, 13.4, 13.5
     */
    static std::string GenerateIntegrationTest(const std::vector<ComponentInfo>& components);

    /**
     * @brief Generate test fixture for a component
     * 
     * Creates a test fixture class that provides setup and teardown
     * functionality for component tests. Includes mock data and helper methods.
     * 
     * @param component Component to generate fixture for
     * @return Generated test fixture code
     * 
     * Requirements: 13.7
     */
    static std::string GenerateTestFixture(const ComponentInfo& component);

    /**
     * @brief Generate mock object for a component
     * 
     * Creates a mock implementation of a component for use in testing.
     * Mocks provide predictable behavior for testing dependent components.
     * 
     * @param component Component to generate mock for
     * @return Generated mock object code
     * 
     * Requirements: 13.7
     */
    static std::string GenerateMockObject(const ComponentInfo& component);

private:
    /**
     * @brief Generate test documentation comment
     * 
     * Creates a documentation comment explaining what the test validates
     * and which requirements it covers.
     * 
     * @param testName Name of the test
     * @param description Description of what the test validates
     * @param requirements Requirements covered by the test
     * @return Generated documentation comment
     * 
     * Requirements: 13.4
     */
    static std::string GenerateTestDocumentation(
        const std::string& testName,
        const std::string& description,
        const std::vector<std::string>& requirements);

    /**
     * @brief Generate positive test cases
     * 
     * Creates test cases that verify correct behavior with valid inputs.
     * 
     * @param component Component to generate tests for
     * @return Generated positive test code
     * 
     * Requirements: 13.5
     */
    static std::string GeneratePositiveTests(const ComponentInfo& component);

    /**
     * @brief Generate negative test cases
     * 
     * Creates test cases that verify error handling with invalid inputs.
     * 
     * @param component Component to generate tests for
     * @return Generated negative test code
     * 
     * Requirements: 13.5
     */
    static std::string GenerateNegativeTests(const ComponentInfo& component);

    /**
     * @brief Generate layer-specific tests
     * 
     * Creates tests specific to the component's defensive layer.
     * 
     * @param component Component to generate tests for
     * @return Generated layer-specific test code
     * 
     * Requirements: 13.1, 13.2
     */
    static std::string GenerateLayerSpecificTests(const ComponentInfo& component);

    /**
     * @brief Generate test includes and setup
     * 
     * Creates the necessary includes and setup code for a test file.
     * 
     * @param component Component being tested
     * @return Generated includes and setup code
     * 
     * Requirements: 13.3, 13.8
     */
    static std::string GenerateTestIncludes(const ComponentInfo& component);

    /**
     * @brief Sanitize name for use in test code
     * 
     * Converts a component name to a valid C++ identifier.
     * 
     * @param name Name to sanitize
     * @return Sanitized name
     */
    static std::string SanitizeName(const std::string& name);

    /**
     * @brief Get test category tags for a component
     * 
     * Returns Catch2 tags based on the component's layer and characteristics.
     * 
     * @param component Component to get tags for
     * @return Catch2 tag string
     * 
     * Requirements: 13.6
     */
    static std::string GetTestTags(const ComponentInfo& component);
};

} // namespace SafetyEvaluation
