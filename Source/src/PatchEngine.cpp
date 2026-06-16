// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

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
