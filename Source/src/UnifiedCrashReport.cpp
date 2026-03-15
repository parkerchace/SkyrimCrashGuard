// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "UnifiedCrashReport.h"
#include "VEH.h"
#include "CrashLoggerDetector.h"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <fmt/chrono.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>

namespace UnifiedCrashReport {

// Static member initialization
bool ReportManager::s_initialized = false;
std::string ReportManager::s_formatVersion = "1.0.0";

std::vector<CExportData> ReportManager::ExportToCFormat(const UnifiedReport& report) {
    std::vector<CExportData> exportData;
    
    auto addExport = [&](const std::string& key, const std::string& value) {
        CExportData data;
        data.key = key.c_str();
        data.value = value.c_str();
        data.source = "VEH";
        exportData.push_back(data);
    };
    
    // Core crash information
    addExport("ExceptionCode", fmt::format("{:#x}", report.exceptionCode));
    addExport("CrashAddress", fmt::format("{:#x}", report.crashAddress));
    addExport("Severity", report.severity);
    addExport("RootCause", report.rootCause);
    addExport("Confidence", fmt::format("{:.2f}", report.confidence));
    
    // Recovery information
    if (!report.recoveryActions.empty()) {
        const auto& lastRecovery = report.recoveryActions.back();
        addExport("LastRecovery.Strategy", lastRecovery.strategy);
        addExport("LastRecovery.Success", lastRecovery.success ? "true" : "false");
    }
    
    // System information
    addExport("SkyrimVersion", report.systemInfo.skyrimVersion);
    addExport("SKSEVersion", report.systemInfo.skseVersion);
    addExport("CrashGuardVersion", report.systemInfo.crashGuardVersion);
    
    // Metadata
    for (const auto& [key, value] : report.metadata) {
        addExport("Metadata." + key, value);
    }
    
    return exportData;
}

std::string ReportManager::GenerateReportText(const UnifiedReport& report) {
    std::stringstream ss;
    
    // Header
    ss << "================================================================================\n";
    ss << "UNIFIED CRASH REPORT\n";
    ss << "================================================================================\n";
    ss << "Report ID:      " << report.reportID << "\n";
    ss << "Timestamp:      " << report.timestamp << "\n";
    ss << "Format Version: " << s_formatVersion << "\n";
    ss << "Primary Source: " << (report.primarySource == DataSource::VEH ? "VEH" : "External") << "\n";
    ss << "\n";
    
    // Quick Summary
    ss << "QUICK SUMMARY\n";
    ss << "--------------------------------------------------------------------------------\n";
    ss << "Exception Code: " << fmt::format("{:#x}", report.exceptionCode) << "\n";
    ss << "Crash Address:  " << fmt::format("{:#x}", report.crashAddress) << "\n";
    ss << "Severity:       " << report.severity << "\n";
    ss << "Root Cause:     " << report.rootCause << "\n";
    ss << "Confidence:     " << fmt::format("{:.1f}%", report.confidence * 100.0f) << "\n";
    ss << "\n";
    
    // Recovery Information
    if (report.recoveryAttempted) {
        ss << "RECOVERY INFORMATION\n";
        ss << "--------------------------------------------------------------------------------\n";
        ss << "Recovery Attempted: Yes\n";
        ss << "Recovery Success:   " << (report.recoverySuccessful ? "Yes" : "No") << "\n";
        
        for (const auto& recovery : report.recoveryActions) {
            ss << "\nStrategy: " << recovery.strategy << "\n";
            ss << "Success:  " << (recovery.success ? "Yes" : "No") << "\n";
            ss << "Time:     " << recovery.timestamp << "\n";
            
            if (!recovery.actions.empty()) {
                ss << "Actions:\n";
                for (size_t i = 0; i < recovery.actions.size(); ++i) {
                    ss << "  " << (i + 1) << ". " << recovery.actions[i] << "\n";
                }
            }
            
            if (!recovery.failureReason.empty()) {
                ss << "Failure Reason: " << recovery.failureReason << "\n";
            }
        }
        ss << "\n";
    }
    
    // System Information
    ss << "SYSTEM INFORMATION\n";
    ss << "--------------------------------------------------------------------------------\n";
    ss << "Skyrim Version:     " << report.systemInfo.skyrimVersion << "\n";
    ss << "SKSE Version:       " << report.systemInfo.skseVersion << "\n";
    ss << "CrashGuard Version: " << report.systemInfo.crashGuardVersion << "\n";
    ss << "OS Version:         " << report.systemInfo.osVersion << "\n";
    ss << "\n";
    
    // Call Stack
    if (!report.callStack.empty()) {
        ss << "CALL STACK\n";
        ss << "--------------------------------------------------------------------------------\n";
        for (size_t i = 0; i < report.callStack.size() && i < 32; ++i) {
            const auto& frame = report.callStack[i];
            ss << fmt::format("[{:2}] {:#x} {}+{:#x} {}\n",
                             i,
                             reinterpret_cast<uintptr_t>(frame.address),
                             frame.moduleName,
                             frame.offset,
                             frame.functionName);
        }
        ss << "\n";
    }
    
    // CPU Registers
    ss << "CPU REGISTERS\n";
    ss << "--------------------------------------------------------------------------------\n";
    ss << fmt::format("RIP: {:#x}  RSP: {:#x}\n", report.cpuRegisters.Rip, report.cpuRegisters.Rsp);
    ss << fmt::format("RAX: {:#x}  RCX: {:#x}\n", report.cpuRegisters.Rax, report.cpuRegisters.Rcx);
    ss << fmt::format("RDX: {:#x}  RBX: {:#x}\n", report.cpuRegisters.Rdx, report.cpuRegisters.Rbx);
    ss << "\n";
    
    // Game State
    if (!report.currentCell.empty() || !report.playerPosition.empty()) {
        ss << "GAME STATE\n";
        ss << "--------------------------------------------------------------------------------\n";
        if (!report.currentCell.empty()) {
            ss << "Current Cell: " << report.currentCell << "\n";
        }
        if (!report.playerPosition.empty()) {
            ss << "Player Position: " << report.playerPosition << "\n";
        }
        ss << "\n";
    }
    
    // Additional Sections
    for (const auto& section : report.sections) {
        ss << section.name << "\n";
        ss << "--------------------------------------------------------------------------------\n";
        ss << section.content << "\n";
    }
    
    ss << "================================================================================\n";
    ss << "END OF UNIFIED CRASH REPORT\n";
    ss << "================================================================================\n";
    
    return ss.str();
}

std::string ReportManager::GenerateJSONReport(const UnifiedReport& report) {
    nlohmann::json j;
    
    // Core information
    j["reportID"] = report.reportID;
    j["timestamp"] = report.timestamp;
    j["formatVersion"] = s_formatVersion;
    j["primarySource"] = static_cast<int>(report.primarySource);
    
    // Crash information
    j["crash"]["exceptionCode"] = fmt::format("{:#x}", report.exceptionCode);
    j["crash"]["crashAddress"] = fmt::format("{:#x}", report.crashAddress);
    j["crash"]["severity"] = report.severity;
    j["crash"]["rootCause"] = report.rootCause;
    j["crash"]["confidence"] = report.confidence;
    
    // CPU registers
    j["registers"]["RIP"] = fmt::format("{:#x}", report.cpuRegisters.Rip);
    j["registers"]["RSP"] = fmt::format("{:#x}", report.cpuRegisters.Rsp);
    j["registers"]["RAX"] = fmt::format("{:#x}", report.cpuRegisters.Rax);
    j["registers"]["RCX"] = fmt::format("{:#x}", report.cpuRegisters.Rcx);
    j["registers"]["RDX"] = fmt::format("{:#x}", report.cpuRegisters.Rdx);
    j["registers"]["RBX"] = fmt::format("{:#x}", report.cpuRegisters.Rbx);
    
    // Call stack
    j["callStack"] = nlohmann::json::array();
    for (const auto& frame : report.callStack) {
        nlohmann::json frameJson;
        frameJson["address"] = fmt::format("{:#x}", reinterpret_cast<uintptr_t>(frame.address));
        frameJson["moduleName"] = frame.moduleName;
        frameJson["functionName"] = frame.functionName;
        frameJson["offset"] = frame.offset;
        j["callStack"].push_back(frameJson);
    }
    
    // Recovery information
    j["recovery"]["attempted"] = report.recoveryAttempted;
    j["recovery"]["successful"] = report.recoverySuccessful;
    j["recovery"]["actions"] = nlohmann::json::array();
    
    for (const auto& recovery : report.recoveryActions) {
        nlohmann::json recoveryJson;
        recoveryJson["strategy"] = recovery.strategy;
        recoveryJson["success"] = recovery.success;
        recoveryJson["timestamp"] = recovery.timestamp;
        recoveryJson["actions"] = recovery.actions;
        if (!recovery.failureReason.empty()) {
            recoveryJson["failureReason"] = recovery.failureReason;
        }
        j["recovery"]["actions"].push_back(recoveryJson);
    }
    
    // System information
    j["system"]["skyrimVersion"] = report.systemInfo.skyrimVersion;
    j["system"]["skseVersion"] = report.systemInfo.skseVersion;
    j["system"]["crashGuardVersion"] = report.systemInfo.crashGuardVersion;
    j["system"]["osVersion"] = report.systemInfo.osVersion;
    
    // Game state
    if (!report.currentCell.empty()) {
        j["gameState"]["currentCell"] = report.currentCell;
    }
    if (!report.playerPosition.empty()) {
        j["gameState"]["playerPosition"] = report.playerPosition;
    }
    
    // Involved object
    if (report.involvedObject.has_value()) {
        const auto& obj = report.involvedObject.value();
        j["involvedObject"]["type"] = obj.type;
        j["involvedObject"]["formID"] = obj.formID;
        j["involvedObject"]["editorID"] = obj.editorID;
        j["involvedObject"]["modName"] = obj.modName;
        j["involvedObject"]["address"] = fmt::format("{:#x}", obj.address);
        j["involvedObject"]["isValid"] = obj.isValid;
    }
    
    // Sections
    j["sections"] = nlohmann::json::array();
    for (const auto& section : report.sections) {
        nlohmann::json sectionJson;
        sectionJson["name"] = section.name;
        sectionJson["content"] = section.content;
        sectionJson["source"] = static_cast<int>(section.source);
        sectionJson["timestamp"] = section.timestamp;
        sectionJson["metadata"] = section.metadata;
        j["sections"].push_back(sectionJson);
    }
    
    // Metadata
    j["metadata"] = report.metadata;
    
    return j.dump(2);
}

// ═══════════════════════════════════════════════════════════════════════
// § 3  Private Implementation
// ═══════════════════════════════════════════════════════════════════════

void ReportManager::ConvertVEHContext(const VEH::CrashContext& context, UnifiedReport& report) {
    // Core crash information
    report.exceptionCode = context.exceptionCode;
    report.crashAddress = reinterpret_cast<uintptr_t>(context.crashAddress);
    report.severity = SeverityToString(context.severity);
    report.rootCause = context.rootCause;
    report.confidence = 1.0f; // VEH data is primary source
    
    // Call stack and registers
    report.callStack = context.callStack;
    report.cpuRegisters = context.cpuContext;
    
    // Involved object (if available)
    if (context.involvedObject) {
        GameObjectData objData;
        objData.address = reinterpret_cast<uintptr_t>(context.involvedObject);
        objData.isValid = true;
        // Additional object data would be filled by GameObjectIntrospector
        report.involvedObject = objData;
    }
    
    // Initialize recovery state
    report.recoveryAttempted = false;
    report.recoverySuccessful = false;
}

SystemInfo ReportManager::GatherSystemInfo() {
    SystemInfo info;
    
    // Get detected crash loggers
    info.activeCrashLoggers = CrashLoggerDetector::Detector::GetDetectedLoggers();
    
    // Get actual Skyrim version from CommonLibSSE
    auto& module = REL::Module::get();
    auto version = module.version();
    info.skyrimVersion = fmt::format("{}.{}.{}.{}", 
        version.major(), version.minor(), version.patch(), version.build());
    
    // Get SKSE version from runtime info
    auto skseVersion = SKSE::PluginDeclaration::GetSingleton()->GetVersion();
    info.skseVersion = fmt::format("{}.{}.{}", 
        skseVersion.major(), skseVersion.minor(), skseVersion.patch());
    
    // CrashGuard version from Config
    info.crashGuardVersion = "2.3.2";
    
    // Get Windows version using RtlGetVersion
    OSVERSIONINFOEXW osInfo = {};
    osInfo.dwOSVersionInfoSize = sizeof(osInfo);
    
    // Use RtlGetVersion for accurate version info (GetVersionEx is deprecated)
    typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    auto ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        auto RtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(
            GetProcAddress(ntdll, "RtlGetVersion"));
        if (RtlGetVersion) {
            RtlGetVersion(reinterpret_cast<PRTL_OSVERSIONINFOW>(&osInfo));
            
            // Determine Windows version
            if (osInfo.dwMajorVersion == 10 && osInfo.dwBuildNumber >= 22000) {
                info.osVersion = fmt::format("Windows 11 (Build {})", osInfo.dwBuildNumber);
            } else if (osInfo.dwMajorVersion == 10) {
                info.osVersion = fmt::format("Windows 10 (Build {})", osInfo.dwBuildNumber);
            } else {
                info.osVersion = fmt::format("Windows {}.{} (Build {})", 
                    osInfo.dwMajorVersion, osInfo.dwMinorVersion, osInfo.dwBuildNumber);
            }
        } else {
            info.osVersion = "Windows (version unknown)";
        }
    } else {
        info.osVersion = "Windows (version unknown)";
    }
    
    return info;
}

std::string ReportManager::GenerateReportID() {
    // Generate unique report ID using timestamp + random component
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&timeT);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    return fmt::format("CG-{:04d}{:02d}{:02d}{:02d}{:02d}{:02d}-{:04d}", 
                      tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                      tm.tm_hour, tm.tm_min, tm.tm_sec, dis(gen));
}

std::string ReportManager::FormatTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string ReportManager::SeverityToString(VEH::SeverityLevel severity) {
    switch (severity) {
    case VEH::SeverityLevel::Safe: return "Safe";
    case VEH::SeverityLevel::Warning: return "Warning";
    case VEH::SeverityLevel::Critical: return "Critical";
    case VEH::SeverityLevel::Fatal: return "Fatal";
    default: return "Unknown";
    }
}

bool ReportManager::Initialize() {
    if (s_initialized) {
        return true;
    }
    
    s_initialized = true;
    spdlog::info("[UnifiedCrashReport] Report manager initialized");
    return true;
}

void ReportManager::Shutdown() {
    if (!s_initialized) {
        return;
    }
    
    s_initialized = false;
    spdlog::info("[UnifiedCrashReport] Report manager shut down");
}

std::shared_ptr<UnifiedReport> ReportManager::CreateReport(const VEH::CrashContext& context) {
    auto report = std::make_shared<UnifiedReport>();
    
    // Convert VEH context to unified format
    ConvertVEHContext(context, *report);
    
    // Generate report metadata
    report->reportID = GenerateReportID();
    report->timestamp = FormatTimestamp();
    report->primarySource = DataSource::VEH;
    
    // Gather system information
    report->systemInfo = GatherSystemInfo();
    
    return report;
}

void ReportManager::MergeCrashLoggerData(std::shared_ptr<UnifiedReport> report,
                                        const std::string& crashLoggerData) {
    if (!report) return;
    
    ReportSection section;
    section.name = "CrashLogger Data";
    section.content = crashLoggerData;
    section.source = DataSource::CrashLogger;
    section.timestamp = FormatTimestamp();
    
    report->sections.push_back(section);
}

void ReportManager::MergeTrainwreckData(std::shared_ptr<UnifiedReport> report,
                                       const std::unordered_map<std::string, std::string>& trainwreckData) {
    if (!report) return;
    
    for (const auto& [key, value] : trainwreckData) {
        report->metadata[key] = value;
    }
}

void ReportManager::AddRecoveryInfo(std::shared_ptr<UnifiedReport> report,
                                   const RecoveryAction& recovery) {
    if (!report) return;
    
    report->recoveryActions.push_back(recovery);
    report->recoveryAttempted = true;
    if (recovery.success) {
        report->recoverySuccessful = true;
    }
}

void ReportManager::AddSection(std::shared_ptr<UnifiedReport> report,
                              const std::string& name, const std::string& content,
                              DataSource source) {
    if (!report) return;
    
    ReportSection section;
    section.name = name;
    section.content = content;
    section.source = source;
    section.timestamp = FormatTimestamp();
    
    report->sections.push_back(section);
}

bool ReportManager::ValidateVersionCompatibility() {
    // Always compatible for now
    return true;
}

std::string ReportManager::GetFormatVersion() {
    return s_formatVersion;
}

}  // namespace UnifiedCrashReport