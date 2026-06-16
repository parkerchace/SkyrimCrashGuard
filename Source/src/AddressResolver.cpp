// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "AddressResolver.h"
#include <Windows.h>
#include <Psapi.h>
#include <spdlog/spdlog.h>

namespace CrashGuard {

std::uintptr_t AddressResolver::_moduleBase = 0;
std::size_t AddressResolver::_moduleSize = 0;

void AddressResolver::InitializeModuleInfo() {
    if (_moduleBase != 0) {
        return; // Already initialized
    }

    HMODULE hModule = GetModuleHandleA(nullptr);
    if (!hModule) {
        spdlog::error("AddressResolver: Failed to get module handle");
        return;
    }

    MODULEINFO modInfo{};
    if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo))) {
        spdlog::error("AddressResolver: Failed to get module information");
        return;
    }

    _moduleBase = reinterpret_cast<std::uintptr_t>(modInfo.lpBaseOfDll);
    _moduleSize = modInfo.SizeOfImage;

    spdlog::info("AddressResolver: Module base = 0x{:X}, size = 0x{:X}", _moduleBase, _moduleSize);
}

std::uintptr_t AddressResolver::GetModuleBase() {
    if (_moduleBase == 0) {
        InitializeModuleInfo();
    }
    return _moduleBase;
}

std::size_t AddressResolver::GetModuleSize() {
    if (_moduleSize == 0) {
        InitializeModuleInfo();
    }
    return _moduleSize;
}

std::uintptr_t AddressResolver::ScanPattern(
    std::uintptr_t start,
    std::size_t size,
    const char* pattern,
    const char* mask
) {
    const auto patternLen = std::strlen(mask);
    const auto scanEnd = start + size - patternLen;

    for (std::uintptr_t addr = start; addr < scanEnd; ++addr) {
        bool found = true;
        for (std::size_t i = 0; i < patternLen; ++i) {
            if (mask[i] == '?') {
                continue; // Wildcard
            }
            if (*reinterpret_cast<const char*>(addr + i) != pattern[i]) {
                found = false;
                break;
            }
        }
        if (found) {
            return addr;
        }
    }

    return 0;
}

std::optional<std::uintptr_t> AddressResolver::FindPattern(const Pattern& pattern) {
    const auto base = GetModuleBase();
    const auto size = GetModuleSize();

    if (base == 0 || size == 0) {
        spdlog::error("AddressResolver: Module info not initialized");
        return std::nullopt;
    }

    // Pattern and mask are pre-parsed byte arrays provided by the caller;
    // ScanPattern performs the Boyer-Moore-Horspool style signature scan.
    const auto result = ScanPattern(base, size, pattern.signature.data(), pattern.mask.data());
    
    if (result == 0) {
        return std::nullopt;
    }

    return result + pattern.offset;
}

std::optional<std::uintptr_t> AddressResolver::FindPatternMulti(const std::vector<Pattern>& patterns) {
    for (const auto& pattern : patterns) {
        if (auto result = FindPattern(pattern)) {
            spdlog::debug("AddressResolver: Pattern matched at 0x{:X}", result.value());
            return result;
        }
    }

    spdlog::warn("AddressResolver: No patterns matched");
    return std::nullopt;
}

} // namespace CrashGuard
