// Copyright (C) 2024-2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <string>
#include <vector>
#include <map>
#include <any>
#include <cstdint>

namespace SafetyEvaluation {

// ============================================================================
// Component Discovery Data Structures
// ============================================================================

/**
 * @brief Represents the defensive layer a component belongs to
 */
enum class DefensiveLayer {
    ProactiveValidation,    ///< Layer 1: Proactive validation components
    SafetyChecks,           ///< Layer 2: Safety check components
    VEH,                    ///< Layer 3: Vectored Exception Handler
    DynamicFixes,           ///< Layer 4: Dynamic fix application
    StateManagement,        ///< Layer 5: State management and rollback
    PatternLearning         ///< Layer 6: Pattern learning system
};

/**
 * @brief Information about a discovered component
 */
struct ComponentInfo {
    std::string name;                           ///< Component name
    std::string filePath;                       ///< Path to component source file
    DefensiveLayer layer;                       ///< Defensive layer classification
    std::vector<std::string> dependencies;      ///< Component dependencies
    bool interactsWithSaveFiles;                ///< Whether component interacts with save files
    bool modifiesGameMemory;                    ///< Whether component modifies game memory
    std::vector<std::string> publicMethods;     ///< Public methods exposed by component
    std::vector<std::string> saveFileOperations;///< Save file operations performed
};

// ============================================================================
// Save Corruption Analysis Data Structures
// ============================================================================

/**
 * @brief Risk rating for save file corruption
 */
enum class SaveRiskRating {
    None,           ///< No save file interaction
    Low,            ///< Read-only or well-validated writes
    Medium,         ///< Writes with some validation
    High,           ///< Writes with minimal validation
    Critical        ///< Writes that modify FormIDs or lack validation
};

/**
 * @brief Information about a save file operation
 */
struct SaveOperation {
    std::string operationName;              ///< Name of the operation
    std::string codeLocation;               ///< Location in code (file:line)
    bool hasValidation;                     ///< Whether operation validates data
    bool hasErrorHandling;                  ///< Whether operation has error handling
    bool hasRollback;                       ///< Whether operation supports rollback
    std::vector<std::string> dataModified;  ///< Types of data modified
};

// ============================================================================
// Functionality Testing Data Structures
// ============================================================================

/**
 * @brief Status of component functionality
 */
enum class FunctionalityStatus {
    Working,            ///< All tests pass
    PartiallyWorking,   ///< Some tests pass
    NotWorking          ///< All tests fail
};

/**
 * @brief A test case for functionality testing
 */
struct TestCase {
    std::string name;                   ///< Test case name
    std::string description;            ///< Test case description
    std::vector<std::any> inputs;       ///< Test inputs
    std::any expectedOutput;            ///< Expected output
    bool expectException;               ///< Whether an exception is expected
};

/**
 * @brief Result of a single test execution
 */
struct TestResult {
    std::string testName;       ///< Name of the test
    bool passed;                ///< Whether the test passed
    std::string failureReason;  ///< Reason for failure (if applicable)
    double executionTimeMs;     ///< Execution time in milliseconds
};

/**
 * @brief Report of component functionality testing
 */
struct FunctionalityReport {
    std::string componentName;              ///< Component being tested
    FunctionalityStatus status;             ///< Overall functionality status
    std::vector<TestResult> testResults;    ///< Individual test results
    int totalTests;                         ///< Total number of tests
    int passedTests;                        ///< Number of passed tests
    int failedTests;                        ///< Number of failed tests
};

// ============================================================================
// Stability Testing Data Structures
// ============================================================================

/**
 * @brief Stability rating for a component
 */
enum class StabilityRating {
    Unstable,           ///< Crashes or fails frequently
    ModeratelyStable,   ///< Occasional failures
    Stable,             ///< Rare failures
    HighlyStable        ///< No failures detected
};

/**
 * @brief Report of component stability testing
 */
struct StabilityReport {
    std::string componentName;              ///< Component being tested
    StabilityRating rating;                 ///< Overall stability rating
    int edgeCasesPassed;                    ///< Number of edge cases passed
    int edgeCasesFailed;                    ///< Number of edge cases failed
    bool threadSafe;                        ///< Whether component is thread-safe
    bool memoryLeaksDetected;               ///< Whether memory leaks were detected
    bool infiniteLoopsDetected;             ///< Whether infinite loops were detected
    std::vector<std::string> failureDetails;///< Details of failures
};

// ============================================================================
// Save Integrity Testing Data Structures
// ============================================================================

/**
 * @brief Safety rating for save file operations
 */
enum class SaveSafetyRating {
    Unsafe,             ///< Corrupts saves
    PotentiallyUnsafe,  ///< May corrupt under certain conditions
    Safe                ///< No corruption detected
};

/**
 * @brief Report of save file integrity testing
 */
struct SaveSafetyReport {
    std::string componentName;                      ///< Component being tested
    SaveSafetyRating rating;                        ///< Overall save safety rating
    bool snapshotSafe;                              ///< Whether snapshots are safe
    bool rollbackSafe;                              ///< Whether rollback is safe
    bool formIDSafe;                                ///< Whether FormID operations are safe
    bool headerSafe;                                ///< Whether save headers remain valid
    std::vector<std::string> corruptionIncidents;   ///< Details of corruption incidents
};

// ============================================================================
// Integration Testing Data Structures
// ============================================================================

/**
 * @brief An integration test scenario
 */
struct IntegrationScenario {
    std::string name;                           ///< Scenario name
    std::vector<std::string> componentsInvolved;///< Components involved in scenario
    std::string description;                    ///< Scenario description
    bool passed;                                ///< Whether scenario passed
    std::string failureReason;                  ///< Reason for failure (if applicable)
};

/**
 * @brief Report of integration testing
 */
struct IntegrationReport {
    std::vector<IntegrationScenario> scenarios; ///< Test scenarios
    int totalScenarios;                         ///< Total number of scenarios
    int passedScenarios;                        ///< Number of passed scenarios
    int failedScenarios;                        ///< Number of failed scenarios
};

// ============================================================================
// Rollback Testing Data Structures
// ============================================================================

/**
 * @brief Type of state modification for rollback testing
 */
enum class StateModificationType {
    ObjectModification,     ///< Object property modification
    InventoryChange,        ///< Inventory modification
    QuestStateChange,       ///< Quest state modification
    ReferenceModification   ///< Reference modification
};

/**
 * @brief A rollback test scenario
 */
struct RollbackScenario {
    std::string name;                       ///< Scenario name
    StateModificationType modificationType; ///< Type of modification
    bool rollbackSuccessful;                ///< Whether rollback succeeded
    bool memoryLeakDetected;                ///< Whether memory leak was detected
    double rollbackTimeMs;                  ///< Rollback time in milliseconds
    std::string failureReason;              ///< Reason for failure (if applicable)
};

/**
 * @brief Report of rollback testing
 */
struct RollbackReport {
    std::vector<RollbackScenario> scenarios;    ///< Test scenarios
    int totalTests;                             ///< Total number of tests
    int successfulRollbacks;                    ///< Number of successful rollbacks
    int failedRollbacks;                        ///< Number of failed rollbacks
    double averageRollbackTimeMs;               ///< Average rollback time
};

// ============================================================================
// Memory Safety Testing Data Structures
// ============================================================================

/**
 * @brief Information about a memory leak
 */
struct MemoryLeak {
    std::string location;       ///< Location of leak (file:line)
    size_t bytesLeaked;         ///< Number of bytes leaked
    std::string allocationStack;///< Stack trace of allocation
};

/**
 * @brief Report of memory safety testing
 */
struct MemorySafetyReport {
    std::string componentName;              ///< Component being tested
    std::vector<MemoryLeak> leaks;          ///< Detected memory leaks
    int useAfterFreeIncidents;              ///< Number of use-after-free incidents
    int doubleFreeIncidents;                ///< Number of double-free incidents
    int bufferOverflows;                    ///< Number of buffer overflows
    bool handlesAllocationFailures;         ///< Whether component handles allocation failures
    size_t totalBytesLeaked;                ///< Total bytes leaked
};

// ============================================================================
// VEH Testing Data Structures
// ============================================================================

/**
 * @brief Type of exception for VEH testing
 */
enum class ExceptionType {
    AccessViolation,    ///< Access violation exception
    DivideByZero,       ///< Divide by zero exception
    StackOverflow,      ///< Stack overflow exception
    IllegalInstruction, ///< Illegal instruction exception
    IntegerOverflow     ///< Integer overflow exception
};

/**
 * @brief A VEH test scenario
 */
struct VEHTestScenario {
    ExceptionType exceptionType;    ///< Type of exception
    bool exceptionCaught;           ///< Whether exception was caught
    bool contextCaptured;           ///< Whether context was captured
    bool callStackValid;            ///< Whether call stack is valid
    std::string failureReason;      ///< Reason for failure (if applicable)
};

/**
 * @brief Report of VEH testing
 */
struct VEHReport {
    std::vector<VEHTestScenario> scenarios; ///< Test scenarios
    int totalTests;                         ///< Total number of tests
    int passedTests;                        ///< Number of passed tests
    double overheadPercentage;              ///< Performance overhead percentage
};

// ============================================================================
// Dynamic Fix Testing Data Structures
// ============================================================================

/**
 * @brief Type of dynamic fix
 */
enum class FixType {
    MeshReplacement,    ///< Mesh replacement fix
    AnimationSwitch,    ///< Animation switch fix
    ScriptDisable,      ///< Script disable fix
    TextureReplacement, ///< Texture replacement fix
    ReferenceRemoval    ///< Reference removal fix
};

/**
 * @brief A fix test scenario
 */
struct FixTestScenario {
    FixType fixType;            ///< Type of fix
    bool fixApplied;            ///< Whether fix was applied
    bool fixPersisted;          ///< Whether fix persisted to save
    bool fixRolledBack;         ///< Whether fix was rolled back
    std::string failureReason;  ///< Reason for failure (if applicable)
};

/**
 * @brief Report of fix applicator testing
 */
struct FixApplicatorReport {
    std::vector<FixTestScenario> scenarios; ///< Test scenarios
    int totalTests;                         ///< Total number of tests
    int successfulFixes;                    ///< Number of successful fixes
    int failedFixes;                        ///< Number of failed fixes
};

// ============================================================================
// Pattern Learning Testing Data Structures
// ============================================================================

/**
 * @brief Report of pattern learning testing
 */
struct PatternLearningReport {
    bool databaseLoadsCorrectly;        ///< Whether database loads correctly
    bool patternsRecordedCorrectly;     ///< Whether patterns are recorded correctly
    double matchingAccuracy;            ///< Pattern matching accuracy (0.0-1.0)
    bool successRatesTracked;           ///< Whether success rates are tracked
    bool databaseWritesSucceed;         ///< Whether database writes succeed
    bool recoversFromCorruption;        ///< Whether system recovers from corruption
    double learningImprovementRate;     ///< Learning improvement rate
    bool exportImportWorks;             ///< Whether export/import works
};

// ============================================================================
// Report Generation Data Structures
// ============================================================================

/**
 * @brief Severity of an issue
 */
enum class IssueSeverity {
    Low,        ///< Low severity issue
    Medium,     ///< Medium severity issue
    High,       ///< High severity issue
    Critical    ///< Critical severity issue
};

/**
 * @brief An identified issue
 */
struct Issue {
    std::string componentName;  ///< Component with the issue
    std::string testName;       ///< Test that identified the issue
    IssueSeverity severity;     ///< Issue severity
    std::string description;    ///< Issue description
    std::string recommendation; ///< Recommended fix
};

/**
 * @brief Complete report for a component
 */
struct ComponentReport {
    ComponentInfo component;                ///< Component information
    FunctionalityReport functionality;      ///< Functionality test results
    StabilityReport stability;              ///< Stability test results
    SaveSafetyReport saveSafety;            ///< Save safety test results
    MemorySafetyReport memorySafety;        ///< Memory safety test results
    std::vector<Issue> issues;              ///< Identified issues
};

/**
 * @brief Summary report for all components
 */
struct SummaryReport {
    int totalComponents;                            ///< Total number of components
    int totalTests;                                 ///< Total number of tests
    int passedTests;                                ///< Number of passed tests
    int failedTests;                                ///< Number of failed tests
    double passRate;                                ///< Pass rate (0.0-1.0)
    std::vector<Issue> criticalIssues;              ///< Critical issues
    std::vector<Issue> highPriorityIssues;          ///< High priority issues
    std::map<DefensiveLayer, int> failuresByLayer;  ///< Failures by layer
};

// ============================================================================
// Test Suite Generation Data Structures
// ============================================================================

/**
 * @brief A generated test
 */
struct GeneratedTest {
    std::string testName;                   ///< Test name
    std::string testCode;                   ///< Generated test code
    std::string documentation;              ///< Test documentation
    std::vector<std::string> dependencies;  ///< Test dependencies
};

/**
 * @brief A test suite
 */
struct TestSuite {
    std::string suiteName;                      ///< Suite name
    std::vector<GeneratedTest> unitTests;       ///< Unit tests
    std::vector<GeneratedTest> integrationTests;///< Integration tests
    std::vector<std::string> fixtures;          ///< Test fixtures
    std::vector<std::string> mocks;             ///< Mock objects
};

// ============================================================================
// Performance Testing Data Structures
// ============================================================================

/**
 * @brief Load level for performance testing
 */
enum class LoadLevel {
    Low,    ///< Low load
    Normal, ///< Normal load
    High    ///< High load
};

/**
 * @brief Performance metrics
 */
struct PerformanceMetrics {
    double executionTimeMs;     ///< Execution time in milliseconds
    size_t memoryUsageBytes;    ///< Memory usage in bytes
    double overheadPercentage;  ///< Overhead percentage
    LoadLevel loadLevel;        ///< Load level
};

/**
 * @brief Report of performance testing
 */
struct PerformanceReport {
    std::string componentName;      ///< Component being tested
    PerformanceMetrics lowLoad;     ///< Low load metrics
    PerformanceMetrics normalLoad;  ///< Normal load metrics
    PerformanceMetrics highLoad;    ///< High load metrics
    double averageOverhead;         ///< Average overhead percentage
    bool exceedsThreshold;          ///< Whether component exceeds threshold
};

// ============================================================================
// Compatibility Testing Data Structures
// ============================================================================

/**
 * @brief Game version for compatibility testing
 */
enum class GameVersion {
    SE_1_5_97,  ///< Skyrim SE version 1.5.97
    VR,         ///< Skyrim VR
    AE_Latest   ///< Skyrim AE (latest version)
};

/**
 * @brief Version compatibility information
 */
struct VersionCompatibility {
    GameVersion version;                ///< Game version
    bool componentWorks;                ///< Whether component works
    bool hooksResolve;                  ///< Whether hooks resolve
    bool savesWork;                     ///< Whether saves work
    std::vector<std::string> issues;    ///< Compatibility issues
};

/**
 * @brief Report of compatibility testing
 */
struct CompatibilityReport {
    std::string componentName;                      ///< Component being tested
    std::vector<VersionCompatibility> versionResults;///< Results per version
    bool addressLibIndependent;                     ///< Whether Address Library independent
    bool patternScanningWorks;                      ///< Whether pattern scanning works
};

// ============================================================================
// Regression Detection Data Structures
// ============================================================================

/**
 * @brief A detected regression
 */
struct Regression {
    std::string componentName;      ///< Component with regression
    std::string testName;           ///< Test that regressed
    bool previouslyPassed;          ///< Whether test previously passed
    bool currentlyPassed;           ///< Whether test currently passes
    std::string changeDescription;  ///< Description of change
};

/**
 * @brief Report of regression detection
 */
struct RegressionReport {
    std::vector<Regression> regressions;        ///< Detected regressions
    std::vector<std::string> newComponents;     ///< New components
    std::vector<std::string> modifiedComponents;///< Modified components
    std::vector<std::string> flakyTests;        ///< Flaky tests
    bool cicdStatus;                            ///< CI/CD status (true = pass)
};

} // namespace SafetyEvaluation
