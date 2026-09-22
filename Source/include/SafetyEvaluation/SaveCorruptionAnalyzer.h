// Copyright (C) 2024-2026 Parker Chace
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of Skyrim Crash Guard.
//
// Skyrim Crash Guard is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version.
//
// Skyrim Crash Guard is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "DataStructures.h"
#include <string>
#include <vector>
#include <regex>

namespace SafetyEvaluation {

/**
 * @brief Analyzes components for save file corruption risks
 * 
 * This class examines component code to identify save file operations,
 * assess their safety, and generate risk ratings. It checks for:
 * - Data validation before writes
 * - Error handling around save operations
 * - Rollback capability
 * - FormID modifications
 * - Save file integrity checks
 */
class SaveCorruptionAnalyzer {
public:
    SaveCorruptionAnalyzer() = default;
    ~SaveCorruptionAnalyzer() = default;

    /**
     * @brief Analyze a component for save corruption risk
     * @param component The component to analyze
     * @return Save risk rating (None, Low, Medium, High, Critical)
     */
    SaveRiskRating AnalyzeSaveRisk(const ComponentInfo& component);

    /**
     * @brief Identify all save file operations in component code
     * @param component The component to analyze
     * @return Vector of identified save operations
     */
    std::vector<SaveOperation> IdentifySaveOperations(const ComponentInfo& component);

    /**
     * @brief Check if a save operation has data validation
     * @param operation The save operation to check
     * @param sourceCode The source code containing the operation
     * @return True if validation is present
     */
    bool HasDataValidation(const SaveOperation& operation, const std::string& sourceCode);

    /**
     * @brief Check if a save operation has error handling
     * @param operation The save operation to check
     * @param sourceCode The source code containing the operation
     * @return True if error handling is present
     */
    bool HasErrorHandling(const SaveOperation& operation, const std::string& sourceCode);

    /**
     * @brief Check if a component has rollback capability
     * @param component The component to check
     * @return True if rollback capability is present
     */
    bool HasRollbackCapability(const ComponentInfo& component);

    /**
     * @brief Check if a component modifies FormIDs
     * @param component The component to check
     * @return True if FormID modifications are detected
     */
    bool ModifiesFormIDs(const ComponentInfo& component);

private:
    /**
     * @brief Read source code from a file
     * @param filePath Path to the source file
     * @return Source code as string
     */
    std::string ReadSourceCode(const std::string& filePath);

    /**
     * @brief Parse source code to find save operations
     * @param sourceCode The source code to parse
     * @param filePath Path to the source file (for location tracking)
     * @return Vector of save operations found
     */
    std::vector<SaveOperation> ParseSaveOperations(const std::string& sourceCode, const std::string& filePath);

    /**
     * @brief Check if code contains validation patterns
     * @param code Code snippet to check
     * @return True if validation patterns are found
     */
    bool ContainsValidationPattern(const std::string& code);

    /**
     * @brief Check if code contains error handling patterns
     * @param code Code snippet to check
     * @return True if error handling patterns are found
     */
    bool ContainsErrorHandlingPattern(const std::string& code);

    /**
     * @brief Extract context around a code location
     * @param sourceCode Full source code
     * @param lineNumber Line number to extract context around
     * @param contextLines Number of lines before/after to include
     * @return Code context as string
     */
    std::string ExtractContext(const std::string& sourceCode, int lineNumber, int contextLines = 5);

    /**
     * @brief Calculate risk rating based on operation analysis
     * @param operations Vector of save operations
     * @param component Component being analyzed
     * @return Calculated risk rating
     */
    SaveRiskRating CalculateRiskRating(const std::vector<SaveOperation>& operations, const ComponentInfo& component);
};

} // namespace SafetyEvaluation
