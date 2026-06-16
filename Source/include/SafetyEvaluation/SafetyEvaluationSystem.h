// Copyright (C) 2026-2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "DataStructures.h"
#include "ComponentDiscovery.h"
#include "ReportGenerator.h"
#include "FunctionalityTester.h"
#include "StabilityTester.h"
#include "SaveIntegrityTester.h"
#include "MemorySafetyTester.h"
#include "PerformanceTester.h"
#include "IntegrationTester.h"
#include "RollbackTester.h"
#include "VEHTester.h"
#include "DynamicFixTester.h"
#include "PatternLearningTester.h"
#include "CompatibilityTester.h"
#include "RegressionDetector.h"
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace SafetyEvaluation {

/**
 * @brief Main orchestration class for the Safety Evaluation System
 * 
 * SafetyEvaluationSystem coordinates all three phases of the evaluation:
 * 1. Discovery Phase: Discover and inventory all components
 * 2. Testing Phase: Execute all tests on discovered components
 * 3. Reporting Phase: Generate comprehensive reports
 * 
 * Supports both full evaluation and regression-only modes for CI/CD integration.
 * 
 * Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 12.1, 12.2, 16.1, 16.2, 16.3
 */
class SafetyEvaluationSystem {
public:
    /**
     * @brief Configuration for the safety evaluation system
     */
    struct Configuration {
        std::string sourceDirectory;        ///< Source directory to scan for components
        std::string outputDirectory;        ///< Output directory for reports
        bool fullEvaluation = true;         ///< Run full evaluation (vs regression only)
        bool generateMarkdown = true;       ///< Generate Markdown reports
        bool generateJSON = true;           ///< Generate JSON reports
        bool verbose = false;               ///< Enable verbose logging
        std::string specificComponent;      ///< Test only this component (empty = all)
    };

    /**
     * @brief Construct a SafetyEvaluationSystem with configuration
     * @param config System configuration
     */
    explicit SafetyEvaluationSystem(const Configuration& config);

    /**
     * @brief Run complete safety evaluation
     * 
     * Executes all three phases: Discovery, Testing, and Reporting.
     * This is the main entry point for full system evaluation.
     * 
     * @return True if evaluation completed successfully
     * 
     * Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 12.1, 12.2
     */
    bool RunFullEvaluation();

    /**
     * @brief Run discovery phase to discover components
     * 
     * Scans the source directory to identify all CrashGuard components,
     * extract metadata, and classify them by defensive layer.
     * 
     * @return Vector of discovered components
     * 
     * Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6
     */
    std::vector<ComponentInfo> RunDiscoveryPhase();

    /**
     * @brief Run testing phase to execute all tests
     * 
     * Executes functionality, stability, safety, performance, and integration
     * tests on all discovered components.
     * 
     * @param components Components to test
     * @return Map of component name to component report
     * 
     * Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6
     */
    std::map<std::string, ComponentReport> RunTestingPhase(
        const std::vector<ComponentInfo>& components);

    /**
     * @brief Run reporting phase to generate reports
     * 
     * Generates summary and component reports in Markdown and/or JSON formats.
     * 
     * @param componentReports All component test reports
     * @return True if reports generated successfully
     * 
     * Requirements: 12.1, 12.2
     */
    bool RunReportingPhase(const std::map<std::string, ComponentReport>& componentReports);

    /**
     * @brief Run regression detection for CI/CD
     * 
     * Identifies changed components, runs tests only on those components,
     * compares with previous results, and generates regression report.
     * 
     * @return True if no regressions detected (CI/CD pass)
     * 
     * Requirements: 16.1, 16.2, 16.3
     */
    bool RunRegressionDetection();

    /**
     * @brief Set progress callback for reporting progress during execution
     * @param callback Function to call with progress messages
     */
    void SetProgressCallback(std::function<void(const std::string&)> callback);

private:
    /**
     * @brief Test a single component comprehensively
     * 
     * Runs all applicable tests on a component and generates a complete report.
     * 
     * @param component Component to test
     * @return Complete component report
     */
    ComponentReport TestComponent(const ComponentInfo& component);

    /**
     * @brief Report progress message
     * @param message Progress message
     */
    void ReportProgress(const std::string& message);

    /**
     * @brief Create output directory if it doesn't exist
     * @return True if directory exists or was created successfully
     */
    bool EnsureOutputDirectory();

    /**
     * @brief Filter components based on configuration
     * 
     * If specificComponent is set, returns only that component.
     * Otherwise returns all components.
     * 
     * @param components All discovered components
     * @return Filtered components
     */
    std::vector<ComponentInfo> FilterComponents(
        const std::vector<ComponentInfo>& components);

    /**
     * @brief Convert component reports map to vector
     * @param reportMap Map of component name to report
     * @return Vector of component reports
     */
    std::vector<ComponentReport> MapToVector(
        const std::map<std::string, ComponentReport>& reportMap);

    /**
     * @brief Convert component reports vector to map
     * @param reports Vector of component reports
     * @return Map of component name to report
     */
    std::map<std::string, std::vector<TestResult>> ReportsToTestResults(
        const std::vector<ComponentReport>& reports);

    Configuration m_config;                                     ///< System configuration
    ComponentDiscovery m_discovery;                             ///< Component discovery
    ReportGenerator m_reportGenerator;                          ///< Report generator
    std::function<void(const std::string&)> m_progressCallback; ///< Progress callback
};

} // namespace SafetyEvaluation
