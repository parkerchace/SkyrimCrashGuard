// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

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
