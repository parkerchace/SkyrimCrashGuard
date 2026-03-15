// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <cstdint>

namespace CrashGuard {

/**
 * @brief Hooks Skyrim's memory allocators for leak tracking
 * 
 * Hooks MemoryManager and ScrapHeap allocators to track all allocations.
 * Can be enabled/disabled via configuration for stability testing.
 */
class MemoryAllocatorHooks {
public:
    static MemoryAllocatorHooks& GetInstance();

    // Initialize and install hooks
    bool Initialize();

    // Shutdown and remove hooks
    void Shutdown();

    // Check if hooks are enabled
    bool AreHooksEnabled() const { return m_hooksEnabled; }

    // Enable/disable hooks at runtime
    void SetHooksEnabled(bool enabled);

private:
    MemoryAllocatorHooks() = default;
    ~MemoryAllocatorHooks() = default;
    MemoryAllocatorHooks(const MemoryAllocatorHooks&) = delete;
    MemoryAllocatorHooks& operator=(const MemoryAllocatorHooks&) = delete;

    // Install individual hooks
    bool InstallMemoryManagerHooks();
    bool InstallScrapHeapHooks();

    // Hook functions for MemoryManager
    static void* Hook_MemoryManagerAllocate(void* manager, size_t size, uint32_t alignment, bool aligned);
    static void Hook_MemoryManagerDeallocate(void* manager, void* ptr, bool aligned);

    // Hook functions for ScrapHeap
    static void* Hook_ScrapHeapAlloc(void* heap, size_t size, uint32_t alignment);
    static void Hook_ScrapHeapFree(void* heap, void* ptr);

    // Original function pointers (trampolines)
    static inline REL::Relocation<decltype(Hook_MemoryManagerAllocate)> Original_MemoryManagerAllocate;
    static inline REL::Relocation<decltype(Hook_MemoryManagerDeallocate)> Original_MemoryManagerDeallocate;
    static inline REL::Relocation<decltype(Hook_ScrapHeapAlloc)> Original_ScrapHeapAlloc;
    static inline REL::Relocation<decltype(Hook_ScrapHeapFree)> Original_ScrapHeapFree;

    bool m_initialized = false;
    bool m_hooksEnabled = false;
    bool m_memoryManagerHooked = false;
    bool m_scrapHeapHooked = false;
};

} // namespace CrashGuard
