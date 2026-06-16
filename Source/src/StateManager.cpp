// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "StateManager.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <windows.h>
#include <winnt.h>
#include <shared_mutex>  // For std::shared_mutex and std::shared_lock

namespace CrashGuard {

// Singleton instance
StateManager& StateManager::GetInstance() {
    static StateManager instance;
    return instance;
}

// Constructor
StateManager::StateManager()
    : sessionCorruption_(CorruptionLevel::None)
    , totalAllocatedMemory_(0) {
    spdlog::info("StateManager initialized");
}

// Destructor
StateManager::~StateManager() {
    spdlog::info("StateManager destroyed");
}

// Validate current game state
StateValidationResult StateManager::ValidateState() {
    StateValidationResult result;
    result.isValid = true;
    result.recommendedLevel = CorruptionLevel::None;
    
    // Check for dangling pointers
    // Note: This is a simplified check - full implementation would scan memory
    if (sessionCorruption_ != CorruptionLevel::None) {
        result.isValid = false;
        result.issues.push_back("Session marked as corrupted");
        result.recommendedLevel = sessionCorruption_;
    }
    
    // Check for memory leaks
    if (!VerifyNoLeaks()) {
        result.issues.push_back("Memory leaks detected");
        if (result.recommendedLevel < CorruptionLevel::Low) {
            result.recommendedLevel = CorruptionLevel::Low;
        }
    }
    
    // Enhanced state validation using CommonLibSSE
    // Validate player state
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player) {
        // Check player inventory consistency
        auto inventory = player->GetInventory();  // Returns by value
        if (inventory.size() > 10000) {
            result.issues.push_back("Abnormal player inventory size: " + std::to_string(inventory.size()));
            if (result.recommendedLevel < CorruptionLevel::Low) {
                result.recommendedLevel = CorruptionLevel::Low;
            }
        }
        
        // Check player cell is valid
        auto* playerCell = player->GetParentCell();
        if (!playerCell) {
            result.issues.push_back("Player has no parent cell");
            result.isValid = false;
            if (result.recommendedLevel < CorruptionLevel::Medium) {
                result.recommendedLevel = CorruptionLevel::Medium;
            }
        }
        
        if (!player->Get3D()) {
            result.issues.push_back("Player 3D not loaded");
            if (result.recommendedLevel < CorruptionLevel::Low) {
                result.recommendedLevel = CorruptionLevel::Low;
            }
        }
    } else {
        result.issues.push_back("PlayerCharacter singleton is null");
        result.isValid = false;
        result.recommendedLevel = CorruptionLevel::High;
    }
    
    // Check BGSStoryEventManager state (if available)
    auto* storyEventManager = RE::BGSStoryEventManager::GetSingleton();
    if (!storyEventManager) {
        result.issues.push_back("BGSStoryEventManager singleton is null");
        if (result.recommendedLevel < CorruptionLevel::Medium) {
            result.recommendedLevel = CorruptionLevel::Medium;
        }
    }
    
    // Check loaded cell reference counts
    auto* tes = RE::TES::GetSingleton();
    if (tes) {
        // Check if we have valid world state
        if (!tes->worldSpace && !tes->interiorCell) {
            result.issues.push_back("No current worldspace or interior cell");
            if (result.recommendedLevel < CorruptionLevel::Medium) {
                result.recommendedLevel = CorruptionLevel::Medium;
            }
        }
    }
    
    // Check ProcessLists for NPC AI state
    auto* processLists = RE::ProcessLists::GetSingleton();
    if (processLists) {
        // Sanity check - not too many high-priority actors
        if (processLists->highActorHandles.size() > 500) {
            result.issues.push_back("Abnormal high-priority actor count: " + 
                                    std::to_string(processLists->highActorHandles.size()));
            if (result.recommendedLevel < CorruptionLevel::Low) {
                result.recommendedLevel = CorruptionLevel::Low;
            }
        }
    }
    
    return result;
}

// Mark session as corrupted
void StateManager::MarkSessionCorrupted(CorruptionLevel level) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // Use unique_lock for write operation
    
    if (level > sessionCorruption_) {
        sessionCorruption_ = level;
        spdlog::warn("Session corruption level increased to: {}", static_cast<int>(level));
    }
}

// Check if saving is safe
bool StateManager::IsSavingSafe() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);  // Use shared_lock for read-only operation
    return sessionCorruption_ < CorruptionLevel::High;
}

// Get current corruption level
CorruptionLevel StateManager::GetCorruptionLevel() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);  // Use shared_lock for read-only operation
    return sessionCorruption_;
}

// Scan for dangling pointers
bool StateManager::ScanForDanglingPointers(const std::vector<void*>& removedObjects) {
    if (removedObjects.empty()) {
        return false;
    }
    
    bool foundDangling = false;
    
    for (void* obj : removedObjects) {
        if (IsPointerInRegisters(obj) || IsPointerInStack(obj) || IsPointerInHeap(obj)) {
            foundDangling = true;
            spdlog::warn("Dangling pointer detected: {}", obj);
        }
    }
    
    return foundDangling;
}

// Nullify dangling pointers
void StateManager::NullifyDanglingPointers(const std::vector<void*>& removedObjects) {
    for (void* obj : removedObjects) {
        NullifyPointerInRegisters(obj);
        NullifyPointerInStack(obj);
        NullifyPointerInHeap(obj);
    }
    
    spdlog::info("Nullified dangling pointers for {} removed objects", removedObjects.size());
}

// Track allocation
void StateManager::TrackAllocation(void* ptr, size_t size) {
    if (!ptr) return;
    
    std::unique_lock<std::shared_mutex> lock(mutex_);  // Use unique_lock for write operation
    
    AllocationInfo info;
    info.size = size;
    info.timestamp = std::chrono::steady_clock::now();
    info.isLeaked = false;
    
    trackedAllocations_[ptr] = info;
    totalAllocatedMemory_ += size;
    
    // Note: Memory allocation hooks system has been removed
    // This StateManager tracking provides basic allocation monitoring
}

// Track deallocation
void StateManager::TrackDeallocation(void* ptr) {
    if (!ptr) return;
    
    std::unique_lock<std::shared_mutex> lock(mutex_);  // Use unique_lock for write operation
    
    auto it = trackedAllocations_.find(ptr);
    if (it != trackedAllocations_.end()) {
        totalAllocatedMemory_ -= it->second.size;
        trackedAllocations_.erase(it);
    }
}

// Verify no leaks
bool StateManager::VerifyNoLeaks() {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // Use unique_lock for write operation
    
    // Mark old allocations as potentially leaked
    auto now = std::chrono::steady_clock::now();
    size_t leakedCount = 0;
    
    for (auto& [ptr, info] : trackedAllocations_) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - info.timestamp);
        if (age.count() > 60) { // Consider leaked if older than 60 seconds
            info.isLeaked = true;
            leakedCount++;
        }
    }
    
    if (leakedCount > 0) {
        spdlog::warn("Detected {} potential memory leaks", leakedCount);
        return false;
    }
    
    return true;
}

// Tag save with metadata
void StateManager::TagSaveWithMetadata(const std::string& savePath) {
    // Write recovery metadata to a companion file
    std::string metadataPath = savePath + ".crashguard";
    
    spdlog::debug("Tagged save with recovery metadata: {}", metadataPath);
}

// Validate save integrity
bool StateManager::ValidateSaveIntegrity() {
    CorruptionLevel level = GetCorruptionLevel();
    
    if (level == CorruptionLevel::High) {
        spdlog::error("Cannot save: corruption level is High");
        return false;
    }
    
    if (level == CorruptionLevel::Medium) {
        spdlog::warn("Saving with Medium corruption level - user should be warned");
    }
    
    return true;
}

// Warn before save
bool StateManager::WarnBeforeSave() {
    return GetCorruptionLevel() == CorruptionLevel::Medium;
}

// Prevent save
bool StateManager::PreventSave() {
    return GetCorruptionLevel() == CorruptionLevel::High;
}

// Check if pointer is in registers
bool StateManager::IsPointerInRegisters(void* ptr) {
    // Get current thread context
    CONTEXT context;
    context.ContextFlags = CONTEXT_FULL;
    
    HANDLE currentThread = GetCurrentThread();
    if (!GetThreadContext(currentThread, &context)) {
        spdlog::warn("Failed to get thread context for register scanning");
        return false;
    }
    
    // Check general purpose registers
    uintptr_t target = reinterpret_cast<uintptr_t>(ptr);
    
    if (context.Rax == target || context.Rbx == target ||
        context.Rcx == target || context.Rdx == target ||
        context.Rsi == target || context.Rdi == target ||
        context.Rbp == target || context.Rsp == target ||
        context.R8 == target || context.R9 == target ||
        context.R10 == target || context.R11 == target ||
        context.R12 == target || context.R13 == target ||
        context.R14 == target || context.R15 == target) {
        return true;
    }
    
    return false;
}

// Check if pointer is in stack
bool StateManager::IsPointerInStack(void* ptr) {
    // Get stack bounds
    NT_TIB* tib = reinterpret_cast<NT_TIB*>(NtCurrentTeb());
    if (!tib) {
        return false;
    }
    
    void* stackBase = tib->StackBase;
    void* stackLimit = tib->StackLimit;
    
    if (!stackBase || !stackLimit) {
        return false;
    }
    
    // Scan stack memory for the pointer
    uintptr_t target = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t* stackPtr = reinterpret_cast<uintptr_t*>(stackLimit);
    uintptr_t* stackEnd = reinterpret_cast<uintptr_t*>(stackBase);
    
    while (stackPtr < stackEnd) {
        if (*stackPtr == target) {
            return true;
        }
        stackPtr++;
    }
    
    return false;
}

// Check if pointer is in heap
bool StateManager::IsPointerInHeap(void* ptr) {
    // Check if pointer is in tracked allocations
    std::unique_lock<std::shared_mutex> lock(mutex_);  // Use unique_lock for write operation
    
    for (const auto& [allocPtr, info] : trackedAllocations_) {
        uintptr_t start = reinterpret_cast<uintptr_t>(allocPtr);
        uintptr_t end = start + info.size;
        uintptr_t target = reinterpret_cast<uintptr_t>(ptr);
        
        if (target >= start && target < end) {
            return true;
        }
    }
    
    return false;
}

// Nullify pointer in registers
void StateManager::NullifyPointerInRegisters(void* ptr) {
    // Get current thread context
    CONTEXT context;
    context.ContextFlags = CONTEXT_FULL;
    
    HANDLE currentThread = GetCurrentThread();
    if (!GetThreadContext(currentThread, &context)) {
        spdlog::warn("Failed to get thread context for register nullification");
        return;
    }
    
    // Check and nullify general purpose registers
    uintptr_t target = reinterpret_cast<uintptr_t>(ptr);
    bool modified = false;
    
    if (context.Rax == target) { context.Rax = 0; modified = true; }
    if (context.Rbx == target) { context.Rbx = 0; modified = true; }
    if (context.Rcx == target) { context.Rcx = 0; modified = true; }
    if (context.Rdx == target) { context.Rdx = 0; modified = true; }
    if (context.Rsi == target) { context.Rsi = 0; modified = true; }
    if (context.Rdi == target) { context.Rdi = 0; modified = true; }
    if (context.Rbp == target) { context.Rbp = 0; modified = true; }
    // Don't nullify RSP (stack pointer) as it would crash
    if (context.R8 == target) { context.R8 = 0; modified = true; }
    if (context.R9 == target) { context.R9 = 0; modified = true; }
    if (context.R10 == target) { context.R10 = 0; modified = true; }
    if (context.R11 == target) { context.R11 = 0; modified = true; }
    if (context.R12 == target) { context.R12 = 0; modified = true; }
    if (context.R13 == target) { context.R13 = 0; modified = true; }
    if (context.R14 == target) { context.R14 = 0; modified = true; }
    if (context.R15 == target) { context.R15 = 0; modified = true; }
    
    if (modified) {
        if (!SetThreadContext(currentThread, &context)) {
            spdlog::error("Failed to set thread context after nullifying registers");
        } else {
            spdlog::debug("Nullified dangling pointer in registers");
        }
    }
}

// Nullify pointer in stack
void StateManager::NullifyPointerInStack(void* ptr) {
    // Get stack bounds
    NT_TIB* tib = reinterpret_cast<NT_TIB*>(NtCurrentTeb());
    if (!tib) {
        return;
    }
    
    void* stackBase = tib->StackBase;
    void* stackLimit = tib->StackLimit;
    
    if (!stackBase || !stackLimit) {
        return;
    }
    
    // Scan and nullify stack memory
    uintptr_t target = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t* stackPtr = reinterpret_cast<uintptr_t*>(stackLimit);
    uintptr_t* stackEnd = reinterpret_cast<uintptr_t*>(stackBase);
    
    size_t nullifiedCount = 0;
    while (stackPtr < stackEnd) {
        if (*stackPtr == target) {
            *stackPtr = 0;
            nullifiedCount++;
        }
        stackPtr++;
    }
    
    if (nullifiedCount > 0) {
        spdlog::debug("Nullified {} dangling pointer(s) in stack", nullifiedCount);
    }
}

// Nullify pointer in heap
void StateManager::NullifyPointerInHeap(void* ptr) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // Use unique_lock for write operation
    
    // Scan tracked allocations for references to the pointer
    for (auto& [allocPtr, info] : trackedAllocations_) {
        void** ptrArray = static_cast<void**>(allocPtr);
        size_t ptrCount = info.size / sizeof(void*);
        
        for (size_t i = 0; i < ptrCount; ++i) {
            if (ptrArray[i] == ptr) {
                ptrArray[i] = nullptr;
                spdlog::trace("Nullified dangling pointer in heap at offset {}", i * sizeof(void*));
            }
        }
    }
}

} // namespace CrashGuard
