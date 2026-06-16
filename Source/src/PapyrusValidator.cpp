// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "PapyrusValidator.h"
#include "Config.h"
#include <spdlog/spdlog.h>
#include <windows.h>

namespace PapyrusValidation {

    // ========================================================================
    // Parameter Validation
    // ========================================================================

    std::vector<ValidationResult> ParameterValidator::ValidateParameters(
        const std::string& className,
        const std::string& functionName,
        RE::BSScript::Variable* parameters,
        size_t parameterCount
    ) {
        std::vector<ValidationResult> results;
        
        if (!parameters || parameterCount == 0) {
            return results;
        }

        // Get function-specific rules if available
        const FunctionRule* rule = FunctionRegistry::GetRule(className, functionName);

        // Validate each parameter
        for (size_t i = 0; i < parameterCount; ++i) {
            std::string paramName = "param" + std::to_string(i);
            
            // Use rule parameter name if available
            if (rule && i < rule->parameterNames.size()) {
                paramName = rule->parameterNames[i];
            }

            auto result = ValidateParameter(parameters[i], paramName, i);
            
            if (!result.isValid) {
                // Check if null is allowed for this parameter
                bool allowNull = rule && i < rule->allowNull.size() && rule->allowNull[i];
                
                if (!allowNull) {
                    results.push_back(result);
                    
                    // Track failure
                    std::lock_guard<std::mutex> lock(s_statsMutex);
                    s_stats.failedValidations++;
                    s_stats.failuresByFunction[className + "::" + functionName]++;
                    s_stats.failuresByParameter[paramName]++;
                }
            }
        }

        // Update stats
        {
            std::lock_guard<std::mutex> lock(s_statsMutex);
            s_stats.totalCalls++;
            if (!results.empty()) {
                s_stats.validatedCalls++;
            }
        }

        return results;
    }

    ValidationResult ParameterValidator::ValidateParameter(
        RE::BSScript::Variable& parameter,
        const std::string& parameterName,
        size_t parameterIndex
    ) {
        ValidationResult result;
        result.parameterName = parameterName;
        result.parameterIndex = parameterIndex;
        result.isValid = true;
        result.wasModified = false;

        // Get parameter type
        auto typeInfo = parameter.GetType();
        auto rawType = typeInfo.GetRawType();

        // Validate based on type
        if (rawType == RE::BSScript::TypeInfo::RawType::kObject) {
            if (!ValidateObjectReference(parameter)) {
                result.isValid = false;
                result.reason = "Null or invalid object reference";
                result.wasModified = true;
                
                // Replace with None
                parameter.SetNone();
                
                std::lock_guard<std::mutex> lock(s_statsMutex);
                s_stats.nullPointersFixed++;
            }
        } else if (rawType == RE::BSScript::TypeInfo::RawType::kString) {
            if (!ValidateString(parameter)) {
                result.isValid = false;
                result.reason = "Null or invalid string";
                result.wasModified = true;
                
                // Replace with empty string
                parameter.SetString("");
                
                std::lock_guard<std::mutex> lock(s_statsMutex);
                s_stats.invalidStringsFixed++;
            }
        } else if (rawType == RE::BSScript::TypeInfo::RawType::kObjectArray || 
                   rawType == RE::BSScript::TypeInfo::RawType::kStringArray ||
                   rawType == RE::BSScript::TypeInfo::RawType::kIntArray ||
                   rawType == RE::BSScript::TypeInfo::RawType::kFloatArray ||
                   rawType == RE::BSScript::TypeInfo::RawType::kBoolArray) {
            if (!ValidateArray(parameter)) {
                result.isValid = false;
                result.reason = "Null or invalid array";
                result.wasModified = true;
                
                // Replace with empty array
                parameter.SetArray(RE::BSTSmartPointer<RE::BSScript::Array>());
                
                std::lock_guard<std::mutex> lock(s_statsMutex);
                s_stats.invalidArraysFixed++;
            }
        }

        return result;
    }

    // ========================================================================
    // Type-Specific Validation
    // ========================================================================

    bool ParameterValidator::ValidateObjectReference(RE::BSScript::Variable& parameter) {
        // Check if variable contains an object
        if (parameter.IsNoneObject()) {
            return false;  // None is not valid
        }

        // Try to get the object
        auto object = parameter.GetObject();
        if (!object) {
            return false;
        }

        // Check if object pointer is valid
        if (!IsPointerValid(object.get())) {
            return false;
        }

        return true;
    }

    bool ParameterValidator::ValidateString(RE::BSScript::Variable& parameter) {
        // Get the string
        auto str = parameter.GetString();
        
        // Check if string data is valid
        if (str.empty()) {
            // Empty strings are valid
            return true;
        }

        // Check if pointer is in valid memory
        if (!IsPointerValid(str.data())) {
            return false;
        }

        return true;
    }

    bool ParameterValidator::ValidateArray(RE::BSScript::Variable& parameter) {
        // Get the array
        auto array = parameter.GetArray();
        if (!array) {
            return false;
        }

        // Check if array pointer is valid
        if (!IsPointerValid(array.get())) {
            return false;
        }

        return true;
    }

    bool ParameterValidator::ValidateForm(RE::BSScript::Variable& parameter) {
        // Form handles in Papyrus are stored the same way as object references
        // at the bytecode level. ValidateObjectReference checks that the handle
        // value is non-null and has a valid type — enough to prevent the most
        // common crash: passing a deleted or None form to a native function.
        return ValidateObjectReference(parameter);
    }

    bool ParameterValidator::IsPointerValid(const void* ptr) {
        if (!ptr) {
            return false;
        }

        // Check if pointer is in valid memory range
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0) {
            return false;
        }

        // Check if memory is committed
        if (mbi.State != MEM_COMMIT) {
            return false;
        }

        // Check if memory is readable
        if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | 
                             PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) {
            return false;
        }

        return true;
    }

    // ========================================================================
    // Safe Defaults
    // ========================================================================

    RE::BSScript::Variable ParameterValidator::GetSafeDefault(
        RE::BSScript::TypeInfo::RawType type
    ) {
        RE::BSScript::Variable safeValue;

        if (type == RE::BSScript::TypeInfo::RawType::kNone) {
            safeValue.SetNone();
        } else if (type == RE::BSScript::TypeInfo::RawType::kObject) {
            safeValue.SetNone();
        } else if (type == RE::BSScript::TypeInfo::RawType::kString) {
            safeValue.SetString("");
        } else if (type == RE::BSScript::TypeInfo::RawType::kInt) {
            safeValue.SetSInt(0);
        } else if (type == RE::BSScript::TypeInfo::RawType::kFloat) {
            safeValue.SetFloat(0.0f);
        } else if (type == RE::BSScript::TypeInfo::RawType::kBool) {
            safeValue.SetBool(false);
        } else if (type == RE::BSScript::TypeInfo::RawType::kObjectArray ||
                   type == RE::BSScript::TypeInfo::RawType::kStringArray ||
                   type == RE::BSScript::TypeInfo::RawType::kIntArray ||
                   type == RE::BSScript::TypeInfo::RawType::kFloatArray ||
                   type == RE::BSScript::TypeInfo::RawType::kBoolArray) {
            safeValue.SetArray(RE::BSTSmartPointer<RE::BSScript::Array>());
        } else {
            safeValue.SetNone();
        }

        return safeValue;
    }

    // ========================================================================
    // Statistics
    // ========================================================================

    ValidationStats ParameterValidator::GetStats() {
        std::lock_guard<std::mutex> lock(s_statsMutex);
        return s_stats;
    }

    void ParameterValidator::ResetStats() {
        std::lock_guard<std::mutex> lock(s_statsMutex);
        s_stats = ValidationStats();
    }

    // ========================================================================
    // Function Registry
    // ========================================================================

    void FunctionRegistry::RegisterRule(const FunctionRule& rule) {
        std::lock_guard<std::mutex> lock(s_rulesMutex);
        
        // Check if rule already exists
        for (auto& existingRule : s_rules) {
            if (existingRule.className == rule.className &&
                existingRule.functionName == rule.functionName) {
                // Update existing rule
                existingRule = rule;
                return;
            }
        }
        
        // Add new rule
        s_rules.push_back(rule);
    }

    const FunctionRule* FunctionRegistry::GetRule(
        const std::string& className,
        const std::string& functionName
    ) {
        std::lock_guard<std::mutex> lock(s_rulesMutex);
        
        for (const auto& rule : s_rules) {
            if (rule.className == className && rule.functionName == functionName) {
                return &rule;
            }
        }
        
        return nullptr;
    }

    void FunctionRegistry::LoadDefaultRules() {
        // SmartHarvest NotifyActivated - the crash from the user report
        RegisterRule({
            .className = "shse_pluginproxy",
            .functionName = "NotifyActivated",
            .parameterNames = {
                "itemForm", "itemType", "collectible", "refrID", 
                "baseID", "notify", "baseName", "count", 
                "isContainer", "isWhitelisted", "isBlacklisted"
            },
            .allowNull = {
                false, false, false, false,  // itemForm, itemType, collectible, refrID
                false, false, false, false,  // baseID, notify, baseName, count
                false, false, false          // isContainer, isWhitelisted, isBlacklisted
            },
            .enabled = true,
            .strictMode = false
        });

        spdlog::info("[PapyrusValidation] Loaded {} default validation rules", s_rules.size());
    }

    const std::vector<FunctionRule>& FunctionRegistry::GetAllRules() {
        std::lock_guard<std::mutex> lock(s_rulesMutex);
        return s_rules;
    }

}  // namespace PapyrusValidation
