# Changelog

All notable changes to SkyrimCrashGuard are documented here.

---

## [2.3.6] - 2026-05-29

### Fixed

- **Build file** — `src/MenuInputObserver.cpp` was listed twice in CMakeLists; second entry removed
- **CrashLogger detection** — Injection-detection was searching for `"CRASHGUARD CRASH RECOVERY ACTIVE"` but the header writer produces `"NOTE: SkyrimCrashGuard"`. Fixed to match
- **Plugin version** — `Plugin.h` had `"2.2.2"` hardcoded; now reads from `PLUGIN_VERSION_MAJOR/MINOR/PATCH` macros (resolves to 2.3.6)
- **SKSE version** — The SKSE version pointer is only valid during `SKSEPlugin_Load`; the value is now stored at that point and decoded later when needed. Three locations that previously showed hardcoded or empty version strings now display correct values
- **Nexus description** — Version number and layer count corrected (v2.3.6, 6-Layer)
- **Copyright years** — All 85 source and header files updated to 2026
- **Save file plugin detection** — Index iteration bug: was returning `true` before checking whether the plugin pointer at that index was non-null
- **Deadlock watchdog thread** — Real background watchdog implemented: wakes every `deadlockTimeout/2`, takes a non-blocking lock snapshot, logs any detected violation, and joins cleanly on shutdown
- **Hotkey persistence** — Hotkey bindings now read from and write to `SkyrimCrashGuard_hotkeys.toml` correctly
- **SmartHarvest compatibility** — Native function hook now checks for SmartHarvest DLL presence before registering the `NotifyActivated` wrapper; when SmartHarvest is installed, VEH handles crash protection without conflicting with its registration
- **Load last save** — Now queues the save load via `SKSE::GetTaskInterface()->AddTask()` so it executes on the main game thread
- **Papyrus validation** — `RegisterValidationWrappers` is now wired into `SKSEPlugin_Load` via `SKSE::GetPapyrusInterface()->Register()`
- **Scene graph operations** — Detach, attach, and bounding volume update now call real CommonLibSSE-NG APIs. Previously logged only
- **Animation reset** — `UpdateAnimationState` now calls `actor->NotifyAnimationGraph("IdleStop")` to reset via the behavior graph
- **Memory region crash classification** — Was classifying all addresses below 4 GB as Fatal. Now uses `VirtualQuery` to distinguish free/reserved (Warning), executable (Fatal), and committed data (Warning)
- **Stat counters thread safety** — Five FormIDValidator counters promoted to `std::atomic<size_t>`
- **VEH master enable and VEH toggle** — Disabling CrashGuard or VEH in the F11 menu now takes effect immediately for the current session. Previously the VEH handler stayed active until the next game restart regardless of what was toggled
- **F11 menu** — Duplicate toggles for User Notifications and Pattern Learning removed from Settings tab. Read-only "Input Conflict Prevention" section with no controls removed. Auto-Recover Safe / Auto-Recover Warning checkboxes removed (VEH handles exceptions synchronously and cannot pause to prompt the user). Seven tooltips rewritten to describe what each setting actually does. System health tests no longer show "Not recovered" — the VEH recovery chain display is suppressed for non-crash tests.
- **Crash history toast** — Startup toast now shows the most recent recovery (address, instruction, strategy, session total) when recoveries have occurred
- **Aggregated log entries** — `[AGGREGATED xN]` prefix now emitted when `aggregatePatterns = true` and the same crash site is seen more than once in a session
- **Nexus description** — Testimonial quote bodies had the username duplicated inline (`tcbflashtcbflash...`, `visentinelvisentinel...`); removed the duplicate prefixes
- **Nexus description** — Interior cell lighting crash bullet: corrected "null shadow frustum pointer" to "null pointer to a light struct"; corrected "read fault" to "access violation"
- **Nexus description** — FAQ incorrectly stated CrashGuard "only responds to access violations"; corrected to include integer divide-by-zero and illegal CPU instruction
- **README** — Log level comment corrected (was missing level 4 trace and had wrong labels for levels 1-3); `[Hotkeys]` TOML block removed (F11 is hardcoded, no TOML key since v2.3.6); `[EngineOptimizations]` troubleshooting tip removed (feature removed in v2.3.5); camera-zoom tip updated to direct users to the log instead of a non-existent F11 menu list; crash-type "can be recovered" list expanded to include integer divide-by-zero and illegal CPU instruction
- **Video script** — Narration corrected: visentinel crash now describes "null pointer to a light struct" and "exits the shadow function via a stack walk"; 

### Added

- **Interior cell lighting crash detection** — `IsInteriorCellLightingCrash()` classifies shadow and lighting crashes in interior cells. Eight signature patterns added: BSShadowFrustumLight, BSLightingShaderProperty, NiPointLight, NiDirectionalLight, BSShaderAccumulator, TESWaterReflections, NiParticleSystem, BSEffectShader. Crash classification now checks for interior lighting patterns before falling through to generic cell crash handling

### Removed

- **ActorLODManager.h** — Empty header with no implementation; deleted
- **NPC benchmark actions** — `HideNearbyNPCs` and `RestoreNearbyNPCs` had no functional code; removed
- **Dead config sections** — `[NPCManagement]`, `[ActorLOD]`, `[ActorLODDiagnostics]`, `[SafetyChecks]`, `[StateManagement]`, `[Hotkeys]`, `[Benchmark]` removed from Config.h, Config.cpp, and SkyrimCrashGuard.toml
- **Orphaned headers** — Three header files that were never compiled or referenced anywhere deleted
- **17 dead config fields** — Removed from Config.h, Config.cpp, and SkyrimCrashGuard.toml: `autoRecoverSafe`, `autoRecoverWarning`, `batchSimilarCrashes`, `logAllRecoveries`, `logSilentRecoveries`, `enableStateSnapshots`, `maxSnapshotsPerSession`, `allowBuiltinActions`, `enableInputDebugLogging`, `enableVehDebugLogging`, `enablePatchDebugLogging`, `enablePapyrusDebugLogging`, `enablePerfTracing`, `menuToggleKey`, `enableNullChecks`, `enableBoundsChecks`, `enableFormIDChecks`. All were saved to TOML but never checked by any feature
- **Dead code** — Dead functionings have been removed throughout the codebase
- **Dead F11 menu controls** — Safety Checks collapsible (Null Pointer Checks, Bounds Checks, FormID Validation), State Management collapsible, and all controls for the 17 removed config fields removed from the Advanced Config tab

---

## [2.3.5] - 2026-03-22

### Removed

- **NPC Manager System** — NPC counting, spawn prevention, and dead body cleanup removed. The mod now focuses exclusively on crash prevention and recovery
  - Removed NPCManager.cpp, NPCManager.h, NPC Tools tab from F11 menu, and all NPC-related TOML config

---

## [2.3.2] - 2026-03-15

### Fixed

- **Documentation** — README and Nexus description corrected to reflect actual feature status: mesh validation is utility functions only; Papyrus validation covers one function; monitoring is passive, not active intervention
- **GitHub release folder** — Internal audit files removed; DLL (7.3MB), PDB (59MB), TOML (15.6KB) verified

### Removed

- **Internal testing framework** — CompatibilityTester, FunctionalityTester, IntegrationTester, MemorySafetyTester, PerformanceTester removed from production build
- **RmlUI system** — Deprecated UI framework removed; ImGui is the only UI system

---

## [3.3.3 FIX] - 2026-03-15

*Released under a temporary version number;*

### Added

- **High-frequency crash throttling** — Tracks crash frequency per module. After 20 crashes from the same module within 2 seconds, enters silent recovery mode: crashes are still intercepted but logging is suppressed. Summary messages log every 60 seconds while in silent mode. Eliminates the stutter and FPS drops caused by high-frequency crash handling overhead
  - Configurable via TOML: `enableModuleThrottling`, `moduleThrottleThreshold` (default: 20), `moduleThrottleWindowMs` (default: 2000), `moduleSilentDurationMs` (default: 30000), `moduleRelogIntervalMs` (default: 60000)

---

## [3.3.3] - 2026-03-15

*Released under a temporary version number; this work belongs between v2.3.2 and v2.2.8 in the release timeline.*

### Removed

- **Fake address library generation** — Removed code that generated empty address library `.bin` files at startup. These were breaking other SKSE plugins. The real Address Library for SKSE (SE/AE/VR) is now required

---

## Unreleased - 2026-03-04

- Save-safety wording in Nexus description clarified; removed any implication of guaranteed save safety
- `[ActorLOD]` TOML section added; initialization is opt-in via `enabled = true`

---

## Prior Releases

**v2.2.8** — CrashLogger cooperation mode: CrashGuard now writes `CrashGuard-recovery-*.log` files for every recovered crash, so users have records even when the game doesn't crash. At startup, historical CrashLogger `crash-*.log` files are scanned to build awareness of past crash patterns. XMM register support added for SIMD null-dereference recovery (XMM0–XMM15). Brittle SE v1.6.1170-only crash site offsets removed in favor of version-independent L1b pattern matching.

**v2.2.7** — Crash recovery extended to mod DLLs (skee64/RaceMenu, OBody, hdtSMP, and others). Pre-registered known crash sites for six RaceMenu crash patterns. L1b instruction-pattern matching introduced: decodes the faulting instruction with Zydis and matches on semantic pattern rather than fixed memory offset, making recovery version-independent across SE, AE, and VR.

**v2.2.6** — Single-DLL multi-runtime build using CommonLibSSE-NG (SE/AE/VR from one binary).

**v2.2.4** — Papyrus native function validation for SmartHarvest and other mods. F11 menu can be fully disabled via `[ImGui] disableMenu = true` in TOML for compatibility with other ImGui mods.

**v2.2.3** — Fixed plugin not loading with older SKSE versions (dual `SKSEPlugin_Info` / `SKSEPlugin_Version` exports). Fixed menu name mismatch that prevented camera zoom and D-pad input blocking from working. S.L.A.C.K. and CrashLogger compatibility modes switched to auto-detection.

**v2.2.2 / v2.2.1** — QuickLoot compatibility: mouse wheel scrolling and D-pad navigation in loot menus. Menu input conflict prevention system. Three-tier crash notification system (silent / toast / dialog).

**v2.2.0 and earlier** — F11 configuration menu, pattern learning, root cause analysis, resource monitoring, state snapshots, six-layer VEH defense architecture. See repository releases for full notes.
