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
