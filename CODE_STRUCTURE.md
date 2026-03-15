# SkyrimCrashGuard Code Structure

This document provides a comprehensive overview of all 133 C++ source files in the SkyrimCrashGuard codebase (68 headers + 65 implementations), including file purposes, key classes/functions, and interdependencies.

## Core Systems

### Crash Detection & Recovery (VEH System)
- **VEH.cpp/h** (3,400+ lines) - 7-layer Vectored Exception Handler chain (L1-L6 recovery strategies)
  - **Purpose**: Main crash interception and recovery system using Windows VEH
  - **Key Classes**: `VEHExceptionHandler` - manages exception filtering, thread pausing, and recovery coordination
  - **Key Functions**: `ExceptionFilter()` - main VEH callback; `AnalyzeException()` - builds crash context; `IsRecoverable()` - determines if crash can be recovered
  - **Dependencies**: Uses RootCauseAnalyzer, SeverityAnalyzer, PatternLearningSystem, DynamicFixApplicator for recovery decisions
  - **Interdependencies**: Called by Windows on exceptions; invokes DiagnosticLogger, CrashCollector, RecoveryStatistics

- **RootCauseAnalyzer.cpp/h** - Analyzes crash context to determine root cause
  - **Purpose**: Diagnoses underlying cause of crashes (mesh, animation, script, AI, cell, memory, grid boundary)
  - **Key Classes**: `RootCauseAnalyzer` - performs crash classification and analysis
  - **Key Enums**: `CrashCategory` - categorizes crash types
  - **Key Functions**: `AnalyzeCrash()` - main analysis entry point; `DetectGridBoundaryCrash()` - specialized grid boundary detection; `RankSuspectedMods()` - identifies likely culprit mods
  - **Dependencies**: Uses VEH::CrashContext, GameObjectIntrospector for object analysis
  - **Interdependencies**: Used by VEH, DynamicFixApplicator, DiagnosticLogger for crash diagnosis

- **SeverityAnalyzer.cpp/h** - Classifies crash severity (Safe/Warning/Critical/Fatal)
  - **Purpose**: Provides detailed crash severity classification with human-readable explanations
  - **Key Classes**: `SeverityAnalyzer` - analyzes crashes and generates severity reports
  - **Key Structs**: `SeverityAnalysis` - contains severity level, explanations, risks, recommendations
  - **Key Functions**: `AnalyzeCrash()` - generates detailed severity analysis; `ClassifyByCallStack()` - scans for dangerous function names
  - **Dependencies**: Uses VEH::CrashContext, RootCauseAnalysis::RootCauseResult
  - **Interdependencies**: Used by VEH, DiagnosticLogger, UserNotificationManager for severity-based decisions

- **PatternLearningSystem.cpp/h** - L2 learned-site cache for runtime pattern recognition
  - **Purpose**: Learns from crashes and improves recovery strategies over time
  - **Key Classes**: `PatternLearningSystem` - manages pattern database and strategy selection
  - **Key Structs**: `PatternEntry` - tracks crash signatures and strategy success rates; `StrategyRecord` - records strategy performance
  - **Key Functions**: `RecordSuccess()`/`RecordFailure()` - update pattern database; `GetBestStrategy()` - selects optimal recovery strategy; `ExportPatterns()`/`ImportPatterns()` - persist learned patterns
  - **Dependencies**: Uses VEH::CrashContext, DynamicFix::RecoveryStrategy
  - **Interdependencies**: Used by VEH for strategy selection; persists to patterns.json file

- **DynamicFixApplicator.cpp/h** - Applies dynamic fixes during recovery
  - **Purpose**: Applies runtime fixes to resolve crashes (mesh repair, animation retry, script skip, etc.)
  - **Key Classes**: `DynamicFixApplicator` - applies recovery strategies
  - **Key Enums**: `RecoveryStrategy` - defines available recovery strategies (MeshRepair, AnimationRetry, ScriptSkip, etc.)
  - **Key Functions**: `ApplyFix()` - main fix application entry point; `FixNullPointer()` - handles null pointer crashes; `PatchInstruction()` - patches crash site instructions
  - **Dependencies**: Uses VEH::CrashContext, RootCauseAnalysis::RootCauseResult
  - **Interdependencies**: Used by VEH for crash recovery; works with PatternLearningSystem for strategy selection

- **RealTimeFixApplicator.cpp/h** - Real-time fix application with dangling pointer detection
  - **Purpose**: Applies fixes to game objects in real-time without requiring a crash (proactive fixes)
  - **Key Classes**: `RealTimeFixApplicator` - manages real-time fixes
  - **Key Functions**: `ReplaceMeshRealTime()` - replaces bad meshes immediately; `SwitchAnimationRealTime()` - switches to default idle on animation failure; `DisableScriptRealTime()` - disables corrupted scripts; `RemoveReferenceRealTime()` - removes invalid references
  - **Dependencies**: Uses RE::Skyrim (CommonLibSSE) for game object manipulation
  - **Interdependencies**: Used by validation systems (MeshValidator, PapyrusValidator) for proactive fixes

### Crash Logging & Reporting
- **DiagnosticLogger.cpp/h** - Comprehensive crash logging with register dumps and stack traces
  - **Purpose**: Advanced diagnostic logging system with user-friendly crash reports
  - **Key Classes**: `DiagnosticLogger` - manages structured logging and crash report generation
  - **Key Structs**: `CrashReport` - comprehensive crash analysis with game state, performance metrics, recovery actions; `LogEntry` - structured log entry with metadata; `GameStateContext` - captures player location, quests, NPCs, activity
  - **Key Functions**: `CreateCrashReport()` - generates comprehensive crash reports; `WriteCrashReport()` - outputs in multiple formats (JSON, Markdown, HTML); `GenerateInGameSummary()` - creates user-friendly summaries for notifications
  - **Dependencies**: Uses VEH::CrashContext, RootCauseAnalysis, SeverityAnalysis
  - **Interdependencies**: Used by all systems for logging; generates files in SKSE/Plugins/CrashGuard/logs/

- **UnifiedCrashReport.cpp/h** - Unified crash report generation
  - **Purpose**: Merges crash data from multiple sources (VEH, CrashLogger, Trainwreck) into unified format
  - **Key Classes**: `ReportManager` - manages unified report creation and merging
  - **Key Structs**: `UnifiedReport` - combines data from all crash logging sources; `RecoveryAction` - tracks recovery attempts; `SystemInfo` - captures system and mod information
  - **Key Functions**: `CreateReport()` - creates unified report from VEH context; `MergeCrashLoggerData()`/`MergeTrainwreckData()` - merges external crash logger data; `ExportToCFormat()` - exports for external APIs
  - **Dependencies**: Uses VEH::CrashContext, CrashLoggerDetector::LoggerInfo
  - **Interdependencies**: Used by DiagnosticLogger, CrashLoggerIntegration for unified reporting

- **CrashCollector.cpp/h** - Collects crash data for analysis
  - **Purpose**: Records crash data from VEH catches to identify new patch candidates
  - **Key Structs**: `CrashRecord` - tracks fault address, exception code, module, hit count
  - **Key Functions**: `Record()` - thread-safe crash recording; `Flush()` - writes to CrashPatterns.json on shutdown; `GetRecords()` - retrieves all recorded crashes
  - **Dependencies**: None (standalone crash tracking)
  - **Interdependencies**: Used by VEH to record crashes; persists to CrashPatterns.json

- **RecoveryStatistics.cpp/h** - Tracks recovery success rates and statistics
  - **Purpose**: Tracks recovery statistics for current session (displayed in F11 menu)
  - **Key Classes**: `RecoveryStatistics` - singleton tracking recovery counts by severity and user choices
  - **Key Functions**: `RecordRecovery()` - records crash recovery event; `RecordUserChoice()` - tracks user notification responses; `Reset()` - clears statistics
  - **Dependencies**: Uses VEH::SeverityLevel, UserNotifications::UserChoice
  - **Interdependencies**: Used by VEH, UserNotificationManager; displayed in ImGuiConfigMenu

- **RecoveryNotifications.cpp/h** - User notifications for crash recoveries
  - **Purpose**: Generates user-facing notifications for crash recoveries
  - **Key Functions**: Notification message generation and formatting
  - **Dependencies**: Uses VEH::CrashContext, SeverityAnalysis
  - **Interdependencies**: Used by UserNotificationManager for notification content

- **UserNotificationManager.cpp/h** - Manages user-facing notifications
  - **Purpose**: Manages user notifications for crashes based on severity and configuration
  - **Key Classes**: `UserNotificationManager` - coordinates notification display and user choices
  - **Key Enums**: `UserChoice` - Continue, LoadSave, Teleport, ViewLog, CrashAnyway, Timeout
  - **Key Functions**: Notification display, user choice handling, timeout management
  - **Dependencies**: Uses VEH::SeverityLevel, SeverityAnalysis, Config settings
  - **Interdependencies**: Used by VEH for user interaction; updates RecoveryStatistics

### CrashLogger Integration
- **CrashLoggerDetector.cpp/h** - Detects CrashLogger presence
  - **Purpose**: Detects presence of CrashLogger/Trainwreck and retrieves version information
  - **Key Classes**: `CrashLoggerDetector` - scans for crash logger DLLs and exports
  - **Key Structs**: `LoggerInfo` - contains logger name, version, path, capabilities
  - **Key Functions**: `DetectCrashLoggers()` - scans for installed crash loggers; `GetLoggerInfo()` - retrieves logger details
  - **Dependencies**: Windows API for DLL scanning
  - **Interdependencies**: Used by CrashLoggerIntegration, UnifiedCrashReport for compatibility

- **CrashLoggerIntegration.cpp/h** - Cooperates with CrashLogger for unrecoverable crashes
  - **Purpose**: Coordinates with CrashLogger to handle unrecoverable crashes (complementary operation)
  - **Key Classes**: `CrashLoggerIntegration` - manages cooperation with external crash loggers
  - **Key Functions**: `Initialize()` - sets up integration; `HandoffToLogger()` - passes unrecoverable crashes to CrashLogger; `MergeReports()` - combines crash data
  - **Dependencies**: Uses CrashLoggerDetector, UnifiedCrashReport
  - **Interdependencies**: Used by VEH for unrecoverable crash handling

- **TrainwreckBridge.cpp/h** - Bridge to Trainwreck crash logger
  - **Purpose**: Specific integration bridge for Trainwreck crash logger
  - **Key Classes**: `TrainwreckBridge` - handles Trainwreck-specific API calls
  - **Key Functions**: Trainwreck API integration, data exchange
  - **Dependencies**: Trainwreck API (if available)
  - **Interdependencies**: Used by CrashLoggerIntegration for Trainwreck-specific operations

- **PostMortemCoordination.cpp/h** - Coordinates post-crash analysis
  - **Purpose**: Coordinates post-crash analysis and cleanup across multiple systems
  - **Key Classes**: `PostMortemCoordination` - manages post-crash workflow
  - **Key Functions**: Post-crash cleanup, report finalization, system state restoration
  - **Dependencies**: Uses VEH::CrashContext, UnifiedCrashReport
  - **Interdependencies**: Used by VEH after crash recovery or handoff

## Validation Systems

### Proactive Validation (Layer 1)
- **MeshValidator.cpp/h** - Validates mesh data before rendering
  - **Purpose**: Validates and repairs 3D mesh files to prevent crashes from corrupted geometry
  - **Key Classes**: `MeshValidator` - validates vertex data, normals, UVs, bone weights, triangles
  - **Key Structs**: `ValidationResult` - contains validation errors, warnings, repair capability
  - **Key Functions**: `ValidateMesh()` - validates mesh before loading; `RepairMesh()` - attempts procedural repair (recalculate normals, generate UVs, remove degenerate triangles); `GetPlaceholderMesh()` - returns fallback cube mesh
  - **Dependencies**: Uses RE::NiAVObject, RE::NiGeometry (CommonLibSSE)
  - **Interdependencies**: Used by FunctionHookManager mesh hooks; may invoke RealTimeFixApplicator for mesh replacement

- **PapyrusValidator.cpp/h** - Validates Papyrus script execution
  - **Purpose**: Validates Papyrus script execution to prevent script-related crashes
  - **Key Classes**: `PapyrusValidator` - validates script calls and parameters
  - **Key Functions**: Script validation, parameter checking, error detection
  - **Dependencies**: Uses RE::BSScript (CommonLibSSE)
  - **Interdependencies**: Used by PapyrusNativeFunctionHook; may invoke RealTimeFixApplicator to disable corrupted scripts

- **FormIDValidator.cpp/h** - Validates FormID references
  - **Purpose**: Validates FormID references to prevent crashes from invalid or missing forms
  - **Key Classes**: `FormIDValidator` - validates form references
  - **Key Functions**: FormID validation, reference checking, mod load order verification
  - **Dependencies**: Uses RE::TESForm (CommonLibSSE)
  - **Interdependencies**: Used by various systems that handle form references

- **ScriptMonitor.cpp/h** - Monitors script execution for errors
  - **Purpose**: Monitors Papyrus script execution for errors and performance issues
  - **Key Classes**: `ScriptMonitor` - tracks script execution, timeouts, errors
  - **Key Functions**: Script execution monitoring, timeout detection, error logging
  - **Dependencies**: Uses RE::BSScript (CommonLibSSE), Config for timeout settings
  - **Interdependencies**: Works with PapyrusValidator; reports to DiagnosticLogger

### Papyrus Native Function Hooks
- **PapyrusNativeFunctionHook.cpp/h** - Intercepts Papyrus native calls
  - **Purpose**: Intercepts Papyrus native function calls for validation and error prevention
  - **Key Classes**: `PapyrusNativeFunctionHook` - manages native function interception
  - **Key Functions**: Hook installation, native call interception, parameter validation
  - **Dependencies**: Uses RE::BSScript (CommonLibSSE), SKSE for hooking
  - **Interdependencies**: Works with PapyrusNativeFunctions, PapyrusValidator

- **PapyrusNativeFunctions.cpp/h** - Validates native function parameters
  - **Purpose**: Validates parameters for Papyrus native functions before execution
  - **Key Classes**: `PapyrusNativeFunctions` - parameter validation logic
  - **Key Functions**: Parameter type checking, range validation, null checking
  - **Dependencies**: Uses RE::BSScript (CommonLibSSE)
  - **Interdependencies**: Used by PapyrusNativeFunctionHook for validation

## Resource Management

### Memory & Performance
- **MemoryManager.cpp/h** - Memory monitoring and resource collection
  - **Purpose**: Manages memory resources and handles allocation failures (monitoring only, not direct freeing)
  - **Key Classes**: `MemoryManager` - singleton managing memory monitoring and warnings
  - **Key Structs**: `MemoryStats` - tracks physical/virtual memory, working set, usage percent; `ResourceInfo` - tracks resource address, size, access patterns
  - **Key Functions**: `GetMemoryStats()` - retrieves current memory statistics; `MonitorMemoryUsage()` - checks memory thresholds; `WarnUserAboutMemory()` - displays memory warnings; `FreeDistantCellResources()`/`FreeUnusedTextures()` - resource collection suggestions
  - **Dependencies**: Windows API for memory statistics
  - **Interdependencies**: Used by MemoryPressureDetector, PerformanceMetrics; displays warnings via UserNotificationManager

- **MemoryPressureDetector.cpp/h** - Detects memory pressure with 4 severity levels
  - **Purpose**: Detects memory pressure and triggers appropriate responses (Normal, Low, Critical, Emergency)
  - **Key Classes**: `MemoryPressureDetector` - monitors memory pressure levels
  - **Key Enums**: Memory pressure levels (Normal, Low, Critical, Emergency)
  - **Key Functions**: `DetectPressureLevel()` - determines current pressure; `TriggerPressureResponse()` - initiates resource freeing
  - **Dependencies**: Uses MemoryManager for statistics
  - **Interdependencies**: Triggers NPCManager, MemoryManager for resource management

- **PerformanceMetrics.cpp/h** - Tracks performance metrics
  - **Purpose**: Tracks FPS, frame time, memory usage, crash statistics for performance monitoring
  - **Key Classes**: `PerformanceMetrics` - collects and tracks performance data
  - **Key Structs**: Performance data structures for FPS, frame time, memory
  - **Key Functions**: `RecordFrame()` - records frame metrics; `GetAverageFPS()` - calculates FPS; `GetMemoryUsage()` - retrieves memory stats
  - **Dependencies**: Uses MemoryManager, Windows API for timing
  - **Interdependencies**: Used by ImGuiConfigMenu for performance overlay; used by DiagnosticLogger for crash reports

- **PerformanceOptimizations.h** - Header-only optimization library (inline/template functions for fast-path checks, validation caching, symbol resolution)
  - **Purpose**: Header-only library providing inline/template functions for performance-critical operations
  - **Key Features**: Fast-path validation checks, caching mechanisms, optimized symbol resolution
  - **Key Functions**: Inline validation helpers, template-based caching, fast symbol lookups
  - **Dependencies**: None (header-only)
  - **Interdependencies**: Included by performance-critical systems (VEH, validators, hooks)

- **BenchmarkManager.cpp/h** - Performance benchmarking
  - **Purpose**: Provides performance benchmarking capabilities for testing and optimization
  - **Key Classes**: `BenchmarkManager` - manages benchmark execution and results
  - **Key Functions**: Benchmark execution, timing, result collection and reporting
  - **Dependencies**: Uses PerformanceMetrics
  - **Interdependencies**: Used for development and testing; controlled by Config::allowBuiltinActions

### Actor & NPC Management
- **NPCManager.cpp/h** - NPC lifecycle management and emergency deletion
  - **Purpose**: Manages NPC population, emergency deletion, and performance optimization
  - **Key Classes**: `NPCManager` - manages NPC lifecycle, culling, restoration
  - **Key Functions**: `ManageNPCPopulation()` - maintains NPC count within limits; `EmergencyDeleteNPCs()` - removes NPCs under memory pressure; `DisableNPC()`/`RestoreNPC()` - disable/restore NPCs; `CleanupDeadBodies()` - removes corpses
  - **Key Features**: Per-cell baseline tracking, smart prioritization (burden weights), whitelist/blacklist keywords, behind-player restoration
  - **Dependencies**: Uses RE::Actor, RE::TESObjectCELL (CommonLibSSE), Config for thresholds
  - **Interdependencies**: Triggered by MemoryPressureDetector; controlled via ImGuiConfigMenu NPC Tools tab

- **CellManager.cpp/h** - Cell loading and validation
  - **Purpose**: Manages cell loading, validation, and resource tracking
  - **Key Classes**: `CellManager` - validates cells, tracks resources
  - **Key Functions**: `ValidateCell()` - validates cell data; `TrackCellResources()` - monitors cell resource usage; `UnloadDistantCells()` - unloads distant cell resources
  - **Dependencies**: Uses RE::TESObjectCELL (CommonLibSSE)
  - **Interdependencies**: Used by MemoryManager for resource collection; works with CellLoadingEventHandler

- **CellLoadingEventHandler.h** - Handles cell loading events
  - **Purpose**: Event handler for cell loading/unloading events
  - **Key Classes**: `CellLoadingEventHandler` - SKSE event sink for cell events
  - **Key Functions**: Event handling for cell load/unload
  - **Dependencies**: Uses RE::TESObjectCELL, SKSE event system
  - **Interdependencies**: Notifies CellManager, StateManager of cell changes

## State Management

### Game State
- **StateManager.cpp/h** - Game state validation and corruption tracking
  - **Purpose**: Validates game state, tracks corruption, manages state snapshots for rollback
  - **Key Classes**: `StateManager` - manages game state validation and snapshots
  - **Key Functions**: `ValidateGameState()` - checks for state corruption; `CreateSnapshot()` - saves state snapshot; `RestoreSnapshot()` - rolls back to previous state; `DetectCorruption()` - identifies corrupted state
  - **Dependencies**: Uses RE::Skyrim (CommonLibSSE), Config for snapshot limits
  - **Interdependencies**: Used by VEH for state rollback recovery; works with SaveLoadResilience

- **SaveLoadResilience.cpp/h** - Save/load resilience features
  - **Purpose**: Provides save/load resilience to prevent save corruption during crashes
  - **Key Classes**: `SaveLoadResilience` - manages save safety features
  - **Key Functions**: `ProtectSaveOperation()` - guards save operations; `ValidateSaveData()` - checks save integrity; `RecoverCorruptedSave()` - attempts save recovery
  - **Dependencies**: Uses RE::BGSSaveLoadManager (CommonLibSSE)
  - **Interdependencies**: Works with StateManager, CoSaveManager for save protection

- **CoSaveManager.cpp/h** - Cosave coordination for save safety
  - **Purpose**: Manages cosave files for CrashGuard data and coordinates with other cosave systems (S.L.A.C.K. detection)
  - **Key Classes**: `CoSaveManager` - handles cosave read/write operations
  - **Key Functions**: `WriteCosave()` - writes CrashGuard cosave data; `ReadCosave()` - loads cosave data; `DetectSLACK()` - detects S.L.A.C.K. presence
  - **Dependencies**: SKSE cosave API
  - **Interdependencies**: Used by SaveLoadResilience, StateManager for persistent data

- **PhaseTracker.cpp/h** - Tracks game phase transitions
  - **Purpose**: Tracks game phase transitions (startup, loading, gameplay, menu, shutdown) for context-aware behavior
  - **Key Classes**: `PhaseTracker` - monitors game phase changes
  - **Key Enums**: Game phases (Startup, Loading, Gameplay, Menu, Shutdown)
  - **Key Functions**: `GetCurrentPhase()` - returns current phase; `OnPhaseChange()` - handles phase transitions
  - **Dependencies**: SKSE event system
  - **Interdependencies**: Used by various systems for phase-aware behavior (e.g., disable validation during loading)

### Object Introspection
- **GameObjectIntrospector.cpp/h** - Introspects game objects for crash analysis
  - **Purpose**: Introspects game objects to extract information for crash analysis (FormID, EditorID, mod name, type)
  - **Key Classes**: `GameObjectIntrospector` - extracts object information
  - **Key Structs**: `GameObjectInfo` - contains FormID, EditorID, mod name, type, validity
  - **Key Functions**: `IntrospectObject()` - extracts object information; `GetModName()` - identifies source mod; `ValidateObject()` - checks object validity
  - **Dependencies**: Uses RE::TESForm, RE::TESObjectREFR (CommonLibSSE)
  - **Interdependencies**: Used by RootCauseAnalyzer, DiagnosticLogger for crash analysis

- **CrosshairObserver.cpp/h** - Observes crosshair target for context
  - **Purpose**: Observes crosshair target to provide context for crashes (what player was looking at)
  - **Key Classes**: `CrosshairObserver` - tracks crosshair target
  - **Key Functions**: `GetCurrentTarget()` - returns current crosshair target; `TrackTarget()` - monitors target changes
  - **Dependencies**: Uses RE::CrosshairPickData (CommonLibSSE)
  - **Interdependencies**: Used by DiagnosticLogger, RootCauseAnalyzer for crash context

## Hook Systems

### Function Hooks
- **FunctionHookManager.cpp/h** - Manages all function hooks (mesh, animation, script)
  - **Purpose**: Central manager for all function hooks (mesh loading, animation, script execution)
  - **Key Classes**: `FunctionHookManager` - coordinates hook installation and management
  - **Key Functions**: `InstallHooks()` - installs all hooks; `UninstallHooks()` - removes hooks; `RegisterHook()` - registers new hook; manages mesh, animation, and script hooks
  - **Dependencies**: Uses Hooks.cpp for low-level hooking, SKSE for address resolution
  - **Interdependencies**: Installs hooks for MeshValidator, PapyrusValidator, PapyrusNativeFunctionHook

- **Hooks.cpp/h** - Core hooking infrastructure
  - **Purpose**: Low-level hooking infrastructure using SKSE trampoline
  - **Key Classes**: `Hooks` - provides hooking utilities
  - **Key Functions**: `InstallHook()` - installs function hook; `CreateTrampoline()` - creates trampoline for original function; hook management utilities
  - **Dependencies**: SKSE trampoline API
  - **Interdependencies**: Used by FunctionHookManager, PatchEngine for function hooking

- **PatchEngine.cpp/h** - Engine patching system
  - **Purpose**: Applies runtime patches to game engine code for crash prevention
  - **Key Classes**: `PatchEngine` - manages engine patches
  - **Key Functions**: `ApplyPatch()` - applies memory patch; `RevertPatch()` - removes patch; `ValidatePatch()` - verifies patch integrity
  - **Dependencies**: Uses Hooks.cpp, Windows API for memory protection
  - **Interdependencies**: Used by Patches.cpp for specific patch implementations

- **Patches.cpp/h** - Specific game patches
  - **Purpose**: Contains specific game engine patches for known crash sites
  - **Key Functions**: Individual patch implementations for known crash locations
  - **Dependencies**: Uses PatchEngine for patch application
  - **Interdependencies**: Called by FunctionHookManager during initialization

### Rendering Hooks
- **PresentHook.cpp/h** - D3D11 Present hook for overlay rendering
  - **Purpose**: Hooks D3D11 Present function for ImGui overlay rendering
  - **Key Classes**: `PresentHook` - manages D3D11 Present hook
  - **Key Functions**: `InstallHook()` - hooks Present function; `PresentCallback()` - renders ImGui; `GetSwapChain()` - retrieves D3D11 swap chain
  - **Dependencies**: D3D11 API, ImGui
  - **Interdependencies**: Used by ImGuiRenderer for overlay rendering

- **ImGuiRenderer.cpp/h** - ImGui rendering backend
  - **Purpose**: ImGui rendering backend for D3D11
  - **Key Classes**: `ImGuiRenderer` - manages ImGui rendering
  - **Key Functions**: `Initialize()` - sets up ImGui; `Render()` - renders ImGui frame; `Shutdown()` - cleanup
  - **Dependencies**: ImGui, D3D11
  - **Interdependencies**: Used by PresentHook, ImGuiConfigMenu for UI rendering

- **PerformanceOverlay.h** - Performance overlay rendering (header-only)
  - **Purpose**: Renders performance overlay (FPS, memory, crash stats) in-game
  - **Key Functions**: Overlay rendering, metric display
  - **Dependencies**: ImGui, PerformanceMetrics
  - **Interdependencies**: Used by ImGuiRenderer when overlay is enabled

## User Interface

### ImGui Menu System
- **ImGuiConfigMenu.cpp/h** - F11 configuration menu
  - **Purpose**: Main F11 configuration menu with multiple tabs (Overview, Settings, Resource Monitor, NPC Tools, Recovery, Performance, Advanced, Debug)
  - **Key Classes**: `ImGuiConfigMenu` - singleton managing menu UI
  - **Key Enums**: `Tab` - menu tabs (Overview, Settings, ResourceMonitor, NPCTools, CrashHistory, Performance, Advanced, Debug, SeverityGuide, RecoveryStats, Recovery)
  - **Key Functions**: `Render()` - renders menu UI; `Toggle()` - shows/hides menu; `OpenToTab()` - opens specific tab; `RenderOverviewTab()`/`RenderSettingsTab()`/etc. - individual tab rendering
  - **Key Features**: Real-time config editing, NPC management tools, crash history, performance overlay settings, recovery statistics, severity guide
  - **Dependencies**: ImGui, Config, PerformanceMetrics, NPCManager, RecoveryStatistics
  - **Interdependencies**: Rendered by ImGuiRenderer; controlled by HotkeyManager (F11)

- **ConfigMenu.cpp/h** - Configuration menu logic
  - **Purpose**: Configuration menu logic and state management (backend for ImGuiConfigMenu)
  - **Key Classes**: `ConfigMenu` - manages menu state and logic
  - **Key Functions**: Configuration validation, change detection, save/load logic
  - **Dependencies**: Config
  - **Interdependencies**: Used by ImGuiConfigMenu for configuration management

- **MessageBoxMenu.cpp/h** - Message box dialogs
  - **Purpose**: Displays message box dialogs for user notifications and choices
  - **Key Classes**: `MessageBoxMenu` - manages message box display
  - **Key Functions**: `ShowMessageBox()` - displays message box; `GetUserChoice()` - retrieves user selection; timeout handling
  - **Dependencies**: ImGui
  - **Interdependencies**: Used by UserNotificationManager for crash notifications

- **ToastNotificationManager.cpp/h** - Toast notifications
  - **Purpose**: Displays toast notifications for non-critical events (auto-recoveries, NPC actions)
  - **Key Classes**: `ToastNotificationManager` - manages toast display queue
  - **Key Functions**: `ShowToast()` - displays toast notification; `UpdateToasts()` - manages toast lifecycle; fade in/out animations
  - **Dependencies**: ImGui
  - **Interdependencies**: Used by UserNotificationManager, NPCManager for notifications

### Input Management
- **InputEventHandler.cpp/h** - Handles input events
  - **Purpose**: Handles input events from SKSE for menu control and hotkeys
  - **Key Classes**: `InputEventHandler` - SKSE input event sink
  - **Key Functions**: `ProcessEvent()` - handles input events; key press detection
  - **Dependencies**: SKSE input event system
  - **Interdependencies**: Notifies HotkeyManager, MenuInputManager of input events

- **InputBlocker.cpp/h** - Blocks input during menu interaction
  - **Purpose**: Blocks game input when ImGui menu is open to prevent conflicts
  - **Key Classes**: `InputBlocker` - manages input blocking
  - **Key Functions**: `BlockInput()` - blocks game input; `UnblockInput()` - restores input; selective blocking (camera, favorites, etc.)
  - **Dependencies**: SKSE input system, Config for conflict prevention settings
  - **Interdependencies**: Used by ImGuiConfigMenu to prevent input conflicts

- **InputDiagnostics.cpp/h** - Input diagnostics and debugging
  - **Purpose**: Diagnostic logging for input system debugging (F11 menu issues)
  - **Key Classes**: `InputDiagnostics` - logs input events for debugging
  - **Key Functions**: `LogInputEvent()` - logs input events; `DumpInputState()` - dumps current input state
  - **Dependencies**: SKSE input system, Config::enableInputDiagnostics
  - **Interdependencies**: Used by InputEventHandler for debugging

- **MenuInputManager.cpp/h** - Menu input coordination
  - **Purpose**: Coordinates input between game menus and ImGui menu
  - **Key Classes**: `MenuInputManager` - manages menu input state
  - **Key Functions**: `IsMenuOpen()` - checks if any menu is open; `CanOpenImGui()` - determines if ImGui can open; conflict detection
  - **Dependencies**: RE::UI (CommonLibSSE)
  - **Interdependencies**: Used by ImGuiConfigMenu, InputBlocker for menu coordination

- **MenuInputObserver.cpp/h** - Observes menu input state
  - **Purpose**: Observes menu input state for conflict prevention
  - **Key Classes**: `MenuInputObserver` - monitors menu input
  - **Key Functions**: `ObserveMenuState()` - tracks menu state changes; detects modded menus
  - **Dependencies**: RE::UI (CommonLibSSE)
  - **Interdependencies**: Used by MenuInputManager for state tracking

- **MenuInputTracker.cpp/h** - Tracks menu input patterns
  - **Purpose**: Tracks which inputs each menu uses for conflict prevention
  - **Key Classes**: `MenuInputTracker` - tracks menu input patterns
  - **Key Functions**: `TrackMenuInput()` - records menu input usage; `GetMenuInputs()` - retrieves tracked inputs
  - **Dependencies**: SKSE input system, Config::enableInputTracking
  - **Interdependencies**: Used by InputBlocker for selective input blocking

- **HotkeyManager.cpp/h** - Hotkey management (F11 toggle)
  - **Purpose**: Manages hotkeys (F11 menu toggle, debug hotkeys)
  - **Key Classes**: `HotkeyManager` - manages hotkey registration and handling
  - **Key Functions**: `RegisterHotkey()` - registers hotkey; `HandleHotkey()` - processes hotkey press; F11 toggle logic
  - **Dependencies**: SKSE input system, Config for hotkey settings
  - **Interdependencies**: Used by InputEventHandler; controls ImGuiConfigMenu visibility

- **HotkeyMenu.cpp** - Hotkey menu implementation
  - **Purpose**: Hotkey menu UI implementation (part of ImGuiConfigMenu)
  - **Key Functions**: Hotkey configuration UI, key binding
  - **Dependencies**: ImGui, HotkeyManager
  - **Interdependencies**: Rendered by ImGuiConfigMenu Debug tab

## Configuration & Initialization

### Configuration
- **Config.cpp/h** - TOML configuration loading
  - **Purpose**: Loads and manages TOML configuration from SkyrimCrashGuard.toml
  - **Key Structs**: `Settings` - contains all configuration options (General, VEH, Patches, ProactiveValidation, SafetyChecks, StateManagement, Learning, Notifications, UserNotifications, Performance, Logging, Compatibility, PapyrusValidation, InputConflictPrevention, ImGui, PerformanceOverlay, ResourceLimiter, Hotkeys, ActorLOD, Benchmark, NPC Tools, NPC Management Strategy, Burden Weights)
  - **Key Functions**: `Load()` - loads TOML configuration; `Save()` - saves configuration; `Get()` - retrieves read-only settings; `GetMutable()` - retrieves mutable settings for MCM
  - **Key Features**: Comprehensive configuration with 100+ settings, per-subsystem debug toggles, NPC management tuning, performance overlay settings
  - **Dependencies**: toml11 library for TOML parsing
  - **Interdependencies**: Used by all systems for configuration; modified by ImGuiConfigMenu

- **NotificationThresholdManager.cpp/h** - Manages notification thresholds
  - **Purpose**: Manages notification thresholds for severity-based notifications
  - **Key Classes**: `NotificationThresholdManager` - manages threshold configuration
  - **Key Functions**: `ShouldNotify()` - determines if notification should be shown based on severity and config; threshold management
  - **Dependencies**: Config, VEH::SeverityLevel
  - **Interdependencies**: Used by UserNotificationManager for notification decisions

### Initialization
- **DllMain.cpp** - DLL entry point
  - **Purpose**: DLL entry point for Windows (DllMain function)
  - **Key Functions**: `DllMain()` - DLL initialization and cleanup
  - **Dependencies**: Windows API
  - **Interdependencies**: Entry point for plugin loading

- **main.cpp** - Plugin initialization
  - **Purpose**: SKSE plugin initialization and system startup
  - **Key Functions**: `SKSEPlugin_Load()` - main plugin initialization; initializes all systems in correct order; registers SKSE listeners
  - **Key Initialization Order**: Config → DiagnosticLogger → AddressResolver → FunctionHookManager → VEH → ImGui → etc.
  - **Dependencies**: SKSE plugin API, all CrashGuard systems
  - **Interdependencies**: Initializes all systems; called by SKSE on plugin load

- **SKSEExports.cpp** - SKSE plugin exports
  - **Purpose**: SKSE plugin exports (version info, compatibility)
  - **Key Functions**: `SKSEPlugin_Query()` - provides plugin metadata; `SKSEPlugin_Version` - version structure
  - **Dependencies**: SKSE plugin API
  - **Interdependencies**: Called by SKSE during plugin discovery

- **Plugin.h** - Plugin metadata
  - **Purpose**: Plugin metadata and version information
  - **Key Constants**: Plugin name, version, author, description
  - **Dependencies**: None
  - **Interdependencies**: Used by SKSEExports, main.cpp for plugin identification

- **Version.h.in** - CMake template for version information (generates Version.h during build)
  - **Purpose**: CMake template for version information (generates Version.h during build)
  - **Key Features**: Version numbers populated by CMake from project configuration
  - **Dependencies**: CMake build system
  - **Interdependencies**: Included by Plugin.h for version information

## Address Resolution

### Multi-Runtime Support
- **AddressLib.cpp/h** - Address Library integration
  - **Purpose**: Integrates with Address Library for SE/AE/VR address resolution
  - **Key Classes**: `AddressLib` - manages Address Library integration
  - **Key Functions**: `Initialize()` - loads Address Library; `GetAddress()` - resolves address by ID; version detection
  - **Dependencies**: Address Library for SKSE (external)
  - **Interdependencies**: Used by AddressResolver for address resolution; fallback to AddressLibraryStub if not available

- **AddressLibraryStub.h** - Address Library stub for fallback
  - **Purpose**: Stub implementation when Address Library is not available (fallback to hardcoded offsets)
  - **Key Functions**: Stub address resolution functions
  - **Dependencies**: None
  - **Interdependencies**: Used by AddressLib as fallback

- **AddressResolver.cpp/h** - Resolves addresses across SE/AE/VR
  - **Purpose**: Resolves game engine addresses across SE 1.5.97, AE 1.6.x, VR 1.4.15
  - **Key Classes**: `AddressResolver` - central address resolution system
  - **Key Functions**: `ResolveAddress()` - resolves address by ID or offset; `GetGameVersion()` - detects game version; caching for performance
  - **Dependencies**: AddressLib, GameDetect, VROffsets
  - **Interdependencies**: Used by all systems that need game engine addresses (hooks, patches, validators)

- **GameDetect.cpp/h** - Detects game version (SE/AE/VR)
  - **Purpose**: Detects game version (SE 1.5.97, AE 1.6.x, VR 1.4.15) at runtime
  - **Key Classes**: `GameDetect` - detects game version
  - **Key Enums**: `GameVersion` - SE, AE, VR
  - **Key Functions**: `DetectVersion()` - detects game version from executable; `GetVersionString()` - returns version string
  - **Dependencies**: Windows API for file version info
  - **Interdependencies**: Used by AddressResolver, AddressLib for version-specific behavior

- **VROffsets.h** - VR-specific hardcoded offsets
  - **Purpose**: Hardcoded offsets for VR 1.4.15 (fallback when Address Library unavailable)
  - **Key Features**: VR-specific address offsets for critical functions
  - **Dependencies**: None (header-only)
  - **Interdependencies**: Used by AddressResolver for VR address resolution

## Utility Systems

### Threading & Synchronization
- **DeadlockDetector.cpp/h** - Detects potential deadlocks
  - **Purpose**: Detects potential deadlocks in multi-threaded operations
  - **Key Classes**: `DeadlockDetector` - monitors lock acquisition patterns
  - **Key Functions**: `RegisterLock()` - registers lock acquisition; `DetectDeadlock()` - analyzes lock patterns for deadlock potential; `ReportDeadlock()` - logs deadlock warnings
  - **Dependencies**: Windows threading API
  - **Interdependencies**: Used by systems with complex locking (PatternLearningSystem, MemoryManager)

- **DeadlockWatchdog.cpp/h** - Watchdog thread for deadlock prevention
  - **Purpose**: Watchdog thread that monitors for deadlocks and takes corrective action
  - **Key Classes**: `DeadlockWatchdog` - watchdog thread manager
  - **Key Functions**: `StartWatchdog()` - starts watchdog thread; `MonitorThread()` - monitors for deadlocks; `BreakDeadlock()` - attempts deadlock resolution
  - **Dependencies**: Windows threading API, DeadlockDetector
  - **Interdependencies**: Works with DeadlockDetector to prevent system hangs

- **LockFreeStructures.cpp/h** - Lock-free data structures
  - **Purpose**: Lock-free data structures for high-performance concurrent access
  - **Key Classes**: Lock-free queue, stack, and other concurrent data structures
  - **Key Features**: Atomic operations, wait-free algorithms for performance-critical paths
  - **Dependencies**: C++ atomics
  - **Interdependencies**: Used by VEH, CrashCollector, PatternLearningSystem for thread-safe operations

### Testing
- **RendererTest.cpp** - Renderer testing utilities
  - **Purpose**: Testing utilities for renderer and ImGui integration
  - **Key Functions**: Renderer test functions, ImGui test rendering
  - **Dependencies**: ImGui, D3D11
  - **Interdependencies**: Used for development and testing

### Batch Operations
- **BatchOperations.h** - Batch operation utilities
  - **Purpose**: Header-only utilities for batch operations (batch validation, batch processing)
  - **Key Features**: Template-based batch processing, parallel operations
  - **Dependencies**: None (header-only)
  - **Interdependencies**: Used by validators, resource managers for batch processing

## Precompiled Headers
- **PCH.cpp/h** - Precompiled header for faster compilation
  - **Purpose**: Precompiled header containing commonly used headers (Windows, STL, CommonLibSSE, SKSE)
  - **Key Includes**: Windows.h, STL headers, RE/Skyrim.h, SKSE/SKSE.h, spdlog, fmt
  - **Dependencies**: All external libraries
  - **Interdependencies**: Included by all .cpp files for faster compilation

## Third-Party Integration
- **openvr.h** - OpenVR header for VR support
  - **Purpose**: OpenVR header for VR-specific functionality
  - **Key Features**: VR API declarations for VR support
  - **Dependencies**: OpenVR SDK
  - **Interdependencies**: Used by VR-specific code paths when VR is detected

---

## File Count Summary

| Category | Count |
|----------|-------|
| Header files (.h) | 69 |
| Source files (.cpp) | 65 |
| **Total** | **134** |

**Notes:**
- Some files have both .h and .cpp, while some are header-only or source-only
- PerformanceOptimizations.h is header-only (no .cpp needed)
- Version.h.in is a CMake template that generates Version.h during build
- Backup files (.backup) are excluded from this count

---

## Architecture Overview

The codebase follows a modular architecture with clear separation of concerns:

1. **Detection Layer** - Proactive validation before crashes occur
   - MeshValidator, PapyrusValidator, FormIDValidator, ScriptMonitor
   - Validates data before it causes crashes
   - May invoke RealTimeFixApplicator for proactive fixes

2. **Recovery Layer** - VEH-based crash interception and recovery
   - VEH.cpp (main exception handler)
   - Coordinates with RootCauseAnalyzer, SeverityAnalyzer, PatternLearningSystem, DynamicFixApplicator
   - Implements 7-layer recovery chain (L1-L6)

3. **Analysis Layer** - Root cause analysis and pattern learning
   - RootCauseAnalyzer (crash categorization)
   - SeverityAnalyzer (severity classification with user explanations)
   - PatternLearningSystem (learns from crashes, improves strategies)
   - GameObjectIntrospector (extracts object information)

4. **Reporting Layer** - Comprehensive logging and user notifications
   - DiagnosticLogger (comprehensive crash reports in multiple formats)
   - UnifiedCrashReport (merges data from VEH, CrashLogger, Trainwreck)
   - CrashCollector (records crash patterns to CrashPatterns.json)
   - UserNotificationManager (severity-based user notifications)
   - RecoveryStatistics (tracks recovery success rates)

5. **Management Layer** - Resource and state management
   - MemoryManager (memory monitoring and warnings)
   - MemoryPressureDetector (4-level pressure detection)
   - NPCManager (NPC population management with smart prioritization)
   - CellManager (cell resource tracking)
   - StateManager (state validation and snapshots)
   - SaveLoadResilience (save protection)

6. **UI Layer** - ImGui-based configuration and monitoring
   - ImGuiConfigMenu (F11 menu with 10+ tabs)
   - MessageBoxMenu (crash notifications)
   - ToastNotificationManager (non-critical notifications)
   - PresentHook + ImGuiRenderer (D3D11 overlay rendering)
   - HotkeyManager (F11 toggle)
   - InputBlocker (prevents input conflicts)

7. **Hook Layer** - Function hooks and patches
   - FunctionHookManager (coordinates all hooks)
   - Hooks.cpp (low-level hooking infrastructure)
   - PatchEngine + Patches.cpp (engine patches)
   - PapyrusNativeFunctionHook (Papyrus validation hooks)

8. **Integration Layer** - External system integration
   - CrashLoggerDetector + CrashLoggerIntegration (CrashLogger cooperation)
   - TrainwreckBridge (Trainwreck integration)
   - CoSaveManager (cosave coordination, S.L.A.C.K. detection)
   - AddressResolver (SE/AE/VR address resolution)

All systems are designed to work across Skyrim SE 1.5.97, AE 1.6.x, and VR 1.4.15 without version-specific updates.

## Key Interdependencies

### Crash Recovery Flow
1. **Exception occurs** → VEH::ExceptionFilter() catches it
2. **Analysis** → RootCauseAnalyzer + SeverityAnalyzer diagnose crash
3. **Strategy selection** → PatternLearningSystem selects best recovery strategy
4. **Fix application** → DynamicFixApplicator applies fix
5. **Logging** → DiagnosticLogger creates comprehensive crash report
6. **Recording** → CrashCollector records pattern, RecoveryStatistics updates counters
7. **User notification** → UserNotificationManager shows notification (if configured)

### Proactive Validation Flow
1. **Hook triggers** → FunctionHookManager hook intercepts function call
2. **Validation** → MeshValidator/PapyrusValidator validates data
3. **Fix application** → RealTimeFixApplicator applies proactive fix (if needed)
4. **Logging** → DiagnosticLogger logs validation failure (if configured)

### Configuration Flow
1. **Startup** → Config::Load() loads SkyrimCrashGuard.toml
2. **Runtime** → ImGuiConfigMenu allows real-time editing
3. **Save** → Config::Save() writes changes back to TOML
4. **All systems** → Read Config::Get() for behavior settings

### Memory Management Flow
1. **Monitoring** → MemoryManager::UpdateMemoryStats() tracks memory
2. **Pressure detection** → MemoryPressureDetector detects pressure levels
3. **Response** → NPCManager culls NPCs, MemoryManager suggests resource freeing
4. **Warning** → UserNotificationManager warns user if critical

### Address Resolution Flow
1. **Initialization** → GameDetect determines SE/AE/VR
2. **Address Library** → AddressLib loads Address Library (if available)
3. **Resolution** → AddressResolver resolves addresses by ID or offset
4. **Fallback** → VROffsets provides hardcoded offsets for VR if needed
5. **Usage** → All hooks/patches use AddressResolver for addresses

