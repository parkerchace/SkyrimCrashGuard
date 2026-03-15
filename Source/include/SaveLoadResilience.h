// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace CrashGuard {

// Save file validation result
struct SaveValidationResult {
    bool isValid;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    bool canLoad;  // Can attempt partial load
};

// Save file header structure (Skyrim .ess format)
struct SaveFileHeader {
    uint32_t magic;           // "TESV_SAVEGAME" magic number
    uint32_t headerSize;
    uint32_t version;
    uint32_t saveNumber;
    std::string playerName;
    uint32_t playerLevel;
    std::string playerLocation;
    std::string gameDate;
    std::string playerRaceEditorId;
    uint16_t playerSex;
    float playerCurExp;
    float playerLvlUpExp;
    std::vector<uint8_t> screenshotData;
    uint8_t formVersion;
    uint32_t pluginInfoSize;
};

// Chunk information
struct SaveChunk {
    uint32_t type;
    uint32_t version;
    uint32_t length;
    uint64_t offset;
    bool isValid;
};

// SaveLoadResilience class - handles save file validation and recovery
class SaveLoadResilience {
public:
    // Singleton access
    static SaveLoadResilience& GetInstance();
    
    // Delete copy/move constructors
    SaveLoadResilience(const SaveLoadResilience&) = delete;
    SaveLoadResilience& operator=(const SaveLoadResilience&) = delete;
    SaveLoadResilience(SaveLoadResilience&&) = delete;
    SaveLoadResilience& operator=(SaveLoadResilience&&) = delete;
    
    // Save file validation
    SaveValidationResult ValidateSaveFile(const std::filesystem::path& savePath);
    bool ValidateHeader(const std::filesystem::path& savePath, SaveFileHeader& outHeader);
    bool ValidateVersion(uint32_t version);
    bool ValidateChunkSizes(const std::filesystem::path& savePath);
    
    // Partial save loading
    bool LoadSavePartial(const std::filesystem::path& savePath);
    bool SkipCorruptedSection(std::ifstream& file, const SaveChunk& chunk);
    bool LoadValidData(std::ifstream& file, const SaveChunk& chunk);
    bool RemoveInvalidFormIDs(std::vector<uint32_t>& formIDs);
    bool DisableBrokenScripts(const std::vector<std::string>& scriptNames);
    
    // Save loading fallback
    std::optional<std::filesystem::path> FindPreviousAutosave(const std::filesystem::path& savePath);
    bool OfferLoadPreviousAutosave(const std::filesystem::path& currentSave);
    bool WarnAboutPotentialIssues(const SaveValidationResult& validation);
    bool ValidateDataBeforeWriting(const std::vector<uint8_t>& data);
    
    // Helper methods
    std::vector<SaveChunk> ParseChunks(const std::filesystem::path& savePath);
    bool IsFormIDValid(uint32_t formID);
    bool IsScriptBroken(const std::string& scriptName);
    std::vector<std::filesystem::path> GetAutosaveList(const std::filesystem::path& saveDir);
    
private:
    SaveLoadResilience();
    ~SaveLoadResilience();
    
    // Constants
    static constexpr uint32_t SAVE_MAGIC = 0x53564554;  // "TESV" in little-endian
    static constexpr uint32_t MIN_HEADER_SIZE = 64;
    static constexpr uint32_t MAX_HEADER_SIZE = 1024 * 1024;  // 1MB max
    static constexpr uint32_t SUPPORTED_VERSION_MIN = 12;
    static constexpr uint32_t SUPPORTED_VERSION_MAX = 12;
    
    // Chunk types
    enum class ChunkType : uint32_t {
        FormIDs = 0,
        Unknown = 1,
        GlobalData = 2,
        ChangeRecords = 3,
        Unknown4 = 4,
        Unknown5 = 5,
        FormIDArray = 6
    };
    
    // Validation helpers
    bool ReadHeader(std::ifstream& file, SaveFileHeader& header);
    bool ValidateMagicNumber(uint32_t magic);
    bool ValidateHeaderSize(uint32_t size);
    bool ValidatePluginInfo(std::ifstream& file);
    
    // Chunk validation helpers
    bool ValidateChunk(const SaveChunk& chunk, uint64_t fileSize);
    bool IsChunkSizeReasonable(uint32_t size);
    
    // FormID validation helpers
    bool ValidateFormIDAgainstPlugins(uint32_t formID);
    bool IsPluginLoaded(uint8_t pluginIndex);
    
    // Script validation helpers
    bool ValidateScriptData(const std::vector<uint8_t>& scriptData);
    void BlacklistScript(const std::string& scriptName);
    
    // Member variables
    std::vector<std::string> blacklistedScripts_;
    std::vector<uint32_t> invalidFormIDs_;
};

} // namespace CrashGuard
