// Copyright (C) 2026-2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "DataStructures.h"
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace SafetyEvaluation {

/**
 * @brief Detects regressions in continuous testing
 * 
 * The RegressionDetector identifies changed and new components, compares
 * test results with previous runs, and generates regression reports for
 * CI/CD integration. It tracks test history to identify flaky tests and
 * integrates with version control systems.
 * 
 * Requirements: 16.1, 16.2, 16.3, 16.4, 16.5, 16.6, 16.7, 16.8
 */
class RegressionDetector {
public:
    /**
     * @brief Detect regressions by comparing current and previous test results
     * 
     * Identifies changed and new components, compares test results, and
     * generates a comprehensive regression report with CI/CD status.
     * 
     * @param components Current list of components
     * @return Regression report with detected regressions and CI/CD status
     * 
     * Requirements: 16.1, 16.4, 16.5, 16.7
     */
    static RegressionReport DetectRegressions(const std::vector<ComponentInfo>& components);

    /**
     * @brief Identify components that have been modified since last run
     * 
     * Uses version control integration to detect which components have
     * changed and need to be re-tested.
     * 
     * @return List of modified components
     * 
     * Requirements: 16.1, 16.3, 16.6
     */
    static std::vector<ComponentInfo> IdentifyChangedComponents();

    /**
     * @brief Identify new components that were added since last run
     * 
     * Detects components that don't exist in the previous test results
     * and need initial testing.
     * 
     * @return List of new components
     * 
     * Requirements: 16.2
     */
    static std::vector<ComponentInfo> IdentifyNewComponents();

    /**
     * @brief Compare current test results with previous results
     * 
     * Identifies tests that previously passed but now fail (regressions)
     * and tests that previously failed but now pass (fixes).
     * 
     * @param currentResults Current test results
     * @param previousResults Previous test results
     * @return List of detected regressions
     * 
     * Requirements: 16.4, 16.5
     */
    static std::vector<Regression> CompareResults(
        const std::map<std::string, std::vector<TestResult>>& currentResults,
        const std::map<std::string, std::vector<TestResult>>& previousResults);

    /**
     * @brief Integrate with version control system
     * 
     * Queries the VCS (Git) to identify changed files and components.
     * Returns true if VCS integration is successful.
     * 
     * @return True if VCS integration successful, false otherwise
     * 
     * Requirements: 16.6
     */
    static bool IntegrateWithVCS();

    /**
     * @brief Identify flaky tests by tracking test history
     * 
     * Analyzes test history to find tests that intermittently pass and fail
     * without code changes, indicating flaky behavior.
     * 
     * @return List of flaky test names
     * 
     * Requirements: 16.8
     */
    static std::vector<std::string> IdentifyFlakyTests();

    /**
     * @brief Load previous test results from disk
     * 
     * Reads the previous test results from a JSON file for comparison.
     * 
     * @param filePath Path to previous results file
     * @return Map of component name to test results, or empty if file doesn't exist
     * 
     * Requirements: 16.4
     */
    static std::map<std::string, std::vector<TestResult>> LoadPreviousResults(
        const std::string& filePath);

    /**
     * @brief Save current test results to disk
     * 
     * Writes the current test results to a JSON file for future comparison.
     * 
     * @param results Current test results
     * @param filePath Path to save results file
     * 
     * Requirements: 16.4
     */
    static void SaveCurrentResults(
        const std::map<std::string, std::vector<TestResult>>& results,
        const std::string& filePath);

    /**
     * @brief Load test history from disk
     * 
     * Reads the test history used for flaky test detection.
     * 
     * @param filePath Path to test history file
     * @return Test history data
     * 
     * Requirements: 16.8
     */
    static std::map<std::string, std::vector<bool>> LoadTestHistory(
        const std::string& filePath);

    /**
     * @brief Update test history with current results
     * 
     * Appends current test results to the test history for flaky test detection.
     * 
     * @param history Current test history
     * @param results Current test results
     * @param filePath Path to save updated history
     * 
     * Requirements: 16.8
     */
    static void UpdateTestHistory(
        std::map<std::string, std::vector<bool>>& history,
        const std::map<std::string, std::vector<TestResult>>& results,
        const std::string& filePath);

private:
    /**
     * @brief Get list of changed files from Git
     * 
     * Executes git commands to identify changed files since last commit.
     * 
     * @return List of changed file paths
     * 
     * Requirements: 16.6
     */
    static std::vector<std::string> GetChangedFilesFromGit();

    /**
     * @brief Check if a component file has changed
     * 
     * Determines if a component's source file is in the list of changed files.
     * 
     * @param component Component to check
     * @param changedFiles List of changed file paths
     * @return True if component has changed
     * 
     * Requirements: 16.1, 16.3
     */
    static bool IsComponentChanged(
        const ComponentInfo& component,
        const std::vector<std::string>& changedFiles);

    /**
     * @brief Calculate flakiness score for a test
     * 
     * Analyzes test history to determine how often a test changes state
     * without code changes. Higher score indicates more flakiness.
     * 
     * @param testHistory History of pass/fail results for a test
     * @return Flakiness score (0.0 = stable, 1.0 = very flaky)
     * 
     * Requirements: 16.8
     */
    static double CalculateFlakinessScore(const std::vector<bool>& testHistory);

    /**
     * @brief Determine CI/CD status based on regressions
     * 
     * Returns true (pass) if no regressions detected, false (fail) otherwise.
     * 
     * @param regressions List of detected regressions
     * @return CI/CD status (true = pass, false = fail)
     * 
     * Requirements: 16.7
     */
    static bool DetermineCICDStatus(const std::vector<Regression>& regressions);

    // Default file paths for persistence
    static constexpr const char* DEFAULT_RESULTS_FILE = "safety_evaluation_results.json";
    static constexpr const char* DEFAULT_HISTORY_FILE = "test_history.json";
};

} // namespace SafetyEvaluation
