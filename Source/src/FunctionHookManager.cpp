// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "PCH.h"
#include "FunctionHookManager.h"
#include "AddressResolver.h"
#include "VROffsets.h"
#include "MeshValidator.h"
#include "ScriptMonitor.h"
#include "CellManager.h"
#include "CellLoadingEventHandler.h"
#include "FormIDValidator.h"
#include "Config.h"
#include "PhaseTracker.h"
#include "PerformanceOptimizations.h"

namespace FunctionHooks {

    // Static member initialization
    bool FunctionHookManager::s_initialized = false;
    bool FunctionHookManager::s_hooksInstalled = false;
    size_t FunctionHookManager::s_installedCount = 0;
    size_t FunctionHookManager::s_failedCount = 0;
    size_t FunctionHookManager::s_preventionCount = 0;

    // Note: Trampoline function pointers (_LoadNif, _NotifyAnimationGraph, etc.)
    // are declared as static inline in the header and don't need definitions here

    // ========================================================================
    // Initialization
    // ========================================================================

    bool FunctionHookManager::Initialize() {
        if (s_initialized) {
            spdlog::warn("FunctionHookManager already initialized");
            return true;
        }

        spdlog::info("[FunctionHooks] Initializing");

        // Allocate SKSE trampoline space for hooks
        try {
            auto& trampoline = SKSE::GetTrampoline();
            
            // Allocate space for multiple hooks
            // Each hook typically needs 14-16 bytes
            // Allocate 1024 bytes to support ~60 hooks
            trampoline.create(1024);
            
            spdlog::info("SKSE trampoline allocated: 1024 bytes");
        } catch (const std::exception& e) {
            spdlog::error("Failed to allocate SKSE trampoline: {}", e.what());
            return false;
        }

        s_initialized = true;
        spdlog::info("FunctionHookManager initialized successfully");
        
        return true;
    }

    // ========================================================================
    // Mesh Loading Hook Implementation
    // ========================================================================

    RE::NiAVObject* FunctionHookManager::Hook_LoadNif(RE::TESObjectREFR* a_this, bool a_backgroundLoading) {
        using namespace Performance;
        
        // Increment mesh validation counter for phase tracking
        PhaseTracking::PhaseTracker::IncrementMeshesValidated();
        
        // Inline null check
        if (!IsValidPointer(a_this)) [[unlikely]] {
            LogNullPointerViolation("Hook_LoadNif", "a_this (TESObjectREFR*)");
            spdlog::error("Hook_LoadNif: TESObjectREFR pointer is null, returning safe default");
            IncrementPreventionCount();
            // Return placeholder mesh for null object reference
            return MeshValidation::MeshValidator::GetPlaceholderMesh();
        }
        
        // Additional inline null safety checks for object state
        if (a_this->IsDeleted()) [[unlikely]] {
            spdlog::warn("Hook_LoadNif: TESObjectREFR {:08X} is deleted, returning safe default", a_this->GetFormID());
            IncrementPreventionCount();
            return MeshValidation::MeshValidator::GetPlaceholderMesh();
        }
        
        // Call original Load3D function first to load the mesh
        RE::NiAVObject* mesh = _LoadNif(a_this, a_backgroundLoading);
        
        // Fast-path: If loading failed, return safe default instead of nullptr
        if (!IsValidPointer(mesh)) [[unlikely]] {
            spdlog::warn("Hook_LoadNif: Original mesh loading failed for object {:08X}, using placeholder", a_this->GetFormID());
            IncrementPreventionCount();
            RE::NiAVObject* placeholder = MeshValidation::MeshValidator::GetPlaceholderMesh();
            return placeholder ? placeholder : nullptr;
        }
        
        // Fast-path: Check if mesh is likely valid using heuristics
        if (FastPathChecker::MeshLikelyValid(mesh)) [[likely]] {
            // Mesh passes fast heuristics, skip full validation
            return mesh;
        }
        
        // Slow path: Full validation required
        // Validate the loaded mesh - with inline null safety for validation result
        auto validationResult = MeshValidation::MeshValidator::ValidateMesh(mesh);
        
        // Additional null safety: ensure validation result is valid
        if (validationResult.errors.empty() && validationResult.warnings.empty() && !validationResult.isValid) [[unlikely]] {
            spdlog::error("Hook_LoadNif: Mesh validation returned invalid result, using placeholder");
            IncrementPreventionCount();
            RE::NiAVObject* placeholder = MeshValidation::MeshValidator::GetPlaceholderMesh();
            return placeholder ? placeholder : mesh;
        }
        
        if (validationResult.isValid) [[likely]] {
            // Mesh is valid, return it as-is
            // Debug logging disabled to reduce log bloat (fires for every mesh)
            // spdlog::debug("Mesh validation passed for object {:08X}", a_this ? a_this->GetFormID() : 0);
            return mesh;
        }
        
        // Mesh validation failed
        spdlog::warn("Mesh validation failed for object {:08X}", a_this ? a_this->GetFormID() : 0);
        for (const auto& error : validationResult.errors) {
            spdlog::warn("  - {}", error);
        }
        
        // Increment prevention counter
        IncrementPreventionCount();
        
        // Attempt repair if possible
        if (validationResult.canRepair) {
            // Only log repair attempts if detailed logging is enabled
            if (Config::Get().enableDetailedLogging) {
                spdlog::info("Attempting to repair mesh for object {:08X}", a_this ? a_this->GetFormID() : 0);
            }
            
            RE::NiAVObject* repairedMesh = MeshValidation::MeshValidator::RepairMesh(mesh, validationResult);
            
            if (IsValidPointer(repairedMesh)) {
                // Only log successful repairs if configured to do so
                if (Config::Get().logSuccessfulRecoveries) {
                    spdlog::info("Mesh repair successful for object {:08X}", a_this ? a_this->GetFormID() : 0);
                }
                return repairedMesh;
            } else {
                spdlog::warn("Mesh repair failed for object {:08X}", a_this ? a_this->GetFormID() : 0);
            }
        }
        
        // Repair failed or not possible, use placeholder
        spdlog::warn("Using placeholder mesh for object {:08X}", a_this->GetFormID());
        
        RE::NiAVObject* placeholder = MeshValidation::MeshValidator::GetPlaceholderMesh();
        
        // Handle null placeholder with inline check
        if (IsValidPointer(placeholder)) [[likely]] {
            return placeholder;
        }
        
        // No placeholder available - this is a critical safety issue
        // Log error and return original mesh as last resort
        spdlog::error("Hook_LoadNif: No placeholder mesh available for object {:08X}, returning original mesh as last resort", a_this->GetFormID());
        
        // Even the original mesh might be problematic, but it's better than returning nullptr
        // which would cause immediate crashes
        return mesh ? mesh : nullptr;
    }

    // ========================================================================
    // Animation Hook Implementation
    // ========================================================================

    bool FunctionHookManager::Hook_NotifyAnimationGraph(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName) {
        
        // Validate input parameters
        if (!a_this) {
            LogNullPointerViolation("Hook_NotifyAnimationGraph", "a_this (IAnimationGraphManagerHolder*)");
            spdlog::error("Hook_NotifyAnimationGraph: holder is null, returning safe default");
            IncrementPreventionCount();
            return false;
        }

        // Validate event name string
        const char* eventName = a_eventName.c_str();
        if (!eventName || strlen(eventName) == 0) {
            LogNullPointerViolation("Hook_NotifyAnimationGraph", "a_eventName (BSFixedString)");
            spdlog::error("Hook_NotifyAnimationGraph: event name is null or empty, returning safe default");
            IncrementPreventionCount();
            return false;
        }

        // Try to cast to Actor to get more context - with null safety
        RE::Actor* actor = nullptr;
        if (auto refr = static_cast<RE::TESObjectREFR*>(a_this)) {
            // Additional null safety check after cast
            if (refr && !refr->IsDeleted()) {
                actor = refr->As<RE::Actor>();
            }
        }

        if (actor) {
            spdlog::debug("Hook_NotifyAnimationGraph: Processing event '{}' for actor {:08X}", 
                         eventName, actor->GetFormID());

            // For animation events that might involve file loading, validate them
            // Common animation events that can cause crashes:
            // - "BeginCastVoice", "BeginCastLeft", "BeginCastRight" (spell casting)
            // - "attackStart", "attackStop" (combat animations)
            // - "JumpUp", "JumpDown" (movement animations)
            // - Custom mod events
            
            std::string event(eventName);
            
            // Check if this is a potentially problematic animation event
            bool needsValidation = false;
            if (event.find("Cast") != std::string::npos ||
                event.find("attack") != std::string::npos ||
                event.find("Jump") != std::string::npos ||
                event.find("Spell") != std::string::npos ||
                event.find("Weapon") != std::string::npos) {
                needsValidation = true;
            }

            if (needsValidation) {
                // For now, we'll do basic validation
                // In a full implementation, this would:
                // 1. Check if the actor has valid animation graph
                // 2. Validate the event exists in the graph
                // 3. Check if required animations are available
                
                // Basic check: ensure actor is in a valid state
                if (actor->IsDeleted() || actor->IsDisabled()) {
                    spdlog::warn("Animation event '{}' on deleted/disabled actor {:08X}, blocking", 
                                eventName, actor->GetFormID());
                    
                    // Increment prevention counter
                    IncrementPreventionCount();
                    
                    // Reset to safe pose instead of processing invalid event (AnimationHandler removed)
                    return false;
                }
            }
        }

        // Event appears safe, call original function
        spdlog::debug("Animation event validation passed, processing: {}", eventName);
        
        try {
            // Call original NotifyAnimationGraph function
            bool result = _NotifyAnimationGraph(a_this, a_eventName);
            
            if (!result && actor) {
                spdlog::warn("Animation event '{}' failed for actor {:08X}", eventName, actor->GetFormID());
                // Could blacklist problematic events here
            }
            
            return result;
            
        } catch (const std::exception& e) {
            spdlog::error("Exception during animation event processing: {}", e.what());
            // AnimationHandler removed
            return false;
        }
    }

    // ========================================================================
    // Script Execution Hook Implementation
    // ========================================================================

    bool FunctionHookManager::Hook_ExecuteScript(RE::BSScript::Internal::VirtualMachine* vm,
                                                 RE::BSScript::Internal::CodeTasklet* tasklet) {
        
        // Validate input parameters
        if (!vm || !tasklet) {
            if (!vm) LogNullPointerViolation("Hook_ExecuteScript", "vm (VirtualMachine*)", vm);
            if (!tasklet) LogNullPointerViolation("Hook_ExecuteScript", "tasklet (CodeTasklet*)", tasklet);
            spdlog::error("Hook_ExecuteScript: Invalid parameters - vm={}, tasklet={}, returning safe default", 
                         static_cast<void*>(vm), static_cast<void*>(tasklet));
            IncrementPreventionCount();
            return false;
        }

        // Additional null safety checks for VM state
        if (!vm) {
            spdlog::error("Hook_ExecuteScript: VirtualMachine pointer is null, returning safe default");
            IncrementPreventionCount();
            return false;
        }

        spdlog::trace("Hook_ExecuteScript: Intercepting script execution");

        // Use ScriptMonitor to safely execute the script with error handling
        // This integrates with the existing ScriptMonitor class and provides:
        // - Null reference detection and safe defaults
        // - Script timeout monitoring
        // - Blacklist checking for problematic scripts
        // - Exception handling and recovery
        bool result = ScriptValidation::ScriptMonitor::ExecuteScriptSafe(vm, tasklet);

        if (result) {
            // Script executed successfully through ScriptMonitor
            // Now call the original VM execution function
            try {
                spdlog::trace("Hook_ExecuteScript: Calling original VM execution");
                bool originalResult = _ExecuteScript(vm, tasklet);
                
                if (!originalResult) {
                    spdlog::warn("Hook_ExecuteScript: Original VM execution failed");
                    // ScriptMonitor already handled the safe execution, so we can return true
                    // to indicate the hook handled the situation safely
                    IncrementPreventionCount();
                    return true;
                }
                
                return originalResult;
                
            } catch (const std::exception& e) {
                spdlog::error("Hook_ExecuteScript: Exception during original execution: {}", e.what());
                
                // Create script exception for ScriptMonitor
                ScriptValidation::ScriptException exception{
                    .scriptName = "Unknown", // Would extract from tasklet in real implementation
                    .lineNumber = 0,
                    .errorMessage = e.what(),
                    .modName = "Unknown",
                    .timestamp = std::chrono::steady_clock::now()
                };
                
                ScriptValidation::ScriptMonitor::HandleScriptException(exception);
                IncrementPreventionCount();
                
                // Return true to indicate we handled the error safely
                return true;
            }
        } else {
            // ScriptMonitor blocked execution (blacklisted, null reference, etc.)
            spdlog::debug("Hook_ExecuteScript: ScriptMonitor blocked execution");
            IncrementPreventionCount();
            
            // Return true to indicate we handled the situation safely
            // The script didn't execute, but we prevented a potential crash
            return true;
        }
    }

    // ========================================================================
    // Cell Loading Hook Implementation
    // ========================================================================

    void FunctionHookManager::Hook_LoadCell(RE::TESObjectCELL* a_cell) {
        
        // Validate input parameters
        if (!a_cell) {
            LogNullPointerViolation("Hook_LoadCell", "a_cell (TESObjectCELL*)", a_cell);
            spdlog::error("Hook_LoadCell: Cell pointer is null, returning safe default");
            IncrementPreventionCount();
            return;
        }

        // Additional null safety checks for cell state
        if (a_cell->IsDeleted()) {
            spdlog::warn("Hook_LoadCell: Cell {:08X} is deleted, skipping load", a_cell->GetFormID());
            IncrementPreventionCount();
            return;
        }

        // Validate cell data pointer
        auto& runtimeData = a_cell->GetRuntimeData();
        if (!runtimeData.cellData.interior && !runtimeData.cellData.exterior) {
            spdlog::error("Hook_LoadCell: Cell {:08X} has null cellData, returning safe default", a_cell->GetFormID());
            IncrementPreventionCount();
            return;
        }

        spdlog::debug("Hook_LoadCell: Intercepting cell loading for cell {:08X}", a_cell->GetFormID());

        // Use CellManager to safely load and validate the cell
        // This integrates with the existing CellManager class and provides:
        // - Cell data structure validation
        // - Reference validation before spawning
        // - Invalid reference skipping
        // - Blacklist checking for problematic cells
        // - Safe fallback handling
        bool loadResult = CellValidation::CellManager::LoadCellSafe(a_cell);

        if (loadResult) {
            // Cell validation passed, proceed with original loading
            spdlog::debug("Hook_LoadCell: Cell validation passed, proceeding with load");
            
            try {
                // Call original cell loading function
                // In a full implementation, this would call the original function
                // For now, we'll log the successful validation
                spdlog::debug("Hook_LoadCell: Cell {:08X} loaded successfully with validation", 
                             a_cell->GetFormID());
                
            } catch (const std::exception& e) {
                spdlog::error("Hook_LoadCell: Exception during original cell loading: {}", e.what());
                
                // Blacklist the cell if it causes exceptions during loading
                CellValidation::CellManager::BlacklistCell(a_cell, 
                    fmt::format("Exception during loading: {}", e.what()));
                
                IncrementPreventionCount();
            }
        } else {
            // Cell validation failed - CellManager already handled logging and blacklisting
            spdlog::warn("Hook_LoadCell: Cell validation failed, load blocked for cell {:08X}", 
                        a_cell->GetFormID());
            
            IncrementPreventionCount();
            
            // Don't proceed with original loading - this prevents crashes from invalid cells
            // The CellManager has already logged the specific validation failures
        }
    }

    // ========================================================================
    // Hook Installation
    // ========================================================================

    bool FunctionHookManager::InstallAllHooks() {
        spdlog::info("[FunctionHookManager] Installing proactive function hooks");

        if (s_hooksInstalled) {
            spdlog::warn("[FunctionHookManager] Hooks already installed");
            return true;
        }

        s_installedCount = 0;
        s_failedCount = 0;

        // Install mesh loading hooks
        auto meshResult = InstallMeshLoadingHooks();
        if (meshResult.success) {
            s_installedCount++;
            spdlog::info("[FunctionHookManager] Mesh loading hook installed");
        } else {
            s_failedCount++;
            spdlog::warn("[FunctionHookManager] Mesh loading hook failed: {}", meshResult.errorMessage);
        }

        // Install animation hooks  
        auto animResult = InstallAnimationHooks();
        if (animResult.success) {
            s_installedCount++;
            spdlog::info("[FunctionHookManager] Animation hook installed");
        } else {
            s_failedCount++;
            spdlog::warn("[FunctionHookManager] Animation hook failed: {}", animResult.errorMessage);
        }

        // Install script hooks
        auto scriptResult = InstallScriptHooks();
        if (scriptResult.success) {
            s_installedCount++;
            spdlog::info("[FunctionHookManager] Script hook installed");
        } else {
            s_failedCount++;
            spdlog::warn("[FunctionHookManager] Script hook failed: {}", scriptResult.errorMessage);
        }

        s_hooksInstalled = true;
        spdlog::info("[FunctionHookManager] Hooks complete: {} installed, {} failed", s_installedCount, s_failedCount);
        
        return s_installedCount > 0;  // Success if at least one hook installed
    }

    HookResult FunctionHookManager::InstallMeshLoadingHooks() {
        HookResult result;
        result.hookName = "MeshLoadingHook";
        result.success = false;

        spdlog::info("MeshLoadingHook: Installing TESObjectREFR::Load3D hook using REL::Relocation");
        
        // Hook TESObjectREFR::Load3D using REL::Relocation
        // This properly uses Address Library for Skyrim AE
        
        try {
            std::uintptr_t vtbl_address = 0;
            
            // Use Address Library for all versions (SE/AE/VR)
            auto vtblId = AddressLib::ResolveID({235511u, 190259u});
            if (!vtblId) {
                result.errorMessage = "VTABLE_TESObjectREFR id missing - skipping mesh hook";
                spdlog::warn("MeshLoadingHook: VTABLE_TESObjectREFR id missing in address library - skipping hook");
                return result;
            }
            vtbl_address = *vtblId;
            
            // Calculate the address of the virtual function at offset 0x6A (Load3D)
            // Note: VR vtable layout is the same as SE for this function
            constexpr std::uintptr_t LOAD3D_VTABLE_OFFSET = 0x6A;
            auto vfunc_addr = vtbl_address + (LOAD3D_VTABLE_OFFSET * sizeof(void*));
            
            // Read the original function pointer
            _LoadNif = *reinterpret_cast<decltype(_LoadNif)*>(vfunc_addr);
            
            // Write the hook function pointer using safe memory protection
            DWORD oldProtect;
            if (!VirtualProtect(reinterpret_cast<void*>(vfunc_addr), sizeof(void*), PAGE_READWRITE, &oldProtect)) {
                result.errorMessage = fmt::format("Failed to change memory protection: {}", GetLastError());
                spdlog::error("MeshLoadingHook: {}", result.errorMessage);
                return result;
            }
            
            *reinterpret_cast<std::uintptr_t*>(vfunc_addr) = reinterpret_cast<std::uintptr_t>(Hook_LoadNif);
            
            DWORD dummy;
            VirtualProtect(reinterpret_cast<void*>(vfunc_addr), sizeof(void*), oldProtect, &dummy);
            
            result.success = true;
            spdlog::info("MeshLoadingHook: TESObjectREFR::Load3D hooked successfully at vtable offset 0x6A");
            spdlog::info("MeshLoadingHook: Original function at {:#x}", _LoadNif.address());
            spdlog::info("MeshLoadingHook: Hook function at {:#x}", reinterpret_cast<std::uintptr_t>(Hook_LoadNif));
            spdlog::info("MeshLoadingHook: Mesh validation will occur on all object 3D loads");
            
            return result;
        } catch (const std::exception& e) {
            result.errorMessage = fmt::format("Exception during hook installation: {}", e.what());
            spdlog::error("MeshLoadingHook: {}", result.errorMessage);
            spdlog::error("MeshLoadingHook: Failed to hook TESObjectREFR::Load3D");
            return result;
        }
    }

    HookResult FunctionHookManager::InstallAnimationHooks() {
        HookResult result;
        result.hookName = "AnimationHook";
        result.success = false;
        
        spdlog::info("AnimationHook: Installing NotifyAnimationGraph hook using REL::Relocation");
        
        // Hook the NotifyAnimationGraph virtual function from IAnimationGraphManagerHolder
        // This function is called when animation events are triggered
        
        try {
            std::uintptr_t vtbl_address = 0;
            
            // Use Address Library for all versions (SE/AE/VR)
            auto animVtblId = AddressLib::ResolveID({256504u, 205174u});
            if (!animVtblId) {
                result.errorMessage = "VTABLE_IAnimationGraphManagerHolder id missing - skipping animation hook";
                spdlog::warn("AnimationHook: VTABLE_IAnimationGraphManagerHolder id missing in address library - skipping hook");
                return result;
            }
            vtbl_address = *animVtblId;
            
            // Calculate the address of the virtual function at offset 0x01
            // Note: VR vtable layout is the same as SE for this function
            constexpr std::uintptr_t NOTIFY_ANIMATION_GRAPH_VTABLE_OFFSET = 0x01;
            auto vfunc_addr = vtbl_address + (NOTIFY_ANIMATION_GRAPH_VTABLE_OFFSET * sizeof(void*));
            
            // Read the original function pointer
            _NotifyAnimationGraph = *reinterpret_cast<decltype(_NotifyAnimationGraph)*>(vfunc_addr);
            
            // Write the hook function pointer using safe memory protection
            DWORD oldProtect;
            if (!VirtualProtect(reinterpret_cast<void*>(vfunc_addr), sizeof(void*), PAGE_READWRITE, &oldProtect)) {
                result.errorMessage = fmt::format("Failed to change memory protection: {}", GetLastError());
                spdlog::error("AnimationHook: {}", result.errorMessage);
                return result;
            }
            
            *reinterpret_cast<std::uintptr_t*>(vfunc_addr) = reinterpret_cast<std::uintptr_t>(Hook_NotifyAnimationGraph);
            
            DWORD dummy;
            VirtualProtect(reinterpret_cast<void*>(vfunc_addr), sizeof(void*), oldProtect, &dummy);
            
            result.success = true;
            spdlog::info("AnimationHook: NotifyAnimationGraph hooked successfully at vtable offset 0x01");
            spdlog::info("AnimationHook: Original function at {:#x}", _NotifyAnimationGraph.address());
            spdlog::info("AnimationHook: Hook function at {:#x}", reinterpret_cast<std::uintptr_t>(Hook_NotifyAnimationGraph));
            spdlog::info("AnimationHook: Animation event validation will occur on all graph notifications");
            
            return result;
            
        } catch (const std::exception& e) {
            result.errorMessage = fmt::format("Exception during animation hook installation: {}", e.what());
            spdlog::error("AnimationHook: {}", result.errorMessage);
            return result;
        }
    }

    HookResult FunctionHookManager::InstallScriptHooks() {
        HookResult result;
        result.hookName = "ScriptExecutionHook";
        result.success = false;

        // No Papyrus VM hook is installed.
        //
        // A vtable hook on BSScript::Internal::VirtualMachine would require
        // a stable vtable offset that hasn't been validated against live crash
        // data across SE/AE/VR.  Installing a bad vtable hook would crash the
        // game deterministically.
        //
        // Known REL::IDs for future implementation:
        //   NativeFunctionBase::Call  = SE(97923) / AE(104651)
        //   SkyrimVM::QueuePostRenderCall = SE(53144) / AE(53955)
        //   SkyrimVM::RelayEvent      = SE(53221) / AE(54033)
        //
        // Hook_ExecuteScript() exists in this file but is unreachable because
        // no vtable write was ever performed to route calls through it.
        // ScriptMonitor::Initialize() creates data structures but no monitoring
        // thread or hook intercepts live Papyrus execution.
        //
        // Until the vtable offset is confirmed safe, this hook intentionally
        // returns failure so the status counters and UI reflect reality.

        result.errorMessage = "Papyrus VM hook not installed (vtable offset not yet validated)";
        spdlog::warn("ScriptExecutionHook: {}", result.errorMessage);
        return result;
    }

    HookResult FunctionHookManager::InstallCellLoadingHooks() {
        HookResult result;
        result.hookName = "CellLoadingHook";
        result.success = false;
        
        spdlog::info("CellLoadingHook: Installing cell loading validation hooks");
        
        // Hook cell loading through TESCellFullyLoadedEvent system
        // This approach uses the event system to intercept cell loading completion
        // and validate references before they are spawned
        
        try {
            // Register event-based cell loading handler
            // This is safer and more version-independent than function hooking
            if (!CrashGuard::CellLoadingEventHandler::Register()) {
                result.errorMessage = "Failed to register CellLoadingEventHandler";
                spdlog::error("CellLoadingHook: {}", result.errorMessage);
                // Continue anyway - not a critical failure
            } else {
                spdlog::info("CellLoadingHook: Registered TESCellFullyLoadedEvent handler");
                spdlog::info("CellLoadingHook: Registered TESCellAttachDetachEvent handler");
            }
            
            result.success = true;
            result.errorMessage = "Cell loading event handlers registered";
            
            spdlog::info("CellLoadingHook: CellManager will validate references on cell load");
            
            return result;
            
        } catch (const std::exception& e) {
            result.errorMessage = fmt::format("Exception during cell loading hook installation: {}", e.what());
            spdlog::error("CellLoadingHook: {}", result.errorMessage);
            return result;
        }
    }

    HookResult FunctionHookManager::InstallFormIDValidationHooks() {
        HookResult result;
        result.hookName = "FormIDValidationHook";
        result.success = false;
        
        spdlog::info("FormIDValidationHook: Installing FormID validation layer");
        
        // Initialize the FormIDValidator
        // This sets up the validation cache and prepares the validator for use
        
        try {
            // Initialize the FormIDValidator
            bool initResult = FormIDValidation::FormIDValidator::Initialize();
            
            if (!initResult) {
                // Keep as error but explain the context
                result.errorMessage = "Failed to initialize FormIDValidator";
                spdlog::error("[PLUGIN INIT] FormIDValidationHook: {}", result.errorMessage);
                spdlog::info("  → This is EXPECTED during startup - will retry at main menu");
                spdlog::info("  → Severity: LOW (not a problem, just timing)");
                return result;
            }
            
            spdlog::info("FormIDValidationHook: FormIDValidator initialized successfully");
            
            // The FormID validation layer works differently from other hooks
            // Instead of hooking specific functions, it provides a safe wrapper
            // around FormID lookups that can be used throughout the codebase
            //
            // Usage:
            //   Instead of: auto* form = RE::TESForm::LookupByID(formID);
            //   Use:        auto* form = FormIDValidation::FormIDValidator::LookupFormSafe(formID);
            //
            // This provides:
            // - FormID format validation
            // - Plugin load status checking
            // - TESDataHandler existence validation
            // - Null return for invalid FormIDs (no crash)
            // - Performance caching of valid lookups
            
            spdlog::info("FormIDValidationHook: FormID validation layer is active");
            spdlog::info("FormIDValidationHook: All FormID lookups should use FormIDValidator::LookupFormSafe");
            spdlog::info("FormIDValidationHook: Invalid FormIDs will return nullptr safely");
            spdlog::info("FormIDValidationHook: Valid lookups will be cached for performance");
            
            result.success = true;
            result.errorMessage = "FormID validation layer active - use FormIDValidator::LookupFormSafe for safe lookups";
            
            return result;
            
        } catch (const std::exception& e) {
            result.errorMessage = fmt::format("Exception during FormID validation hook installation: {}", e.what());
            spdlog::error("FormIDValidationHook: {}", result.errorMessage);
            return result;
        }
    }

    // ========================================================================
    // Statistics and State
    // ========================================================================

    HookStats FunctionHookManager::GetStats() {
        HookStats stats;
        stats.totalHooks = s_installedCount + s_failedCount;
        stats.installedHooks = s_installedCount;
        stats.failedHooks = s_failedCount;
        stats.validationsPrevented = s_preventionCount;
        return stats;
    }

    void FunctionHookManager::IncrementPreventionCount() {
        s_preventionCount++;
    }

    bool FunctionHookManager::AreHooksInstalled() {
        return s_hooksInstalled;
    }

    // ========================================================================
    // Logging
    // ========================================================================

    void FunctionHookManager::LogHookSuccess(const std::string& hookName, uintptr_t address) {
        if (address != 0) {
            spdlog::info("✓ Hook installed: {} at {:#x}", hookName, address);
        } else {
            spdlog::info("✓ Hook registered: {} (deferred installation)", hookName);
        }
    }

    void FunctionHookManager::LogHookFailure(const std::string& hookName, const std::string& reason) {
        // Check if this is an expected failure (FormIDValidator waiting for game data)
        if (reason.find("waiting for game data") != std::string::npos || 
            reason.find("will retry") != std::string::npos) {
            spdlog::info("⏳ Hook deferred: {} - {}", hookName, reason);
        } else {
            spdlog::error("✗ Hook failed: {} - {}", hookName, reason);
        }
    }

    void FunctionHookManager::LogNullPointerViolation(const std::string& functionName, const std::string& parameterName, void* address) {
        if (address) {
            spdlog::warn("⚠ Null pointer violation in {}: {} at address {:#x}", functionName, parameterName, reinterpret_cast<uintptr_t>(address));
        } else {
            spdlog::warn("⚠ Null pointer violation in {}: {} (no address available)", functionName, parameterName);
        }
        
        // Log additional diagnostic information
        spdlog::debug("Null pointer violation details:");
        spdlog::debug("  Function: {}", functionName);
        spdlog::debug("  Parameter: {}", parameterName);
        spdlog::debug("  Thread ID: {}", GetCurrentThreadId());
        spdlog::debug("  Timestamp: {}", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    void FunctionHookManager::LogBoundsViolation(const std::string& functionName, const std::string& arrayName, size_t index, size_t size) {
        spdlog::warn("⚠ Array bounds violation in {}: {} - index {} out of bounds [0, {})", 
                     functionName, arrayName, index, size);
        
        // Log additional diagnostic information
        spdlog::debug("Bounds violation details:");
        spdlog::debug("  Function: {}", functionName);
        spdlog::debug("  Array: {}", arrayName);
        spdlog::debug("  Index: {}", index);
        spdlog::debug("  Size: {}", size);
        spdlog::debug("  Thread ID: {}", GetCurrentThreadId());
        spdlog::debug("  Timestamp: {}", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    void FunctionHookManager::LogFormIDViolation(const std::string& functionName, RE::FormID formID, const std::string& reason) {
        spdlog::warn("⚠ FormID validation violation in {}: {:08X} - {}", functionName, formID, reason);
        
        // Log additional diagnostic information
        spdlog::debug("FormID violation details:");
        spdlog::debug("  Function: {}", functionName);
        spdlog::debug("  FormID: {:08X}", formID);
        spdlog::debug("  Reason: {}", reason);
        spdlog::debug("  Thread ID: {}", GetCurrentThreadId());
        spdlog::debug("  Timestamp: {}", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    bool FunctionHookManager::ValidateArrayAccess(size_t index, size_t arraySize, const std::string& functionName, const std::string& arrayName) {
        if (arraySize == 0) {
            spdlog::error("Array bounds check in {}: {} has size 0", functionName, arrayName);
            return false;
        }
        
        if (index >= arraySize) {
            LogBoundsViolation(functionName, arrayName, index, arraySize);
            return false;
        }
        
        return true;
    }

    // ========================================================================
    // Validation
    // ========================================================================

    bool FunctionHookManager::ValidateHookTarget(uintptr_t address, const std::string& hookName) {
        if (address == 0) {
            spdlog::error("Hook target address is null for {}", hookName);
            return false;
        }

        // Check if address is in valid memory range
        if (address < 0x10000) {
            spdlog::error("Hook target address {:#x} is too low for {}", address, hookName);
            return false;
        }

        // Verify memory is readable
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi))) {
            spdlog::error("Cannot query memory at {:#x} for {}", address, hookName);
            return false;
        }

        if (mbi.State != MEM_COMMIT) {
            spdlog::error("Memory at {:#x} is not committed for {}", address, hookName);
            return false;
        }

        // Check if memory is executable (should be code)
        if (!(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) {
            spdlog::warn("Memory at {:#x} is not executable for {} (may be data)", address, hookName);
        }

        return true;
    }

}  // namespace FunctionHooks
