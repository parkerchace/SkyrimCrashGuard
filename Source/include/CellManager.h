// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <chrono>
#include <shared_mutex>
#include <cstdint>

/// Cell Manager for proactive validation
/// Handles cell loading with validation and recovery, providing safe teleportation
/// when cell-related crashes occur
namespace CellValidation {

    /// Cell validation result structure
    struct CellValidationResult {
        bool isValid;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        bool canRepair;
        uint32_t invalidReferenceCount;
        uint32_t totalReferenceCount;
    };

    /// Cell blacklist entry
    struct CellBlacklistEntry {
        RE::FormID cellFormID;
        std::string cellName;
        std::string reason;
        std::chrono::steady_clock::time_point blacklistedAt;
        uint32_t failureCount;
    };

    /// Safe cell information
    struct SafeCellInfo {
        RE::FormID formID;
        std::string name;
        std::string description;
        RE::NiPoint3 safePosition;
        bool isInterior;
    };

    /// Main cell manager class
    class CellManager {
    public:
        /// Initialize the cell manager
        static bool Initialize();

        /// Load cell with validation
        static bool LoadCellSafe(RE::TESObjectCELL* cell);

        /// Validate cell references before spawning
        static CellValidationResult ValidateCellReferences(RE::TESObjectCELL* cell);

        /// Teleport player to safe location
        static void TeleportToSafeCell(RE::Actor* player);

        /// Check if cell is blacklisted
        static bool IsCellBlacklisted(RE::TESObjectCELL* cell);

        /// Blacklist problematic cell
        static void BlacklistCell(RE::TESObjectCELL* cell, const std::string& reason);

        /// Get safe cell for teleportation
        static RE::TESObjectCELL* GetSafeCell();

        /// Get safe position within a cell
        static RE::NiPoint3 GetSafeCellPosition(RE::TESObjectCELL* cell);

        /// Get statistics
        static size_t GetBlacklistSize();
        static size_t GetValidationCount();
        static size_t GetFailureCount();
        static size_t GetSafeCellCount();

        /// Clear blacklist (for testing)
        static void ClearBlacklist();

    private:
        /// Validate individual reference
        static bool ValidateReference(RE::TESObjectREFR* ref);

        /// Skip invalid references during cell loading
        static void SkipInvalidReferences(RE::TESObjectCELL* cell);

        /// Initialize safe cell list
        static void InitializeSafeCells();

        /// Check if cell is safe for teleportation
        static bool IsSafeCell(RE::TESObjectCELL* cell);

        /// Find nearest safe cell to given position
        static RE::TESObjectCELL* FindNearestSafeCell(const RE::NiPoint3& position);

        /// Validate cell data structure
        static bool ValidateCellData(RE::TESObjectCELL* cell);

        /// Check for circular references in cell
        static bool CheckCircularReferences(RE::TESObjectCELL* cell);

        /// Validate FormID is valid and loaded
        static bool ValidateFormID(RE::FormID formID);

        /// Get cell name for logging
        static std::string GetCellName(RE::TESObjectCELL* cell);

        /// Get cell FormID safely
        static RE::FormID GetCellFormID(RE::TESObjectCELL* cell);

        /// Check if reference is in valid state
        static bool IsReferenceValid(RE::TESObjectREFR* ref);

        /// Check if reference has valid base form
        static bool HasValidBaseForm(RE::TESObjectREFR* ref);

        /// Check if reference position is valid
        static bool IsPositionValid(const RE::NiPoint3& position);

        // State tracking
        static bool s_initialized;
        static std::unordered_set<RE::FormID> s_blacklistedCells;
        static std::vector<CellBlacklistEntry> s_blacklistEntries;
        static std::vector<SafeCellInfo> s_safeCells;
        static size_t s_validationCount;
        static size_t s_failureCount;
        static std::shared_mutex s_blacklistMutex;
        static std::shared_mutex s_safeCellMutex;
    };

}  // namespace CellValidation