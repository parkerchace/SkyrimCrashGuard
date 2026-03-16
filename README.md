# Skyrim Crash Guard

**Version 2.3.3** — Engine-Level Crash Recovery & Performance Optimization System for Skyrim SE/AE/VR

An SKSE plugin that **attempts** to prevent and recover from crashes through validation utilities, resource monitoring & management, and a 7-layer exception handling system with instruction pattern matching.

**Status:** Experimental - Use at your own risk. Always back up your saves.

**IMPORTANT:** This mod does NOT guarantee crash prevention or save protection. Recovery success varies with crash type, load order, and system configuration. Some crashes cannot be recovered. We recommend using alongside CrashLogger/Trainwreck for comprehensive crash analysis.

## Quick Links
- [Installation](#installation--quick-start)
- [How It Works](#how-it-works--6-layer-veh-recovery-chain--pattern-matching)
- [Configuration](#configuration)
- [Building from Source](#building-from-source)
- [Documentation](docs/) - Full documentation in docs/ folder
  - [CODE_STRUCTURE.md](docs/CODE_STRUCTURE.md) - Complete codebase overview (133 C++ files)
  - [DEPENDENCIES.md](docs/DEPENDENCIES.md) - All build and runtime dependencies
  - [CREDITS.md](docs/CREDITS.md) - Credits and licenses

## Supported Versions
- Skyrim Special Edition 1.5.97 (pre-AE)
- Skyrim Anniversary Edition 1.6.x (all versions)
- Skyrim VR 1.4.15

---

## Installation & Quick Start

### Requirements

**System Requirements:**
- **Windows 10/11** (64-bit) - Required for Vectored Exception Handling
- **DirectX 11** - Required for ImGui overlay (F11 menu)
- **4GB+ RAM** - Recommended for monitoring systems

**Game Requirements:**
- **SKSE64** (Skyrim Script Extender) for your game version:
  - SE 1.5.97: SKSE64 2.0.20+
  - AE 1.6.x: SKSE64 2.1.5+ (or latest)
  - VR 1.4.15: SKSEVR 2.0.12+
- **Address Library for SKSE Plugins** (automatically handles version differences)

### Installation Steps

**Manual Installation:**
1. Install SKSE64 for your runtime (SE/AE/VR).
2. Extract the mod archive.
3. Copy the `SKSE` folder to your Skyrim `Data` folder.
   - Final path: `Data/SKSE/Plugins/SkyrimCrashGuard.dll`
   - Final path: `Data/SKSE/Plugins/SkyrimCrashGuard.toml`
4. Launch the game through SKSE64.

**Mod Organizer 2 Installation:**
1. Install SKSE64 for your runtime.
2. Install the mod through MO2 (drag and drop archive or use "Install Mod").
3. Enable the mod in the left pane.
4. Launch the game through SKSE64 via MO2.

**Verification:**
- Press `F11` in-game to open the configuration menu.
- If the menu appears, installation is successful.
- Check the log file (see [Troubleshooting](#troubleshooting--logs)) for confirmation.

**Note:** No ESP/ESM plugin is required. This is a pure SKSE DLL plugin.

---

## How It Works — 6-Layer VEH Recovery Chain + Pattern Matching

**EXPERIMENTAL:** Crash recovery is best-effort and may not work for all crash types. Some crashes cannot be recovered. Always back up your saves.

When a crash occurs, Skyrim Crash Guard **attempts** to intercept it via Windows Vectored Exception Handling and recover through six progressively more aggressive strategies, plus instruction pattern matching:

| Layer | Name | Description |
|-------|------|-------------|
| **L1** | Known Site | Pre-analyzed crash addresses (game exe + mod DLLs) — instant recovery attempt |
| **L1b** | Instruction Pattern | Version-independent pattern matching via Zydis decoder |
| **L2** | Learned Site | Previously decoded at runtime, cached for instant replay |
| **L3** | Register Fixup | Attempts to redirect faulting base register to safety buffer |
| **L4** | Instruction Skip | Decodes instruction, zeros dest register, advances RIP |
| **L5** | Function Return | Pops return address, zeros RAX, returns to caller |
| **L6** | Deep Stack Walk | Scans stack for valid executable return addresses |

**L1b patterns** (v2.2.7+) attempt version-independent recovery:
- **P1**: `call [reg+disp]` with corrupted vtable → function return (prevents cascade crashes)
- **P2**: `jmp [reg+disp]` with corrupted vtable → function return
- **P3**: Read from null/invalid pointer → zero dest register (GP or XMM), skip instruction
- **P4**: Write to null/invalid pointer → skip write (game code) or function return (system DLLs)

**Crash types that MAY be recovered:** Access violations (null pointer reads/writes), corrupted vtable calls, null reference dereferences. Common examples include NiParticleSystem vtable corruption (fire spells, particle emitters), BSFadeNode skeleton initialization, hkaRagdollInstance, BSAnimationGraphManager, RaceMenu/OBody morph chains, water reflections (TESWaterReflections).

**Crash types that CANNOT be recovered:** Stack overflow, heap corruption, system DLL internal crashes, crashes during game exit, crashes after cascade protection limits are exceeded.

---

## CrashLogger Compatibility (v2.2.8+)

CrashGuard is designed to **cooperate** with CrashLogger, not compete with it. **We recommend using both together** for comprehensive crash analysis.

### How It Works
- CrashGuard's VEH handler runs **before** CrashLogger's
- When CrashGuard **successfully recovers** a crash → game keeps running, CrashLogger never sees it
- When CrashGuard **fails to recover** → CrashLogger receives the exception and writes its full analysis

### Two Sets of Logs
| Log Type | Location | Meaning |
|----------|----------|---------|
| `CrashGuard-recovery-*.log` | `My Games/Skyrim */SKSE/` | Crashes CrashGuard **attempted to recover** (game may have kept running) |
| `crash-*.log` (CrashLogger) | `My Games/Skyrim */SKSE/` | Crashes CrashGuard **could not recover** (game crashed) |

**Cross-reference both** for a complete picture of your session's stability.

### CrashLogger Log Ingestion
At startup, CrashGuard scans CrashLogger's crash logs from previous sessions and attempts to extract crash patterns. This helps build awareness of historical crash sites.

### What This Means for Users
- If CrashLogger writes a crash log, **that crash was real** — CrashGuard tried and failed to recover it
- CrashLogger's analysis of unrecovered crashes is **accurate** (CrashGuard doesn't corrupt the CONTEXT)
- Check CrashGuard's recovery reports to see crashes that **may have** crashed your game but were recovered

---

## Proactive Validation Systems (EXPERIMENTAL)

**IMPORTANT:** These are passive utility systems that validate data when explicitly called by other CrashGuard systems. They do NOT automatically hook into all game engine operations. Validation coverage is limited.

### Mesh Validation
- **Status:** Implemented but not actively integrated into mesh loading pipeline
- **What it does:** Validates vertex data, normals, UV coordinates, bone weights when called
- **What it does NOT do:** Does not automatically validate all meshes during loading
- **Limitation:** Intentionally permissive to avoid false positives; may not catch all mesh issues

### Papyrus Validation
- **Status:** Implemented with limited function coverage
- **What it does:** Validates parameters for specific problematic Papyrus native functions
- **Coverage:** Currently validates 1 function (SmartHarvest::NotifyActivated)
- **What it does NOT do:** Does not intercept all Papyrus function calls
- **Limitation:** Requires manual registration of validation wrappers for each function

### FormID Validation
- **Status:** Fully implemented utility system
- **What it does:** Provides safe FormID lookup with validation and caching
- **What it does NOT do:** Does not automatically intercept all FormID lookups
- **Limitation:** Other code can bypass validation by using direct lookups

### Cell Validation
- **Status:** Implemented with API limitations
- **What it does:** Validates cell data and references when called
- **What it does NOT do:** Cannot remove invalid references (CommonLibSSE API limitation)
- **Limitation:** Teleportation system is partially implemented

**Overall:** These systems provide validation infrastructure but have limited automatic integration. They work best when explicitly called by crash recovery systems.

---

## Monitoring & Metrics Systems

**IMPORTANT:** CrashGuard distinguishes between **monitoring** (passive observation) and **management** (active intervention).

### Pure Monitoring Systems (Passive Observation Only)
These systems track and report data but do NOT modify game behavior:

- **Performance Metrics**: Tracks FPS, frame time, memory usage, crash statistics
- **Memory Pressure Detector**: Monitors memory pressure levels (Normal/Elevated/High/Critical), provides recommendations but does NOT free memory
- **Recovery Statistics**: Records crash recovery events and user choices for display
- **Phase Tracker**: Tracks game phases, session statistics, and component status

### Hybrid Systems (Monitor + Manage)
These systems both monitor AND actively intervene:

- **Memory Manager**: 
  - **Monitors:** Memory usage, thresholds, warnings (4 pressure levels)
  - **Does NOT free memory:** Tracks resources but actual memory management is handled by game engine
  - **Limitation:** Despite the name, this is primarily a monitoring system
  
- **NPC Manager** (ACTIVE INTERVENTION):
  - **Monitors:** NPC counts, burden scores, cell baselines
  - **Manages:** Actively deletes/disables excess NPCs when threshold exceeded (default: 50 max)
  - **Emergency mode:** Ultra-fast deletion at 2x threshold (100+ NPCs)
  - **Limitation:** Reactive deletion only (PlaceAtMe hook disabled due to stability issues)
  
- **Script Monitor**:
  - **Monitors:** Script execution, timeouts, failures
  - **Manages:** Blacklists problematic scripts, blocks execution, terminates runaway scripts
  - **Limitation:** Limited VM integration, some features are stubbed

- **Deadlock Detector**:
  - **Monitors:** Lock acquisition, wait times, circular waits
  - **Manages:** Breaks detected deadlocks by releasing oldest locks
  - **Note:** Actively intervenes to prevent thread deadlocks

**Key Distinction:** Only NPCManager actively removes game objects. Memory tracking does NOT free memory—it only monitors and warns.

---

## Configuration

CrashGuard works with default settings out of the box. Configuration is optional and can be done through:
1. **F11 In-Game Menu** (recommended) - Visual interface with real-time changes
2. **TOML Configuration File** - Manual editing for advanced users

### Configuration File Location
`Data/SKSE/Plugins/SkyrimCrashGuard.toml`

### Common Configuration Options

**[General]**
```toml
enabled = true              # Enable/disable the entire plugin
logLevel = 1                # 0=off, 1=errors/warnings, 2=info, 3=debug, 4=trace
```

**[Hotkeys]**
```toml
menuToggleKey = "F11"       # Key to open configuration menu (F1-F12, Insert, Delete, etc.)
```
For controller users: Use Steam Input or similar software to map a controller button to F11.

**[VEH]** (Crash Recovery)
```toml
enabled = true              # Enable Vectored Exception Handler
cascadeLimit = 3            # Max recovery attempts during cascade crashes
```

**[UserNotifications]** (Crash Recovery Behavior)
```toml
NotifyOnSafe = false        # Auto-recover visual glitches silently
NotifyOnWarning = false     # Auto-recover missing resources with toast
NotifyOnCritical = true     # Always ask user for save-affecting crashes
NotifyOnFatal = true        # Always ask user for unrecoverable crashes
ShowToastForAutoRecovery = true    # Show brief notification for auto-recoveries
DialogTimeoutSeconds = 30   # Auto-select after timeout
```

**[NPCManagement]** (Performance Management)
```toml
autoManageNPCs = true       # Enable automatic NPC management
usePerCellBaseline = true   # Learn normal NPC count per cell (adapts to modded cities)
cellNPCDelta = 20           # How many NPCs above baseline to allow
disableInsteadOfDelete = true  # Disable NPCs instead of deleting (allows restoration)
```

**[InputConflictPrevention]** (Menu Scrolling Fix)
```toml
enabled = true              # Prevent camera zoom when scrolling menus
blockCameraZoom = true      # Block mouse wheel camera control in menus
autoDetectModdedMenus = true  # Auto-detect and learn modded menus
```

**[ImGui]** (F11 Menu)
```toml
disableMenu = false         # Set to true to disable F11 menu entirely
```

**[Logging]**
```toml
enableDetailedLogging = false  # Enable for debugging only (increases log size)
logOnlyFailures = true      # Only log validation failures
maxLogSizeMB = 10           # Rotate logs at 10MB
maxLogFiles = 3             # Keep last 3 log files
```

### Configuration Changes
- **F11 Menu changes:** Take effect immediately (most settings)
- **TOML file changes:** Require game restart
- **Recommended:** Use F11 menu for runtime adjustments, TOML for permanent changes

---

## Troubleshooting & Logs

### Log File Locations

**Primary Log:**
- **Path:** `Documents\My Games\Skyrim Special Edition\SKSE\SkyrimCrashGuard.log`
  - (SE: `Skyrim Special Edition`, AE: `Skyrim Special Edition`, VR: `Skyrim VR`)
- **Contains:** Plugin initialization, crash recovery attempts, validation failures, performance metrics

**CrashGuard Recovery Logs:**
- **Path:** `Documents\My Games\Skyrim Special Edition\SKSE\CrashGuard-recovery-*.log`
- **Contains:** Detailed crash recovery attempts (crashes CrashGuard tried to recover)

**CrashLogger Logs (if installed):**
- **Path:** `Documents\My Games\Skyrim Special Edition\SKSE\crash-*.log`
- **Contains:** Crashes CrashGuard could NOT recover (game crashed despite recovery attempts)
- **Note:** CrashGuard's log header appears at the top of CrashLogger logs to indicate recovery was attempted

### Common Issues & Solutions

**Plugin Not Loading**
- **Symptom:** F11 menu doesn't appear, no log file created
- **Check:**
  1. Verify SKSE is installed: Look for `skse64_loader.exe` in game folder
  2. Launch game through SKSE, not Steam/launcher
  3. Check SKSE log: `Documents\My Games\Skyrim Special Edition\SKSE\skse64.log`
  4. Verify DLL location: `Data/SKSE/Plugins/SkyrimCrashGuard.dll` must exist
  5. Check Address Library is installed (required dependency)
- **Solution:** Reinstall SKSE and Address Library, verify file paths

**F11 Menu Not Appearing**
- **Symptom:** Plugin loads but menu doesn't open
- **Check:**
  1. Open `SkyrimCrashGuard.toml`
  2. Verify `[ImGui] disableMenu = false`
  3. Try a different key: Change `[Hotkeys] menuToggleKey = "F12"` (or another key)
  4. Check log for ImGui initialization errors
- **Solution:** Enable menu in TOML, try different hotkey, check for ImGui conflicts

**Camera Zooms When Scrolling Menus**
- **Symptom:** Mouse wheel controls camera instead of scrolling in dialogue/inventory menus
- **Check:**
  1. Open `SkyrimCrashGuard.toml`
  2. Verify `[InputConflictPrevention] enabled = true`
  3. Set `logLevel = 2` in `[General]` to see menu detection
  4. Open the problematic menu and check log for menu name
- **Solution:** 
  - Add menu name to `customScrollableMenus` in TOML:
    ```toml
    customScrollableMenus = ["CustomDialogueMenu", "ModdedInventoryMenu"]
    ```
  - Or check F11 menu Settings tab to see detected menus

**Crashes Still Happening**
- **Symptom:** Game crashes despite CrashGuard installed
- **Understanding:**
  - CrashGuard cannot prevent ALL crashes (see [Known Limitations](#known-limitations--what-crashguard-cannot-do))
  - Some crash types are unrecoverable (stack overflow, heap corruption, system DLL crashes)
- **Check:**
  1. Look for `crash-*.log` files (CrashLogger) - these are crashes CrashGuard couldn't recover
  2. Look for `CrashGuard-recovery-*.log` files - these are crashes CrashGuard attempted to recover
  3. Check main log for "Recovery failed" or "Rejected" messages
- **Solution:**
  - Cross-reference both log types for complete picture
  - Use CrashLogger/Trainwreck alongside CrashGuard for unrecoverable crashes
  - Report patterns to mod author with both log types

**High Memory Usage / Performance Issues**
- **Symptom:** Game stutters, high RAM usage, FPS drops
- **Check:**
  1. Open F11 menu → Performance tab
  2. Check memory pressure level (Normal/Elevated/High/Critical)
  3. Check NPC count (if > 50, NPCManager should activate)
  4. Review `[NPCManagement]` settings in TOML
- **Solution:**
  - Lower `cellNPCDelta` (default: 20) to be more aggressive
  - Enable `[EngineOptimizations]` features for LOD management
  - Check for mods that spawn excessive NPCs (PlaceAtMe abuse)

**Script Errors / Papyrus Issues**
- **Symptom:** Script errors in Papyrus log, mod features not working
- **Note:** CrashGuard does NOT include Papyrus scripts (.pex files)
- **Check:**
  1. Verify no script files were installed (CrashGuard is DLL-only)
  2. Check for conflicts with script-heavy mods
  3. Review `[PapyrusValidation]` settings if validation is blocking calls
- **Solution:**
  - Set `[PapyrusValidation] strictMode = false` (default)
  - CrashGuard only validates 1 function (SmartHarvest::NotifyActivated)

**Build Errors (Developers)**
- **Symptom:** CMake or compilation errors when building from source
- **Check:**
  1. Verify Visual Studio 2022 with Desktop C++ workload
  2. Verify CMake 3.21+ installed
  3. Verify vcpkg installed and `VCPKG_ROOT` environment variable set
  4. Check vcpkg dependencies: spdlog, fmt, zydis, nlohmann-json, toml11, directxtk, imgui
- **Solution:** See [Building from Source](#building-from-source-windows--msvc)

### Understanding Log Output

**Sample Log Entry (Crash Recovery):**
```
[2026-03-10 17:02:04] [ERROR] VEH: Access violation at 0x7FF71C9176DE
[2026-03-10 17:02:04] [INFO] VEH: Attempting L1 (Known Site) recovery...
[2026-03-10 17:02:04] [INFO] VEH: L1 failed, attempting L1b (Pattern Match)...
[2026-03-10 17:02:04] [INFO] VEH: Pattern P3 matched (null pointer read)
[2026-03-10 17:02:04] [INFO] VEH: Recovery successful, game continuing
```

**Sample Log Entry (Recovery Failed):**
```
[2026-03-10 17:15:22] [ERROR] VEH: Stack overflow detected at 0x7FF71C9176DE
[2026-03-10 17:15:22] [WARN] VEH: Crash type rejected (stack overflow unrecoverable)
[2026-03-10 17:15:22] [INFO] VEH: Passing exception to CrashLogger
```

**Log Levels:**
- `ERROR`: Crashes, critical failures, recovery attempts
- `WARN`: Validation failures, resource warnings, cascade limits
- `INFO`: Successful recoveries, system initialization, configuration changes
- `DEBUG`: Detailed diagnostics, menu detection, input tracking
- `TRACE`: Per-frame data, verbose debugging (not recommended for normal use)

### Reporting Issues

When reporting issues, include:
1. **SkyrimCrashGuard.log** (primary log)
2. **crash-*.log** (CrashLogger, if applicable)
3. **CrashGuard-recovery-*.log** (recovery attempts, if applicable)
4. **Load order** (modlist.txt or MO2 export)
5. **Game version** (SE 1.5.97, AE 1.6.x, VR 1.4.15)
6. **SKSE version**
7. **Steps to reproduce** (if possible)

**Where to find logs:**
- All logs: `Documents\My Games\<Skyrim Version>\SKSE\`
- Or check F11 menu → Logs tab for quick access

---

## Known Limitations & What CrashGuard CANNOT Do

### Crash Types That CAN Be Recovered
CrashGuard **attempts** to recover these crash types (success varies):
- **Access violations** - Null pointer reads/writes, corrupted vtable calls
- **Null reference dereferences** - Common in particle systems, animations, rendering
- **Corrupted vtable calls** - NiParticleSystem, BSFadeNode, TESObjectARMO
- **Invalid memory reads** - Mesh loading, texture access, skeleton initialization
- **Invalid memory writes** - Game code writes to null/invalid pointers

**Common examples:** Fire spell particles, water reflections, RaceMenu morphs, ragdoll physics, LOD transitions, save/load rendering.

### Crash Types That CANNOT Be Recovered
- **Stack overflow crashes** - Explicitly rejected, no recovery attempted
- **Heap corruption** - Classified as fatal, no recovery possible
- **System DLL internal crashes** - Crashes in ntdll.dll, kernel32.dll, VCRUNTIME140.dll are rejected
- **CrashGuard's own crashes** - Self-crashes rejected to prevent recursive VEH entry
- **Crashes during game exit** - All exceptions during exit phase are rejected
- **Crashes after cascade limits** - Recovery stops after 100 crashes in 5 seconds (global limit)

### Validation System Limitations
- **Mesh validation:** No automatic hooks into mesh loading pipeline (utility functions only)
- **Papyrus validation:** Limited to 1 registered function (SmartHarvest::NotifyActivated)
- **Cell validation:** Cannot remove invalid references due to CommonLibSSE API limitations
- **FormID validation:** Passive system, other code can bypass by using direct lookups

### Resource Management Limitations
- **Memory Manager:** Tracks memory but does NOT free it (game engine handles actual freeing)
- **NPC Manager:** Reactive deletion only (PlaceAtMe hook disabled due to stability issues)
- **Script Monitor:** Limited VM integration, some features are stubbed

### Save Protection
- **EXPERIMENTAL - Best Effort Only**
- **NOT a guarantee** - Under some failure modes, saves may still be at risk
- **Always back up your saves** before using this mod
- CoSave coordination and state validation are experimental features

### Recovery Side Effects
- Recovered crashes may cause visual glitches (missing particles, effects)
- Recovered crashes may cause missing resources (textures, meshes not loaded)
- Cascade protection may give up after limits exceeded
- Some recoveries may leave game in inconsistent state (save and reload recommended)

### Compatibility Notes

**Compatible Mods:**
- **CrashLogger / Trainwreck** - Designed to work together (CrashGuard runs first, CrashLogger analyzes unrecoverable crashes)
- **SSE Engine Fixes** - Compatible, both can run simultaneously without VEH conflicts
- **Most SKSE plugins** - Generally compatible unless they also use VEH handlers

**Known Compatibility Considerations:**
- **Load order:** Results vary significantly with load order and mod combinations
- **Script-heavy mods:** Heavy script loads may exceed monitoring capabilities
- **Validation conflicts:** Some mods may conflict with validation systems (rare)
- **Performance mods:** Performance optimizations are experimental and may not work with all setups

**Mod Conflict Detection:**
- CrashGuard includes basic mod conflict detection for file-level conflicts
- Check logs for conflict warnings if experiencing issues
- Use xEdit for detailed load order conflict analysis

**Bottom Line:** CrashGuard is a best-effort experimental tool. It may help with some crashes but cannot prevent all crashes. Use alongside CrashLogger/Trainwreck for comprehensive crash analysis.

---

## Building from Source (Windows / MSVC)

Prerequisites:
- Visual Studio 2022 (Desktop C++)
- CMake 3.21+
- vcpkg (set `VCPKG_ROOT`)

Quick commands (from `Source`):

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Notes:
- The project uses CommonLibSSE-NG for a single-DLL multi-runtime build (SE/AE/VR).
- vcpkg dependencies: spdlog, fmt, zydis, nlohmann_json, toml11, directxtk, imgui.
- See `CMakeLists.txt` and `docs/README_FULL.md` for details.

---

## Developer Notes & Credits

- Multi-runtime support via CommonLibSSE-NG's `add_commonlibsse_plugin()` helper.
- Per-subsystem logging toggles reduce log noise by default.
- Zydis x86-64 decoder used for instruction-level crash analysis (L1b, L3, L4).

See `docs/CREDITS.md` for full attributions and licenses.

---

## History & Legacy Docs

Full changelog: `CHANGELOG.md` (root).

Legacy per-topic docs have been archived in `docs/archive/` for reference.

---

If you prefer a different structure (e.g., split docs, MCM-oriented readme, or keep shorter Quick Start at top-level), tell me which sections to expand or remove.

````
