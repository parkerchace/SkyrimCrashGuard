// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

namespace CrashGuard {
namespace PapyrusNatives {

/**
 * @brief Register Papyrus native functions for MCM support
 * 
 * These functions are called from Papyrus scripts (CrashGuardNative.psc)
 * and provide the bridge between MCM menu and C++ configuration.
 */
void Register();

} // namespace PapyrusNatives
} // namespace CrashGuard
