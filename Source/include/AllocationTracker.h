// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <cstdint>
#include <chrono>
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace CrashGuard {

/**
 * @brief Tracks memory allocations for leak detection
 * 
 * Thread-safe allocation tracking with call stack capture.
 * Integrates with StateManager for leak detection.
 */
class AllocationTracker {
public:
    // Lock-free ring buffer entry structure
    struct AllocationEntry {
        void* ptr;
        size_t size;
        uint64_t timestamp;
        char allocatorType[16];
        uint32_t callStackDepth;
        void* callStack[16];
        bool isFreed;
    };

    // Allocation information structure (for external API)
    struct AllocationInfo {
        void* address;
        size_t size;
        std::chrono::steady_clock::time_point timestamp;
        std::string allocatorType; // "MemoryManager", "ScrapHeap", "malloc"
        void* callStack[16]; // Call stack at allocation time
        uint32_t callStackDepth;
        bool isFreed;
    };

    static AllocationTracker& GetInstance();

    // Initialize tracking system
    bool Initialize();

    // Shutdown and cleanup
    void Shutdown();

    // Track allocation
    void TrackAllocation(void* ptr, size_t size, const std::string& allocatorType);

    // Track deallocation
    void TrackDeallocation(void* ptr);

    // Get allocation info
    bool GetAllocationInfo(void* ptr, AllocationInfo& outInfo) const;

    // Get all tracked allocations
    std::vector<AllocationInfo> GetAllAllocations() const;

    // Get allocations older than specified age
    std::vector<AllocationInfo> GetOldAllocations(std::chrono::seconds minAge) const;

    // Get total allocated memory
    size_t GetTotalAllocatedMemory() const;

    // Get allocation count
    size_t GetAllocationCount() const;

    // Clear all tracking data
    void Clear();

    // Deferred initialization control
    bool IsTrackingEnabled() const { return m_trackingEnabled.load(std::memory_order_relaxed); }
    void SetTrackingEnabled(bool enabled) { m_trackingEnabled.store(enabled, std::memory_order_relaxed); }

    // Reentrancy depth control
    int GetReentrancyDepth() const { return m_reentrancyDepth.load(std::memory_order_relaxed); }

private:
    AllocationTracker() = default;
    ~AllocationTracker() = default;
    AllocationTracker(const AllocationTracker&) = delete;
    AllocationTracker& operator=(const AllocationTracker&) = delete;

    // Capture call stack at current location
    uint32_t CaptureCallStack(void** outCallStack, uint32_t maxDepth);

    // Background thread for async call stack capture
    void BackgroundThreadFunc();
    std::thread m_backgroundThread;
    std::queue<void*> m_pendingCallStacks;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCV;
    std::atomic<bool> m_shutdownRequested{false};

    // Lock-free ring buffer for allocation tracking
    std::vector<AllocationEntry> m_ringBuffer;
    std::atomic<size_t> m_head{0};
    std::atomic<size_t> m_tail{0};
    size_t m_ringBufferSize = 65536;

    // Statistics (atomic for lock-free access)
    std::atomic<size_t> m_totalAllocatedMemory{0};
    std::atomic<size_t> m_totalAllocationCount{0};
    std::atomic<size_t> m_totalDeallocationCount{0};

    // Deferred initialization flag - tracking remains dormant until explicitly enabled
    std::atomic<bool> m_trackingEnabled{false};

    // Multi-level reentrancy protection - prevents both direct and indirect recursion
    std::atomic<int> m_reentrancyDepth{0};

    // Configuration options (loaded from Config.h)
    bool m_deferredInitialization = true;
    int m_samplingRate = 1;
    int m_callStackCaptureDepth = 16;

    // Sampling support - track 1 in N allocations
    std::atomic<uint64_t> m_allocationCounter{0};

    bool m_initialized = false;
};

} // namespace CrashGuard
