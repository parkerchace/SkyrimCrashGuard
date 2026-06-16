// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

// ═══════════════════════════════════════════════════════════════════════
// TrainwreckBridge.cpp — Integration with Trainwreck crash logger API
// ═══════════════════════════════════════════════════════════════════════
//
// Purpose: Provide seamless integration with Trainwreck's version-independent
// crash logging system while maintaining modern C++23 style internally.
//
// Architecture:
// - Runtime API loading via GetProcAddress (no .lib dependency)
// - C++ wrapper around Trainwreck's C API
// - Unified crash reporting across VEH, CrashLogger, and Trainwreck
// - Modern C++23 internally, C-compatible exports to Trainwreck
//
// ═══════════════════════════════════════════════════════════════════════

#include "TrainwreckBridge.h"
#include "VEH.h"
#include "CrashLoggerDetector.h"
#include "CrashLoggerIntegration.h"
#include "UnifiedCrashReport.h"
#include "PostMortemCoordination.h"

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace TrainwreckBridge {

// ═══════════════════════════════════════════════════════════════════════
// § 1  TrainwreckAPI Static Members
// ═══════════════════════════════════════════════════════════════════════

HMODULE TrainwreckAPI::s_trainwreckModule = nullptr;
bool TrainwreckAPI::s_initialized = false;

API::RegisterPlugin_t TrainwreckAPI::s_registerPlugin = nullptr;
API::AddCrashInfo_t TrainwreckAPI::s_addCrashInfo = nullptr;
API::AddSection_t TrainwreckAPI::s_addSection = nullptr;
API::IsAvailable_t TrainwreckAPI::s_isAvailable = nullptr;
API::GetVersion_t TrainwreckAPI::s_getVersion = nullptr;
API::SetCrashCallback_t TrainwreckAPI::s_setCrashCallback = nullptr;
API::SetStatusCallback_t TrainwreckAPI::s_setStatusCallback = nullptr;

// ═══════════════════════════════════════════════════════════════════════
// § 2  TrainwreckAPI Implementation
// ═══════════════════════════════════════════════════════════════════════

bool TrainwreckAPI::Initialize() {
    if (s_initialized) return true;
    
    auto log = spdlog::default_logger();
    
    // Try to load Trainwreck DLL
    if (!LoadAPI()) {
        if (log) log->info("[TrainwreckBridge] Trainwreck not detected - running standalone");
        return false;
    }
    
    // Register as a plugin
    if (!RegisterPlugin()) {
        if (log) log->warn("[TrainwreckBridge] Failed to register with Trainwreck");
        UnloadAPI();
        return false;
    }
    
    // Set up crash callbacks
    SetupCrashCallbacks();
    
    s_initialized = true;
    
    if (log) {
        log->info("[Trainwreck] Integration active - Version: {}, Extended logging enabled", GetVersion());
    }
    
    return true;
}

void TrainwreckAPI::Shutdown() {
    if (!s_initialized) return;
    
    UnloadAPI();
    s_initialized = false;
    
    auto log = spdlog::default_logger();
    if (log) log->info("[TrainwreckBridge] Shutdown complete");
}

bool TrainwreckAPI::IsAvailable() {
    return s_initialized && s_isAvailable && s_isAvailable();
}

bool TrainwreckAPI::RegisterPlugin() {
    if (!s_registerPlugin) return false;
    
    const char* pluginName = "SkyrimCrashGuard";
    const char* pluginVersion = "2.1.0";
    
    bool success = s_registerPlugin(pluginName, pluginVersion);
    
    if (success) {
        auto log = spdlog::default_logger();
        if (log) log->info("[TrainwreckBridge] Successfully registered as Trainwreck plugin");
    }
    
    return success;
}

bool TrainwreckAPI::SetupCrashCallbacks() {
    // Set up crash callback if available (optional API)
    if (s_setCrashCallback) {
        // Trainwreck exports a crash-callback setter, but its expected callback
        // signature requires data that CrashGuard only has inside VEH recovery —
        // at which point we can't safely call back into Trainwreck's allocator.
        // We use the direct export (ProvideExtendedCrashInfo) instead.
        auto log = spdlog::default_logger();
        if (log) log->debug("[TrainwreckBridge] Trainwreck crash callback skipped (use direct export instead)");
    }
    
    return true;
}

void TrainwreckAPI::ProvideExtendedCrashInfo(const VEH::CrashContext& context) {
    if (!IsAvailable()) return;
    
    // This is the main entry point for providing extended crash information
    // It calls the existing ExportCrashContext but with additional metadata
    
    // Mark this as extended crash information
    AddCrashInfo("CrashGuard.Extended", "true");
    AddCrashInfo("CrashGuard.API.Version", "2.1.0");
    AddCrashInfo("CrashGuard.Integration.Type", "Plugin");
    
    // Export the full crash context
    ExportCrashContext(context);
    
    // Add additional extended information
    AddCrashInfo("CrashGuard.Capabilities", "6-Layer Recovery, Pattern Learning, State Management");
    AddCrashInfo("CrashGuard.Recovery.Available", "true");
}

void TrainwreckAPI::ExportCrashContext(const VEH::CrashContext& context) {
    if (!IsAvailable()) return;
    
    // Export exception information
    AddCrashInfo("CrashGuard.ExceptionCode", 
                 fmt::format("{:#x}", context.exceptionCode));
    AddCrashInfo("CrashGuard.CrashAddress", 
                 fmt::format("{:#x}", reinterpret_cast<uintptr_t>(context.crashAddress)));
    
    // Export severity classification
    std::string severityStr;
    switch (context.severity) {
    case VEH::SeverityLevel::Safe: severityStr = "Safe"; break;
    case VEH::SeverityLevel::Warning: severityStr = "Warning"; break;
    case VEH::SeverityLevel::Critical: severityStr = "Critical"; break;
    case VEH::SeverityLevel::Fatal: severityStr = "Fatal"; break;
    default: severityStr = "Unknown"; break;
    }
    AddCrashInfo("CrashGuard.Severity", severityStr);
    
    // Export root cause if available
    if (!context.rootCause.empty() && context.rootCause != "Unknown") {
        AddCrashInfo("CrashGuard.RootCause.Description", context.rootCause);
    }
    
    // Export call stack
    if (!context.callStack.empty()) {
        std::string stackTrace;
        for (size_t i = 0; i < context.callStack.size() && i < 32; ++i) {
            const auto& frame = context.callStack[i];
            stackTrace += fmt::format("[{:2}] {:#x} {}+{:#x} {}\n",
                                     i,
                                     reinterpret_cast<uintptr_t>(frame.address),
                                     frame.moduleName,
                                     frame.offset,
                                     frame.functionName);
        }
        AddSection("CrashGuard Call Stack", stackTrace);
    }
    
    // Export CPU registers
    AddCrashInfo("CrashGuard.RIP", fmt::format("{:#x}", context.cpuContext.Rip));
    AddCrashInfo("CrashGuard.RSP", fmt::format("{:#x}", context.cpuContext.Rsp));
    AddCrashInfo("CrashGuard.RAX", fmt::format("{:#x}", context.cpuContext.Rax));
    AddCrashInfo("CrashGuard.RCX", fmt::format("{:#x}", context.cpuContext.Rcx));
    AddCrashInfo("CrashGuard.RDX", fmt::format("{:#x}", context.cpuContext.Rdx));
    AddCrashInfo("CrashGuard.RBX", fmt::format("{:#x}", context.cpuContext.Rbx));
}

void TrainwreckAPI::AddCrashInfo(const std::string& key, const std::string& value) {
    if (!IsAvailable() || !s_addCrashInfo) return;
    s_addCrashInfo(key.c_str(), value.c_str());
}

void TrainwreckAPI::AddSection(const std::string& sectionName, const std::string& content) {
    if (!IsAvailable() || !s_addSection) return;
    s_addSection(sectionName.c_str(), content.c_str());
}

void TrainwreckAPI::ExportGameObjectInfo(void* object, const std::string& type,
                                        const std::string& formID, const std::string& editorID) {
    if (!IsAvailable()) return;
    
    AddCrashInfo("CrashGuard.InvolvedObject.Type", type);
    AddCrashInfo("CrashGuard.InvolvedObject.FormID", formID);
    AddCrashInfo("CrashGuard.InvolvedObject.EditorID", editorID);
    AddCrashInfo("CrashGuard.InvolvedObject.Address", 
                 fmt::format("{:#x}", reinterpret_cast<uintptr_t>(object)));
}

void TrainwreckAPI::ExportRootCause(const std::string& category, const std::string& description,
                                   float confidence, const std::vector<std::string>& suspectedMods) {
    if (!IsAvailable()) return;
    
    AddCrashInfo("CrashGuard.RootCause.Category", category);
    AddCrashInfo("CrashGuard.RootCause.Description", description);
    AddCrashInfo("CrashGuard.RootCause.Confidence", fmt::format("{:.2f}", confidence));
    
    if (!suspectedMods.empty()) {
        std::string modList;
        for (const auto& mod : suspectedMods) {
            if (!modList.empty()) modList += ", ";
            modList += mod;
        }
        AddCrashInfo("CrashGuard.RootCause.SuspectedMods", modList);
    }
}

void TrainwreckAPI::ExportRecoveryActions(const std::vector<std::string>& actions,
                                         const std::string& strategy, bool success) {
    if (!IsAvailable()) return;
    
    AddCrashInfo("CrashGuard.Recovery.Strategy", strategy);
    AddCrashInfo("CrashGuard.Recovery.Success", success ? "true" : "false");
    
    if (!actions.empty()) {
        std::string actionList;
        for (size_t i = 0; i < actions.size(); ++i) {
            actionList += fmt::format("{}. {}\n", i + 1, actions[i]);
        }
        AddSection("CrashGuard Recovery Actions", actionList);
    }
}

void TrainwreckAPI::ExportSeverity(const std::string& severity, const std::string& reason) {
    if (!IsAvailable()) return;
    
    AddCrashInfo("CrashGuard.Severity.Level", severity);
    AddCrashInfo("CrashGuard.Severity.Reason", reason);
}

void TrainwreckAPI::ExportPatternData(const std::string& signature, uint32_t occurrences,
                                     const std::string& bestStrategy, float successRate) {
    if (!IsAvailable()) return;
    
    AddCrashInfo("CrashGuard.Pattern.Signature", signature);
    AddCrashInfo("CrashGuard.Pattern.Occurrences", fmt::format("{}", occurrences));
    AddCrashInfo("CrashGuard.Pattern.BestStrategy", bestStrategy);
    AddCrashInfo("CrashGuard.Pattern.SuccessRate", fmt::format("{:.1f}%", successRate * 100.0f));
}

std::string TrainwreckAPI::GetVersion() {
    if (!s_getVersion) return "Unknown";
    const char* version = s_getVersion();
    return version ? std::string(version) : "Unknown";
}

bool TrainwreckAPI::LoadAPI() {
    // Try common Trainwreck DLL names
    const char* dllNames[] = {
        "trainwreck.dll",
        "trainwreck_x64.dll",
        "Trainwreck.dll"
    };
    
    for (const char* dllName : dllNames) {
        s_trainwreckModule = GetModuleHandleA(dllName);
        if (s_trainwreckModule) break;
    }
    
    if (!s_trainwreckModule) return false;
    
    // Resolve API functions
    bool success = true;
    success &= ResolveFunction(s_trainwreckModule, "Trainwreck_RegisterPlugin", s_registerPlugin);
    success &= ResolveFunction(s_trainwreckModule, "Trainwreck_AddCrashInfo", s_addCrashInfo);
    success &= ResolveFunction(s_trainwreckModule, "Trainwreck_AddSection", s_addSection);
    success &= ResolveFunction(s_trainwreckModule, "Trainwreck_IsAvailable", s_isAvailable);
    success &= ResolveFunction(s_trainwreckModule, "Trainwreck_GetVersion", s_getVersion);
    
    // Optional extended API functions (don't fail if not available)
    ResolveFunction(s_trainwreckModule, "Trainwreck_SetCrashCallback", s_setCrashCallback);
    ResolveFunction(s_trainwreckModule, "Trainwreck_SetStatusCallback", s_setStatusCallback);
    
    if (!success) {
        auto log = spdlog::default_logger();
        if (log) log->warn("[TrainwreckBridge] Failed to resolve all API functions");
        UnloadAPI();
        return false;
    }
    
    return true;
}

void TrainwreckAPI::UnloadAPI() {
    s_registerPlugin = nullptr;
    s_addCrashInfo = nullptr;
    s_addSection = nullptr;
    s_isAvailable = nullptr;
    s_getVersion = nullptr;
    s_setCrashCallback = nullptr;
    s_setStatusCallback = nullptr;
    s_trainwreckModule = nullptr;
}

template<typename T>
bool TrainwreckAPI::ResolveFunction(HMODULE module, const char* name, T& outFunc) {
    auto proc = GetProcAddress(module, name);
    if (!proc) {
        auto log = spdlog::default_logger();
        if (log) log->debug("[TrainwreckBridge] Failed to resolve: {}", name);
        return false;
    }
    outFunc = reinterpret_cast<T>(proc);
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// § 3  UnifiedCrashReporter Implementation
// ═══════════════════════════════════════════════════════════════════════

bool UnifiedCrashReporter::s_crashLoggerAvailable = false;
bool UnifiedCrashReporter::s_trainwreckAvailable = false;

void UnifiedCrashReporter::Initialize() {
    auto log = spdlog::default_logger();
    
    // Initialize crash logger detector
    CrashLoggerDetector::Detector::Initialize();
    
    // Initialize unified crash report system
    UnifiedCrashReport::ReportManager::Initialize();
    
    // Initialize post-mortem coordination
    PostMortemCoordination::Coordinator::Initialize();
    
    // Update availability flags based on detection results
    s_crashLoggerAvailable = CrashLoggerDetector::Detector::IsCrashLoggerPresent();
    s_trainwreckAvailable = CrashLoggerDetector::Detector::IsTrainwreckPresent();
    
    // Initialize Trainwreck if detected
    if (s_trainwreckAvailable) {
        s_trainwreckAvailable = TrainwreckAPI::Initialize();
    }
    
    // Initialize CrashLogger integration if detected
    if (s_crashLoggerAvailable) {
        CrashLoggerIntegration::LogInjector::Initialize();
    }
    
    if (log) {
        log->info("[UnifiedReporter] CrashLogger: {}, Trainwreck: {}, Post-Mortem: {}, Format: v{}",
                  s_crashLoggerAvailable ? "Yes" : "No",
                  s_trainwreckAvailable ? "Yes" : "No",
                  PostMortemCoordination::Coordinator::IsPostMortemAvailable() ? "Yes" : "No",
                  UnifiedCrashReport::ReportManager::GetFormatVersion());
    }
}

void UnifiedCrashReporter::Shutdown() {
    TrainwreckAPI::Shutdown();
    CrashLoggerIntegration::LogInjector::Shutdown();
    PostMortemCoordination::Coordinator::Shutdown();
    UnifiedCrashReport::ReportManager::Shutdown();
    CrashLoggerDetector::Detector::Shutdown();
    s_crashLoggerAvailable = false;
    s_trainwreckAvailable = false;
}

void UnifiedCrashReporter::ReportCrash(const VEH::CrashContext& context) {
    // Create unified crash report
    auto unifiedReport = UnifiedCrashReport::ReportManager::CreateReport(context);
    if (!unifiedReport) {
        auto log = spdlog::default_logger();
        if (log) log->error("[UnifiedCrashReporter] Failed to create unified crash report");
        return;
    }
    
    // Share with post-mortem plugin if available
    if (PostMortemCoordination::Coordinator::IsPostMortemAvailable()) {
        PostMortemCoordination::Coordinator::ShareUnifiedReport(*unifiedReport);
    }
    
    // Export to Trainwreck if available
    if (s_trainwreckAvailable) {
        // Use the enhanced API to provide extended crash information
        TrainwreckAPI::ProvideExtendedCrashInfo(context);
        
        // Export unified report data in C-compatible format
        auto cExportData = UnifiedCrashReport::ReportManager::ExportToCFormat(*unifiedReport);
        for (const auto& data : cExportData) {
            TrainwreckAPI::AddCrashInfo(data.key, data.value);
        }
        
        // Add unified report as a section
        std::string reportText = UnifiedCrashReport::ReportManager::GenerateReportText(*unifiedReport);
        TrainwreckAPI::AddSection("CrashGuard Unified Report", reportText);
    }
    
    // Inject into CrashLogger if available
    if (s_crashLoggerAvailable) {
        // Inject warning header first
        CrashLoggerIntegration::LogInjector::InjectWarningHeader();
        
        // Inject crash context
        CrashLoggerIntegration::LogInjector::InjectCrashContext(context);
        
        // Inject unified report
        std::string reportText = UnifiedCrashReport::ReportManager::GenerateReportText(*unifiedReport);
        CrashLoggerIntegration::LogInjector::CreateSeparateSection("Unified Crash Report", reportText);
    }
}

void UnifiedCrashReporter::ReportRecovery(const std::string& strategy, bool success,
                                         const std::vector<std::string>& actions) {
    // Export to Trainwreck if available
    if (s_trainwreckAvailable) {
        TrainwreckAPI::ExportRecoveryActions(actions, strategy, success);
    }
    
    // Inject into CrashLogger if available
    if (s_crashLoggerAvailable) {
        CrashLoggerIntegration::RecoveryInfo recovery;
        recovery.strategy = strategy;
        recovery.success = success;
        recovery.actions = actions;
        
        // Format timestamp
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
        recovery.timestamp = ss.str();
        
        CrashLoggerIntegration::LogInjector::AppendRecoveryInfo(recovery);
    }
}

bool UnifiedCrashReporter::IsCrashLoggerAvailable() {
    return s_crashLoggerAvailable;
}

bool UnifiedCrashReporter::IsTrainwreckAvailable() {
    return s_trainwreckAvailable;
}

std::string UnifiedCrashReporter::GetStatusString() {
    std::string status = "CrashGuard (VEH)";
    if (s_crashLoggerAvailable) status += " + CrashLogger";
    if (s_trainwreckAvailable) status += " + Trainwreck";
    return status;
}

std::vector<CrashLoggerDetector::LoggerInfo> UnifiedCrashReporter::GetDetectedLoggers() {
    return CrashLoggerDetector::Detector::GetDetectedLoggers();
}

}  // namespace TrainwreckBridge
