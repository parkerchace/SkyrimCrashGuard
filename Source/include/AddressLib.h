// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <optional>
#include <string>

namespace AddressLib {

/**
 * @brief Check if the address library is valid and ready to use
 * @return true if address library is initialized and valid
 */
inline bool IsValid() {
    try {
        // Check if REL's IDDatabase is initialized
        (void)REL::IDDatabase::get();
        return true;
    } catch (...) {
        return false;
    }
}

/**
 * @brief Get the reason why address library is invalid
 * @return String describing the reason
 */
inline std::string Reason() {
    try {
        (void)REL::IDDatabase::get();
        return "valid";
    } catch (const std::exception& e) {
        return e.what();
    } catch (...) {
        return "unknown error";
    }
}

/**
 * @brief Resolve an address ID for SE/AE
 * @param ids Pair of {SE_ID, AE_ID}
 * @return Optional address if found
 */
inline std::optional<std::uintptr_t> ResolveID(std::pair<std::uint64_t, std::uint64_t> ids) {
    try {
        // Use REL::ID which automatically selects the correct ID based on runtime
        // For SE/AE, use the first ID for SE and second for AE
        REL::ID id(REL::Module::IsAE() ? ids.second : ids.first);
        return id.address();
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace AddressLib
