// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

// ═══════════════════════════════════════════════════════════════════════
// CrashLoggerIntegration.cpp — CrashLogger Log Injection and Coordination
// ═══════════════════════════════════════════════════════════════════════
//
// Purpose: Inject CrashGuard warning headers and recovery information into
// CrashLogger output files while coordinating to avoid duplicate logging.
// ═══════════════════════════════════════════════════════════════════════

#include "CrashLoggerIntegration.h"
#include "VEH.h"

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <shlobj.h>  // For SHGetKnownFolderPath

namespace CrashLoggerIntegration {

// ═══════════════════════════════════════════════════════════════════════
// § 1  Static Members
// ═══════════════════════════════════════════════════════════════════════

bool LogInjector::s_initialized = false;
bool LogInjector::s_preventDuplicateLogging = true;
std::filesystem::path LogInjector::s_logDirectory;
std::vector<LogFileInfo> LogInjector::s_processedLogs;

// ═══════════════════════════════════════════════════════════════════════
// § 2  Public Interface
// ═══════════════════════════════════════════════════════════════════════

bool LogInjector::Initialize() {
    if (s_initialized) return true;
    
    auto log = spdlog::default_logger();
    
    // Determine CrashLogger log directory
    // CrashLogger typically writes to Documents/My Games/Skyrim Special Edition/SKSE/
    std::filesystem::path documentsPath;
    
    // Get Documents folder
    PWSTR documentsPathPtr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documentsPathPtr))) {
        documentsPath = documentsPathPtr;
        CoTaskMemFree(documentsPathPtr);
    } else {
        if (log) log->warn("[CrashLoggerIntegration] Failed to get Documents folder");
        return false;
    }
    
    // Build CrashLogger log path
    s_logDirectory = documentsPath / "My Games" / "Skyrim Special Edition" / "SKSE";
    
    // Check if directory exists
    if (!std::filesystem::exists(s_logDirectory)) {
        if (log) log->info("[CrashLoggerIntegration] CrashLogger log directory not found: {}", 
                          s_logDirectory.string());
        return false;
    }
    
    s_initialized = true;
    
    if (log) {
        log->info("[CrashLogger] Integration active - Log dir: {}, Duplicate prevention: {}",
                  s_logDirectory.filename().string(),
                  s_preventDuplicateLogging ? "On" : "Off");
    }
    
    return true;
}

void LogInjector::Shutdown() {
    if (!s_initialized) return;
    
    s_processedLogs.clear();
    s_initialized = false;
    
    auto log = spdlog::default_logger();
    if (log) log->info("[CrashLoggerIntegration] Shutdown complete");
}

bool LogInjector::IsCrashLoggerActive() {
    return s_initialized && std::filesystem::exists(s_logDirectory);
}

bool LogInjector::InjectWarningHeader() {
    if (!IsCrashLoggerActive()) return false;
    
    auto latestLog = FindLatestLogFile();
    if (!latestLog.has_value()) {
        auto log = spdlog::default_logger();
        if (log) log->debug("[CrashLoggerIntegration] No CrashLogger log file found for header injection");
        return false;
    }
    
    // Check if already injected
    if (HasExistingInjection(latestLog->filePath)) {
        return true; // Already injected
    }
    
    std::string header = CreateWarningHeader();
    bool success = InjectIntoLogFile(latestLog->filePath, header, true);
    
    if (success) {
        // Mark as processed
        LogFileInfo info = latestLog.value();
        info.hasInjection = true;
        s_processedLogs.push_back(info);
        
        auto log = spdlog::default_logger();
        if (log) log->info("[CrashLoggerIntegration] Warning header injected into CrashLogger log");
    }
    
    return success;
}

bool LogInjector::AppendRecoveryInfo(const RecoveryInfo& recovery) {
    if (!IsCrashLoggerActive()) return false;
    
    auto latestLog = FindLatestLogFile();
    if (!latestLog.has_value()) return false;
    
    std::string recoverySection = CreateRecoverySection(recovery);
    bool success = InjectIntoLogFile(latestLog->filePath, recoverySection, false);
    
    if (success) {
        auto log = spdlog::default_logger();
        if (log) log->info("[CrashLoggerIntegration] Recovery information appended to CrashLogger log");
    }
    
    return success;
}

bool LogInjector::InjectCrashContext(const VEH::CrashContext& context) {
    if (!IsCrashLoggerActive()) return false;
    
    auto latestLog = FindLatestLogFile();
    if (!latestLog.has_value()) return false;
    
    std::string contextSection = CreateCrashContextSection(context);
    bool success = InjectIntoLogFile(latestLog->filePath, contextSection, false);
    
    if (success) {
        auto log = spdlog::default_logger();
        if (log) log->info("[CrashLoggerIntegration] Crash context injected into CrashLogger log");
    }
    
    return success;
}

void LogInjector::SetDuplicateLoggingPrevention(bool enabled) {
    s_preventDuplicateLogging = enabled;
    
    auto log = spdlog::default_logger();
    if (log) {
        log->info("[CrashLoggerIntegration] Duplicate logging prevention: {}", 
                  enabled ? "Enabled" : "Disabled");
    }
}

bool LogInjector::CreateSeparateSection(const std::string& sectionName, 
                                       const std::string& content) {
    if (!IsCrashLoggerActive()) return false;
    
    auto latestLog = FindLatestLogFile();
    if (!latestLog.has_value()) return false;
    
    // Create separated section with clear boundaries
    std::string section = fmt::format(
        "\n"
        "================================================================================\n"
        "CRASHGUARD SECTION: {}\n"
        "================================================================================\n"
        "{}\n"
        "================================================================================\n"
        "END CRASHGUARD SECTION: {}\n"
        "================================================================================\n\n",
        sectionName, content, sectionName
    );
    
    return InjectIntoLogFile(latestLog->filePath, section, false);
}

std::optional<LogFileInfo> LogInjector::FindLatestLogFile() {
    if (!std::filesystem::exists(s_logDirectory)) return std::nullopt;
    
    std::optional<LogFileInfo> latestLog;
    std::filesystem::file_time_type latestTime{};
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(s_logDirectory)) {
            if (!entry.is_regular_file()) continue;
            
            const auto& path = entry.path();
            std::string filename = path.filename().string();
            
            // Look for CrashLogger log files (typically crash-*.log or similar)
            if (filename.find("crash") != std::string::npos && 
                path.extension() == ".log") {
                
                auto modTime = GetFileModTime(path);
                if (modTime.has_value() && 
                    (!latestLog.has_value() || modTime.value() > latestTime)) {
                    
                    LogFileInfo info;
                    info.filePath = path;
                    info.originalSize = std::filesystem::file_size(path);
                    info.hasInjection = HasExistingInjection(path);
                    
                    // Format timestamp
                    auto timeT = std::chrono::system_clock::to_time_t(
                        std::chrono::clock_cast<std::chrono::system_clock>(modTime.value()));
                    std::stringstream ss;
                    ss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
                    info.timestamp = ss.str();
                    
                    latestLog = info;
                    latestTime = modTime.value();
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        auto log = spdlog::default_logger();
        if (log) log->warn("[CrashLoggerIntegration] Error scanning log directory: {}", e.what());
        return std::nullopt;
    }
    
    return latestLog;
}

std::filesystem::path LogInjector::GetLogDirectory() {
    return s_logDirectory;
}

// ═══════════════════════════════════════════════════════════════════════
// § 3  Private Implementation
// ═══════════════════════════════════════════════════════════════════════

bool LogInjector::InjectIntoLogFile(const std::filesystem::path& logFile,
                                   const std::string& content,
                                   bool atBeginning) {
    try {
        if (atBeginning) {
            // Read existing content
            std::ifstream inFile(logFile, std::ios::binary);
            if (!inFile) return false;
            
            std::string existingContent((std::istreambuf_iterator<char>(inFile)),
                                       std::istreambuf_iterator<char>());
            inFile.close();
            
            // Write new content + existing content
            std::ofstream outFile(logFile, std::ios::binary | std::ios::trunc);
            if (!outFile) return false;
            
            outFile << content << existingContent;
            outFile.close();
        } else {
            // Append to end
            std::ofstream outFile(logFile, std::ios::binary | std::ios::app);
            if (!outFile) return false;
            
            outFile << content;
            outFile.close();
        }
        
        return true;
    } catch (const std::exception& e) {
        auto log = spdlog::default_logger();
        if (log) log->error("[CrashLoggerIntegration] Failed to inject into log file: {}", e.what());
        return false;
    }
}

std::string LogInjector::CreateWarningHeader() {
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&timeT);

    return fmt::format(
        "================================================================================\n"
        "NOTE: SkyrimCrashGuard (experimental) was also installed during this session.\n"
        "Timestamp: {:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}\n"
        "\n"
        "CrashGuard attempted to recover this crash via its 6-layer VEH chain before\n"
        "CrashLogger received it. All recovery layers (L1, L1b, L2, L3, L4, L5, L6)\n"
        "were exhausted. CrashGuard may have modified register state during those attempts.\n"
        "\n"
        "Read this crash log with a grain of salt. Cross-reference SkyrimCrashGuard.log\n"
        "(same SKSE folder) for what CrashGuard intercepted and tried during this session.\n"
        "================================================================================\n\n",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec
    );
}

std::string LogInjector::CreateRecoverySection(const RecoveryInfo& recovery) {
    std::string actionsText;
    for (size_t i = 0; i < recovery.actions.size(); ++i) {
        actionsText += fmt::format("  {}. {}\n", i + 1, recovery.actions[i]);
    }
    
    return fmt::format(
        "\n"
        "================================================================================\n"
        "CRASHGUARD RECOVERY INFORMATION\n"
        "================================================================================\n"
        "Recovery Strategy: {}\n"
        "Recovery Success:  {}\n"
        "Root Cause:        {}\n"
        "Severity Level:    {}\n"
        "Recovery Time:     {}\n"
        "\n"
        "Actions Performed:\n"
        "{}"
        "================================================================================\n\n",
        recovery.strategy,
        recovery.success ? "YES" : "NO",
        recovery.rootCause.empty() ? "Unknown" : recovery.rootCause,
        recovery.severity.empty() ? "Unknown" : recovery.severity,
        recovery.timestamp,
        actionsText
    );
}

std::string LogInjector::CreateCrashContextSection(const VEH::CrashContext& context) {
    std::string severityStr;
    switch (context.severity) {
    case VEH::SeverityLevel::Safe: severityStr = "Safe"; break;
    case VEH::SeverityLevel::Warning: severityStr = "Warning"; break;
    case VEH::SeverityLevel::Critical: severityStr = "Critical"; break;
    case VEH::SeverityLevel::Fatal: severityStr = "Fatal"; break;
    default: severityStr = "Unknown"; break;
    }
    
    std::string callStackText;
    for (size_t i = 0; i < context.callStack.size() && i < 10; ++i) {
        const auto& frame = context.callStack[i];
        callStackText += fmt::format("  [{:2}] {:#x} {}+{:#x} {}\n",
                                     i,
                                     reinterpret_cast<uintptr_t>(frame.address),
                                     frame.moduleName,
                                     frame.offset,
                                     frame.functionName);
    }
    
    return fmt::format(
        "\n"
        "================================================================================\n"
        "CRASHGUARD CRASH CONTEXT\n"
        "================================================================================\n"
        "Exception Code:    {:#x}\n"
        "Crash Address:     {:#x}\n"
        "Severity:          {}\n"
        "Root Cause:        {}\n"
        "Involved Object:   {:#x}\n"
        "\n"
        "CPU Registers:\n"
        "  RIP: {:#x}  RSP: {:#x}\n"
        "  RAX: {:#x}  RCX: {:#x}\n"
        "  RDX: {:#x}  RBX: {:#x}\n"
        "\n"
        "Call Stack (Top 10):\n"
        "{}"
        "================================================================================\n\n",
        context.exceptionCode,
        reinterpret_cast<uintptr_t>(context.crashAddress),
        severityStr,
        context.rootCause.empty() ? "Unknown" : context.rootCause,
        reinterpret_cast<uintptr_t>(context.involvedObject),
        context.cpuContext.Rip, context.cpuContext.Rsp,
        context.cpuContext.Rax, context.cpuContext.Rcx,
        context.cpuContext.Rdx, context.cpuContext.Rbx,
        callStackText
    );
}

bool LogInjector::HasExistingInjection(const std::filesystem::path& logFile) {
    try {
        std::ifstream file(logFile);
        if (!file) return false;
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("CRASHGUARD CRASH RECOVERY ACTIVE") != std::string::npos) {
                return true;
            }
        }
        
        return false;
    } catch (const std::exception&) {
        return false;
    }
}

std::optional<std::filesystem::file_time_type> LogInjector::GetFileModTime(
    const std::filesystem::path& file) {
    try {
        return std::filesystem::last_write_time(file);
    } catch (const std::filesystem::filesystem_error&) {
        return std::nullopt;
    }
}

}  // namespace CrashLoggerIntegration