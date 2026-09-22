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
#include <mutex>

/// Records crash data from VEH catches so we can identify
/// new patch candidates. Written to CrashPatterns.json on shutdown.
namespace CrashCollector {

    struct CrashRecord {
        uintptr_t   faultAddress;      // RIP where crash occurred
        uintptr_t   accessAddress;     // Address being read/written
        uint32_t    exceptionCode;     // EXCEPTION_ACCESS_VIOLATION etc.
        std::string moduleName;        // Module containing faultAddress
        uintptr_t   moduleOffset;      // Offset within module
        uint32_t    hitCount;          // How many times this site crashed
        std::string timestamp;         // First occurrence
    };

    /// Initialize the collector.
    void Init();

    /// Record a crash. Thread-safe.
    void Record(uintptr_t faultAddr, uintptr_t accessAddr, uint32_t code);

    /// Write collected data to disk. Call on shutdown.
    void Flush();

    /// Get all records (for logging).
    std::vector<CrashRecord> GetRecords();

}  // namespace CrashCollector
