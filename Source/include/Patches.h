// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

/// Concrete engine patches for known crash sites.
/// Each patch is registered with PatchEngine during plugin init.
namespace Patches {

    /// Register all known patches with the PatchEngine.
    void RegisterAll();

}  // namespace Patches
