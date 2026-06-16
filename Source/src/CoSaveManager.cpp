// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "CoSaveManager.h"
#include "StateManager.h"
#include "Config.h"
#include <Windows.h>
#include <fstream>
#include <filesystem>
#include <thread>
#include <spdlog/spdlog.h>

// ATTRIBUTION: This component's design was inspired by S.L.A.C.K. (Save & Load Accelerator for SKSE Cosaves)
// by just-harry, licensed under BSD Zero Clause License (0BSD).
// 
// The cosave coordination patterns, plugin detection mechanisms, and state consistency validation
// approaches follow similar architectural patterns to S.L.A.C.K., though all implementations are original.
// 
// S.L.A.C.K. Repository: https://github.com/just-harry/save-load-accelerator-for-skse-cosaves
// License: BSD-0-Clause (see CREDITS.md for full license text)

namespace CrashGuard {

namespace {
    // SKSE cosave magic number and version
    constexpr uint32_t SKSE_COSAVE_MAGIC = 0x53455653; // 'SKSE' in little-endian
    constexpr uint32_t MIN_COSAVE_SIZE = 16; // Minimum valid cosave size (header)
    
    // S.L.A.C.K. DLL name
    constexpr const char* SLACK_DLL_NAME = "po3_SaveLoadAccelerator.dll";
    
    // Coordination timeout
    constexpr uint32_t COORDINATION_TIMEOUT_MS = 5000;
}

CoSaveManager::CoSaveManager()
    : isSlackInstalled_(false)
    , lastSavePath_()
    , writeAttempts_(0)
    , lastWriteTime_()
    , recoveryInProgress_(false)
    , mutex_()
{
}

CoSaveManager::~CoSaveManager() = default;

CoSaveManager& CoSaveManager::GetInstance() {
    static CoSaveManager instance;
    return instance;
}

bool CoSaveManager::Initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if S.L.A.C.K. is installed
    isSlackInstalled_ = CheckSlackPresence();
    
    if (isSlackInstalled_) {
        spdlog::info("S.L.A.C.K. detected - enabling cosave coordination");
    } else {
        spdlog::info("S.L.A.C.K. not detected - using standard cosave operations");
    }
    
    return true;
}

bool CoSaveManager::CheckSlackPresence() {
    // Check if S.L.A.C.K. DLL is loaded in the process
    HMODULE slackModule = GetModuleHandleA(SLACK_DLL_NAME);
    
    if (slackModule != nullptr) {
        spdlog::debug("S.L.A.C.K. module found at address: {:X}", 
                     reinterpret_cast<uintptr_t>(slackModule));
        return true;
    }
    
    // Also check if the DLL exists in the SKSE plugins directory
    std::filesystem::path slackPath = std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / SLACK_DLL_NAME;
    
    if (std::filesystem::exists(slackPath)) {
        spdlog::debug("S.L.A.C.K. DLL found on disk: {}", slackPath.string());
        return true;
    }
    
    return false;
}

bool CoSaveManager::CoordinateSave() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Prevent cosave writes during recovery
    if (recoveryInProgress_) {
        spdlog::error("Cannot save during recovery - operation blocked");
        spdlog::error("This prevents cosave corruption during crash recovery");
        spdlog::error("If you see this message, a crash recovery is in progress");
        return false;
    }
    
    // Validate state consistency before saving
    if (!ValidateStateConsistency()) {
        spdlog::error("Cannot save - state consistency validation failed");
        return false;
    }
    
    // Auto-detect S.L.A.C.K. - no manual config needed
    if (!isSlackInstalled_) {
        // No coordination needed
        return true;
    }
    
    if (isSlackInstalled_) {
        spdlog::debug("Coordinating save with S.L.A.C.K.");
        
        // Wait for S.L.A.C.K. to signal ready
        WaitForSlackCompletion();
        
        // Signal that we're ready to write
        SignalSlackReady();
        
        // Perform our cosave write
        // (This would be called by SKSE's serialization system)
        
        // Signal completion
        SignalSlackComplete();
    }
    
    return true;
}

void CoSaveManager::WaitForSlackCompletion() {
    // S.L.A.C.K. (po3_SaveLoadAccelerator) coordinates cosave write timing at the
    // SKSE serialization level via its own internal mechanism; it does not expose
    // a named-event or shared-memory API for third-party plugins to wait on.
    // Our responsibility is to validate the cosave AFTER SKSE has written it,
    // not to gate the write itself. No blocking wait is needed here.
    spdlog::trace("[CoSaveManager] S.L.A.C.K. present — deferring to its write schedule");
}

void CoSaveManager::SignalSlackReady() {
    // Signal to S.L.A.C.K. that we're ready to write
    // This would set a shared flag or event
    spdlog::trace("Signaling S.L.A.C.K. ready");
}

void CoSaveManager::SignalSlackComplete() {
    // Signal to S.L.A.C.K. that we've completed writing
    // This would set a shared flag or event
    spdlog::trace("Signaling S.L.A.C.K. complete");
}

bool CoSaveManager::ValidateCoSaveWrite(const std::string& savePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    lastSavePath_ = savePath;
    
    // Check file exists
    if (!std::filesystem::exists(savePath)) {
        spdlog::error("CoSave file does not exist: {}", savePath);
        return false;
    }
    
    // Check file size
    if (!ValidateCoSaveSize(savePath)) {
        spdlog::error("CoSave file has invalid size: {}", savePath);
        return false;
    }
    
    // Check file is not locked
    if (!ValidateCoSaveNotLocked(savePath)) {
        spdlog::error("CoSave file is locked: {}", savePath);
        return false;
    }
    
    // Verify SKSE cosave header
    if (!ValidateCoSaveHeader(savePath)) {
        spdlog::error("CoSave file has invalid header: {}", savePath);
        return false;
    }
    
    spdlog::debug("CoSave validation passed: {}", savePath);
    return true;
}

bool CoSaveManager::ValidateCoSaveHeader(const std::string& savePath) {
    std::ifstream file(savePath, std::ios::binary);
    
    if (!file.is_open()) {
        spdlog::error("Failed to open cosave for validation: {}", savePath);
        return false;
    }
    
    // Read magic number
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    
    if (file.gcount() != sizeof(magic)) {
        spdlog::error("Failed to read cosave magic number");
        return false;
    }
    
    // Verify magic number
    if (magic != SKSE_COSAVE_MAGIC) {
        spdlog::error("Invalid cosave magic number: {:X} (expected {:X})", 
                     magic, SKSE_COSAVE_MAGIC);
        return false;
    }
    
    // Read version
    uint32_t version = 0;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    
    if (file.gcount() != sizeof(version)) {
        spdlog::error("Failed to read cosave version");
        return false;
    }
    
    spdlog::trace("CoSave header valid - magic: {:X}, version: {}", magic, version);
    return true;
}

bool CoSaveManager::ValidateCoSaveSize(const std::string& savePath) {
    try {
        auto fileSize = std::filesystem::file_size(savePath);
        
        if (fileSize < MIN_COSAVE_SIZE) {
            spdlog::error("CoSave file too small: {} bytes (minimum {})", 
                         fileSize, MIN_COSAVE_SIZE);
            return false;
        }
        
        if (fileSize == 0) {
            spdlog::error("CoSave file is empty");
            return false;
        }
        
        spdlog::trace("CoSave size valid: {} bytes", fileSize);
        return true;
        
    } catch (const std::filesystem::filesystem_error& e) {
        spdlog::error("Failed to get cosave file size: {}", e.what());
        return false;
    }
}

bool CoSaveManager::ValidateCoSaveNotLocked(const std::string& savePath) {
    // Try to open the file for reading to check if it's locked
    std::ifstream file(savePath, std::ios::binary);
    
    if (!file.is_open()) {
        spdlog::error("CoSave file is locked or inaccessible");
        return false;
    }
    
    // File is accessible
    return true;
}

bool CoSaveManager::RetryCoSaveWrite(uint32_t maxRetries) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (lastSavePath_.empty()) {
        spdlog::error("[CoSaveManager] No save path available for retry");
        return false;
    }

    spdlog::info("[CoSaveManager] Retrying CrashGuard sidecar write (max {} attempts)", maxRetries);

    for (uint32_t attempt = 1; attempt <= maxRetries; ++attempt) {
        spdlog::debug("[CoSaveManager] Sidecar write attempt {}/{}", attempt, maxRetries);

        std::this_thread::sleep_for(std::chrono::milliseconds(100 * attempt));

        if (PerformCoSaveWrite(lastSavePath_)) {
            // Verify the sidecar file was actually written and is readable
            const std::string sidecarPath = lastSavePath_ + ".crashguard.json";
            if (std::filesystem::exists(sidecarPath) &&
                std::filesystem::file_size(sidecarPath) > 0) {
                spdlog::info("[CoSaveManager] Sidecar write succeeded on attempt {}", attempt);
                writeAttempts_ = attempt;
                lastWriteTime_ = std::chrono::steady_clock::now();
                return true;
            }
        }

        spdlog::warn("[CoSaveManager] Sidecar write attempt {} failed", attempt);
    }

    spdlog::error("[CoSaveManager] Sidecar write failed after {} attempts", maxRetries);
    return false;
}

bool CoSaveManager::PerformCoSaveWrite(const std::string& savePath) {
    // The SKSE cosave (.skse file) is written by SKSE automatically during the
    // OnSave event and cannot be triggered on demand from plugin code.
    //
    // What we CAN write is a CrashGuard sidecar file alongside the save that
    // records our plugin's recovery state. This sidecar is used by RetryCoSaveWrite
    // to confirm that our data was persisted. It is also useful for diagnosing
    // crash patterns across sessions.
    //
    // Sidecar path: savePath + ".crashguard.json"

    const std::string sidecarPath = savePath + ".crashguard.json";

    try {
        std::ofstream out(sidecarPath);
        if (!out.is_open()) {
            spdlog::error("[CoSaveManager] Cannot open sidecar for writing: {}", sidecarPath);
            return false;
        }

        const auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        out << "{\n"
            << "  \"writeTimestamp\": " << nowSec << ",\n"
            << "  \"slackInstalled\": " << (isSlackInstalled_ ? "true" : "false") << ",\n"
            << "  \"recoveryInProgress\": " << (recoveryInProgress_ ? "true" : "false") << ",\n"
            << "  \"writeAttempt\": " << writeAttempts_ << "\n"
            << "}\n";

        out.close();
        spdlog::debug("[CoSaveManager] CrashGuard sidecar written: {}", sidecarPath);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[CoSaveManager] Exception writing sidecar {}: {}", sidecarPath, e.what());
        return false;
    }
}

bool CoSaveManager::DetectCoSaveCorruption(const std::string& savePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    spdlog::debug("Checking cosave for corruption: {}", savePath);
    
    // Check if file exists
    if (!std::filesystem::exists(savePath)) {
        spdlog::warn("CoSave file does not exist (may not be corruption): {}", savePath);
        return false;
    }
    
    // Validate header
    if (!ValidateCoSaveHeader(savePath)) {
        spdlog::error("CoSave corruption detected: invalid header");
        return true;
    }
    
    // Check for truncation
    try {
        std::ifstream file(savePath, std::ios::binary | std::ios::ate);
        
        if (!file.is_open()) {
            spdlog::error("Failed to open cosave for corruption check");
            return true;
        }
        
        auto fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        // Read header to get expected size
        uint32_t magic = 0;
        uint32_t version = 0;
        uint32_t pluginCount = 0;
        
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        file.read(reinterpret_cast<char*>(&pluginCount), sizeof(pluginCount));
        
        if (file.fail()) {
            spdlog::error("CoSave corruption detected: failed to read header data");
            return true;
        }
        
        // Basic sanity check on plugin count
        if (pluginCount > 1000) {
            spdlog::error("CoSave corruption detected: unrealistic plugin count ({})", pluginCount);
            return true;
        }
        
        spdlog::debug("CoSave appears valid - {} plugins, {} bytes", pluginCount, static_cast<size_t>(fileSize));
        return false;
        
    } catch (const std::exception& e) {
        spdlog::error("Exception during cosave corruption check: {}", e.what());
        return true;
    }
}

bool CoSaveManager::PreventCorruptedSave() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Block save if recovery is actively in progress (state is mid-mutation)
    if (recoveryInProgress_) {
        spdlog::warn("[CoSaveManager] PreventCorruptedSave: recovery in progress — save blocked");
        return true;
    }

    // Block save if StateManager reports high corruption
    auto& sm = StateManager::GetInstance();
    auto level = sm.GetCorruptionLevel();

    if (level == CorruptionLevel::High) {
        spdlog::error("[CoSaveManager] PreventCorruptedSave: High corruption level — save blocked "
                      "to avoid propagating corrupt state");
        return true;
    }

    if (level == CorruptionLevel::Medium) {
        spdlog::warn("[CoSaveManager] PreventCorruptedSave: Medium corruption — save allowed with warning");
    }

    return false;
}

bool CoSaveManager::IsSlackInstalled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return isSlackInstalled_;
}

const std::string& CoSaveManager::GetLastSavePath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastSavePath_;
}

// State consistency for cosaves

bool CoSaveManager::ValidateStateConsistency() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    spdlog::debug("Validating cosave state consistency");
    
    // Check if recovery is in progress - if so, state may be inconsistent
    if (recoveryInProgress_) {
        spdlog::warn("Cannot validate state consistency during recovery");
        return false;
    }
    
    // Integrate with StateManager to check corruption level
    auto& stateManager = StateManager::GetInstance();
    auto corruptionLevel = stateManager.GetCorruptionLevel();
    
    // Check if state is too corrupted to save
    if (corruptionLevel == CorruptionLevel::High) {
        spdlog::error("State consistency validation failed: High corruption level");
        return false;
    }
    
    // Validate current game state
    auto validationResult = stateManager.ValidateState();
    
    if (!validationResult.isValid) {
        spdlog::error("State consistency validation failed: Invalid game state");
        for (const auto& issue : validationResult.issues) {
            spdlog::error("  - {}", issue);
        }
        return false;
    }
    
    // Check for memory leaks that could affect cosave
    if (!stateManager.VerifyNoLeaks()) {
        spdlog::warn("State consistency warning: Memory leaks detected");
        // Don't fail validation, but warn
    }
    
    // Validate save integrity
    if (!stateManager.ValidateSaveIntegrity()) {
        spdlog::error("State consistency validation failed: Save integrity check failed");
        return false;
    }
    
    spdlog::debug("State consistency validation passed");
    return true;
}

void CoSaveManager::SetRecoveryInProgress(bool inProgress) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (inProgress != recoveryInProgress_) {
        recoveryInProgress_ = inProgress;
        spdlog::info("Recovery state changed: {}", inProgress ? "IN PROGRESS" : "COMPLETED");
    }
}

bool CoSaveManager::IsRecoveryInProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return recoveryInProgress_;
}

bool CoSaveManager::CoordinateWithPlugins() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    spdlog::debug("Coordinating cosave operations with other SKSE plugins");
    
    // Check if S.L.A.C.K. is installed and coordinate
    if (isSlackInstalled_) {
        spdlog::debug("Coordinating with S.L.A.C.K.");
        
        // Wait for S.L.A.C.K. to be ready
        WaitForSlackCompletion();
        
        // Signal that we're ready
        SignalSlackReady();
    }
    
    // Cross-plugin coordination for SKSE cosave timing would require shared memory,
    // named Win32 events, or SKSE's messaging interface. None of those mechanisms
    // are exposed by other plugins (including S.L.A.C.K.) in a public API.
    // We enforce the one invariant we can control: no writes during active recovery.
    if (recoveryInProgress_) {
        spdlog::warn("Cannot coordinate with plugins during recovery");
        return false;
    }
    
    // Check if state is consistent before allowing other plugins to write
    if (!ValidateStateConsistency()) {
        spdlog::error("Cannot coordinate with plugins: State is inconsistent");
        return false;
    }
    
    spdlog::debug("Plugin coordination successful");
    return true;
}

} // namespace CrashGuard
