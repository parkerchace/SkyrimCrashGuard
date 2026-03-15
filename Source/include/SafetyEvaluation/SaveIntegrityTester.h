// Copyright (C) 2024-2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "DataStructures.h"
#include <string>
#include <vector>
#include <filesystem>

namespace SafetyEvaluation {

/**
 * @brief Tests that components do not corrupt save files
 * 
 * The SaveIntegrityTester validates that components properly handle save files,
 * ensuring snapshots, rollbacks, and save operations maintain data integrity.
 * 
 * Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.8, 5.9
 */
class SaveIntegrityTester {
public:
    /**
     * @brief Test save integrity comprehensively
     * @param component Component to test
     * @return Save safety report with integrity test results
     * 
     * Requirement 5.9: Generate save safety rating
     */
    static SaveSafetyReport TestSaveIntegrity(const ComponentInfo& component);

    /**
     * @brief Verify StateManager snapshots can be captured without corrupting saves
     * @return True if snapshot capture is safe
     * 
     * Requirement 5.1: Test StateManager snapshot capture
     */
    static bool TestSnapshotCapture();

    /**
     * @brief Verify rollback preserves save file validity
     * @return True if rollback maintains save integrity
     * 
     * Requirement 5.2: Test StateManager rollback integrity
     */
    static bool TestRollbackIntegrity();

    /**
     * @brief Test SaveLoadResilience with corrupted save files
     * @return True if corrupted saves are handled safely
     * 
     * Requirement 5.3: Test SaveLoadResilience with corrupted saves
     */
    static bool TestCorruptedSaveHandling();

    /**
     * @brief Verify CoSaveManager writes valid cosave data
     * @return True if cosave writes are valid
     * 
     * Requirement 5.4: Verify CoSaveManager writes valid cosave data
     */
    static bool TestCoSaveValidity();

    /**
     * @brief Test that FormID modifications don't create invalid references
     * @return True if FormID operations are safe
     * 
     * Requirement 5.6: Test FormID modifications
     */
    static bool TestFormIDModifications();

    /**
     * @brief Verify save file headers remain valid after operations
     * @return True if save headers are preserved
     * 
     * Requirement 5.7: Verify save file headers remain valid
     */
    static bool TestSaveHeaderValidity();

    /**
     * @brief Test loading saves after component failures
     * @return True if saves remain loadable after failures
     * 
     * Requirement 5.8: Test save loading after component failures
     */
    static bool TestPostFailureLoading();

private:
    /**
     * @brief Create a test save file for testing
     * @param testName Name of the test (for unique file naming)
     * @return Path to created test save file
     */
    static std::filesystem::path CreateTestSaveFile(const std::string& testName);

    /**
     * @brief Validate a save file's integrity
     * @param savePath Path to save file
     * @return True if save file is valid
     */
    static bool ValidateSaveFile(const std::filesystem::path& savePath);

    /**
     * @brief Create a corrupted test save file
     * @param testName Name of the test
     * @return Path to corrupted test save file
     */
    static std::filesystem::path CreateCorruptedTestSave(const std::string& testName);

    /**
     * @brief Verify save file header structure
     * @param savePath Path to save file
     * @return True if header is valid
     */
    static bool VerifySaveHeader(const std::filesystem::path& savePath);

    /**
     * @brief Check if FormIDs in save are valid
     * @param savePath Path to save file
     * @return True if all FormIDs are valid
     */
    static bool VerifyFormIDs(const std::filesystem::path& savePath);

    /**
     * @brief Simulate a component failure during save operation
     * @param component Component to test
     * @return True if failure was simulated successfully
     */
    static bool SimulateComponentFailure(const ComponentInfo& component);

    /**
     * @brief Clean up test save files
     * @param savePath Path to test save file
     */
    static void CleanupTestSave(const std::filesystem::path& savePath);

    /**
     * @brief Get test save directory
     * @return Path to directory for test saves
     */
    static std::filesystem::path GetTestSaveDirectory();

    /**
     * @brief Create a minimal valid save file for testing
     * @param savePath Path where save should be created
     * @return True if save was created successfully
     */
    static bool CreateMinimalSaveFile(const std::filesystem::path& savePath);

    /**
     * @brief Corrupt specific parts of a save file
     * @param savePath Path to save file to corrupt
     * @param corruptionType Type of corruption to apply
     * @return True if corruption was applied successfully
     */
    static bool CorruptSaveFile(const std::filesystem::path& savePath, const std::string& corruptionType);
};

} // namespace SafetyEvaluation
