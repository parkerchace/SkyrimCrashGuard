// Copyright (C) 2026-2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "DataStructures.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace SafetyEvaluation {

/**
 * @brief Generates comprehensive test reports in multiple formats
 * 
 * ReportGenerator creates summary reports, component-specific reports,
 * and actionable recommendations from test results. It outputs reports
 * in both human-readable (Markdown) and machine-readable (JSON) formats.
 */
class ReportGenerator {
public:
    /**
     * @brief Generates a summary report from all component reports
     * @param reports Vector of all component reports
     * @return SummaryReport containing overall statistics and critical issues
     */
    SummaryReport GenerateSummaryReport(const std::vector<ComponentReport>& reports);

    /**
     * @brief Generates a detailed report for a single component
     * @param component Component information
     * @param functionality Functionality test results
     * @param stability Stability test results
     * @param saveSafety Save safety test results
     * @param memorySafety Memory safety test results
     * @return ComponentReport with all test results and identified issues
     */
    ComponentReport GenerateComponentReport(
        const ComponentInfo& component,
        const FunctionalityReport& functionality,
        const StabilityReport& stability,
        const SaveSafetyReport& saveSafety,
        const MemorySafetyReport& memorySafety
    );

    /**
     * @brief Generates a Markdown report file
     * @param summary Summary report data
     * @param componentReports All component reports
     * @param outputPath Path to output Markdown file
     */
    void GenerateMarkdownReport(
        const SummaryReport& summary,
        const std::vector<ComponentReport>& componentReports,
        const std::string& outputPath
    );

    /**
     * @brief Generates a JSON report file
     * @param summary Summary report data
     * @param componentReports All component reports
     * @param outputPath Path to output JSON file
     */
    void GenerateJSONReport(
        const SummaryReport& summary,
        const std::vector<ComponentReport>& componentReports,
        const std::string& outputPath
    );

    /**
     * @brief Generates actionable recommendations from test results
     * @param componentReport Component report to analyze
     * @return Vector of recommendations with severity and descriptions
     */
    std::vector<Issue> GenerateRecommendations(const ComponentReport& componentReport);

private:
    /**
     * @brief Converts DefensiveLayer enum to string
     * @param layer Defensive layer enum value
     * @return String representation of the layer
     */
    std::string LayerToString(DefensiveLayer layer) const;

    /**
     * @brief Converts IssueSeverity enum to string
     * @param severity Issue severity enum value
     * @return String representation of the severity
     */
    std::string SeverityToString(IssueSeverity severity) const;

    /**
     * @brief Converts FunctionalityStatus enum to string
     * @param status Functionality status enum value
     * @return String representation of the status
     */
    std::string FunctionalityStatusToString(FunctionalityStatus status) const;

    /**
     * @brief Converts StabilityRating enum to string
     * @param rating Stability rating enum value
     * @return String representation of the rating
     */
    std::string StabilityRatingToString(StabilityRating rating) const;

    /**
     * @brief Converts SaveSafetyRating enum to string
     * @param rating Save safety rating enum value
     * @return String representation of the rating
     */
    std::string SaveSafetyRatingToString(SaveSafetyRating rating) const;

    /**
     * @brief Determines issue severity based on test results
     * @param componentReport Component report to analyze
     * @param testName Name of the failed test
     * @return Severity level for the issue
     */
    IssueSeverity DetermineIssueSeverity(
        const ComponentReport& componentReport,
        const std::string& testName
    ) const;

    /**
     * @brief Generates a recommendation message for a specific issue
     * @param componentReport Component report
     * @param testName Name of the failed test
     * @param severity Issue severity
     * @return Recommendation message
     */
    std::string GenerateRecommendationMessage(
        const ComponentReport& componentReport,
        const std::string& testName,
        IssueSeverity severity
    ) const;

    /**
     * @brief Writes Markdown section for summary statistics
     * @param summary Summary report data
     * @return Markdown string for summary section
     */
    std::string GenerateMarkdownSummary(const SummaryReport& summary) const;

    /**
     * @brief Writes Markdown section for critical issues
     * @param issues Vector of critical issues
     * @return Markdown string for critical issues section
     */
    std::string GenerateMarkdownCriticalIssues(const std::vector<Issue>& issues) const;

    /**
     * @brief Writes Markdown section for a single component
     * @param report Component report
     * @return Markdown string for component section
     */
    std::string GenerateMarkdownComponentSection(const ComponentReport& report) const;

    /**
     * @brief Converts summary report to JSON
     * @param summary Summary report data
     * @return JSON object
     */
    nlohmann::json SummaryToJSON(const SummaryReport& summary) const;

    /**
     * @brief Converts component report to JSON
     * @param report Component report
     * @return JSON object
     */
    nlohmann::json ComponentReportToJSON(const ComponentReport& report) const;

    /**
     * @brief Converts issue to JSON
     * @param issue Issue data
     * @return JSON object
     */
    nlohmann::json IssueToJSON(const Issue& issue) const;
};

} // namespace SafetyEvaluation
