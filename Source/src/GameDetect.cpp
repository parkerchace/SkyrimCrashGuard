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

#include "GameDetect.h"

#include <psapi.h>

namespace GameDetect {

    static GameInfo s_info{};
    static bool     s_detected = false;

    const GameInfo& Detect()
    {
        if (s_detected) return s_info;
        s_detected = true;

        // Probe for known Skyrim executables (most specific first)
        struct Probe {
            const char* exe;
            Edition     edition;
            const char* name;
            const char* docs;
        };

        static constexpr Probe probes[] = {
            { "SkyrimVR.exe",  Edition::VR, "Skyrim VR",              "Skyrim VR" },
            { "SkyrimSE.exe",  Edition::SE, "Skyrim Special Edition", "Skyrim Special Edition" },
        };

        for (const auto& p : probes) {
            HMODULE h = GetModuleHandleA(p.exe);
            if (h) {
                s_info.edition  = p.edition;
                s_info.exeName  = p.exe;
                s_info.gameName = p.name;
                s_info.docsDir  = p.docs;
                s_info.hModule  = h;

                MODULEINFO mi{};
                if (GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi))) {
                    s_info.base = reinterpret_cast<uintptr_t>(mi.lpBaseOfDll);
                    s_info.end  = s_info.base + mi.SizeOfImage;
                }
                return s_info;
            }
        }

        // Fallback: unknown exe (maybe a test harness)
        s_info.edition  = Edition::Unknown;
        s_info.exeName  = "unknown.exe";
        s_info.gameName = "Unknown";
        s_info.docsDir  = "Skyrim Special Edition";  // safe default
        return s_info;
    }

}  // namespace GameDetect
