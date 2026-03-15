// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "CrashCollector.h"
#include "GameDetect.h"

#include <spdlog/spdlog.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#include <shlobj.h>          // SHGetKnownFolderPath, FOLDERID_Documents

#include <fstream>
#include <format>
#include <chrono>
#include <algorithm>
#include <filesystem>

namespace CrashCollector {

    static std::mutex              s_mutex;
    static std::vector<CrashRecord> s_records;
    static std::string             s_outputPath;

    static std::string GetModuleName(uintptr_t addr)
    {
        HMODULE hMod = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(addr), &hMod) && hMod) {
            char name[MAX_PATH];
            if (GetModuleFileNameA(hMod, name, MAX_PATH)) {
                std::string path(name);
                auto pos = path.find_last_of("\\/");
                return (pos != std::string::npos) ? path.substr(pos + 1) : path;
            }
        }
        return "unknown";
    }

    static uintptr_t GetModuleOffset(uintptr_t addr)
    {
        HMODULE hMod = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(addr), &hMod) && hMod) {
            return addr - reinterpret_cast<uintptr_t>(hMod);
        }
        return addr;
    }

    static std::string NowTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_s(&tm, &time);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        return buf;
    }

    void Init()
    {
        // Discover log directory: Documents/My Games/{game}/SKSE/
        const auto& game = GameDetect::Detect();
        PWSTR docsPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docsPath))) {
            std::filesystem::path logDir = std::filesystem::path(docsPath)
                / "My Games" / game.docsDir / "SKSE";
            CoTaskMemFree(docsPath);
            std::error_code ec;
            std::filesystem::create_directories(logDir, ec);
            s_outputPath = (logDir / "CrashPatterns.log").string();
        } else {
            if (docsPath) CoTaskMemFree(docsPath);
            s_outputPath = "CrashPatterns.log";
        }
    }

    // Internal flush (caller must hold s_mutex)
    static void FlushUnlocked()
    {
        if (s_records.empty()) return;

        std::ofstream out(s_outputPath, std::ios::app);
        if (!out.is_open()) return;

        out << "=== Crash Patterns - " << NowTimestamp() << " ===\n";
        out << std::format("{:<40} {:<18} {:<18} {:<8} {}\n",
                           "Module+Offset", "FaultAddr", "AccessAddr", "Hits", "FirstSeen");
        out << std::string(100, '-') << "\n";

        auto sorted = s_records;
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) { return a.hitCount > b.hitCount; });

        for (const auto& rec : sorted) {
            out << std::format("{:<40} {:#018x} {:#018x} {:<8} {}\n",
                               rec.moduleName + "+" + std::format("{:#x}", rec.moduleOffset),
                               rec.faultAddress, rec.accessAddress,
                               rec.hitCount, rec.timestamp);
        }
        out << "\n";
    }

    void Record(uintptr_t faultAddr, uintptr_t accessAddr, uint32_t code)
    {
        // Use try_lock: if another thread holds the lock (or we're
        // re-entering from VEH on the same thread), skip recording
        // rather than deadlocking.
        std::unique_lock lock(s_mutex, std::try_to_lock);
        if (!lock.owns_lock()) return;

        // Check if we already have this fault address
        for (auto& rec : s_records) {
            if (rec.faultAddress == faultAddr) {
                rec.hitCount++;
                return;
            }
        }

        // New crash site
        CrashRecord rec{};
        rec.faultAddress  = faultAddr;
        rec.accessAddress = accessAddr;
        rec.exceptionCode = code;
        rec.moduleName    = GetModuleName(faultAddr);
        rec.moduleOffset  = GetModuleOffset(faultAddr);
        rec.hitCount      = 1;
        rec.timestamp     = NowTimestamp();

        s_records.push_back(std::move(rec));

        auto log = spdlog::default_logger();
        if (log) {
            log->info("CrashCollector: new site {}+{:#x} (access {:#x}, code {:#x})",
                s_records.back().moduleName, s_records.back().moduleOffset,
                accessAddr, code);
        }

        // Persist immediately (we already hold the lock)
        FlushUnlocked();
    }

    void Flush()
    {
        std::lock_guard lock(s_mutex);

        FlushUnlocked();

        auto log = spdlog::default_logger();
        if (log) {
            log->info("CrashCollector: flushed {} records to {}",
                                       s_records.size(), s_outputPath);
        }
    }

    std::vector<CrashRecord> GetRecords()
    {
        std::lock_guard lock(s_mutex);
        return s_records;
    }

}  // namespace CrashCollector
