# Changelog

All notable changes to SkyrimCrashGuard are documented here.

---

## [2.4.0] - 2026-09-21

Migration release: CommonLibSSE NG (maintained fork), GPL-3.0-or-later, Skyrim AE 1.7.104 + VR 1.4.15.

### Changed

- **License: MIT to GPL-3.0-or-later** — CharmedBaryon/CommonLibSSE-NG has been unmaintained since September 2024 and does not support Skyrim 1.7.x. The maintained continuation, [alandtse/CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) (branch `ng`), is GPL-3.0-or-later. Crash Guard links it statically, which makes the DLL a combined work, so the plugin is now GPL-3.0-or-later too. CommonLibSSE NG's Modding Exception covers linking against Skyrim, SKSE and the GPU SDKs — it does not cover plugin code. `LICENSE` now holds the GPL-3.0 text and all 157 source and header files carry a GPL notice
- **CommonLibSSE NG 3.6.0 (vcpkg) to v9.0.0 (CMake FetchContent)** — the `vcpkg-colorglass` registry and the in-tree `cmake/ports/commonlibsse-ng` port are gone. CMake now fetches the library pinned to tag `v9.0.0` into `build/_deps/commonlibsse-src` and compiles it in-tree
- **Runtime targets: SE + AE + VR to AE + VR** — `ENABLE_SKYRIM_SE=OFF`, `ENABLE_SKYRIM_AE=ON`, `ENABLE_SKYRIM_VR=ON`. This makes CommonLibSSE define `SKYRIM_CROSS_VR`, where flat-vs-VR struct differences are resolved at runtime. Skyrim SE 1.5.97 is no longer supported
- **Minimum SKSE version 2.3.1** — declared through `add_commonlibsse_plugin(... MINIMUM_SKSE_VERSION 2.3.1)`. This does not lock out VR: `SKSEPluginInfo` also emits the legacy `SKSEPlugin_Query` export that SKSEVR 2.0.12 uses
- **openvr submodule fetched sparsely** — `cmake/FetchOpenVR.cmake` fetches the exact openvr commit CommonLibSSE pins, blobless and limited to `headers/` and `lib/win64`, instead of cloning ~1 GB of ValveSoftware/openvr history for two directories
- **vcpkg baseline** — `d1e11918...` to `6d7bf7ef...`; `directxmath` and `rapidcsv` added (CommonLibSSE NG needs them), `commonlibsse-ng` removed
- **`BUILD_TESTS` renamed to `CRASHGUARD_BUILD_TESTS`** — CommonLibSSE is built in-tree and owns the cache variable `BUILD_TESTS` for its own Catch2 suite
- **`/utf-8` added to the compile flags** — fmt 12 static-asserts on it for Unicode support

### Added

- **F11 Recovery view now describes the actual crash** - the detail panel led with a per-layer source-code walkthrough that read identically for every crash that layer handled. It now opens with this crash: outcome, address, how many times that address has fired this session, what the faulting instruction did (read/write/transfer of control, and how the touched address relates to null), where it faulted, and what CrashGuard changed - naming the register it set to 0 or the write it withheld. The walkthrough is still there, folded into a collapsed "CrashGuard source walkthrough" section. A "Copy report" button puts the entry on the clipboard for bug reports.

  The text states only what CrashGuard observed. It does not say whose fault a crash was: the module holding the faulting instruction is where the code was executing, not evidence that the module produced the bad value, and the panel says so rather than implying blame
- **Modules on the stack are listed for recovered crashes** - the faulting module is usually the game executable, which names nothing useful. On recovery, CrashGuard now reads a shallow slice of raw stack slots and lists the non-game modules found below the fault (`RecoveryEntry::suspectedMods`, previously always empty). System, GPU-driver, crash-logger and CrashGuard frames are filtered out. Presented as "what was present, not a diagnosis", because raw slots can hold stale return addresses. Loader lookups are capped per recovery
- **Self-tests are identified as self-tests** - faults raised by the built-in crash-test suite were recorded like any other crash, and since the stub tiers fault in an allocated page that belongs to no module, they appeared in the recovery list as "unknown". `VEHExceptionHandler::BeginSelfTestScope()` now marks them (labelling only - the stub tiers still run under the real recovery rules, which is their purpose). Such entries are tagged `[test]` in the list, carry a `[SELF-TEST]` badge and a "CrashGuard test harness" module, show no mod attribution, are counted separately in the session summary, prefix their toast with "Self-test:", and no longer advance the "N issues prevented" HUD counter

### Fixed

- **Built-in flat-runtime crash sites are version-gated** — the non-VR entries in `VEH::Initialize` (Moon/Sky rendering, SKSE init string construction, ImpactManager, and the two unconditional particle-system sites) are raw module offsets disassembled on **AE 1.6.1170**; offset `+0D1BF70` is address-library ID 70251 in `versionlib-1-6-1170-0.bin`, matching the "function 70251" in the code comments. The same ID sits at `+0EE0930` on 1.7.104, so on any other build those entries pointed canned register-and-skip repairs at unrelated instructions — and the two unconditional ones were applied to SkyrimVR.exe as well. They are now registered only when the running executable is 1.6.1170; every other runtime relies on L1b pattern matching, which the comments already describe as covering the same crash shapes. `IsInMoonOrSkyFunction` is gated the same way
- **Bone data is read through CommonLibSSE's accessors** — `NiSkinData::BoneData` is 0x58 bytes on flat runtimes and 0x70 in VR, so the old `skinData->boneData[i]` indexing in `MeshValidator` read the wrong bone in VR. Bone count, per-bone vertex data and vertex counts now go through `GetBoneCount()` / `GetBoneDataBoneVertData()` / `GetBoneDataVerts()`
- **Deleted `include/openvr.h`** — a hand-written stub declaring empty `vr::IVRSystem` and friends, left over from when VR was not really built. With VR enabled it would shadow CommonLibSSE's real `openvr.h` on the include path

### API migration (CommonLibSSE NG 3.6 to 9.0)

- `REL::IDDatabase` to `REL::IDDB`
- `RE::BSRenderManager` to `RE::BSGraphics::Renderer`; the swap chain moved from the runtime data to `renderWindows[0].swapChain`, and the D3D pointers are now `REX::W32` declarations that are cast to the real d3d11 types for the ImGui backend
- `RE::DebugNotification` to `RE::SendHUDMessage::ShowHUDMessage` (16 call sites)
- `RE::NiGeometry::RUNTIME_DATA` members lost their `m_` prefix: `m_spModelData` to `spModelData`, `m_spSkinInstance` to `spSkinInstance`
- `RE::ControlMap` members (`ignoreKeyboardMouse`, `ignoreActivateDisabledEvents`) are reached through `GetRuntimeData()` in a cross-VR build, and `ToggleControls` takes a third `storeState` argument
- `RE::TESObjectCELL::ForEachReference` hands the callback a `TESObjectREFR*` instead of a reference; all five callbacks now take a pointer and null-check it
- `RE::TES::worldSpace` moved into the versioned `RUNTIME_DATA2` block: `tes->GetRuntimeData2().worldSpace`
- `RE::CrosshairPickData::target` is one handle per tracked device in a cross-VR build; `GetActiveTarget()` picks the right one
- `ImGuiIO::KeysDown[]` (removed in Dear ImGui 1.87) to `io.ClearInputKeys()`
- Hard-coded vtable ids in `FunctionHookManager` (`{235511, 190259}` and `{256504, 205174}`) replaced by `RE::VTABLE_TESObjectREFR[0]` / `RE::VTABLE_IAnimationGraphManagerHolder[0]`, which carry the AE id and the VR offset together and follow CommonLibSSE updates
- `AddressLib::ResolveID()` removed — a missing address-library id terminates the process inside `REL::IDDB` (`report_and_fail`), so a wrapper that appears to fail softly was misleading

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
