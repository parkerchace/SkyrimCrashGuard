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
 * @brief Tests VEH exception handler thoroughly
 * 
 * The VEHTester validates that the Vectored Exception Handler correctly
 * intercepts and handles various types of exceptions, captures crash context,
 * and manages thread control without introducing significant overhead.
 * 
 * Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.8, 9.9
 */
class VEHTester {
public:
    /**
     * @brief Test VEH functionality comprehensively
     * @return VEH report with all test results
     * 
     * Requirement 9.9: Generate VEH test report
     */
    static VEHReport TestVEH();

    /**
     * @brief Test exception capture for various exception types
     * @param type Type of exception to test
     * @return True if exception was caught correctly
     * 
     * Requirement 9.1: Trigger test exceptions and verify VEH catches them
     * Requirement 9.2: Test various exception types
     */
    static bool TestExceptionCapture(ExceptionType type);

    /**
     * @brief Test that crash context is correctly captured
     * @return True if context capture works correctly
     * 
     * Requirement 9.3: Verify crash context is correctly captured
     */
    static bool TestContextCapture();

    /**
     * @brief Test that call stack building works correctly
     * @return True if call stack is built correctly
     * 
     * Requirement 9.4: Test call stack building
     */
    static bool TestCallStackBuilding();

    /**
     * @brief Test thread pausing and resuming
     * @return True if thread control works without deadlocks
     * 
     * Requirement 9.5: Verify thread pausing and resuming works without deadlocks
     */
    static bool TestThreadControl();

    /**
     * @brief Test VEH handler with multiple simultaneous exceptions
     * @return True if multiple exceptions are handled correctly
     * 
     * Requirement 9.6: Test VEH handler with multiple exceptions
     */
    static bool TestMultipleExceptions();

    /**
     * @brief Test that VEH handler properly chains to other handlers
     * @return True if handler chaining works correctly
     * 
     * Requirement 9.7: Verify VEH handler properly chains to other handlers
     */
    static bool TestHandlerChaining();

    /**
     * @brief Measure VEH handler performance overhead
     * @return Performance overhead as a percentage
     * 
     * Requirement 9.8: Test VEH handler performance
     */
    static double MeasureVEHOverhead();

    /**
     * @brief Set test handler called flag (for testing)
     * @param called Whether test handler was called
     */
    static void SetTestHandlerCalled(bool called);

private:
    /**
     * @brief Trigger an access violation exception
     * @return True if exception was triggered successfully
     */
    static bool TriggerAccessViolation();

    /**
     * @brief Trigger a divide by zero exception
     * @return True if exception was triggered successfully
     */
    static bool TriggerDivideByZero();

    /**
     * @brief Trigger a stack overflow exception
     * @return True if exception was triggered successfully
     */
    static bool TriggerStackOverflow();

    /**
     * @brief Trigger an illegal instruction exception
     * @return True if exception was triggered successfully
     */
    static bool TriggerIllegalInstruction();

    /**
     * @brief Trigger an integer overflow exception
     * @return True if exception was triggered successfully
     */
    static bool TriggerIntegerOverflow();

    /**
     * @brief Verify that exception was caught by VEH handler
     * @param type Type of exception that was triggered
     * @return True if exception was caught
     */
    static bool VerifyExceptionCaught(ExceptionType type);

    /**
     * @brief Verify that crash context was captured correctly
     * @return True if context is valid and complete
     */
    static bool VerifyContextValid();

    /**
     * @brief Verify that call stack was built correctly
     * @return True if call stack is valid and contains expected frames
     */
    static bool VerifyCallStackValid();

    /**
     * @brief Create test threads for thread control testing
     * @return True if threads were created successfully
     */
    static bool CreateTestThreads();

    /**
     * @brief Pause test threads
     * @return True if threads were paused successfully
     */
    static bool PauseTestThreads();

    /**
     * @brief Resume test threads
     * @return True if threads were resumed successfully
     */
    static bool ResumeTestThreads();

    /**
     * @brief Verify threads are in expected state
     * @param shouldBePaused Whether threads should be paused
     * @return True if threads are in expected state
     */
    static bool VerifyThreadState(bool shouldBePaused);

    /**
     * @brief Clean up test threads
     */
    static void CleanupTestThreads();

    /**
     * @brief Trigger multiple exceptions simultaneously
     * @return True if all exceptions were triggered successfully
     */
    static bool TriggerMultipleExceptions();

    /**
     * @brief Verify all exceptions were handled correctly
     * @return True if all exceptions were handled
     */
    static bool VerifyMultipleExceptionsHandled();

    /**
     * @brief Install a test exception handler for chaining test
     * @return True if handler was installed successfully
     */
    static bool InstallTestHandler();

    /**
     * @brief Remove test exception handler
     */
    static void RemoveTestHandler();

    /**
     * @brief Verify that test handler was called (chaining worked)
     * @return True if test handler was called
     */
    static bool VerifyTestHandlerCalled();

    /**
     * @brief Measure baseline performance without VEH
     * @return Baseline execution time in microseconds
     */
    static double MeasureBaseline();

    /**
     * @brief Measure performance with VEH installed
     * @return Execution time with VEH in microseconds
     */
    static double MeasureWithVEH();

    /**
     * @brief Calculate overhead percentage
     * @param baseline Baseline time
     * @param withVEH Time with VEH
     * @return Overhead as percentage
     */
    static double CalculateOverhead(double baseline, double withVEH);

    /**
     * @brief Create a VEH test scenario for reporting
     * @param type Exception type
     * @param caught Whether exception was caught
     * @param contextCaptured Whether context was captured
     * @param callStackValid Whether call stack is valid
     * @param failureReason Reason for failure (if any)
     * @return Populated VEHTestScenario
     */
    static VEHTestScenario CreateScenario(
        ExceptionType type,
        bool caught,
        bool contextCaptured,
        bool callStackValid,
        const std::string& failureReason = ""
    );

    /**
     * @brief Get exception type name as string
     * @param type Exception type
     * @return String representation of exception type
     */
    static std::string GetExceptionTypeName(ExceptionType type);

    // Test state tracking
    static bool s_exceptionCaught;
    static bool s_contextCaptured;
    static bool s_callStackValid;
    static bool s_testHandlerCalled;
    static int s_exceptionsHandled;
    static std::vector<void*> s_testThreads;
};

} // namespace SafetyEvaluation
