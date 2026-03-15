// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "Patches.h"
#include "PatchEngine.h"

#include <spdlog/spdlog.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// SkyrimVR 1.4.15 base address for hardcoded offsets.
// In the future, use Address Library IDs instead.
// To find these: crash log shows "SkyrimVR.exe+0xOFFSET" — that's the offset from module base.

namespace Patches {

    // ────────────────────────────────────────────────────
    // Patch: BGSImpactManager null material check
    //
    // Crash at SkyrimVR.exe+0x2d4c47: cmp [rax], rsi
    // RAX contains a garbage pointer to BGSMaterialType data.
    // The function processes impact effects for actors/objects.
    //
    // Fix: hook the function entry, validate the material pointer
    // before the engine dereferences it. If invalid, return early
    // (skip the impact effect — cosmetic-only loss).
    // ────────────────────────────────────────────────────

    static bool InstallImpactManagerPatch()
    {
        auto log = spdlog::default_logger();

        // ImpactManager crashes are handled via VEH known-site registration in VEH.cpp
        // Two crash sites are registered for VR:
        //   - 0x02D32A5: ImpactManager footstep null deref (mov rax,[rcx+0x20])
        //   - 0x02D4C47: ImpactManager material lookup null ("Reindeer crash")
        //
        // SE/AE: Need user crash reports to identify specific offsets
        // The functions involved are BGSImpactManager::ProcessEvent (footstep)
        // and material lookup code paths
        
        if (REL::Module::IsVR()) {
            log->info("  ImpactManager crash sites registered in VEH: +0x2D32A5, +0x2D4C47 (VR)");
        } else {
            log->info("  ImpactManager: SE/AE offsets pending user crash reports");
        }

        // Return true - VEH handles recovery
        return true;
    }

    // ────────────────────────────────────────────────────
    // Patch: Generic null Actor/TESObjectREFR checks
    //
    // Many crashes in Skyrim are caused by:
    //   actor->someVirtualCall() where actor is null or freed
    //   ref->GetBaseObject() returning null, then caller dereferences
    //
    // These are the most common modded-Skyrim crashes.
    // With CommonLibVR we can hook the actual game functions.
    // ────────────────────────────────────────────────────

    // ────────────────────────────────────────────────────
    // Patch: Comprehensive SIMD/GP null-deref protection
    //
    // During save loading and gameplay, engine subsystems (Sky, Water,
    // weather, lighting, particles, actors) access pointers that may
    // be null when systems haven't finished initializing or when mods
    // provide invalid data.
    //
    // Instead of listing individual crash offsets (which are brittle
    // across game versions), CrashGuard relies on L1b instruction-
    // pattern matching via Zydis disassembly:
    //
    //   P1: call [reg+disp] with corrupted vtable → zero RAX, skip
    //   P2: jmp  [reg+disp] with bad base → function return
    //   P3: read from null/freed pointer → zero dest (GP or XMM), skip
    //   P4: write to null/freed pointer → skip store
    //
    // This covers ALL crash patterns generically on SE, AE, and VR
    // without version-specific offsets. Known examples recovered:
    //   - Moon/Sky null texture (movss xmm13, [rax+0x18])
    //   - TESWaterReflections null this (movss xmm0, [rdi+0xF0])
    //   - NiParticleSystem vtable corruption (call [rax+0x28])
    //   - BSFadeNode LOD transitions, armor form corruption
    //   - Papyrus native call with bad parameters
    //   - SaveLoadManager event cascade
    // ────────────────────────────────────────────────────
    static bool InstallComprehensiveCrashGuard()
    {
        auto log = spdlog::default_logger();

        log->info("  CrashGuard: L1b version-independent pattern matching active");
        log->info("  CrashGuard: Covers GP (RAX-R15) and SIMD (XMM0-XMM15) null-deref recovery");
        log->info("  CrashGuard: Handles read AV, write AV, bad vtable calls, bad jumps");
        log->info("  CrashGuard: Works on SE, AE, and VR without version-specific offsets");
        log->info("  CrashGuard: Recovery reports written to SKSE/CrashGuard-recovery-*.log");

        return true;
    }

    void RegisterAll()
    {
        // Known crash site patches (L1 fast path — version-specific optimizations)
        PatchEngine::Register({
            .name        = "ImpactManager-NullMaterial",
            .description = "Prevents crash at BGSImpactManager material lookup (Reindeer crash)",
            .enabled     = true,
            .install     = InstallImpactManagerPatch
        });

        // Comprehensive crash prevention (L1b — version-independent)
        PatchEngine::Register({
            .name        = "ComprehensiveCrashGuard",
            .description = "Version-independent crash recovery for all null-deref patterns (SE/AE/VR)",
            .enabled     = true,
            .install     = InstallComprehensiveCrashGuard
        });

        // Future patches go here:

        // PatchEngine::Register({
        //     .name = "Actor-NullUpdate",
        //     .description = "Validate Actor pointer before Update3DModel",
        //     .enabled = true,
        //     .install = InstallActorUpdatePatch
        // });

        // PatchEngine::Register({
        //     .name = "AnimGraph-NullEvent",
        //     .description = "Validate animation graph before event dispatch",
        //     .enabled = true,
        //     .install = InstallAnimGraphPatch
        // });
    }

}  // namespace Patches
