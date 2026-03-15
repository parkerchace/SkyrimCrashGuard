// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "LeakPatternDetector.h"
#include <vector>

namespace CrashGuard {

/**
 * @brief Safely cleans up leaked memory
 * 
 * Implements validated cleanup for detected leaks. Only frees memory
 * when allocator type is known and validation checks pass.
 */
class LeakMitigator {
public:
    // Cleanup result
    struct CleanupResult {
        bool success;
        size_t bytesFreed;
        size_t allocationsFreed;
        std::string errorMessage;
    };

    static LeakMitigator& GetInstance();

    // Initialize mitigator
    bool Initialize();

    // Shutdown mitigator
    void Shutdown();

    // Clean up detected leak
    CleanupResult CleanupLeak(const LeakPatternDetector::DetectedLeak& leak);

    // Safe cleanup methods for specific leak types
    CleanupResult SafeFreeTexture(void* texturePtr);
    CleanupResult ForceUnloadDistantCells();
    CleanupResult SafeFreeNPCResources(void* actorHandle);

private:
    LeakMitigator() = default;
    ~LeakMitigator() = default;
    LeakMitigator(const LeakMitigator&) = delete;
    LeakMitigator& operator=(const LeakMitigator&) = delete;

    // Validation checks
    bool ValidateAllocation(void* ptr) const;
    bool CanSafelyFree(void* ptr, const std::string& allocatorType) const;

    // Internal cleanup methods
    bool FreeMemoryManagerAllocation(void* ptr);
    bool FreeScrapHeapAllocation(void* ptr);

    bool m_initialized = false;
    bool m_automaticCleanupEnabled = false;
};

} // namespace CrashGuard
