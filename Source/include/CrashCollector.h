// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

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
