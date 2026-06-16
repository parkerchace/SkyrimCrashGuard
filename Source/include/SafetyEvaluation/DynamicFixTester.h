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
 * @brief Tests dynamic fix applicator functionality
 * 
 * The DynamicFixTester validates that runtime fixes are applied correctly
 * and safely, including mesh replacement, animation switching, script disabling,
 * texture replacement, reference removal, fix persistence, and rollback.
 * 
 * Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8, 10.9
 */
class DynamicFixTester {
public:
    /**
     * @brief Test dynamic fixes comprehensively
     * @return Fix applicator report with all test results
     * 
     * Requirement 10.9: Generate fix applicator test report
     */
    static FixApplicatorReport TestDynamicFixes();

    /**
     * @brief Test mesh replacement functionality
     * @return True if mesh replacement works correctly
     * 
     * Requirement 10.1: Test mesh replacement with valid placeholders
     */
    static bool TestMeshReplacement();

    /**
     * @brief Test scene graph update functionality
     * @return True if scene graph updates complete successfully
     * 
     * Requirement 10.2: Test scene graph updates without dangling pointers
     */
    static bool TestSceneGraphUpdate();

    /**
     * @brief Test animation switching functionality
     * @return True if animation switching works correctly
     * 
     * Requirement 10.3: Test animation switching to default animations
     */
    static bool TestAnimationSwitching();

    /**
     * @brief Test script disabling functionality
     * @return True if script disabling works correctly
     * 
     * Requirement 10.4: Test script disabling stops execution
     */
    static bool TestScriptDisabling();

    /**
     * @brief Test texture replacement functionality
     * @return True if texture replacement works correctly
     * 
     * Requirement 10.5: Test texture replacement loads valid defaults
     */
    static bool TestTextureReplacement();

    /**
     * @brief Test reference removal functionality
     * @return True if reference removal cleans up correctly
     * 
     * Requirement 10.6: Test reference removal cleans up all references
     */
    static bool TestReferenceRemoval();

    /**
     * @brief Test fix persistence to save files
     * @return True if fixes persist correctly
     * 
     * Requirement 10.7: Test fix persistence to save files
     */
    static bool TestFixPersistence();

    /**
     * @brief Test fix rollback capability
     * @return True if fixes can be rolled back
     * 
     * Requirement 10.8: Test fix rollback capability
     */
    static bool TestFixRollback();

private:
    /**
     * @brief Simulate mesh replacement operation
     * @return True if simulation succeeded
     */
    static bool SimulateMeshReplacement();

    /**
     * @brief Verify mesh was replaced with valid placeholder
     * @return True if mesh is valid
     */
    static bool VerifyMeshValid();

    /**
     * @brief Simulate scene graph update
     * @return True if simulation succeeded
     */
    static bool SimulateSceneGraphUpdate();

    /**
     * @brief Check for dangling pointers in scene graph
     * @return True if no dangling pointers found
     */
    static bool CheckNoDanglingPointers();

    /**
     * @brief Simulate animation switch
     * @return True if simulation succeeded
     */
    static bool SimulateAnimationSwitch();

    /**
     * @brief Verify animation switched to default
     * @return True if using default animation
     */
    static bool VerifyDefaultAnimation();

    /**
     * @brief Simulate script disable
     * @return True if simulation succeeded
     */
    static bool SimulateScriptDisable();

    /**
     * @brief Verify script execution stopped
     * @return True if script is not executing
     */
    static bool VerifyScriptStopped();

    /**
     * @brief Simulate texture replacement
     * @return True if simulation succeeded
     */
    static bool SimulateTextureReplacement();

    /**
     * @brief Verify texture is valid default
     * @return True if texture is valid
     */
    static bool VerifyTextureValid();

    /**
     * @brief Simulate reference removal
     * @return True if simulation succeeded
     */
    static bool SimulateReferenceRemoval();

    /**
     * @brief Verify all references cleaned up
     * @return True if no references remain
     */
    static bool VerifyReferencesCleanedUp();

    /**
     * @brief Simulate fix persistence to save
     * @return True if simulation succeeded
     */
    static bool SimulateFixPersistence();

    /**
     * @brief Verify fix persisted to save file
     * @return True if fix is in save
     */
    static bool VerifyFixInSave();

    /**
     * @brief Simulate fix rollback
     * @return True if simulation succeeded
     */
    static bool SimulateFixRollback();

    /**
     * @brief Verify fix was rolled back
     * @return True if fix is no longer applied
     */
    static bool VerifyFixRolledBack();

    /**
     * @brief Create a fix test scenario for reporting
     * @param fixType Type of fix
     * @param fixApplied Whether fix was applied
     * @param fixPersisted Whether fix persisted
     * @param fixRolledBack Whether fix was rolled back
     * @param failureReason Reason for failure (if any)
     * @return Populated FixTestScenario
     */
    static FixTestScenario CreateScenario(
        FixType fixType,
        bool fixApplied,
        bool fixPersisted,
        bool fixRolledBack,
        const std::string& failureReason = ""
    );

    /**
     * @brief Get fix type name as string
     * @param fixType Fix type
     * @return String representation of fix type
     */
    static std::string GetFixTypeName(FixType fixType);

    // Test state tracking
    static bool s_meshReplaced;
    static bool s_sceneGraphUpdated;
    static bool s_animationSwitched;
    static bool s_scriptDisabled;
    static bool s_textureReplaced;
    static bool s_referencesRemoved;
    static bool s_fixPersisted;
    static bool s_fixRolledBack;
};

} // namespace SafetyEvaluation
