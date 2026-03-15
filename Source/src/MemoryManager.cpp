#include "MemoryManager.h"
#include "Config.h"
#include <windows.h>
#include <psapi.h>
#include <algorithm>

namespace CrashGuard {

MemoryManager& MemoryManager::GetInstance() {
    static MemoryManager instance;
    return instance;
}

bool MemoryManager::Initialize() {
    if (m_initialized) {
        return true;
    }

    SKSE::log::info("Initializing MemoryManager...");

    // Update initial memory stats
    UpdateMemoryStats();

    m_lastWarningTime = std::chrono::steady_clock::now() - WARNING_COOLDOWN;
    m_warningCount = 0;
    m_criticalMemoryMode = false;

    m_initialized = true;
    SKSE::log::info("MemoryManager initialized successfully");
    return true;
}

void MemoryManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    SKSE::log::info("Shutting down MemoryManager...");

    std::lock_guard<std::mutex> lock(m_resourceMutex);
    m_trackedCells.clear();
    m_trackedTextures.clear();
    m_cachedResources.clear();

    m_initialized = false;
}

// ============================================================================
// Dynamic Memory Freeing
// ============================================================================

void MemoryManager::FreeDistantCellResources() {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_resourceMutex);

    std::vector<CellResourceInfo> distantCells;
    CollectDistantCells(distantCells);

    // Remove distant cell resources from tracking
    for (const auto& cellInfo : distantCells) {
        for (void* resource : cellInfo.resources) {
            // Remove from tracking only - actual freeing handled by game engine
            m_cachedResources.erase(
                std::remove_if(m_cachedResources.begin(), m_cachedResources.end(),
                    [resource](const ResourceInfo& info) {
                        return info.address == resource;
                    }),
                m_cachedResources.end());
        }
    }

    if (!distantCells.empty()) {
        SKSE::log::info("Removed {} distant cells from tracking", distantCells.size());
    }
}

void MemoryManager::FreeUnusedTextures() {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_resourceMutex);

    std::vector<TextureInfo> unusedTextures;
    CollectUnusedTextures(unusedTextures);

    // Remove unused textures from tracking
    for (const auto& texInfo : unusedTextures) {
        // Remove from tracking only - actual freeing handled by game engine
        m_cachedResources.erase(
            std::remove_if(m_cachedResources.begin(), m_cachedResources.end(),
                [&texInfo](const ResourceInfo& info) {
                    return info.address == texInfo.texturePtr;
                }),
            m_cachedResources.end());
    }

    if (!unusedTextures.empty()) {
        SKSE::log::info("Removed {} unused textures from tracking", unusedTextures.size());
    }
}

void MemoryManager::FreeCachedData() {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_resourceMutex);

    std::vector<ResourceInfo> cachedData;
    CollectCachedData(cachedData);

    // Remove cached data from tracking
    for (const auto& resource : cachedData) {
        // Remove from tracking only - actual freeing handled by game engine
        m_cachedResources.erase(
            std::remove_if(m_cachedResources.begin(), m_cachedResources.end(),
                [&resource](const ResourceInfo& info) {
                    return info.address == resource.address;
                }),
            m_cachedResources.end());
    }

    if (!cachedData.empty()) {
        SKSE::log::info("Removed {} cached resources from tracking", cachedData.size());
    }
}

size_t MemoryManager::FreeMemoryByPriority(size_t targetBytes) {
    if (!m_initialized) {
        return 0;
    }

    SKSE::log::info("Removing tracked resources by priority (target: {} bytes)", targetBytes);

    // Priority 1: Remove distant cell resources from tracking
    FreeDistantCellResources();
    UpdateMemoryStats();
    
    if (m_currentStats.availablePhysical >= targetBytes) {
        return 0;
    }

    // Priority 2: Remove unused textures from tracking
    FreeUnusedTextures();
    UpdateMemoryStats();
    
    if (m_currentStats.availablePhysical >= targetBytes) {
        return 0;
    }

    // Priority 3: Remove cached data from tracking
    FreeCachedData();
    UpdateMemoryStats();

    return 0;
}

// ============================================================================
// Allocation Failure Handling
// ============================================================================

void* MemoryManager::HandleAllocationFailure(size_t requestedSize) {
    if (!m_initialized) {
        return nullptr;
    }

    SKSE::log::warn("Allocation failure for {} bytes, attempting recovery", requestedSize);

    // Attempt to free memory
    if (AttemptResourceFreeing(requestedSize)) {
        // Try allocation again after freeing
        void* ptr = malloc(requestedSize);
        if (ptr) {
            SKSE::log::info("Allocation succeeded after freeing resources");
            return ptr;
        }
    }

    SKSE::log::error("Failed to allocate {} bytes even after freeing resources", requestedSize);
    return nullptr;
}

bool MemoryManager::AttemptResourceFreeing(size_t neededBytes) {
    if (!m_initialized) {
        return false;
    }

    SKSE::log::info("Attempting to free {} bytes of resources", neededBytes);

    // Add some overhead to ensure we have enough
    size_t targetBytes = neededBytes * 2;

    size_t freedBytes = FreeMemoryByPriority(targetBytes);

    UpdateMemoryStats();

    bool success = m_currentStats.availablePhysical >= neededBytes;
    if (success) {
        SKSE::log::info("Successfully freed enough memory for allocation");
    } else {
        SKSE::log::warn("Could not free enough memory for allocation");
    }

    return success;
}

void MemoryManager::RegisterAllocationHook() {
    // DISABLED: Allocation hook pending future implementation
    // 
    // Memory allocation hooks have been removed due to:
    // - VCRUNTIME140 crashes during C++ runtime initialization
    // - Reentrancy issues causing recursive allocation loops
    // - Extreme performance overhead (millions of allocations per second)
    // - Complexity of safely hooking low-level allocators
    //
    // See ALLOCATION_HOOK_ANALYSIS.md for full analysis of why this approach failed
    // See PERFORMANCE_MEMORY_STRATEGY.md for recommended alternatives:
    // - Object pooling for frequently allocated objects
    // - Frame budget systems to limit allocations per frame
    // - Cache-coherent data layout to reduce allocations
    // - Streaming and LOD systems to manage memory usage
    //
    // Current approach: MonitorMemoryUsage() called periodically from main loop
    // This provides system-level monitoring without allocation-level risks
    
    SKSE::log::info("[MemoryManager] Allocation hook disabled - using polling-based monitoring");
    SKSE::log::info("  Future implementation will focus on:");
    SKSE::log::info("  - Object pooling for NPCs, particles, effects");
    SKSE::log::info("  - Frame budget systems to prevent allocation spikes");
    SKSE::log::info("  - Cache-coherent data layout for better performance");
}

// ============================================================================
// Memory Warnings
// ============================================================================

void MemoryManager::MonitorMemoryUsage() {
    // Called periodically from main loop to monitor system memory
    // Initialized in main.cpp via MemoryManager::GetInstance().Initialize()
    // Provides system-level monitoring without allocation-level hooking risks
    
    if (!m_initialized) {
        return;
    }

    UpdateMemoryStats();
    CheckMemoryThresholds();
}

bool MemoryManager::IsMemoryCriticallyLow() {
    if (!m_initialized) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_statsMutex);

    return m_currentStats.availablePhysical < CRITICAL_MEMORY_THRESHOLD ||
           m_currentStats.usagePercent > CRITICAL_USAGE_PERCENT;
}

void MemoryManager::WarnUserAboutMemory() {
    if (!m_initialized || !ShouldWarnUser()) {
        return;
    }

    RecordMemoryWarning();

    std::string message = "Memory is running low!\n\n";
    message += "Available: " + std::to_string(m_currentStats.availablePhysical / (1024 * 1024)) + " MB\n";
    message += "Usage: " + std::to_string(static_cast<int>(m_currentStats.usagePercent)) + "%\n\n";
    
    if (m_criticalMemoryMode) {
        message += "CRITICAL: Consider saving and restarting the game.";
    } else {
        message += "Consider reducing graphics settings or closing other applications.";
    }

    // Show notification to user
    SKSE::log::warn("Memory warning: {}", message);
    
    // In a real implementation, this would show an in-game message
    // For now, we just log it
}

void MemoryManager::SuggestMemoryReduction() {
    if (!m_initialized) {
        return;
    }

    SKSE::log::info("Suggesting memory reduction strategies:");
    SKSE::log::info("  - Reduce texture quality settings");
    SKSE::log::info("  - Reduce shadow quality");
    SKSE::log::info("  - Reduce actor fade distance");
    SKSE::log::info("  - Close unnecessary background applications");
    
    if (m_warningCount > 3) {
        SKSE::log::warn("  - RECOMMENDED: Save and restart the game");
    }
}

// ============================================================================
// Memory Statistics
// ============================================================================

MemoryManager::MemoryStats MemoryManager::GetMemoryStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_currentStats;
}

void MemoryManager::UpdateMemoryStats() {
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    
    if (!GlobalMemoryStatusEx(&memStatus)) {
        SKSE::log::error("Failed to get memory status");
        return;
    }

    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), 
                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), 
                              sizeof(pmc))) {
        SKSE::log::error("Failed to get process memory info");
        return;
    }

    std::lock_guard<std::mutex> lock(m_statsMutex);

    m_currentStats.totalPhysical = memStatus.ullTotalPhys;
    m_currentStats.availablePhysical = memStatus.ullAvailPhys;
    m_currentStats.totalVirtual = memStatus.ullTotalVirtual;
    m_currentStats.availableVirtual = memStatus.ullAvailVirtual;
    m_currentStats.processWorkingSet = pmc.WorkingSetSize;
    m_currentStats.processPrivateBytes = pmc.PrivateUsage;
    m_currentStats.usagePercent = static_cast<float>(memStatus.dwMemoryLoad);
    m_currentStats.lastUpdate = std::chrono::steady_clock::now();
}

// ============================================================================
// Internal Helper Methods
// ============================================================================

void MemoryManager::CollectDistantCells(std::vector<CellResourceInfo>& outCells) {
    // Get player position
    auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return;
    }

    auto playerPos = player->GetPosition();

    // Collect cells that are far from player
    for (const auto& cellInfo : m_trackedCells) {
        if (cellInfo.distanceFromPlayer > DISTANT_CELL_THRESHOLD) {
            outCells.push_back(cellInfo);
        }
    }

    // Sort by distance (furthest first)
    std::sort(outCells.begin(), outCells.end(),
        [](const CellResourceInfo& a, const CellResourceInfo& b) {
            return a.distanceFromPlayer > b.distanceFromPlayer;
        });
}

void MemoryManager::CollectUnusedTextures(std::vector<TextureInfo>& outTextures) {
    auto now = std::chrono::steady_clock::now();
    auto unusedThreshold = std::chrono::minutes(5);

    for (const auto& texInfo : m_trackedTextures) {
        auto timeSinceUsed = now - texInfo.lastUsed;
        
        // Texture is unused if not referenced and not used recently
        if (texInfo.referenceCount == 0 && timeSinceUsed > unusedThreshold) {
            outTextures.push_back(texInfo);
        }
    }

    // Sort by size (largest first)
    std::sort(outTextures.begin(), outTextures.end(),
        [](const TextureInfo& a, const TextureInfo& b) {
            return a.size > b.size;
        });
}

void MemoryManager::CollectCachedData(std::vector<ResourceInfo>& outResources) {
    for (const auto& resource : m_cachedResources) {
        float priority = CalculateResourcePriority(resource);
        
        // Lower priority = more likely to free
        if (priority < 0.5f) {
            outResources.push_back(resource);
        }
    }

    // Sort by priority (lowest first)
    std::sort(outResources.begin(), outResources.end(),
        [this](const ResourceInfo& a, const ResourceInfo& b) {
            return CalculateResourcePriority(a) < CalculateResourcePriority(b);
        });
}

float MemoryManager::CalculateResourcePriority(const ResourceInfo& resource) {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceAccess = std::chrono::duration_cast<std::chrono::seconds>(
        now - resource.lastAccessed).count();

    // Priority based on access count and recency
    // Higher access count = higher priority (keep)
    // More recent access = higher priority (keep)
    float accessPriority = std::min(resource.accessCount / 100.0f, 1.0f);
    float recencyPriority = 1.0f / (1.0f + timeSinceAccess / 60.0f);

    return (accessPriority + recencyPriority) / 2.0f;
}

void MemoryManager::CheckMemoryThresholds() {
    std::lock_guard<std::mutex> lock(m_statsMutex);

    bool wasCritical = m_criticalMemoryMode;

    // Check if memory is critically low
    m_criticalMemoryMode = m_currentStats.availablePhysical < CRITICAL_MEMORY_THRESHOLD ||
                           m_currentStats.usagePercent > CRITICAL_USAGE_PERCENT;

    // Check if we should warn
    bool shouldWarn = m_currentStats.availablePhysical < WARNING_MEMORY_THRESHOLD ||
                      m_currentStats.usagePercent > WARNING_USAGE_PERCENT;

    if (m_criticalMemoryMode && !wasCritical) {
        SKSE::log::error("Memory entered CRITICAL state!");
        WarnUserAboutMemory();
        SuggestMemoryReduction();
    } else if (shouldWarn && ShouldWarnUser()) {
        WarnUserAboutMemory();
    }
}

bool MemoryManager::ShouldWarnUser() {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastWarning = now - m_lastWarningTime;

    return timeSinceLastWarning >= WARNING_COOLDOWN;
}

void MemoryManager::RecordMemoryWarning() {
    m_lastWarningTime = std::chrono::steady_clock::now();
    m_warningCount++;
}

} // namespace CrashGuard
