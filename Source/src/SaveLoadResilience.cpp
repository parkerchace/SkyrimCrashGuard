// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "SaveLoadResilience.h"
#include <fstream>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <RE/Skyrim.h>

namespace CrashGuard {

SaveLoadResilience::SaveLoadResilience() {
    spdlog::info("[SaveLoadResilience] Initialized");
}

SaveLoadResilience::~SaveLoadResilience() {
    spdlog::info("[SaveLoadResilience] Shutdown");
}

SaveLoadResilience& SaveLoadResilience::GetInstance() {
    static SaveLoadResilience instance;
    return instance;
}

SaveValidationResult SaveLoadResilience::ValidateSaveFile(const std::filesystem::path& savePath) {
    SaveValidationResult result;
    result.isValid = true;
    result.canLoad = true;
    
    spdlog::info("[SaveLoadResilience] Validating save file: {}", savePath.string());
    
    // Check if file exists
    if (!std::filesystem::exists(savePath)) {
        result.isValid = false;
        result.canLoad = false;
        result.errors.push_back("Save file does not exist");
        spdlog::error("[SaveLoadResilience] Save file not found: {}", savePath.string());
        return result;
    }
    
    // Check file size
    auto fileSize = std::filesystem::file_size(savePath);
    if (fileSize < MIN_HEADER_SIZE) {
        result.isValid = false;
        result.canLoad = false;
        result.errors.push_back("Save file too small (corrupted)");
        spdlog::error("[SaveLoadResilience] Save file too small: {} bytes", fileSize);
        return result;
    }
    
    // Validate header
    SaveFileHeader header;
    if (!ValidateHeader(savePath, header)) {
        result.isValid = false;
        result.canLoad = false;
        result.errors.push_back("Invalid save file header");
        spdlog::error("[SaveLoadResilience] Header validation failed");
        return result;
    }
    
    // Validate version
    if (!ValidateVersion(header.version)) {
        result.isValid = false;
        result.warnings.push_back("Unsupported save version: " + std::to_string(header.version));
        spdlog::warn("[SaveLoadResilience] Unsupported version: {}", header.version);
        // Still might be loadable with warnings
    }
    
    // Validate chunk sizes
    if (!ValidateChunkSizes(savePath)) {
        result.isValid = false;
        result.warnings.push_back("Some chunks have invalid sizes");
        result.canLoad = true;  // Can attempt partial load
        spdlog::warn("[SaveLoadResilience] Chunk validation found issues");
    }
    
    if (result.isValid) {
        spdlog::info("[SaveLoadResilience] Save file validation passed");
    } else if (result.canLoad) {
        spdlog::warn("[SaveLoadResilience] Save file has issues but may be partially loadable");
    }
    
    return result;
}

bool SaveLoadResilience::ValidateHeader(const std::filesystem::path& savePath, SaveFileHeader& outHeader) {
    std::ifstream file(savePath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("[SaveLoadResilience] Failed to open save file for header validation");
        return false;
    }
    
    return ReadHeader(file, outHeader);
}

bool SaveLoadResilience::ReadHeader(std::ifstream& file, SaveFileHeader& header) {
    // Read magic number
    file.read(reinterpret_cast<char*>(&header.magic), sizeof(header.magic));
    if (!ValidateMagicNumber(header.magic)) {
        spdlog::error("[SaveLoadResilience] Invalid magic number: 0x{:08X}", header.magic);
        return false;
    }
    
    // Read header size
    file.read(reinterpret_cast<char*>(&header.headerSize), sizeof(header.headerSize));
    if (!ValidateHeaderSize(header.headerSize)) {
        spdlog::error("[SaveLoadResilience] Invalid header size: {}", header.headerSize);
        return false;
    }
    
    // Read version
    file.read(reinterpret_cast<char*>(&header.version), sizeof(header.version));
    
    // Read save number
    file.read(reinterpret_cast<char*>(&header.saveNumber), sizeof(header.saveNumber));
    
    spdlog::debug("[SaveLoadResilience] Header: magic=0x{:08X}, size={}, version={}, saveNum={}", 
                  header.magic, header.headerSize, header.version, header.saveNumber);
    
    return true;
}

bool SaveLoadResilience::ValidateMagicNumber(uint32_t magic) {
    return magic == SAVE_MAGIC;
}

bool SaveLoadResilience::ValidateHeaderSize(uint32_t size) {
    return size >= MIN_HEADER_SIZE && size <= MAX_HEADER_SIZE;
}

bool SaveLoadResilience::ValidateVersion(uint32_t version) {
    bool isValid = version >= SUPPORTED_VERSION_MIN && version <= SUPPORTED_VERSION_MAX;
    if (!isValid) {
        spdlog::warn("[SaveLoadResilience] Version {} outside supported range [{}, {}]",
                     version, SUPPORTED_VERSION_MIN, SUPPORTED_VERSION_MAX);
    }
    return isValid;
}

bool SaveLoadResilience::ValidateChunkSizes(const std::filesystem::path& savePath) {
    auto chunks = ParseChunks(savePath);
    if (chunks.empty()) {
        spdlog::warn("[SaveLoadResilience] No chunks found in save file");
        return false;
    }
    
    auto fileSize = std::filesystem::file_size(savePath);
    bool allValid = true;
    
    for (const auto& chunk : chunks) {
        if (!ValidateChunk(chunk, fileSize)) {
            spdlog::warn("[SaveLoadResilience] Invalid chunk: type={}, size={}, offset={}",
                         chunk.type, chunk.length, chunk.offset);
            allValid = false;
        }
    }
    
    return allValid;
}

std::vector<SaveChunk> SaveLoadResilience::ParseChunks(const std::filesystem::path& savePath) {
    std::vector<SaveChunk> chunks;
    std::ifstream file(savePath, std::ios::binary);
    
    if (!file.is_open()) {
        spdlog::error("[SaveLoadResilience] Failed to open save file for chunk parsing");
        return chunks;
    }
    
    // Skip header (simplified - real implementation would parse full header)
    SaveFileHeader header;
    if (!ReadHeader(file, header)) {
        return chunks;
    }
    
    // Skip to chunk data (after header)
    file.seekg(header.headerSize, std::ios::beg);
    
    // Parse chunks until end of file
    while (file.good() && !file.eof()) {
        SaveChunk chunk;
        chunk.offset = file.tellg();
        
        // Read chunk header
        file.read(reinterpret_cast<char*>(&chunk.type), sizeof(chunk.type));
        if (file.eof()) break;
        
        file.read(reinterpret_cast<char*>(&chunk.version), sizeof(chunk.version));
        file.read(reinterpret_cast<char*>(&chunk.length), sizeof(chunk.length));
        
        chunk.isValid = IsChunkSizeReasonable(chunk.length);
        chunks.push_back(chunk);
        
        // Skip chunk data
        if (chunk.isValid && chunk.length > 0) {
            file.seekg(chunk.length, std::ios::cur);
        } else {
            // Invalid chunk, stop parsing
            break;
        }
    }
    
    spdlog::debug("[SaveLoadResilience] Parsed {} chunks", chunks.size());
    return chunks;
}

bool SaveLoadResilience::ValidateChunk(const SaveChunk& chunk, uint64_t fileSize) {
    // Check if chunk extends beyond file
    if (chunk.offset + chunk.length > fileSize) {
        return false;
    }
    
    // Check if chunk size is reasonable
    if (!IsChunkSizeReasonable(chunk.length)) {
        return false;
    }
    
    return true;
}

bool SaveLoadResilience::IsChunkSizeReasonable(uint32_t size) {
    // Chunks should be between 0 and 100MB
    constexpr uint32_t MAX_CHUNK_SIZE = 100 * 1024 * 1024;
    return size <= MAX_CHUNK_SIZE;
}

bool SaveLoadResilience::LoadSavePartial(const std::filesystem::path& savePath) {
    spdlog::info("[SaveLoadResilience] Attempting partial load of: {}", savePath.string());
    
    std::ifstream file(savePath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("[SaveLoadResilience] Failed to open save file for partial load");
        return false;
    }
    
    // Parse chunks
    auto chunks = ParseChunks(savePath);
    if (chunks.empty()) {
        spdlog::error("[SaveLoadResilience] No valid chunks found");
        return false;
    }
    
    int loadedChunks = 0;
    int skippedChunks = 0;
    
    for (const auto& chunk : chunks) {
        if (chunk.isValid) {
            if (LoadValidData(file, chunk)) {
                loadedChunks++;
            } else {
                spdlog::warn("[SaveLoadResilience] Failed to load chunk at offset {}", chunk.offset);
            }
        } else {
            if (SkipCorruptedSection(file, chunk)) {
                skippedChunks++;
                spdlog::info("[SaveLoadResilience] Skipped corrupted chunk at offset {}", chunk.offset);
            }
        }
    }
    
    spdlog::info("[SaveLoadResilience] Partial load complete: {} loaded, {} skipped",
                 loadedChunks, skippedChunks);
    
    return loadedChunks > 0;
}

bool SaveLoadResilience::SkipCorruptedSection(std::ifstream& file, const SaveChunk& chunk) {
    // Skip to next chunk or end of file
    try {
        file.seekg(chunk.offset + sizeof(chunk.type) + sizeof(chunk.version) + sizeof(chunk.length), 
                   std::ios::beg);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[SaveLoadResilience] Failed to skip corrupted section: {}", e.what());
        return false;
    }
}

bool SaveLoadResilience::LoadValidData(std::ifstream& file, const SaveChunk& chunk) {
    // Seek to chunk data
    file.seekg(chunk.offset + sizeof(chunk.type) + sizeof(chunk.version) + sizeof(chunk.length),
               std::ios::beg);
    
    // Read chunk data
    std::vector<uint8_t> data(chunk.length);
    file.read(reinterpret_cast<char*>(data.data()), chunk.length);
    
    if (!file.good()) {
        spdlog::error("[SaveLoadResilience] Failed to read chunk data");
        return false;
    }
    
    // Process chunk based on type
    switch (static_cast<ChunkType>(chunk.type)) {
        case ChunkType::FormIDs:
        case ChunkType::FormIDArray: {
            // Extract FormIDs and validate
            std::vector<uint32_t> formIDs;
            for (size_t i = 0; i + 3 < data.size(); i += 4) {
                uint32_t formID = *reinterpret_cast<uint32_t*>(&data[i]);
                formIDs.push_back(formID);
            }
            return RemoveInvalidFormIDs(formIDs);
        }
        
        default:
            // For other chunk types, just validate the data is readable
            return ValidateDataBeforeWriting(data);
    }
}

bool SaveLoadResilience::RemoveInvalidFormIDs(std::vector<uint32_t>& formIDs) {
    size_t originalCount = formIDs.size();
    
    // Remove invalid FormIDs
    formIDs.erase(
        std::remove_if(formIDs.begin(), formIDs.end(),
            [this](uint32_t formID) {
                if (!IsFormIDValid(formID)) {
                    invalidFormIDs_.push_back(formID);
                    spdlog::warn("[SaveLoadResilience] Removed invalid FormID: 0x{:08X}", formID);
                    return true;
                }
                return false;
            }),
        formIDs.end()
    );
    
    size_t removedCount = originalCount - formIDs.size();
    if (removedCount > 0) {
        spdlog::info("[SaveLoadResilience] Removed {} invalid FormIDs", removedCount);
    }
    
    return true;
}

bool SaveLoadResilience::IsFormIDValid(uint32_t formID) {
    // Check if FormID is in valid range
    if (formID == 0 || formID == 0xFFFFFFFF) {
        return false;
    }
    
    // Extract plugin index (top byte)
    uint8_t pluginIndex = (formID >> 24) & 0xFF;
    
    // Check if plugin is loaded
    return IsPluginLoaded(pluginIndex);
}

bool SaveLoadResilience::IsPluginLoaded(uint8_t pluginIndex) {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        return false;
    }

    // Walk to the exact index and verify the plugin entry is non-null.
    // Previous bug: `count >= pluginIndex` returned true as soon as the iterator
    // reached the target index without ever checking whether a valid plugin sits there.
    uint8_t count = 0;
    for (auto it = dataHandler->files.begin(); it != dataHandler->files.end(); ++it, ++count) {
        if (count == pluginIndex) {
            return (*it) != nullptr;
        }
    }

    // pluginIndex exceeds the number of loaded plugins
    return false;
}

bool SaveLoadResilience::DisableBrokenScripts(const std::vector<std::string>& scriptNames) {
    spdlog::info("[SaveLoadResilience] Disabling {} broken scripts", scriptNames.size());
    
    for (const auto& scriptName : scriptNames) {
        if (IsScriptBroken(scriptName)) {
            BlacklistScript(scriptName);
            spdlog::warn("[SaveLoadResilience] Disabled broken script: {}", scriptName);
        }
    }
    
    return true;
}

bool SaveLoadResilience::IsScriptBroken(const std::string& scriptName) {
    // Check if script is in blacklist
    return std::find(blacklistedScripts_.begin(), blacklistedScripts_.end(), scriptName) 
           != blacklistedScripts_.end();
}

void SaveLoadResilience::BlacklistScript(const std::string& scriptName) {
    if (!IsScriptBroken(scriptName)) {
        blacklistedScripts_.push_back(scriptName);
        spdlog::info("[SaveLoadResilience] Blacklisted script: {}", scriptName);
    }
}

std::optional<std::filesystem::path> SaveLoadResilience::FindPreviousAutosave(
    const std::filesystem::path& savePath) {
    
    spdlog::info("[SaveLoadResilience] Searching for previous autosave");
    
    // Get save directory
    auto saveDir = savePath.parent_path();
    auto autosaves = GetAutosaveList(saveDir);
    
    if (autosaves.empty()) {
        spdlog::warn("[SaveLoadResilience] No autosaves found");
        return std::nullopt;
    }
    
    // Sort by modification time (newest first)
    std::sort(autosaves.begin(), autosaves.end(),
        [](const std::filesystem::path& a, const std::filesystem::path& b) {
            return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
        });
    
    // Return the most recent autosave that's not the current save
    for (const auto& autosave : autosaves) {
        if (autosave != savePath) {
            spdlog::info("[SaveLoadResilience] Found previous autosave: {}", autosave.string());
            return autosave;
        }
    }
    
    return std::nullopt;
}

std::vector<std::filesystem::path> SaveLoadResilience::GetAutosaveList(
    const std::filesystem::path& saveDir) {
    
    std::vector<std::filesystem::path> autosaves;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(saveDir)) {
            if (entry.is_regular_file()) {
                auto filename = entry.path().filename().string();
                // Check if it's an autosave (starts with "Autosave" or "Save")
                if (filename.find("Autosave") == 0 || filename.find("Save") == 0) {
                    if (entry.path().extension() == ".ess") {
                        autosaves.push_back(entry.path());
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        spdlog::error("[SaveLoadResilience] Failed to list autosaves: {}", e.what());
    }
    
    spdlog::debug("[SaveLoadResilience] Found {} autosaves", autosaves.size());
    return autosaves;
}

bool SaveLoadResilience::OfferLoadPreviousAutosave(const std::filesystem::path& currentSave) {
    auto previousSave = FindPreviousAutosave(currentSave);
    
    if (!previousSave) {
        spdlog::warn("[SaveLoadResilience] No previous autosave available");
        return false;
    }
    
    // Validate the previous save
    auto validation = ValidateSaveFile(*previousSave);
    
    if (!validation.canLoad) {
        spdlog::error("[SaveLoadResilience] Previous autosave is also corrupted");
        return false;
    }
    
    // Warn about potential issues
    if (!WarnAboutPotentialIssues(validation)) {
        spdlog::info("[SaveLoadResilience] User declined to load previous autosave");
        return false;
    }
    
    // Log the recommendation and return true so the caller knows a valid fallback
    // save exists. Triggering an actual load from within a crash handler isn't safe
    // — it requires the game's save system to be in a stable state, which it may
    // not be at this point. The user should load the save manually from the main menu.
    spdlog::info("[SaveLoadResilience] Fallback save available: {}",
                 previousSave->string());
    return true;
}

bool SaveLoadResilience::WarnAboutPotentialIssues(const SaveValidationResult& validation) {
    if (validation.warnings.empty() && validation.errors.empty()) {
        return true;  // No issues to warn about
    }
    
    spdlog::warn("[SaveLoadResilience] Save file has potential issues:");
    
    for (const auto& error : validation.errors) {
        spdlog::warn("[SaveLoadResilience]   ERROR: {}", error);
    }
    
    for (const auto& warning : validation.warnings) {
        spdlog::warn("[SaveLoadResilience]   WARNING: {}", warning);
    }
    
    // Showing a blocking dialog from inside a save-load callback isn't safe
    // — it can freeze the game's loading thread. The warnings are written to
    // the CrashGuard log file where the user can review them after the fact.
    return true;
}

bool SaveLoadResilience::ValidateDataBeforeWriting(const std::vector<uint8_t>& data) {
    // Basic validation: check data is not empty and not too large
    if (data.empty()) {
        spdlog::warn("[SaveLoadResilience] Data is empty");
        return false;
    }
    
    constexpr size_t MAX_DATA_SIZE = 100 * 1024 * 1024;  // 100MB
    if (data.size() > MAX_DATA_SIZE) {
        spdlog::warn("[SaveLoadResilience] Data too large: {} bytes", data.size());
        return false;
    }
    
    // Check for obvious corruption patterns (all zeros, all 0xFF, etc.)
    bool allZeros = std::all_of(data.begin(), data.end(), [](uint8_t b) { return b == 0; });
    bool allOnes = std::all_of(data.begin(), data.end(), [](uint8_t b) { return b == 0xFF; });
    
    if (allZeros || allOnes) {
        spdlog::warn("[SaveLoadResilience] Data appears corrupted (uniform pattern)");
        return false;
    }
    
    return true;
}

bool SaveLoadResilience::ValidateFormIDAgainstPlugins(uint32_t formID) {
    return IsFormIDValid(formID);
}

bool SaveLoadResilience::ValidateScriptData(const std::vector<uint8_t>& scriptData) {
    return ValidateDataBeforeWriting(scriptData);
}

bool SaveLoadResilience::ValidatePluginInfo(std::ifstream& file) {
    // Simplified plugin info validation
    // Real implementation would parse the full plugin list
    return file.good();
}

} // namespace CrashGuard
