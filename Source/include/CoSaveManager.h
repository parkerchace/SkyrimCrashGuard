// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

namespace CrashGuard {

// CoSaveManager class - ensures SKSE cosave integrity and S.L.A.C.K. compatibility
//
// ATTRIBUTION: This component's design was inspired by S.L.A.C.K. (Save & Load Accelerator for SKSE Cosaves)
// by just-harry, licensed under BSD Zero Clause License (0BSD).
// 
// The cosave coordination patterns, plugin detection mechanisms, and state consistency validation
// approaches follow similar architectural patterns to S.L.A.C.K., though all implementations are original.
// 
// S.L.A.C.K. Repository: https://github.com/just-harry/save-load-accelerator-for-skse-cosaves
// License: BSD-0-Clause (see CREDITS.md for full license text)
class CoSaveManager {
public:
    // Singleton access
    static CoSaveManager& GetInstance();
    
    // Delete copy/move constructors
    CoSaveManager(const CoSaveManager&) = delete;
    CoSaveManager& operator=(const CoSaveManager&) = delete;
    CoSaveManager(CoSaveManager&&) = delete;
    CoSaveManager& operator=(CoSaveManager&&) = delete;
    
    // Initialize and detect S.L.A.C.K.
    bool Initialize();
    
    // Coordinate save operation
    bool CoordinateSave();
    
    // Validate cosave write succeeded
    bool ValidateCoSaveWrite(const std::string& savePath);
    
    // Retry failed cosave write
    bool RetryCoSaveWrite(uint32_t maxRetries = 3);
    
    // Detect cosave corruption
    bool DetectCoSaveCorruption(const std::string& savePath);
    
    // Prevent save if corruption detected
    bool PreventCorruptedSave();
    
    // Check if S.L.A.C.K. is installed
    bool IsSlackInstalled() const;
    
    // Get last save path
    const std::string& GetLastSavePath() const;
    
    // Ensure cosave data matches game state
    bool ValidateStateConsistency();
    
    // Prevent cosave writes during recovery
    void SetRecoveryInProgress(bool inProgress);
    bool IsRecoveryInProgress() const;
    
    // Coordinate with other SKSE plugins
    bool CoordinateWithPlugins();
    
private:
    CoSaveManager();
    ~CoSaveManager();
    
    // S.L.A.C.K. detection
    bool CheckSlackPresence();
    
    // S.L.A.C.K. coordination
    void WaitForSlackCompletion();
    void SignalSlackReady();
    void SignalSlackComplete();
    
    // Cosave validation helpers
    bool ValidateCoSaveHeader(const std::string& savePath);
    bool ValidateCoSaveSize(const std::string& savePath);
    bool ValidateCoSaveNotLocked(const std::string& savePath);
    
    // Cosave write helpers
    bool PerformCoSaveWrite(const std::string& savePath);
    
    // Member variables
    bool isSlackInstalled_;
    std::string lastSavePath_;
    uint32_t writeAttempts_;
    std::chrono::steady_clock::time_point lastWriteTime_;
    
    // Recovery state tracking
    bool recoveryInProgress_;
    
    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace CrashGuard
