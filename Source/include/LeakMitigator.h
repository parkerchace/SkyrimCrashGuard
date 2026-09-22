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
