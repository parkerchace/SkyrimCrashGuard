// Copyright (C) 2024-2026 Parker Chace
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
 * @brief Tests how components work together
 * 
 * The IntegrationTester validates that components interact correctly,
 * testing complete workflows through multiple layers, component pairs,
 * and failure isolation to ensure the system functions as a cohesive whole.
 * 
 * Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8, 6.9
 */
class IntegrationTester {
public:
    /**
     * @brief Test integration comprehensively
     * @return Integration report with all test results
     * 
     * Requirement 6.9: Generate integration test report
     */
    static IntegrationReport TestIntegration();

    /**
     * @brief Test complete crash recovery flow through all layers
     * @return True if crash recovery flow works correctly
     * 
     * Requirement 6.1: Test complete crash recovery flow from VEH through all layers
     */
    static bool TestCrashRecoveryFlow();

    /**
     * @brief Test interaction between specific layer pairs
     * @param layer1 First defensive layer
     * @param layer2 Second defensive layer
     * @return True if layers interact correctly
     * 
     * Requirement 6.2: Test Layer 1 validation with Layer 4 fixes
     * Requirement 6.3: Test specific layer interactions
     */
    static bool TestLayerInteraction(DefensiveLayer layer1, DefensiveLayer layer2);

    /**
     * @brief Test two components working together
     * @param comp1 First component
     * @param comp2 Second component
     * @return True if components work together correctly
     * 
     * Requirement 6.3: Test StateManager with RealTimeFixApplicator
     * Requirement 6.4: Test PatternLearningSystem with DynamicFixApplicator
     */
    static bool TestComponentPair(const ComponentInfo& comp1, const ComponentInfo& comp2);

    /**
     * @brief Test multiple validators running simultaneously
     * @return True if validators work without conflicts
     * 
     * Requirement 6.5: Test multiple validators simultaneously
     */
    static bool TestMultipleValidators();

    /**
     * @brief Test component initialization order
     * @return True if dependencies are initialized in correct order
     * 
     * Requirement 6.7: Test component initialization order
     */
    static bool TestInitializationOrder();

    /**
     * @brief Test that component failures don't cascade
     * @return True if failures are properly isolated
     * 
     * Requirement 6.8: Verify component failures don't cascade
     */
    static bool TestFailureIsolation();

    /**
     * @brief Test VEH with StateManager snapshot timing
     * @return True if snapshot timing is correct
     * 
     * Requirement 6.6: Test VEH with StateManager snapshot timing
     */
    static bool TestVEHWithStateManagerTiming();

    /**
     * @brief Get component initialization dependencies
     * @param componentName Name of component
     * @return List of dependencies
     */
    static std::vector<std::string> GetComponentDependencies(const std::string& componentName);

    /**
     * @brief Get layer name as string
     * @param layer Defensive layer
     * @return String representation of layer
     */
    static std::string GetLayerName(DefensiveLayer layer);

private:
    /**
     * @brief Simulate VEH catching an exception
     * @return True if VEH simulation succeeded
     */
    static bool SimulateVEHException();

    /**
     * @brief Simulate Layer 1 validation
     * @return True if validation simulation succeeded
     */
    static bool SimulateLayer1Validation();

    /**
     * @brief Simulate Layer 2 safety checks
     * @return True if safety check simulation succeeded
     */
    static bool SimulateLayer2SafetyChecks();

    /**
     * @brief Simulate Layer 3 VEH handling
     * @return True if VEH handling simulation succeeded
     */
    static bool SimulateLayer3VEH();

    /**
     * @brief Simulate Layer 4 dynamic fix application
     * @return True if fix application simulation succeeded
     */
    static bool SimulateLayer4DynamicFix();

    /**
     * @brief Simulate Layer 5 state management
     * @return True if state management simulation succeeded
     */
    static bool SimulateLayer5StateManagement();

    /**
     * @brief Simulate Layer 6 pattern learning
     * @return True if pattern learning simulation succeeded
     */
    static bool SimulateLayer6PatternLearning();

    /**
     * @brief Verify crash was recovered successfully
     * @return True if recovery was successful
     */
    static bool VerifyCrashRecovered();

    /**
     * @brief Test Layer 1 (Validation) with Layer 4 (Dynamic Fixes)
     * @return True if layers work together correctly
     */
    static bool TestLayer1WithLayer4();

    /**
     * @brief Test StateManager with RealTimeFixApplicator
     * @return True if components work together correctly
     */
    static bool TestStateManagerWithFixApplicator();

    /**
     * @brief Test PatternLearningSystem with DynamicFixApplicator
     * @return True if components work together correctly
     */
    static bool TestPatternLearningWithDynamicFix();

    /**
     * @brief Simulate multiple validators running concurrently
     * @return True if all validators completed successfully
     */
    static bool SimulateMultipleValidators();

    /**
     * @brief Check for conflicts between validators
     * @return True if no conflicts detected
     */
    static bool CheckValidatorConflicts();

    /**
     * @brief Verify initialization order is correct
     * @param initOrder Actual initialization order
     * @return True if order respects dependencies
     */
    static bool VerifyInitializationOrder(const std::vector<std::string>& initOrder);

    /**
     * @brief Simulate a component failure
     * @param componentName Name of component to fail
     * @return True if failure was simulated
     */
    static bool SimulateComponentFailure(const std::string& componentName);

    /**
     * @brief Check if failure cascaded to other components
     * @param failedComponent Name of failed component
     * @return True if no cascade detected
     */
    static bool CheckNoCascade(const std::string& failedComponent);

    /**
     * @brief Create an integration scenario for reporting
     * @param name Scenario name
     * @param components Components involved
     * @param description Scenario description
     * @param passed Whether scenario passed
     * @param failureReason Reason for failure (if any)
     * @return Populated IntegrationScenario
     */
    static IntegrationScenario CreateScenario(
        const std::string& name,
        const std::vector<std::string>& components,
        const std::string& description,
        bool passed,
        const std::string& failureReason = ""
    );

    // Test state tracking
    static bool s_vehTriggered;
    static bool s_validationPassed;
    static bool s_safetyCheckPassed;
    static bool s_fixApplied;
    static bool s_stateManaged;
    static bool s_patternLearned;
    static int s_validatorsRunning;
    static std::vector<std::string> s_failedComponents;
};

} // namespace SafetyEvaluation
