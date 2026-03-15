// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <cstdint>
#include <string_view>
#include <optional>
#include <vector>

namespace CrashGuard {

/**
 * @brief Built-in address resolution system
 * 
 * Uses pattern scanning to find function addresses at runtime.
 * This makes the plugin independent of Address Library while
 * maintaining compatibility across different game versions.
 */
class AddressResolver {
public:
    /**
     * @brief Pattern matching result
     */
    struct Pattern {
        std::string_view signature;  // Byte pattern (e.g., "48 89 5C 24 ?? 57")
        std::string_view mask;       // Mask for wildcards (e.g., "xxxx?x")
        std::ptrdiff_t offset;       // Offset from pattern match to actual address
        
        Pattern(std::string_view sig, std::string_view m = "", std::ptrdiff_t off = 0)
            : signature(sig), mask(m), offset(off) {}
    };

    /**
     * @brief Find an address using pattern scanning
     * @param pattern Byte pattern to search for
     * @return Address if found, nullopt otherwise
     */
    static std::optional<std::uintptr_t> FindPattern(const Pattern& pattern);

    /**
     * @brief Find an address with multiple fallback patterns
     * @param patterns Vector of patterns to try (for version compatibility)
     * @return Address if any pattern matches, nullopt otherwise
     */
    static std::optional<std::uintptr_t> FindPatternMulti(const std::vector<Pattern>& patterns);

    /**
     * @brief Get the base address of the game executable
     */
    static std::uintptr_t GetModuleBase();

    /**
     * @brief Get the size of the game executable
     */
    static std::size_t GetModuleSize();

    /**
     * @brief Scan memory for a byte pattern
     * @param start Start address
     * @param size Size of region to scan
     * @param pattern Byte pattern
     * @param mask Mask for wildcards ('x' = match, '?' = wildcard)
     * @return Address if found, 0 otherwise
     */
    static std::uintptr_t ScanPattern(
        std::uintptr_t start,
        std::size_t size,
        const char* pattern,
        const char* mask
    );

private:
    static std::uintptr_t _moduleBase;
    static std::size_t _moduleSize;
    
    static void InitializeModuleInfo();
};

/**
 * @brief Helper macro for defining version-independent function addresses
 * 
 * Usage:
 *   RESOLVE_ADDRESS(MyFunction, 
 *       Pattern("48 89 5C 24 ?? 57", "xxxx?x"),
 *       Pattern("48 89 5C 24 ?? 48", "xxxx?x")  // Fallback for different version
 *   )
 */
#define RESOLVE_ADDRESS(name, ...) \
    inline std::uintptr_t Get##name##Address() { \
        static std::optional<std::uintptr_t> cached; \
        if (!cached) { \
            cached = CrashGuard::AddressResolver::FindPatternMulti({__VA_ARGS__}); \
        } \
        return cached.value_or(0); \
    }

} // namespace CrashGuard
