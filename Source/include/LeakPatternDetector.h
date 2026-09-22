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

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

namespace CrashGuard {

/**
 * @brief Detects common memory leak patterns in Skyrim
 * 
 * Analyzes tracked allocations to identify texture leaks, NPC leaks,
 * script leaks, and cell leaks based on known patterns.
 */
class LeakPatternDetector {
public:
    // Leak classification
    enum class LeakType {
        Unknown,
        Texture,
        NPC,
        Script,
        Cell,
        General
    };

    enum class LeakSeverity {
        Low,      // < 10 MB
        Medium,   // 10-50 MB
        High,     // 50-100 MB
        Critical  // > 100 MB
    };

    // Detected leak information
    struct DetectedLeak {
        LeakType type;
        LeakSeverity severity;
        float confidence; // 0.0 - 1.0
        size_t totalSize;
        size_t allocationCount;
        std::chrono::seconds age;
        std::string description;
        std::vector<void*> leakedAddresses;
    };

    static LeakPatternDetector& GetInstance();

    // Initialize detector
    bool Initialize();

    // Shutdown detector
    void Shutdown();

    // Scan for leaks
    std::vector<DetectedLeak> ScanForLeaks();

    // Detect specific leak types
    DetectedLeak DetectTextureLeaks();
    DetectedLeak DetectNPCLeaks();
    DetectedLeak DetectScriptLeaks();
    DetectedLeak DetectCellLeaks();

private:
    LeakPatternDetector() = default;
    ~LeakPatternDetector() = default;
    LeakPatternDetector(const LeakPatternDetector&) = delete;
    LeakPatternDetector& operator=(const LeakPatternDetector&) = delete;

    // Helper methods
    LeakSeverity CalculateSeverity(size_t totalSize) const;
    bool IsTextureAllocation(void* address) const;
    bool IsNPCAllocation(void* address) const;
    bool IsScriptAllocation(void* address) const;
    bool IsCellAllocation(void* address) const;

    // Configuration
    std::chrono::minutes m_leakAgeThreshold{5}; // Consider allocations older than 5 minutes as potential leaks
    bool m_initialized = false;
};

} // namespace CrashGuard
