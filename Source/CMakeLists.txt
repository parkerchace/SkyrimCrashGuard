# Copyright (C) 2024-2026 Parker Chace
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of Skyrim Crash Guard.
# Skyrim Crash Guard is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option) any
# later version. See the LICENSE file in the project root for details.

cmake_minimum_required(VERSION 3.21)

# ── vcpkg toolchain (must come before project()) ──
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain")
endif()

set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")

project(
    SkyrimCrashGuard
    VERSION 2.4.0
    LANGUAGES CXX
)

# Option to enable automatic deploy to MO2 mods folder (disabled by default)
option(ENABLE_MO2_DEPLOY "Automatically copy build outputs to MO2 mods folder" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Applies to CommonLibSSE as well: it is compiled in-tree, so both halves of the
# link must agree on the CRT. Matches vcpkg's x64-windows-static-md triplet.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# ═══════════════════════════════════════════════════════════════════════
# ENGINE-LEVEL CRASH RECOVERY SYSTEM
# SKSE plugin using CommonLibSSE for game introspection and manipulation
# Six-layer defense system: L1-L6 (Proactive → Learning)
# ═══════════════════════════════════════════════════════════════════════

# ── CommonLibSSE NG (maintained fork) ──
# CharmedBaryon/CommonLibSSE-NG has been unmaintained since Sept 2024 and has no
# support for Skyrim 1.7.x. alandtse/CommonLibSSE-NG (branch `ng`) is the maintained
# continuation; it is GPL-3.0-or-later, which is why this plugin is GPL-3.0-or-later
# too (static linking makes the DLL a combined work).
#
# Runtime targets: AE (1.6.x/1.7.x) + VR (1.4.15). SE (1.5.97) is deliberately off,
# which makes CommonLibSSE define SKYRIM_CROSS_VR: flat-vs-VR struct differences are
# resolved at runtime through GetRuntimeData()/GetVRRuntimeData() and REL::VariantID.
set(ENABLE_SKYRIM_SE OFF CACHE BOOL "Skyrim SE 1.5.97 support" FORCE)
set(ENABLE_SKYRIM_AE ON  CACHE BOOL "Skyrim AE 1.6.x/1.7.x support" FORCE)
set(ENABLE_SKYRIM_VR ON  CACHE BOOL "Skyrim VR 1.4.15 support" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "CommonLibSSE unit tests" FORCE)

include(FetchContent)
FetchContent_Declare(
    commonlibsse
    GIT_REPOSITORY https://github.com/alandtse/CommonLibSSE-NG.git
    GIT_TAG v9.0.0
    GIT_SHALLOW TRUE
    # Submodules off, then the VR headers and openvr_api.lib are fetched by the
    # patch step below: a plain submodule update clones all of ValveSoftware/openvr
    # (~1 GB of history) for two directories we actually need.
    GIT_SUBMODULES ""
    PATCH_COMMAND ${CMAKE_COMMAND}
        -D "COMMONLIB_SOURCE_DIR=<SOURCE_DIR>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchOpenVR.cmake"
)
FetchContent_MakeAvailable(commonlibsse)

# add_commonlibsse_plugin() lives in a helper the root CMakeLists only auto-includes
# for prebuilt bundles, so pull it in explicitly for the source build.
include("${commonlibsse_SOURCE_DIR}/cmake/CommonLibSSE.cmake")

# ── Dependencies (vcpkg) ──
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(zydis CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(toml11 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

# ── Plugin sources ──
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
    @ONLY
)

# Generate Version.h from template
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/Version.h"
    @ONLY
)

set(PLUGIN_SOURCES
    src/PCH.cpp
    src/main.cpp
    src/GameDetect.cpp
    src/VEH.cpp
    src/Hooks.cpp
    src/PatchEngine.cpp
    src/Patches.cpp
    src/CrashCollector.cpp
    src/Config.cpp
    src/TrainwreckBridge.cpp
    src/CrashLoggerDetector.cpp
    src/CrashLoggerIntegration.cpp
    src/UnifiedCrashReport.cpp
    src/PostMortemCoordination.cpp
    src/GameObjectIntrospector.cpp
    src/RootCauseAnalyzer.cpp
    src/MeshValidator.cpp
    src/CellManager.cpp
    src/FunctionHookManager.cpp
    src/ScriptMonitor.cpp
    src/FormIDValidator.cpp
    src/DiagnosticLogger.cpp
    src/DynamicFixApplicator.cpp
    src/PhaseTracker.cpp
    src/StateManager.cpp
    src/PatternLearningSystem.cpp
    src/CoSaveManager.cpp
    src/UserNotificationManager.cpp
    src/MemoryManager.cpp
    src/PapyrusNativeFunctions.cpp
    src/PapyrusValidator.cpp
    src/PapyrusNativeFunctionHook.cpp
    src/ImGuiConfigMenu.cpp
    src/ImGuiRenderer.cpp
    src/PresentHook.cpp
    src/InputBlocker.cpp
    src/MenuInputManager.cpp
    src/MenuInputObserver.cpp
    src/MenuInputTracker.cpp
    src/InputDiagnostics.cpp
    src/SaveLoadResilience.cpp
    src/RealTimeFixApplicator.cpp
    src/HotkeyManager.cpp
    src/PerformanceMetrics.cpp
    src/BenchmarkManager.cpp
    src/AddressResolver.cpp
    src/DeadlockDetector.cpp
    src/LockFreeStructures.cpp
    src/RecoveryNotifications.cpp
    src/RecoveryStatistics.cpp
    src/CrashTestSuite.cpp
    src/MemoryPressureDetector.cpp
    src/CrosshairObserver.cpp
    src/SeverityAnalyzer.cpp
    src/NotificationThresholdManager.cpp
    src/ToastNotificationManager.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
)

set(PLUGIN_HEADERS
    include/PCH.h
    include/Plugin.h
    include/GameDetect.h
    include/VEH.h
    include/Hooks.h
    include/PatchEngine.h
    include/Patches.h
    include/CrashCollector.h
    include/Config.h
    include/TrainwreckBridge.h
    include/CrashLoggerDetector.h
    include/CrashLoggerIntegration.h
    include/UnifiedCrashReport.h
    include/PostMortemCoordination.h
    include/GameObjectIntrospector.h
    include/RootCauseAnalyzer.h
    include/MeshValidator.h
    include/CellManager.h
    include/FunctionHookManager.h
    include/ScriptMonitor.h
    include/FormIDValidator.h
    include/DiagnosticLogger.h
    include/DynamicFixApplicator.h
    include/PhaseTracker.h
    include/StateManager.h
    include/CoSaveManager.h
    include/UserNotificationManager.h
    include/MemoryManager.h
    include/PapyrusNativeFunctions.h
    include/PapyrusValidator.h
    include/PapyrusNativeFunctionHook.h
    include/ImGuiConfigMenu.h
    include/ImGuiRenderer.h
    include/PresentHook.h
    include/InputBlocker.h
    include/MenuInputManager.h
    include/InputDiagnostics.h
    include/SaveLoadResilience.h
    include/RealTimeFixApplicator.h
    include/HotkeyManager.h
    include/PerformanceMetrics.h
    include/BenchmarkManager.h
    include/AddressResolver.h
    include/PerformanceOptimizations.h
    include/BatchOperations.h
    include/DeadlockDetector.h
    include/LockFreeStructures.h
    include/RecoveryNotifications.h
    include/RecoveryStatistics.h
    include/LayerTrace.h
    include/CrashTestSuite.h
    include/MemoryPressureDetector.h
    include/SeverityAnalyzer.h
    include/NotificationThresholdManager.h
    include/ToastNotificationManager.h
)

# ── Build the DLL ──
# add_commonlibsse_plugin generates the SKSEPluginInfo declaration, which emits both
# the modern SKSEPlugin_Version data export and the legacy SKSEPlugin_Query entry
# point that SKSEVR uses — a single DLL serves SKSE 2.3.1 (AE) and SKSEVR 2.0.12.
add_commonlibsse_plugin(${PROJECT_NAME}
    NAME "SkyrimCrashGuard"
    AUTHOR "Parker Chace"
    VERSION ${PROJECT_VERSION}
    MINIMUM_SKSE_VERSION 2.3.1
    USE_ADDRESS_LIBRARY
    SOURCES
    ${PLUGIN_SOURCES}
    ${PLUGIN_HEADERS}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/include"  # For generated Version.h
)

# Enable precompiled headers
target_precompile_headers(${PROJECT_NAME}
    PRIVATE
    include/PCH.h
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    CommonLibSSE::CommonLibSSE
    spdlog::spdlog
    fmt::fmt
    Zydis::Zydis
    nlohmann_json::nlohmann_json
    toml11::toml11
    imgui::imgui
    d3d11.lib
    dxgi.lib
    dbghelp.lib
    psapi.lib
    shell32.lib
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    # ENABLE_SKYRIM_AE / ENABLE_SKYRIM_VR (and the derived SKYRIM_CROSS_VR) arrive
    # PUBLIC from CommonLibSSE::CommonLibSSE — never redefine them here, or the
    # plugin and the library disagree about struct layout.
    # Plugin metadata
    PLUGIN_NAME="${PROJECT_NAME}"
    PLUGIN_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    PLUGIN_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    PLUGIN_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

if(MSVC)
    # add_commonlibsse_plugin generates __<target>Plugin.cpp, which writes the plugin
    # name and author as "..."sv string_view literals but includes only REL/Relocation.h
    # and SKSE/SKSE.h. Force-include a header that opens std::string_view_literals so
    # that generated file compiles without being edited.
    target_compile_options(${PROJECT_NAME} PRIVATE
        /FI"${CMAKE_CURRENT_SOURCE_DIR}/cmake/force_string_view_literals.h")

    target_compile_options(${PROJECT_NAME}
        PRIVATE
        /W4
        /wd4100  # unreferenced parameter
        /wd4189  # local variable not referenced
        /wd4200  # zero-sized array
        /wd4201  # nameless struct/union
        /wd4324  # structure padded due to alignment specifier
        /Zc:preprocessor
        /utf-8   # sources are UTF-8; fmt 12's Unicode support static_asserts on this
        /Zi  # Generate debug information (PDB)
    )

    target_link_options(${PROJECT_NAME}
        PRIVATE
        /DEBUG  # Generate PDB file
        /OPT:REF  # Eliminate unreferenced functions/data
        /OPT:ICF  # Enable COMDAT folding
    )
endif()

# Output directly into SKSE/Plugins for MO2
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "SkyrimCrashGuard"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../SKSE/Plugins"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../SKSE/Plugins"
    PDB_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../SKSE/Plugins"
)

# Also copy TOML config on build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/SkyrimCrashGuard.toml"
        "${CMAKE_CURRENT_SOURCE_DIR}/../SKSE/Plugins/SkyrimCrashGuard.toml"
    COMMENT "Copying TOML config to SKSE/Plugins"
)

# Copy to MO2 mods folder for testing (optional). Only attempt when the MO2
# mods folder root exists to avoid failing the build on machines without MO2.
set(MO2_MOD_PATH "$ENV{MO2_MOD_PATH}" CACHE PATH "Optional MO2 mods path (overrides default)")
if(NOT MO2_MOD_PATH)
    set(MO2_MOD_PATH "C:/Modding/MO2/mods/Skyrim Crash Guard/SKSE/Plugins")
endif()

# Only attempt MO2 deploy when the MO2 mods root exists. Make copies non-fatal
# so builds do not fail if the target is unavailable or locked at copy time.
if(ENABLE_MO2_DEPLOY AND EXISTS "${MO2_MOD_PATH}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>"
            "${MO2_MOD_PATH}/SkyrimCrashGuard.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/SkyrimCrashGuard.toml"
            "${MO2_MOD_PATH}/SkyrimCrashGuard.toml"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
            "${MO2_MOD_PATH}/SkyrimCrashGuard.pdb"
        COMMENT "Copying to MO2 mods folder (optional)"
        VERBATIM
    )
else()
    message(STATUS "Skipping MO2 deploy copy (target MO2 path not present: ${MO2_MOD_PATH})")
endif()

# Also copy build outputs to the local Skyrim Special Edition game folder (optional - may not exist)
set(LOCAL_SE_PLUGINS "H:/SteamLibrary/steamapps/common/Skyrim Special Edition/Data/SKSE/Plugins")
if(EXISTS "H:/SteamLibrary/steamapps/common/Skyrim Special Edition/Data/SKSE")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${LOCAL_SE_PLUGINS}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>"
            "${LOCAL_SE_PLUGINS}/SkyrimCrashGuard.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/SkyrimCrashGuard.toml"
            "${LOCAL_SE_PLUGINS}/SkyrimCrashGuard.toml"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
            "${LOCAL_SE_PLUGINS}/SkyrimCrashGuard.pdb" || (exit 0)
        COMMENT "Copying to local Skyrim SE Data/SKSE/Plugins (optional)"
        VERBATIM
    )
else()
    message(STATUS "Skipping local SE deploy copy (H: Skyrim folder not found)")
endif()

# Optional deploy to a local Skyrim VR install (same DLL serves AE and VR)
set(LOCAL_VR_PLUGINS "$ENV{SKYRIMVR_PATH}" CACHE PATH "Optional Skyrim VR install root for deploy")
if(NOT LOCAL_VR_PLUGINS)
    set(LOCAL_VR_PLUGINS "C:/Program Files (x86)/Steam/steamapps/common/SkyrimVR")
endif()
if(EXISTS "${LOCAL_VR_PLUGINS}/SkyrimVR.exe")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${LOCAL_VR_PLUGINS}/Data/SKSE/Plugins"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>"
            "${LOCAL_VR_PLUGINS}/Data/SKSE/Plugins/SkyrimCrashGuard.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/SkyrimCrashGuard.toml"
            "${LOCAL_VR_PLUGINS}/Data/SKSE/Plugins/SkyrimCrashGuard.toml"
        COMMENT "Copying to local Skyrim VR Data/SKSE/Plugins (optional)"
        VERBATIM
    )
else()
    message(STATUS "Skipping Skyrim VR deploy copy (not found: ${LOCAL_VR_PLUGINS})")
endif()

# Auto-deploy to OneDrive Skyrim SE SKSE/Plugins if present (common for MO2+OneDrive setups)
set(ONEDRIVE_SKSE_PLUGINS "$ENV{USERPROFILE}/OneDrive/Documents/My Games/Skyrim Special Edition/SKSE/Plugins" CACHE PATH "Optional OneDrive SKSE Plugins path")
if(EXISTS "${ONEDRIVE_SKSE_PLUGINS}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>"
            "${ONEDRIVE_SKSE_PLUGINS}/SkyrimCrashGuard.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/SkyrimCrashGuard.toml"
            "${ONEDRIVE_SKSE_PLUGINS}/SkyrimCrashGuard.toml"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
            "${ONEDRIVE_SKSE_PLUGINS}/SkyrimCrashGuard.pdb"
        COMMENT "Copying build outputs to OneDrive SKSE/Plugins"
        VERBATIM
    )
else()
    message(STATUS "Skipping OneDrive SKSE deploy (target not present: ${ONEDRIVE_SKSE_PLUGINS})")
endif()

message(STATUS "Skyrim Crash Guard v${PROJECT_VERSION} [Engine-Level Recovery — AE + VR]")
message(STATUS "  CommonLibSSE NG: ${commonlibsse_SOURCE_DIR}")
message(STATUS "  Output: ../SKSE/Plugins/")

# ═══════════════════════════════════════════════════════════════════════
# Testing Configuration
# ═══════════════════════════════════════════════════════════════════════

# Named CRASHGUARD_BUILD_TESTS, not BUILD_TESTS: CommonLibSSE is built in-tree and
# owns the cache variable BUILD_TESTS for its own Catch2 suite.
option(CRASHGUARD_BUILD_TESTS "Build Crash Guard unit tests" OFF)
option(ENABLE_RAPIDCHECK "Enable RapidCheck property-based tests" ON)

if(CRASHGUARD_BUILD_TESTS)
    enable_testing()

    # Find test dependencies
    find_package(Catch2 3 CONFIG REQUIRED)

    if(ENABLE_RAPIDCHECK)
        find_package(rapidcheck CONFIG REQUIRED)
        message(STATUS "RapidCheck enabled for property-based testing")
    else()
        message(STATUS "RapidCheck disabled - property-based tests will be skipped")
    endif()

    # Test executable
    # The property-based and unit test sources were removed from the tree; this
    # target currently compiles the engine-independent sources only.
    set(TEST_SOURCES
        src/Config.cpp
        src/DiagnosticLogger.cpp
        src/SeverityAnalyzer.cpp
        src/NotificationThresholdManager.cpp
    )

    add_executable(CrashGuardTests ${TEST_SOURCES})

    target_include_directories(CrashGuardTests
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )

    target_link_libraries(CrashGuardTests
        PRIVATE
        Catch2::Catch2WithMain
        CommonLibSSE::CommonLibSSE
        spdlog::spdlog
        fmt::fmt
        Zydis::Zydis
        nlohmann_json::nlohmann_json
        toml11::toml11
        dbghelp.lib
        psapi.lib
    )

    if(ENABLE_RAPIDCHECK)
        target_link_libraries(CrashGuardTests PRIVATE rapidcheck)
    endif()

    target_compile_definitions(CrashGuardTests
        PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        BUILD_TESTS=1
    )

    if(MSVC)
        target_compile_options(CrashGuardTests
            PRIVATE
            /W4
            /wd4100
            /wd4189
            /wd4200
            /wd4201
            /wd4324
            /Zc:preprocessor
            /FS
        )
    endif()

    # Register tests with CTest
    include(Catch)
    catch_discover_tests(CrashGuardTests)

    message(STATUS "Tests enabled: CrashGuardTests")
endif()
