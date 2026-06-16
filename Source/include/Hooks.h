// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

// Hooks.h — Proactive inline hooks for known problematic code paths
//
// Philosophy: Prevent crashes by validating pointers BEFORE the engine
// tries to dereference them.  Standalone — no SKSE/CommonLib dependency.

#pragma once

namespace Hooks {

    /// Install all proactive hooks.
    void InstallHooks();

    /// Remove/unhook all patches.
    void UninstallHooks();

    /// Get count of successful preventions.
    size_t GetPreventionCount();

}  // namespace Hooks
