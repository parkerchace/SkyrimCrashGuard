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

#include "PatchEngine.h"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace PatchEngine {

    static std::vector<Patch> s_patches;
    static size_t s_appliedCount = 0;

    void Init()
    {
        s_patches.clear();
        s_appliedCount = 0;
    }

    void Register(Patch patch)
    {
        s_patches.push_back(std::move(patch));
    }

    size_t ApplyAll()
    {
        auto log = spdlog::default_logger();
        s_appliedCount = 0;

        for (auto& patch : s_patches) {
            if (!patch.enabled) {
                log->info("Patch '{}' disabled - skipping", patch.name);
                continue;
            }

            log->info("Applying patch: {} - {}", patch.name, patch.description);

            try {
                if (patch.install && patch.install()) {
                    patch.applied = true;
                    s_appliedCount++;
                    log->info("  -> OK");
                } else {
                    log->warn("  -> FAILED");
                }
            } catch (const std::exception& e) {
                log->error("  -> EXCEPTION: {}", e.what());
            }
        }

        return s_appliedCount;
    }

    const std::vector<Patch>& GetPatches()
    {
        return s_patches;
    }

    size_t GetAppliedCount()
    {
        return s_appliedCount;
    }

}  // namespace PatchEngine
