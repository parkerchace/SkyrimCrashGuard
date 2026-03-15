// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

// ═══════════════════════════════════════════════════════════════════════
// DllMain.cpp — DLL Entry Point with Address Library Stub Injection
// ═══════════════════════════════════════════════════════════════════════
//
// This file handles DLL initialization BEFORE any static initializers run.
// It creates an in-memory stub address library to satisfy CommonLibSSE-NG
// without requiring external files.
//
// ═══════════════════════════════════════════════════════════════════════

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

/**
 * @brief Create a minimal valid address library file
 * 
 * Format version 2 (SSE v2) structure for AE/newer SE:
 * - format: int32 (2)
 * - version: int32[4] (game version)
 * - nameLen: int32 (length of game name string)
 * - name: char[nameLen] (game name)
 * - pointerSize: int32 (8 for x64)
 * - addressCount: int32 (0 - no addresses)
 */
bool CreateStubAddressLibrary(const std::filesystem::path& path, int major, int minor, int patch, int build) {
    try {
        // Create directory if it doesn't exist
        std::filesystem::create_directories(path.parent_path());
        
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }
        
        // Helper to write int32
        auto writeInt32 = [&file](int32_t value) {
            file.write(reinterpret_cast<const char*>(&value), sizeof(int32_t));
        };
        
        // Determine format version based on game version
        // SE 1.5.x uses format 1, AE 1.6.x uses format 2
        int32_t formatVersion = (major == 1 && minor == 5) ? 1 : 2;
        
        // Format version
        writeInt32(formatVersion);
        
        // Game version (4 int32s)
        writeInt32(major);
        writeInt32(minor);
        writeInt32(patch);
        writeInt32(build);
        
        // Game name
        const char* gameName = "SkyrimSE.exe";
        int32_t nameLen = static_cast<int32_t>(strlen(gameName));
        writeInt32(nameLen);
        file.write(gameName, nameLen);
        
        // Pointer size (8 for x64)
        writeInt32(8);
        
        // Address count (0 - empty database)
        writeInt32(0);
        
        file.close();
        return true;
        
    } catch (...) {
        return false;
    }
}

/**
 * @brief Detect Skyrim version and create appropriate stub file
 */
void EnsureAddressLibraryStub() {
    // Get the SKSE plugins directory
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    
    std::filesystem::path exePath(modulePath);
    std::filesystem::path dataPath = exePath.parent_path() / "Data";
    std::filesystem::path sksePath = dataPath / "SKSE" / "Plugins";
    
    // Try to detect game version from executable
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(modulePath, &handle);
    
    int major = 1, minor = 5, patch = 97, build = 0;
    
    if (size > 0) {
        std::vector<BYTE> versionInfo(size);
        if (GetFileVersionInfoW(modulePath, handle, size, versionInfo.data())) {
            VS_FIXEDFILEINFO* fileInfo = nullptr;
            UINT len = 0;
            if (VerQueryValueW(versionInfo.data(), L"\\", reinterpret_cast<LPVOID*>(&fileInfo), &len)) {
                major = HIWORD(fileInfo->dwFileVersionMS);
                minor = LOWORD(fileInfo->dwFileVersionMS);
                patch = HIWORD(fileInfo->dwFileVersionLS);
                build = LOWORD(fileInfo->dwFileVersionLS);
            }
        }
    }
    
    // Determine filename based on version
    std::filesystem::path stubPath;
    if (major == 1 && minor == 5) {
        // Skyrim SE 1.5.x
        stubPath = sksePath / std::format("version-{}-{}-{}-{}.bin", major, minor, patch, build);
    } else if (major == 1 && minor == 6) {
        // Skyrim AE 1.6.x
        stubPath = sksePath / std::format("versionlib-{}-{}-{}-{}.bin", major, minor, patch, build);
    } else {
        // Unknown version, try common names
        stubPath = sksePath / "version-1-5-97-0.bin";
    }
    
    // Check if file already exists
    if (std::filesystem::exists(stubPath)) {
        return; // Already have an address library file
    }
    
    // Create the stub file
    CreateStubAddressLibrary(stubPath, major, minor, patch, build);
}

} // anonymous namespace

/**
 * @brief DLL entry point
 * 
 * This runs BEFORE any static initializers, giving us a chance to
 * create the stub address library file before CommonLibSSE-NG tries
 * to load it.
 */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        // Create stub address library before anything else runs
        EnsureAddressLibraryStub();
    }
    
    return TRUE;
}
