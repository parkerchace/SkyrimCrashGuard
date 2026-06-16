// Copyright (C) 2026-2026 Parker Chace
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
 * @brief Discovers and analyzes CrashGuard components from source code
 * 
 * ComponentDiscovery scans the source directory to identify all components,
 * extract metadata, classify them by defensive layer, and identify their
 * interactions with save files and game memory.
 */
class ComponentDiscovery {
public:
    /**
     * @brief Discovers all components in the source directory
     * @param sourceDir Path to the source directory to scan
     * @return Vector of discovered components with metadata
     */
    std::vector<ComponentInfo> DiscoverComponents(const std::string& sourceDir);

    /**
     * @brief Analyzes a single component file to extract metadata
     * @param filePath Path to the component source file
     * @return ComponentInfo structure with extracted metadata
     */
    ComponentInfo AnalyzeComponent(const std::string& filePath);

    /**
     * @brief Classifies a component by its defensive layer
     * @param component Component to classify
     * @return DefensiveLayer classification
     */
    DefensiveLayer ClassifyByLayer(const ComponentInfo& component);

    /**
     * @brief Identifies save file interactions in a component
     * @param component Component to analyze
     * @return Vector of save file operation descriptions
     */
    std::vector<std::string> IdentifySaveFileInteractions(const ComponentInfo& component);

    /**
     * @brief Identifies memory modifications in a component
     * @param component Component to analyze
     * @return Vector of memory operation descriptions
     */
    std::vector<std::string> IdentifyMemoryModifications(const ComponentInfo& component);

private:
    /**
     * @brief Reads file content from disk
     * @param filePath Path to file
     * @return File content as string
     */
    std::string ReadFileContent(const std::string& filePath);

    /**
     * @brief Extracts class name from file content
     * @param content File content
     * @param filePath File path (for fallback name extraction)
     * @return Class name
     */
    std::string ExtractClassName(const std::string& content, const std::string& filePath);

    /**
     * @brief Extracts public methods from file content
     * @param content File content
     * @return Vector of method signatures
     */
    std::vector<std::string> ExtractPublicMethods(const std::string& content);

    /**
     * @brief Extracts dependencies from file content
     * @param content File content
     * @return Vector of dependency names
     */
    std::vector<std::string> ExtractDependencies(const std::string& content);

    /**
     * @brief Checks if content contains save file operations
     * @param content File content
     * @return True if save operations found
     */
    bool HasSaveFileOperations(const std::string& content);

    /**
     * @brief Checks if content contains memory modifications
     * @param content File content
     * @return True if memory operations found
     */
    bool HasMemoryModifications(const std::string& content);

    /**
     * @brief Finds save file operations in content
     * @param content File content
     * @return Vector of operation descriptions
     */
    std::vector<std::string> FindSaveOperations(const std::string& content);

    /**
     * @brief Finds memory operations in content
     * @param content File content
     * @return Vector of operation descriptions
     */
    std::vector<std::string> FindMemoryOperations(const std::string& content);

    /**
     * @brief Classifies component by layer based on name and content
     * @param name Component name
     * @param content File content
     * @return DefensiveLayer classification
     */
    DefensiveLayer ClassifyByNameAndContent(const std::string& name, const std::string& content);
};

} // namespace SafetyEvaluation
