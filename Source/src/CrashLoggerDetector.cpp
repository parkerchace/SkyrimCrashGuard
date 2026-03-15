// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

// CrashLoggerDetector.cpp
// Crash Logger Detection and Compatibility
//
// Purpose: Detect installed crash loggers (CrashLogger, Trainwreck) and
// enable appropriate compatibility modes for coordinated crash reporting.
// ═══════════════════════════════════════════════════════════════════════

#include "CrashLoggerDetector.h"

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <filesystem>

namespace CrashLoggerDetector {

// ═══════════════════════════════════════════════════════════════════════
// § 1  Static Members
// ═══════════════════════════════════════════════════════════════════════

std::vector<LoggerInfo> Detector::s_detectedLoggers;
bool Detector::s_initialized = false;

// ═══════════════════════════════════════════════════════════════════════
// § 2  Public Interface
// ═══════════════════════════════════════════════════════════════════════

bool Detector::Initialize() {
    if (s_initialized) return true;
    
    auto log = spdlog::default_logger();
    if (log) log->info("[CrashLoggerDetector] Scanning for installed crash loggers...");
    
    // Clear previous detection results
    s_detectedLoggers.clear();
    
    // Detect all crash loggers
    auto detectedLoggers = DetectInstalledLoggers();
    s_detectedLoggers = std::move(detectedLoggers);
    
    // Enable compatibility modes
    EnableCompatibilityModes();
    
    s_initialized = true;
    
    // Log detection results
    if (log) {
        log->info("╔══════════════════════════════════════════════════╗");
        log->info("║  Crash Logger Detection Results                 ║");
        log->info("╠══════════════════════════════════════════════════╣");
        
        if (s_detectedLoggers.empty()) {
            log->info("║  No external crash loggers detected             ║");
            log->info("║  Running in standalone mode                     ║");
        } else {
            for (const auto& logger : s_detectedLoggers) {
                log->info("║  {:<15} {:<30} ║", 
                         logger.name + ":", 
                         logger.isActive ? "Active" : "Inactive");
                if (!logger.version.empty()) {
                    log->info("║    Version: {:<36} ║", logger.version);
                }
            }
        }
        
        log->info("╚══════════════════════════════════════════════════╝");
    }
    
    return true;
}

void Detector::Shutdown() {
    if (!s_initialized) return;
    
    s_detectedLoggers.clear();
    s_initialized = false;
    
    auto log = spdlog::default_logger();
    if (log) log->info("[CrashLoggerDetector] Shutdown complete");
}

std::vector<LoggerInfo> Detector::DetectInstalledLoggers() {
    std::vector<LoggerInfo> loggers;
    
    // Detect CrashLogger
    auto crashLogger = DetectCrashLogger();
    if (crashLogger.has_value()) {
        loggers.push_back(crashLogger.value());
    }
    
    // Detect Trainwreck
    auto trainwreck = DetectTrainwreck();
    if (trainwreck.has_value()) {
        loggers.push_back(trainwreck.value());
    }
    
    return loggers;
}

bool Detector::IsCrashLoggerPresent() {
    for (const auto& logger : s_detectedLoggers) {
        if (logger.type == LoggerType::CrashLogger && logger.isActive) {
            return true;
        }
    }
    return false;
}

bool Detector::IsTrainwreckPresent() {
    for (const auto& logger : s_detectedLoggers) {
        if (logger.type == LoggerType::Trainwreck && logger.isActive) {
            return true;
        }
    }
    return false;
}

std::optional<LoggerInfo> Detector::GetCrashLoggerInfo() {
    for (const auto& logger : s_detectedLoggers) {
        if (logger.type == LoggerType::CrashLogger) {
            return logger;
        }
    }
    return std::nullopt;
}

std::optional<LoggerInfo> Detector::GetTrainwreckInfo() {
    for (const auto& logger : s_detectedLoggers) {
        if (logger.type == LoggerType::Trainwreck) {
            return logger;
        }
    }
    return std::nullopt;
}

void Detector::EnableCompatibilityModes() {
    auto log = spdlog::default_logger();
    
    for (const auto& logger : s_detectedLoggers) {
        if (!logger.isActive) continue;
        
        switch (logger.type) {
        case LoggerType::CrashLogger:
            if (log) {
                log->info("[CrashLoggerDetector] Enabling CrashLogger compatibility mode");
                log->info("[CrashLoggerDetector] ┌─────────────────────────────────────────────────────┐");
                log->info("[CrashLoggerDetector] │  CrashLogger + CrashGuard: Cooperative Mode Active │");
                log->info("[CrashLoggerDetector] ├─────────────────────────────────────────────────────┤");
                log->info("[CrashLoggerDetector] │  CrashGuard intercepts crashes BEFORE CrashLogger. │");
                log->info("[CrashLoggerDetector] │  Recovered crashes → CrashGuard recovery reports   │");
                log->info("[CrashLoggerDetector] │  Unrecovered crashes → CrashLogger crash logs      │");
                log->info("[CrashLoggerDetector] │                                                     │");
                log->info("[CrashLoggerDetector] │  If CrashLogger writes a crash log, it means       │");
                log->info("[CrashLoggerDetector] │  CrashGuard could NOT recover that crash.           │");
                log->info("[CrashLoggerDetector] │  Check CrashGuard-recovery-*.log files for crashes │");
                log->info("[CrashLoggerDetector] │  that WERE recovered (game kept running).           │");
                log->info("[CrashLoggerDetector] └─────────────────────────────────────────────────────┘");
            }
            break;
            
        case LoggerType::Trainwreck:
            if (log) log->info("[CrashLoggerDetector] Enabling Trainwreck compatibility mode");
            // Trainwreck compatibility is already handled by TrainwreckBridge
            break;
            
        default:
            if (log) log->warn("[CrashLoggerDetector] Unknown logger type detected");
            break;
        }
    }
}

std::string Detector::GetDetectionStatusString() {
    if (s_detectedLoggers.empty()) {
        return "No external crash loggers detected";
    }
    
    std::string status = "Detected: ";
    bool first = true;
    
    for (const auto& logger : s_detectedLoggers) {
        if (!first) status += ", ";
        status += logger.name;
        if (logger.isActive) {
            status += " (Active)";
        } else {
            status += " (Inactive)";
        }
        first = false;
    }
    
    return status;
}

const std::vector<LoggerInfo>& Detector::GetDetectedLoggers() {
    return s_detectedLoggers;
}

// ═══════════════════════════════════════════════════════════════════════
// § 3  Private Implementation
// ═══════════════════════════════════════════════════════════════════════

std::optional<LoggerInfo> Detector::DetectCrashLogger() {
    // Try common CrashLogger DLL names
    const char* dllNames[] = {
        "CrashLogger.dll",
        "crashlogger.dll",
        "CrashLoggerSSE.dll"
    };
    
    for (const char* dllName : dllNames) {
        HMODULE module = GetModuleHandleA(dllName);
        if (module) {
            // Validate this is actually CrashLogger
            if (!ValidateLoggerModule(module, LoggerType::CrashLogger)) {
                continue;
            }
            
            LoggerInfo info;
            info.type = LoggerType::CrashLogger;
            info.name = "CrashLogger";
            info.version = GetDLLVersion(module);
            info.dllPath = GetModulePath(module);
            info.moduleHandle = module;
            info.isActive = true;
            
            auto log = spdlog::default_logger();
            if (log) {
                log->info("[CrashLoggerDetector] CrashLogger detected: {} ({})", 
                         dllName, info.version);
            }
            
            return info;
        }
    }
    
    return std::nullopt;
}

std::optional<LoggerInfo> Detector::DetectTrainwreck() {
    // Try common Trainwreck DLL names
    const char* dllNames[] = {
        "trainwreck.dll",
        "trainwreck_x64.dll",
        "Trainwreck.dll"
    };
    
    for (const char* dllName : dllNames) {
        HMODULE module = GetModuleHandleA(dllName);
        if (module) {
            // Validate this is actually Trainwreck
            if (!ValidateLoggerModule(module, LoggerType::Trainwreck)) {
                continue;
            }
            
            LoggerInfo info;
            info.type = LoggerType::Trainwreck;
            info.name = "Trainwreck";
            info.version = GetDLLVersion(module);
            info.dllPath = GetModulePath(module);
            info.moduleHandle = module;
            info.isActive = true;
            
            auto log = spdlog::default_logger();
            if (log) {
                log->info("[CrashLoggerDetector] Trainwreck detected: {} ({})", 
                         dllName, info.version);
            }
            
            return info;
        }
    }
    
    return std::nullopt;
}

std::string Detector::GetDLLVersion(HMODULE module) {
    if (!module) return "Unknown";
    
    // Get module file path
    char path[MAX_PATH];
    if (GetModuleFileNameA(module, path, MAX_PATH) == 0) {
        return "Unknown";
    }
    
    // Get version info size
    DWORD versionInfoSize = GetFileVersionInfoSizeA(path, nullptr);
    if (versionInfoSize == 0) {
        return "Unknown";
    }
    
    // Allocate buffer for version info
    std::vector<BYTE> versionInfo(versionInfoSize);
    if (!GetFileVersionInfoA(path, 0, versionInfoSize, versionInfo.data())) {
        return "Unknown";
    }
    
    // Get file version
    VS_FIXEDFILEINFO* fileInfo = nullptr;
    UINT fileInfoSize = 0;
    if (!VerQueryValueA(versionInfo.data(), "\\", 
                       reinterpret_cast<LPVOID*>(&fileInfo), &fileInfoSize)) {
        return "Unknown";
    }
    
    if (!fileInfo) return "Unknown";
    
    // Format version string
    DWORD major = HIWORD(fileInfo->dwFileVersionMS);
    DWORD minor = LOWORD(fileInfo->dwFileVersionMS);
    DWORD patch = HIWORD(fileInfo->dwFileVersionLS);
    DWORD build = LOWORD(fileInfo->dwFileVersionLS);
    
    return fmt::format("{}.{}.{}.{}", major, minor, patch, build);
}

std::string Detector::GetModulePath(HMODULE module) {
    if (!module) return "";
    
    char path[MAX_PATH];
    if (GetModuleFileNameA(module, path, MAX_PATH) == 0) {
        return "";
    }
    
    return std::string(path);
}

bool Detector::ValidateLoggerModule(HMODULE module, LoggerType type) {
    if (!module) return false;
    
    switch (type) {
    case LoggerType::CrashLogger:
        // CrashLogger typically doesn't export functions, just check if it's loaded
        return true;
        
    case LoggerType::Trainwreck:
        // Validate Trainwreck exports expected API functions
        return (GetProcAddress(module, "Trainwreck_RegisterPlugin") != nullptr &&
                GetProcAddress(module, "Trainwreck_AddCrashInfo") != nullptr);
    
    default:
        return false;
    }
}

}  // namespace CrashLoggerDetector