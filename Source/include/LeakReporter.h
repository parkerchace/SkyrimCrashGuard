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
#include <string>
#include <vector>

namespace CrashGuard {

/**
 * @brief Generates leak reports and user-facing diagnostics
 * 
 * Creates detailed leak reports, tracks leak growth over time,
 * and provides user-facing recommendations.
 */
class LeakReporter {
public:
    // Leak report structure
    struct LeakReport {
        std::chrono::system_clock::time_point timestamp;
        std::vector<LeakPatternDetector::DetectedLeak> leaks;
        size_t totalLeakedBytes;
        size_t totalLeakedAllocations;
        std::string recommendations;
    };

    static LeakReporter& GetInstance();

    // Initialize reporter
    bool Initialize();

    // Shutdown reporter
    void Shutdown();

    // Generate leak report
    LeakReport GenerateLeakReport();

    // Export report to JSON file
    bool ExportLeakReport(const LeakReport& report, const std::string& filePath);

    // Get leak growth rate (bytes per minute)
    float GetLeakGrowthRate() const;

    // Show in-game notification for leak warning
    void ShowLeakWarning(const LeakReport& report);

    // Get user-facing recommendations based on leak types
    std::string GetRecommendations(const std::vector<LeakPatternDetector::DetectedLeak>& leaks) const;

private:
    LeakReporter() = default;
    ~LeakReporter() = default;
    LeakReporter(const LeakReporter&) = delete;
    LeakReporter& operator=(const LeakReporter&) = delete;

    // Track leak history for growth rate calculation
    struct LeakSnapshot {
        std::chrono::steady_clock::time_point timestamp;
        size_t totalBytes;
    };

    std::vector<LeakSnapshot> m_leakHistory;
    std::chrono::steady_clock::time_point m_lastReportTime;
    bool m_initialized = false;
};

} // namespace CrashGuard
