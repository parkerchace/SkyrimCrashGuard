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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>

// ═══════════════════════════════════════════════════════════════════════
// GameDetect.h — Auto-detect Skyrim edition at runtime
// ═══════════════════════════════════════════════════════════════════════
// Works with SE (1.5.x / 1.6.x), AE (1.6.x), and VR (1.4.15).
// Probes for the game executable in order of specificity.

namespace GameDetect {

    enum class Edition {
        Unknown = 0,
        VR,     // SkyrimVR.exe
        SE,     // SkyrimSE.exe (covers both SE and AE)
    };

    struct GameInfo {
        Edition     edition   = Edition::Unknown;
        const char* exeName   = nullptr;      // "SkyrimVR.exe" etc.
        const char* gameName  = nullptr;      // "Skyrim VR" etc.
        const char* docsDir   = nullptr;      // "Skyrim VR" / "Skyrim Special Edition"
        HMODULE     hModule   = nullptr;
        uintptr_t   base      = 0;
        uintptr_t   end       = 0;
    };

    /// Detect which Skyrim edition is hosting us.  Call once at init.
    /// Results are cached — subsequent calls return the same pointer.
    const GameInfo& Detect();

}  // namespace GameDetect
