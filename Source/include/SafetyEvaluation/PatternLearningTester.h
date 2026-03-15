// Copyright (C) 2024-2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "DataStructures.h"
#include <string>

namespace SafetyEvaluation {

/**
 * @brief Tests pattern learning system functionality
 * 
 * The PatternLearningTester validates that the pattern learning system
 * correctly loads databases, records patterns, matches patterns, tracks
 * success rates, handles corruption, and improves over time.
 * 
 * Requirements: 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 11.7, 11.8, 11.9
 */
class PatternLearningTester {
public:
    /**
     * @brief Test pattern learning comprehensively
     * @return Pattern learning report with all test results
     * 
     * Requirement 11.9: Generate pattern learning test report
     */
    static PatternLearningReport TestPatternLearning();

    /**
     * @brief Test pattern database loading
     * @return True if database loads correctly
     * 
     * Requirement 11.1: Test database loading
     */
    static bool TestDatabaseLoading();

    /**
     * @brief Test pattern recording after crashes
     * @return True if patterns are recorded correctly
     * 
     * Requirement 11.2: Test pattern recording after crashes
     */
    static bool TestPatternRecording();

    /**
     * @brief Test pattern matching accuracy
     * @return True if pattern matching works correctly
     * 
     * Requirement 11.3: Test pattern matching accuracy
     */
    static bool TestPatternMatching();

    /**
     * @brief Test success rate tracking
     * @return True if success rates are tracked correctly
     * 
     * Requirement 11.4: Test success rate tracking
     */
    static bool TestSuccessRateTracking();

    /**
     * @brief Test database writes
     * @return True if database writes succeed
     * 
     * Requirement 11.5: Test database writes
     */
    static bool TestDatabaseWrites();

    /**
     * @brief Test database corruption recovery
     * @return True if corruption recovery works
     * 
     * Requirement 11.6: Test database corruption recovery
     */
    static bool TestCorruptionRecovery();

    /**
     * @brief Test learning improvement over time
     * @return True if learning improves
     * 
     * Requirement 11.7: Test learning improvement over time
     */
    static bool TestLearningImprovement();

    /**
     * @brief Test pattern export and import
     * @return True if export/import works correctly
     * 
     * Requirement 11.8: Test pattern export/import
     */
    static bool TestExportImport();

private:
    /**
     * @brief Simulate database loading
     * @return True if simulation succeeded
     */
    static bool SimulateDatabaseLoad();

    /**
     * @brief Verify database loaded correctly
     * @return True if database is valid
     */
    static bool VerifyDatabaseLoaded();

    /**
     * @brief Simulate pattern recording
     * @return True if simulation succeeded
     */
    static bool SimulatePatternRecord();

    /**
     * @brief Verify pattern was recorded
     * @return True if pattern exists in database
     */
    static bool VerifyPatternRecorded();

    /**
     * @brief Simulate pattern matching
     * @return True if simulation succeeded
     */
    static bool SimulatePatternMatch();

    /**
     * @brief Calculate matching accuracy
     * @return Accuracy percentage (0.0 to 1.0)
     */
    static double CalculateMatchingAccuracy();

    /**
     * @brief Simulate success rate update
     * @return True if simulation succeeded
     */
    static bool SimulateSuccessRateUpdate();

    /**
     * @brief Verify success rate tracked
     * @return True if success rate is correct
     */
    static bool VerifySuccessRateTracked();

    /**
     * @brief Simulate database write
     * @return True if simulation succeeded
     */
    static bool SimulateDatabaseWrite();

    /**
     * @brief Verify database write succeeded
     * @return True if data was written
     */
    static bool VerifyDatabaseWritten();

    /**
     * @brief Simulate database corruption
     * @return True if simulation succeeded
     */
    static bool SimulateDatabaseCorruption();

    /**
     * @brief Simulate corruption recovery
     * @return True if simulation succeeded
     */
    static bool SimulateCorruptionRecovery();

    /**
     * @brief Verify database recovered
     * @return True if database is valid after recovery
     */
    static bool VerifyDatabaseRecovered();

    /**
     * @brief Simulate learning over multiple iterations
     * @return True if simulation succeeded
     */
    static bool SimulateLearningIterations();

    /**
     * @brief Calculate learning improvement rate
     * @return Improvement rate (0.0 to 1.0)
     */
    static double CalculateLearningImprovement();

    /**
     * @brief Simulate pattern export
     * @return True if simulation succeeded
     */
    static bool SimulatePatternExport();

    /**
     * @brief Simulate pattern import
     * @return True if simulation succeeded
     */
    static bool SimulatePatternImport();

    /**
     * @brief Verify export/import preserved data
     * @return True if data is intact
     */
    static bool VerifyExportImportIntegrity();

    // Test state tracking
    static bool s_databaseLoaded;
    static bool s_patternRecorded;
    static bool s_patternMatched;
    static bool s_successRateTracked;
    static bool s_databaseWritten;
    static bool s_databaseCorrupted;
    static bool s_databaseRecovered;
    static bool s_learningImproved;
    static bool s_patternsExported;
    static bool s_patternsImported;
    static int s_patternsInDatabase;
    static int s_matchAttempts;
    static int s_successfulMatches;
    static double s_initialAccuracy;
    static double s_finalAccuracy;
};

} // namespace SafetyEvaluation
