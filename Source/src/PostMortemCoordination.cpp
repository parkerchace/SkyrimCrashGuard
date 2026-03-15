// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

// ═══════════════════════════════════════════════════════════════════════
// PostMortemCoordination.cpp — Post-Mortem Plugin Coordination
// ═══════════════════════════════════════════════════════════════════════
//
// Purpose: Coordinate with trainwreck-post-mortem plugin to share game
// object introspection data, root cause analysis results, and recovery
// action history for enhanced post-crash analysis.
//
// ═══════════════════════════════════════════════════════════════════════

#include "PostMortemCoordination.h"
#include "VEH.h"
#include "UnifiedCrashReport.h"

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace PostMortemCoordination {

// ═══════════════════════════════════════════════════════════════════════
// § 1  Static Members
// ═══════════════════════════════════════════════════════════════════════

PostMortemAPI::ShareGameObjectInfo_t Coordinator::s_shareGameObjectInfo = nullptr;
PostMortemAPI::ShareRootCauseInfo_t Coordinator::s_shareRootCauseInfo = nullptr;
PostMortemAPI::ShareRecoveryHistory_t Coordinator::s_shareRecoveryHistory = nullptr;
PostMortemAPI::RegisterDataProvider_t Coordinator::s_registerDataProvider = nullptr;
PostMortemAPI::IsPostMortemActive_t Coordinator::s_isPostMortemActive = nullptr;

HMODULE Coordinator::s_postMortemModule = nullptr;
bool Coordinator::s_initialized = false;
bool Coordinator::s_apiAvailable = false;

// ═══════════════════════════════════════════════════════════════════════
// § 2  Public Interface
// ═══════════════════════════════════════════════════════════════════════

bool Coordinator::Initialize() {
    if (s_initialized) return true;
    
    auto log = spdlog::default_logger();
    
    // Try to load post-mortem plugin API
    if (!LoadPostMortemAPI()) {
        if (log) log->info("[PostMortemCoordination] trainwreck-post-mortem plugin not detected");
        s_initialized = true; // Still initialize, just without API
        return true;
    }
    
    // Register as data provider
    if (!RegisterAsDataProvider()) {
        if (log) log->warn("[PostMortemCoordination] Failed to register as data provider");
        UnloadPostMortemAPI();
        s_initialized = true;
        return true;
    }
    
    s_apiAvailable = true;
    s_initialized = true;
    
    if (log) {
        log->info("╔══════════════════════════════════════════════════╗");
        log->info("║  Post-Mortem Coordination — Active              ║");
        log->info("║  Plugin: trainwreck-post-mortem                 ║");
        log->info("║  Data Sharing: Game Objects, Root Cause, Recovery║");
        log->info("╚══════════════════════════════════════════════════╝");
    }
    
    return true;
}

void Coordinator::Shutdown() {
    if (!s_initialized) return;
    
    UnloadPostMortemAPI();
    s_initialized = false;
    s_apiAvailable = false;
    
    auto log = spdlog::default_logger();
    if (log) log->info("[PostMortemCoordination] Shutdown complete");
}

bool Coordinator::IsPostMortemAvailable() {
    return s_initialized && s_apiAvailable && s_isPostMortemActive && s_isPostMortemActive();
}

bool Coordinator::ShareGameObjectIntrospection(
    const std::vector<SharedGameObjectInfo>& objects) {
    
    if (!IsPostMortemAvailable() || !s_shareGameObjectInfo) return false;
    
    try {
        std::string jsonData = SerializeGameObjectInfo(objects);
        s_shareGameObjectInfo(jsonData.c_str());
        
        auto log = spdlog::default_logger();
        if (log) log->debug("[PostMortemCoordination] Shared {} game objects with post-mortem plugin", 
                           objects.size());
        
        return true;
    } catch (const std::exception& e) {
        auto log = spdlog::default_logger();
        if (log) log->error("[PostMortemCoordination] Failed to share game object data: {}", e.what());
        return false;
    }
}

bool Coordinator::ShareRootCauseAnalysis(const SharedRootCauseInfo& rootCause) {
    if (!IsPostMortemAvailable() || !s_shareRootCauseInfo) return false;
    
    try {
        std::string jsonData = SerializeRootCauseInfo(rootCause);
        s_shareRootCauseInfo(jsonData.c_str());
        
        auto log = spdlog::default_logger();
        if (log) log->debug("[PostMortemCoordination] Shared root cause analysis: {} (confidence: {:.1f}%)", 
                           rootCause.category, rootCause.confidence * 100.0f);
        
        return true;
    } catch (const std::exception& e) {
        auto log = spdlog::default_logger();
        if (log) log->error("[PostMortemCoordination] Failed to share root cause data: {}", e.what());
        return false;
    }
}

bool Coordinator::ShareRecoveryActionHistory(
    const std::vector<SharedRecoveryInfo>& recoveryHistory) {
    
    if (!IsPostMortemAvailable() || !s_shareRecoveryHistory) return false;
    
    try {
        std::string jsonData = SerializeRecoveryHistory(recoveryHistory);
        s_shareRecoveryHistory(jsonData.c_str());
        
        auto log = spdlog::default_logger();
        if (log) log->debug("[PostMortemCoordination] Shared {} recovery actions with post-mortem plugin", 
                           recoveryHistory.size());
        
        return true;
    } catch (const std::exception& e) {
        auto log = spdlog::default_logger();
        if (log) log->error("[PostMortemCoordination] Failed to share recovery history: {}", e.what());
        return false;
    }
}

bool Coordinator::ShareUnifiedReport(const UnifiedCrashReport::UnifiedReport& report) {
    if (!IsPostMortemAvailable()) return false;
    
    bool success = true;
    
    // Share game object data if available
    if (report.involvedObject.has_value()) {
        std::vector<UnifiedCrashReport::GameObjectData> objects = { report.involvedObject.value() };
        auto sharedObjects = ConvertGameObjectsToShared(objects);
        success &= ShareGameObjectIntrospection(sharedObjects);
    }
    
    // Share root cause analysis
    SharedRootCauseInfo rootCause;
    rootCause.category = "VEH"; // From VEH analysis
    rootCause.description = report.rootCause;
    rootCause.confidence = report.confidence;
    rootCause.analysisMethod = "CrashGuard 6-Layer Analysis";
    success &= ShareRootCauseAnalysis(rootCause);
    
    // Share recovery history
    if (!report.recoveryActions.empty()) {
        auto sharedRecovery = ConvertRecoveryActionsToShared(report.recoveryActions);
        success &= ShareRecoveryActionHistory(sharedRecovery);
    }
    
    return success;
}

SharedRootCauseInfo Coordinator::ConvertCrashContextToShared(const VEH::CrashContext& context) {
    SharedRootCauseInfo shared;
    
    // Convert severity to category
    switch (context.severity) {
    case VEH::SeverityLevel::Safe:
        shared.category = "Visual";
        break;
    case VEH::SeverityLevel::Warning:
        shared.category = "Resource";
        break;
    case VEH::SeverityLevel::Critical:
        shared.category = "Data";
        break;
    case VEH::SeverityLevel::Fatal:
        shared.category = "System";
        break;
    default:
        shared.category = "Unknown";
        break;
    }
    
    shared.description = context.rootCause;
    shared.confidence = 0.8f; // VEH analysis confidence
    shared.analysisMethod = "VEH Exception Analysis";
    
    return shared;
}

std::vector<SharedGameObjectInfo> Coordinator::ConvertGameObjectsToShared(
    const std::vector<UnifiedCrashReport::GameObjectData>& objects) {
    
    std::vector<SharedGameObjectInfo> shared;
    shared.reserve(objects.size());
    
    for (const auto& obj : objects) {
        SharedGameObjectInfo sharedObj;
        sharedObj.type = obj.type;
        sharedObj.formID = obj.formID;
        sharedObj.editorID = obj.editorID;
        sharedObj.modName = obj.modName;
        sharedObj.address = obj.address;
        sharedObj.isValid = obj.isValid;
        
        // Add metadata
        sharedObj.properties["Source"] = "CrashGuard";
        sharedObj.properties["IntrospectionMethod"] = "RTTI + FormID";
        
        shared.push_back(sharedObj);
    }
    
    return shared;
}

std::vector<SharedRecoveryInfo> Coordinator::ConvertRecoveryActionsToShared(
    const std::vector<UnifiedCrashReport::RecoveryAction>& actions) {
    
    std::vector<SharedRecoveryInfo> shared;
    shared.reserve(actions.size());
    
    for (const auto& action : actions) {
        SharedRecoveryInfo sharedAction;
        sharedAction.strategy = action.strategy;
        sharedAction.success = action.success;
        sharedAction.actions = action.actions;
        sharedAction.timestamp = action.timestamp;
        sharedAction.failureReason = action.failureReason;
        
        // Add metadata
        sharedAction.metadata["RecoverySystem"] = "CrashGuard 6-Layer";
        sharedAction.metadata["Version"] = "2.1.0";
        
        shared.push_back(sharedAction);
    }
    
    return shared;
}

// ═══════════════════════════════════════════════════════════════════════
// § 3  Private Implementation
// ═══════════════════════════════════════════════════════════════════════

bool Coordinator::LoadPostMortemAPI() {
    // Try common post-mortem plugin DLL names
    const char* dllNames[] = {
        "trainwreck-post-mortem.dll",
        "trainwreck_post_mortem.dll",
        "TrainwreckPostMortem.dll"
    };
    
    for (const char* dllName : dllNames) {
        s_postMortemModule = GetModuleHandleA(dllName);
        if (s_postMortemModule) break;
    }
    
    if (!s_postMortemModule) return false;
    
    // Resolve API functions
    bool success = true;
    success &= ResolveFunction(s_postMortemModule, "PostMortem_ShareGameObjectInfo", s_shareGameObjectInfo);
    success &= ResolveFunction(s_postMortemModule, "PostMortem_ShareRootCauseInfo", s_shareRootCauseInfo);
    success &= ResolveFunction(s_postMortemModule, "PostMortem_ShareRecoveryHistory", s_shareRecoveryHistory);
    success &= ResolveFunction(s_postMortemModule, "PostMortem_RegisterDataProvider", s_registerDataProvider);
    success &= ResolveFunction(s_postMortemModule, "PostMortem_IsActive", s_isPostMortemActive);
    
    if (!success) {
        auto log = spdlog::default_logger();
        if (log) log->warn("[PostMortemCoordination] Failed to resolve all post-mortem API functions");
        UnloadPostMortemAPI();
        return false;
    }
    
    return true;
}

void Coordinator::UnloadPostMortemAPI() {
    s_shareGameObjectInfo = nullptr;
    s_shareRootCauseInfo = nullptr;
    s_shareRecoveryHistory = nullptr;
    s_registerDataProvider = nullptr;
    s_isPostMortemActive = nullptr;
    s_postMortemModule = nullptr;
}

bool Coordinator::RegisterAsDataProvider() {
    if (!s_registerDataProvider) return false;
    
    const char* providerName = "SkyrimCrashGuard";
    const char* providerVersion = "2.1.0";
    
    bool success = s_registerDataProvider(providerName, providerVersion);
    
    if (success) {
        auto log = spdlog::default_logger();
        if (log) log->info("[PostMortemCoordination] Successfully registered as post-mortem data provider");
    }
    
    return success;
}

std::string Coordinator::SerializeGameObjectInfo(
    const std::vector<SharedGameObjectInfo>& objects) {
    
    nlohmann::json j = nlohmann::json::array();
    
    for (const auto& obj : objects) {
        nlohmann::json objJson;
        objJson["type"] = obj.type;
        objJson["formID"] = obj.formID;
        objJson["editorID"] = obj.editorID;
        objJson["modName"] = obj.modName;
        objJson["address"] = fmt::format("{:#x}", obj.address);
        objJson["isValid"] = obj.isValid;
        objJson["properties"] = obj.properties;
        
        j.push_back(objJson);
    }
    
    return j.dump();
}

std::string Coordinator::SerializeRootCauseInfo(const SharedRootCauseInfo& rootCause) {
    nlohmann::json j;
    
    j["category"] = rootCause.category;
    j["description"] = rootCause.description;
    j["confidence"] = rootCause.confidence;
    j["suspectedMods"] = rootCause.suspectedMods;
    j["suggestedFixes"] = rootCause.suggestedFixes;
    j["analysisMethod"] = rootCause.analysisMethod;
    
    return j.dump();
}

std::string Coordinator::SerializeRecoveryHistory(
    const std::vector<SharedRecoveryInfo>& history) {
    
    nlohmann::json j = nlohmann::json::array();
    
    for (const auto& recovery : history) {
        nlohmann::json recoveryJson;
        recoveryJson["strategy"] = recovery.strategy;
        recoveryJson["success"] = recovery.success;
        recoveryJson["actions"] = recovery.actions;
        recoveryJson["timestamp"] = recovery.timestamp;
        
        if (!recovery.failureReason.empty()) {
            recoveryJson["failureReason"] = recovery.failureReason;
        }
        
        recoveryJson["metadata"] = recovery.metadata;
        
        j.push_back(recoveryJson);
    }
    
    return j.dump();
}

template<typename T>
bool Coordinator::ResolveFunction(HMODULE module, const char* name, T& outFunc) {
    auto proc = GetProcAddress(module, name);
    if (!proc) {
        auto log = spdlog::default_logger();
        if (log) log->debug("[PostMortemCoordination] Failed to resolve: {}", name);
        return false;
    }
    outFunc = reinterpret_cast<T>(proc);
    return true;
}

}  // namespace PostMortemCoordination