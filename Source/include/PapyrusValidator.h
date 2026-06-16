// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <RE/Skyrim.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>

/// Papyrus Native Function Parameter Validation System
/// Validates parameters before they reach native C++ implementations
/// Prevents null pointer dereferences and invalid data from causing crashes
namespace PapyrusValidation {

    /// Validation result for a single parameter
    struct ValidationResult {
        bool isValid;
        bool wasModified;
        std::string parameterName;
        std::string reason;
        size_t parameterIndex;
    };

    /// Statistics for validation system
    struct ValidationStats {
        size_t totalCalls = 0;
        size_t validatedCalls = 0;
        size_t failedValidations = 0;
        size_t nullPointersFixed = 0;
        size_t invalidFormsFixed = 0;
        size_t invalidStringsFixed = 0;
        size_t invalidArraysFixed = 0;
        size_t crashesPrevented = 0;
        
        std::map<std::string, size_t> failuresByFunction;
        std::map<std::string, size_t> failuresByParameter;
    };

    /// Main parameter validator class
    class ParameterValidator {
    public:
        /// Validate all parameters for a function call
        static std::vector<ValidationResult> ValidateParameters(
            const std::string& className,
            const std::string& functionName,
            RE::BSScript::Variable* parameters,
            size_t parameterCount
        );

        /// Validate single parameter based on type
        static ValidationResult ValidateParameter(
            RE::BSScript::Variable& parameter,
            const std::string& parameterName,
            size_t parameterIndex
        );

        /// Get safe default value for a type
        static RE::BSScript::Variable GetSafeDefault(
            RE::BSScript::TypeInfo::RawType type
        );

        /// Get validation statistics
        static ValidationStats GetStats();

        /// Reset statistics
        static void ResetStats();

        /// Statistics tracking (public for access from hook system)
        static inline ValidationStats s_stats;
        static inline std::mutex s_statsMutex;

    private:
        /// Validate object reference
        static bool ValidateObjectReference(RE::BSScript::Variable& parameter);

        /// Validate string (BSFixedString)
        static bool ValidateString(RE::BSScript::Variable& parameter);

        /// Validate array
        static bool ValidateArray(RE::BSScript::Variable& parameter);

        /// Validate form reference
        static bool ValidateForm(RE::BSScript::Variable& parameter);

        /// Check if pointer is in valid memory
        static bool IsPointerValid(const void* ptr);
    };

    /// Function-specific validation rules
    struct FunctionRule {
        std::string className;
        std::string functionName;
        std::vector<std::string> parameterNames;
        std::vector<bool> allowNull;  // Per-parameter null allowance
        bool enabled = true;
        bool strictMode = false;  // Fail call if validation fails
    };

    /// Registry for function-specific validation rules
    class FunctionRegistry {
    public:
        /// Register validation rule for a function
        static void RegisterRule(const FunctionRule& rule);

        /// Get rule for a function (returns nullptr if not found)
        static const FunctionRule* GetRule(
            const std::string& className,
            const std::string& functionName
        );

        /// Load default rules for known problematic functions
        static void LoadDefaultRules();

        /// Get all registered rules
        static const std::vector<FunctionRule>& GetAllRules();

    private:
        static inline std::vector<FunctionRule> s_rules;
        static inline std::mutex s_rulesMutex;
    };

}  // namespace PapyrusValidation
