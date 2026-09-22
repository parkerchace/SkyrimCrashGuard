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
