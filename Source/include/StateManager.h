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

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace CrashGuard {

// Forward declaration
struct StateValidationResult;

// Corruption level tracking
enum class CorruptionLevel {
    None,
    Low,      // Allow saving
    Medium,   // Warn before saving
    High      // Prevent saving
};

// State validation result
struct StateValidationResult {
    bool isValid;
    std::vector<std::string> issues;
    CorruptionLevel recommendedLevel;
};

// StateManager class - manages transactional state snapshots and rollback
class StateManager {
public:
    // Singleton access
    static StateManager& GetInstance();
    
    // Delete copy/move constructors
    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;
    StateManager(StateManager&&) = delete;
    StateManager& operator=(StateManager&&) = delete;
    
    // Validate current game state
    StateValidationResult ValidateState();
    
    // Mark session as corrupted
    void MarkSessionCorrupted(CorruptionLevel level);
    
    // Check if saving is safe
    bool IsSavingSafe() const;
    
    // Get current corruption level
    CorruptionLevel GetCorruptionLevel() const;
    
    // Dangling pointer detection
    bool ScanForDanglingPointers(const std::vector<void*>& removedObjects);
    void NullifyDanglingPointers(const std::vector<void*>& removedObjects);
    
    // Memory leak prevention
    void TrackAllocation(void* ptr, size_t size);
    void TrackDeallocation(void* ptr);
    bool VerifyNoLeaks();
    
    // Save integrity validation
    bool ValidateSaveIntegrity();
    bool WarnBeforeSave();
    bool PreventSave();
    void TagSaveWithMetadata(const std::string& savePath);
    
private:
    StateManager();
    ~StateManager();
    
    // Dangling pointer detection helpers
    bool IsPointerInRegisters(void* ptr);
    bool IsPointerInStack(void* ptr);
    bool IsPointerInHeap(void* ptr);
    void NullifyPointerInRegisters(void* ptr);
    void NullifyPointerInStack(void* ptr);
    void NullifyPointerInHeap(void* ptr);
    
    // Memory tracking helpers
    struct AllocationInfo {
        size_t size;
        std::chrono::steady_clock::time_point timestamp;
        bool isLeaked;
    };
    
    // Member variables
    CorruptionLevel sessionCorruption_;
    
    // Memory tracking
    std::unordered_map<void*, AllocationInfo> trackedAllocations_;
    size_t totalAllocatedMemory_;
    
    // Thread safety
    mutable std::shared_mutex mutex_;
};

} // namespace CrashGuard
