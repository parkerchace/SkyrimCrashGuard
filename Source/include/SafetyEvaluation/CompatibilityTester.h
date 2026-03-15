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
 * @brief Tests compatibility with different Skyrim versions
 * 
 * The CompatibilityTester validates that CrashGuard works correctly
 * across Skyrim SE 1.5.97, Skyrim VR, and Skyrim AE (latest), including
 * Address Library independence, pattern scanning fallback, hook resolution,
 * save file handling, and MO2 loading.
 * 
 * Requirements: 15.1, 15.2, 15.3, 15.4, 15.5, 15.6, 15.7, 15.8, 15.9, 15.10, 15.11
 */
class CompatibilityTester {
public:
    /**
     * @brief Test compatibility comprehensively
     * @param component Component to test
     * @return Compatibility report with all test results
     * 
     * Requirement 15.11: Generate compatibility matrix
     */
    static CompatibilityReport TestCompatibility(const ComponentInfo& component);

    /**
     * @brief Test component with Skyrim SE 1.5.97
     * @param component Component to test
     * @return True if component works on SE 1.5.97
     * 
     * Requirement 15.1: Test SE 1.5.97 compatibility
     */
    static bool TestSEVersion(const ComponentInfo& component);

    /**
     * @brief Test component with Skyrim VR
     * @param component Component to test
     * @return True if component works on VR
     * 
     * Requirement 15.2: Test VR compatibility
     */
    static bool TestVRVersion(const ComponentInfo& component);

    /**
     * @brief Test component with Skyrim AE (latest)
     * @param component Component to test
     * @return True if component works on AE
     * 
     * Requirement 15.3: Test AE compatibility
     */
    static bool TestAEVersion(const ComponentInfo& component);

    /**
     * @brief Test Address Library independence
     * @return True if system works without Address Library
     * 
     * Requirement 15.4: Test Address Library independence
     */
    static bool TestAddressLibIndependence();

    /**
     * @brief Test pattern scanning fallback
     * @return True if pattern scanning works on all versions
     * 
     * Requirement 15.5: Test pattern scanning fallback
     */
    static bool TestPatternScanningFallback();

    /**
     * @brief Test hook resolution on specific game version
     * @param version Game version to test
     * @return True if hooks resolve correctly
     * 
     * Requirement 15.6: Test hook resolution on all versions
     */
    static bool TestHookResolution(GameVersion version);

    /**
     * @brief Test save file handling across versions
     * @return True if save handling works on all versions
     * 
     * Requirement 15.7: Test save file handling across versions
     */
    static bool TestSaveFileHandling();

    /**
     * @brief Test MO2 loading on all versions
     * @return True if MO2 loading works on all versions
     * 
     * Requirement 15.8: Test MO2 loading on all versions
     */
    static bool TestMO2Loading();

    /**
     * @brief Test VR-specific features
     * @return True if VR features work correctly
     * 
     * Requirement 15.9: Test VR-specific features
     */
    static bool TestVRSpecificFeatures();

    /**
     * @brief Get game version name as string
     * @param version Game version
     * @return String representation of version
     */
    static std::string GetVersionName(GameVersion version);

private:
    /**
     * @brief Simulate component test on specific version
     * @param component Component to test
     * @param version Game version
     * @return True if test succeeded
     */
    static bool SimulateVersionTest(const ComponentInfo& component, GameVersion version);

    /**
     * @brief Verify component works on version
     * @param component Component to test
     * @param version Game version
     * @return True if component is compatible
     */
    static bool VerifyComponentCompatible(const ComponentInfo& component, GameVersion version);

    /**
     * @brief Simulate Address Library check
     * @return True if simulation succeeded
     */
    static bool SimulateAddressLibCheck();

    /**
     * @brief Verify system works without Address Library
     * @return True if independent
     */
    static bool VerifyAddressLibIndependent();

    /**
     * @brief Simulate pattern scanning
     * @param version Game version
     * @return True if simulation succeeded
     */
    static bool SimulatePatternScanning(GameVersion version);

    /**
     * @brief Verify pattern scanning works
     * @return True if patterns found
     */
    static bool VerifyPatternScanningWorks();

    /**
     * @brief Simulate hook resolution
     * @param version Game version
     * @return True if simulation succeeded
     */
    static bool SimulateHookResolution(GameVersion version);

    /**
     * @brief Verify hooks resolved correctly
     * @return True if all hooks resolved
     */
    static bool VerifyHooksResolved();

    /**
     * @brief Simulate save file operation
     * @param version Game version
     * @return True if simulation succeeded
     */
    static bool SimulateSaveFileOperation(GameVersion version);

    /**
     * @brief Verify save handling works
     * @return True if saves work correctly
     */
    static bool VerifySaveHandlingWorks();

    /**
     * @brief Simulate MO2 loading
     * @param version Game version
     * @return True if simulation succeeded
     */
    static bool SimulateMO2Loading(GameVersion version);

    /**
     * @brief Verify MO2 loading works
     * @return True if MO2 loads correctly
     */
    static bool VerifyMO2LoadingWorks();

    /**
     * @brief Simulate VR feature test
     * @return True if simulation succeeded
     */
    static bool SimulateVRFeatureTest();

    /**
     * @brief Verify VR features work
     * @return True if VR features functional
     */
    static bool VerifyVRFeaturesWork();

    /**
     * @brief Create version compatibility result
     * @param version Game version
     * @param works Whether component works
     * @param hooksResolve Whether hooks resolve
     * @param savesWork Whether saves work
     * @param issues List of issues
     * @return Populated VersionCompatibility
     */
    static VersionCompatibility CreateVersionResult(
        GameVersion version,
        bool works,
        bool hooksResolve,
        bool savesWork,
        const std::vector<std::string>& issues = {}
    );

    // Test state tracking
    static bool s_seVersionWorks;
    static bool s_vrVersionWorks;
    static bool s_aeVersionWorks;
    static bool s_addressLibIndependent;
    static bool s_patternScanningWorks;
    static bool s_hooksResolved;
    static bool s_saveHandlingWorks;
    static bool s_mo2LoadingWorks;
    static bool s_vrFeaturesWork;
};

} // namespace SafetyEvaluation
