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

// Auto-generated from CMakeLists.txt VERSION
// DO NOT EDIT MANUALLY - Update CMakeLists.txt project(VERSION x.y.z) instead

#define CRASHGUARD_VERSION_MAJOR @PLUGIN_VERSION_MAJOR@
#define CRASHGUARD_VERSION_MINOR @PLUGIN_VERSION_MINOR@
#define CRASHGUARD_VERSION_PATCH @PLUGIN_VERSION_PATCH@

#define CRASHGUARD_VERSION_STRING "@PLUGIN_VERSION_MAJOR@.@PLUGIN_VERSION_MINOR@.@PLUGIN_VERSION_PATCH@"
#define CRASHGUARD_VERSION_FULL "Crash Guard v" CRASHGUARD_VERSION_STRING " - Advanced Edition"

namespace CrashGuard {
    namespace Version {
        constexpr int MAJOR = CRASHGUARD_VERSION_MAJOR;
        constexpr int MINOR = CRASHGUARD_VERSION_MINOR;
        constexpr int PATCH = CRASHGUARD_VERSION_PATCH;
        constexpr const char* STRING = CRASHGUARD_VERSION_STRING;
        constexpr const char* FULL = CRASHGUARD_VERSION_FULL;
    }
}
