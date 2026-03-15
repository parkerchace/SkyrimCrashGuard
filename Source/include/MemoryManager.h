// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <cstdint>
#include <chrono>
#include <vector>
#include <unordered_set>
#include <functional>
#include <mutex>

namespace CrashGuard {

/**
 * @brief Manages memory resources and handles allocation failures
 * 
 * Implements memory resource tracking, allocation failure handling,
 * and memory usage monitoring with warnings.
 * 
 * Note: This system tracks resources but does not directly free them.
 * Actual memory management is handled by the game engine.
 */
class MemoryManager {
public:
    static MemoryManager& GetInstance();

    // Initialize memory monitoring
    bool Initialize();

    // Shutdown and cleanup
    void Shutdown();

    // Dynamic memory freeing
    void FreeDistantCellResources();
    void FreeUnusedTextures();
    void FreeCachedData();
    size_t FreeMemoryByPriority(size_t targetBytes);

    // Allocation failure handling
    void* HandleAllocationFailure(size_t requestedSize);
    bool AttemptResourceFreeing(size_t neededBytes);
    void RegisterAllocationHook();

    // Memory warnings
    void MonitorMemoryUsage();
    bool IsMemoryCriticallyLow();
    void WarnUserAboutMemory();
    void SuggestMemoryReduction();

    // Memory statistics
    struct MemoryStats {
        size_t totalPhysical;
        size_t availablePhysical;
        size_t totalVirtual;
        size_t availableVirtual;
        size_t processWorkingSet;
        size_t processPrivateBytes;
        float usagePercent;
        std::chrono::steady_clock::time_point lastUpdate;
    };

    MemoryStats GetMemoryStats();
    void UpdateMemoryStats();

private:
    MemoryManager() = default;
    ~MemoryManager() = default;
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    // Resource tracking
    struct ResourceInfo {
        void* address;
        size_t size;
        std::chrono::steady_clock::time_point lastAccessed;
        uint32_t accessCount;
        float priority; // Lower = more likely to free
    };

    // Cell resource tracking
    struct CellResourceInfo {
        uint32_t cellFormID;
        float distanceFromPlayer;
        std::vector<void*> resources;
        size_t totalSize;
    };

    // Texture tracking
    struct TextureInfo {
        void* texturePtr;
        std::string path;
        size_t size;
        std::chrono::steady_clock::time_point lastUsed;
        uint32_t referenceCount;
    };

    // Internal methods
    void CollectDistantCells(std::vector<CellResourceInfo>& outCells);
    void CollectUnusedTextures(std::vector<TextureInfo>& outTextures);
    void CollectCachedData(std::vector<ResourceInfo>& outResources);
    float CalculateResourcePriority(const ResourceInfo& resource);

    // Memory monitoring
    void CheckMemoryThresholds();
    bool ShouldWarnUser();
    void RecordMemoryWarning();

    // Configuration thresholds
    static constexpr size_t CRITICAL_MEMORY_THRESHOLD = 512 * 1024 * 1024; // 512 MB
    static constexpr size_t WARNING_MEMORY_THRESHOLD = 1024 * 1024 * 1024; // 1 GB
    static constexpr float CRITICAL_USAGE_PERCENT = 90.0f;
    static constexpr float WARNING_USAGE_PERCENT = 80.0f;
    static constexpr auto WARNING_COOLDOWN = std::chrono::minutes(5);
    static constexpr float DISTANT_CELL_THRESHOLD = 8192.0f; // 2 cells away

    // State
    MemoryStats m_currentStats{};
    std::mutex m_statsMutex;
    std::chrono::steady_clock::time_point m_lastWarningTime;
    uint32_t m_warningCount = 0;
    bool m_initialized = false;
    bool m_criticalMemoryMode = false;

    // Resource tracking
    std::vector<CellResourceInfo> m_trackedCells;
    std::vector<TextureInfo> m_trackedTextures;
    std::vector<ResourceInfo> m_cachedResources;
    std::mutex m_resourceMutex;
};

} // namespace CrashGuard
