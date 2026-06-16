// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

// Plugin metadata — pure constexpr, no dependencies
namespace Plugin {
    inline constexpr const char* Name    = "SkyrimCrashGuard";
    inline constexpr const char* Author  = "Parker Chace";
    inline constexpr int VersionMajor    = PLUGIN_VERSION_MAJOR;
    inline constexpr int VersionMinor    = PLUGIN_VERSION_MINOR;
    inline constexpr int VersionPatch    = PLUGIN_VERSION_PATCH;

    // Stored during SKSEPlugin_Load from LoadInterface::SKSEVersion()
    // Packed as (major<<24)|(minor<<16)|(patch<<8)|build
    inline std::uint32_t s_skseVersionPacked = 0;

    // Decode the stored packed SKSE version as "major.minor.patch"
    inline std::string GetSKSEVersionString() {
        return fmt::format("{}.{}.{}",
            (s_skseVersionPacked >> 24) & 0xFF,
            (s_skseVersionPacked >> 16) & 0xFF,
            (s_skseVersionPacked >>  8) & 0xFF);
    }
}
