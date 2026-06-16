// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

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
