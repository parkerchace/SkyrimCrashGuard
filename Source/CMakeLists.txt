cmake_minimum_required(VERSION 3.21)

# ── vcpkg toolchain (must come before project()) ──
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain")
endif()

set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")

project(
    SkyrimCrashGuard
    VERSION 2.3.6
    LANGUAGES CXX
)

# Option to enable automatic deploy to MO2 mods folder (disabled by default)
option(ENABLE_MO2_DEPLOY "Automatically copy build outputs to MO2 mods folder" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ═══════════════════════════════════════════════════════════════════════
# ENGINE-LEVEL CRASH RECOVERY SYSTEM
# SKSE plugin using CommonLibSSE for game introspection and manipulation
# Six-layer defense system: L1-L6 (Proactive → Learning)
# ═══════════════════════════════════════════════════════════════════════

# ── CommonLibSSE-NG setup ──
# Use CommonLibSSE-NG from vcpkg
find_package(CommonLibSSE CONFIG REQUIRED)

# ── Dependencies (vcpkg) ──
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(zydis CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(toml11 CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)
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

# ── Build the DLL (use CommonLibSSE-NG helper for multi-runtime targets) ──
# Replace the normal add_library invocation with add_commonlibsse_plugin provided
# by CommonLibSSE-NG. This enables building a single DLL that supports SE/AE/VR at runtime.
add_commonlibsse_plugin(${PROJECT_NAME}
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
    # CommonLibSSE-NG runtime support
    ENABLE_SKYRIM_SE=1
    ENABLE_SKYRIM_AE=1
    ENABLE_SKYRIM_VR=1
    # Plugin metadata
    PLUGIN_NAME="${PROJECT_NAME}"
    PLUGIN_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    PLUGIN_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    PLUGIN_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    # Disable manual SKSE exports when using CommonLibSSE-NG
    DISABLE_MANUAL_SKSE_EXPORTS=1
)

if(MSVC)
    target_compile_options(${PROJECT_NAME}
        PRIVATE
        /W4
        /wd4100  # unreferenced parameter
        /wd4189  # local variable not referenced
        /wd4200  # zero-sized array
        /wd4201  # nameless struct/union
        /wd4324  # structure padded due to alignment specifier
        /Zc:preprocessor
        /Zi  # Generate debug information (PDB)
    )
    
    target_link_options(${PROJECT_NAME}
        PRIVATE
        /DEBUG  # Generate PDB file
        /OPT:REF  # Eliminate unreferenced functions/data
        /OPT:ICF  # Enable COMDAT folding
    )
    
    # Dynamic CRT linking — matches vcpkg x64-windows-static-md
    # Use /MD for Release, /MDd for Debug
    set_property(TARGET ${PROJECT_NAME} PROPERTY 
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

if(MSVC)
    # Force-include a header that provides the "sv" string literal so generated
    # CommonLibSSE-NG wrapper files compile without needing manual edits.
    target_compile_options(${PROJECT_NAME} PRIVATE
        /FI"${CMAKE_CURRENT_SOURCE_DIR}/cmake/force_string_view_literals.h"
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

message(STATUS "Skyrim Crash Guard v${PROJECT_VERSION} [Engine-Level Recovery — SE/AE]")
message(STATUS "  CommonLibSSE: ${CommonLibSSENG_DIR}")
message(STATUS "  Output: ../SKSE/Plugins/")

# ═══════════════════════════════════════════════════════════════════════
# Testing Configuration
# ═══════════════════════════════════════════════════════════════════════

option(BUILD_TESTS "Build property-based and unit tests" OFF)
option(ENABLE_RAPIDCHECK "Enable RapidCheck property-based tests" ON)  # Enabled with C++20

if(BUILD_TESTS)
    enable_testing()
    
    # Find test dependencies
    find_package(Catch2 3 CONFIG REQUIRED)
    
    # RapidCheck for property-based testing (disabled due to C++20/23 incompatibility)
    if(ENABLE_RAPIDCHECK)
        find_package(rapidcheck CONFIG REQUIRED)
        message(STATUS "RapidCheck enabled for property-based testing")
    else()
        message(STATUS "RapidCheck disabled - property-based tests will be skipped")
    endif()
    
    # Create a separate library for SafetyEvaluation files (no PCH)
    # TEMPORARILY DISABLED - has compilation errors
    # add_library(SafetyEvaluationLib STATIC
    #     src/SafetyEvaluation/ComponentDiscovery.cpp
    #     src/SafetyEvaluation/FunctionalityTester.cpp
    #     src/SafetyEvaluation/StabilityTester.cpp
    #     src/SafetyEvaluation/MemorySafetyTester.cpp
    #     src/SafetyEvaluation/PerformanceTester.cpp
    #     src/SafetyEvaluation/ReportGenerator.cpp
    #     src/SafetyEvaluation/SaveCorruptionAnalyzer.cpp
    #     src/SafetyEvaluation/SaveIntegrityTester.cpp
    #     src/SafetyEvaluation/RollbackTester.cpp
    #     src/SafetyEvaluation/VEHTester.cpp
    #     src/SafetyEvaluation/IntegrationTester.cpp
    #     src/SafetyEvaluation/DynamicFixTester.cpp
    #     src/SafetyEvaluation/PatternLearningTester.cpp
    #     src/SafetyEvaluation/CompatibilityTester.cpp
    #     src/SafetyEvaluation/TestSuiteGenerator.cpp
    #     src/SafetyEvaluation/RegressionDetector.cpp
    #     src/SafetyEvaluation/SafetyEvaluationSystem.cpp
    # )
    # 
    # target_include_directories(SafetyEvaluationLib
    #     PUBLIC
    #     "${CMAKE_CURRENT_SOURCE_DIR}/include"
    # )
    # 
    # target_link_libraries(SafetyEvaluationLib
    #     PUBLIC
    #     CommonLibSSE::CommonLibSSE
    #     spdlog::spdlog
    #     fmt::fmt
    #     nlohmann_json::nlohmann_json
    # )
    # 
    # set_target_properties(SafetyEvaluationLib PROPERTIES
    #     CXX_STANDARD 20
    #     CXX_STANDARD_REQUIRED ON
    #     SKIP_PRECOMPILE_HEADERS ON
    #     DISABLE_PRECOMPILE_HEADERS ON
    # )
    # 
    # # Explicitly disable PCH for all source files in SafetyEvaluationLib
    # target_compile_options(SafetyEvaluationLib PRIVATE /Y-)
    
    # Test executable - Layer 1 Tests
    set(TEST_SOURCES
        # Source files needed by tests (minimal set to avoid game engine dependencies)
        src/Config.cpp
        src/DiagnosticLogger.cpp
        src/SeverityAnalyzer.cpp
        src/NotificationThresholdManager.cpp
        # src/AllocationTracker.cpp  # Excluded - requires SKSE APIs not available in test environment
        # src/MemoryAllocatorHooks.cpp  # Excluded - requires SKSE APIs not available in test environment
        # src/VEH.cpp  # Excluded - complex dependencies
        # src/GameDetect.cpp  # Excluded - game engine dependencies
        # src/GameObjectIntrospector.cpp  # Excluded - game engine dependencies
        # src/RootCauseAnalyzer.cpp  # Excluded - game engine dependencies
        # src/MeshValidator.cpp  # Excluded - game engine dependencies
        # src/AnimationHandler.cpp  # Excluded - game engine dependencies
        # src/CellManager.cpp  # Excluded - too many game engine dependencies
        # src/ScriptMonitor.cpp  # Excluded - game engine dependencies
        # src/FunctionHookManager.cpp  # Excluded - game engine dependencies
        # src/FormIDValidator.cpp  # Excluded - game engine dependencies
        # src/DynamicFixApplicator.cpp  # Excluded - game engine dependencies
        # src/StateManager.cpp  # Excluded - game engine dependencies
        # src/PatternLearningSystem.cpp  # Excluded - game engine dependencies
        # src/CoSaveManager.cpp  # Excluded - game engine dependencies
        # src/UserNotificationManager.cpp  # Excluded - game engine dependencies
        # src/ResourceValidator.cpp  # Excluded - game engine dependencies
        # src/MemoryManager.cpp  # Excluded - uses SKSE logging not available in tests
        # src/SaveLoadResilience.cpp  # Excluded - game engine dependencies
        # src/RealTimeFixApplicator.cpp  # Excluded - game engine dependencies
    )
    
    # Add property-based tests only if RapidCheck is enabled
    if(ENABLE_RAPIDCHECK)
        list(APPEND TEST_SOURCES
            # Property-based tests temporarily disabled due to:
            # 1. RapidCheck C++20/MSVC template compatibility issues
            # 2. Class redefinition conflicts in test files
            # 3. Missing method implementations in mock classes
            # TODO: Refactor property tests to use proper mocking framework
            # tests/VEHPropertyTests.cpp
            # tests/MeshRepairPropertyTests.cpp
            # tests/NullPointerSafetyPropertyTest.cpp
            # tests/AnimationErrorRecoveryPropertyTest.cpp
            # tests/ScriptNullReferencePropertyTest.cpp
            # tests/FormIDValidationPropertyTest.cpp
            # tests/StateRollbackPropertyTest.cpp
            # tests/DanglingPointerPropertyTest.cpp
            # tests/MemoryLeakPropertyTest.cpp
            # tests/PatternLearningPropertyTests.cpp
        )
    endif()
    
    # Add unit tests (always included)
    # Note: All allocation hooks tests have been removed along with the system
    
    add_executable(CrashGuardTests ${TEST_SOURCES})
    
    target_include_directories(CrashGuardTests
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"  # For openvr_stub.h
    )
    
    # Ensure C++20 standard for tests (RapidCheck has C++23 compatibility issues)
    set_target_properties(CrashGuardTests PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    
    target_link_libraries(CrashGuardTests
        PRIVATE
        # SafetyEvaluationLib  # TEMPORARILY DISABLED
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
    
    # Link RapidCheck only if enabled
    if(ENABLE_RAPIDCHECK)
        target_link_libraries(CrashGuardTests PRIVATE rapidcheck)
    endif()

    target_compile_definitions(CrashGuardTests
        PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        ENABLE_SKYRIM_VR=0
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
            # Disable conforming preprocessor for RapidCheck compatibility
            /Zc:preprocessor-
            # Enable parallel PDB writes to avoid C1041 errors
            /FS
        )
        set_property(TARGET CrashGuardTests PROPERTY 
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
    
    # Register tests with CTest
    include(Catch)
    catch_discover_tests(CrashGuardTests)
    
    message(STATUS "Tests enabled: CrashGuardTests")
    
    # ═══════════════════════════════════════════════════════════════════════
    # Safety Evaluation CLI Tool
    # ═══════════════════════════════════════════════════════════════════════
    
    # TEMPORARILY DISABLED - SafetyEvaluationLib has compilation errors
    # add_executable(SafetyEvaluationTool
    #     src/SafetyEvaluation/main.cpp
    # )
    # 
    # target_include_directories(SafetyEvaluationTool
    #     PRIVATE
    #     "${CMAKE_CURRENT_SOURCE_DIR}/include"
    # )
    # 
    # target_link_libraries(SafetyEvaluationTool
    #     PRIVATE
    #     SafetyEvaluationLib
    #     CommonLibSSE::CommonLibSSE
    #     spdlog::spdlog
    #     fmt::fmt
    #     nlohmann_json::nlohmann_json
    # )
    # 
    # target_compile_definitions(SafetyEvaluationTool
    #     PRIVATE
    #     UNICODE
    #     _UNICODE
    #     WIN32_LEAN_AND_MEAN
    #     NOMINMAX
    # )
    
    # if(MSVC)
    #     target_compile_options(SafetyEvaluationTool
    #         PRIVATE
    #         /W4
    #         /wd4100
    #         /wd4189
    #         /wd4200
    #         /wd4201
    #         /wd4324
    #     )
    #     set_property(TARGET SafetyEvaluationTool PROPERTY 
    #         MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    # endif()
    # 
    # message(STATUS "Safety Evaluation Tool enabled: SafetyEvaluationTool")
endif()
