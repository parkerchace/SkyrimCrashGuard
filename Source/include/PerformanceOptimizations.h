// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

// CommonLibSSE must come before any Windows headers
#include <RE/Skyrim.h>

#include <unordered_map>
#include <shared_mutex>
#include <chrono>

namespace Performance {

    // ========================================================================
    // Inline Null Checks for Hot Paths
    // ========================================================================
    
    // Fast inline null check with minimal overhead
    template<typename T>
    [[nodiscard]] inline constexpr bool IsValidPointer(const T* ptr) noexcept {
        return ptr != nullptr;
    }
    
    // Fast inline null check with safe default return
    template<typename T, typename DefaultT>
    [[nodiscard]] inline constexpr T SafeDeref(const T* ptr, DefaultT defaultValue) noexcept {
        return ptr ? *ptr : static_cast<T>(defaultValue);
    }
    
    // Fast inline bounds check with clamping
    template<typename IndexT, typename SizeT>
    [[nodiscard]] inline constexpr IndexT ClampIndex(IndexT index, SizeT size) noexcept {
        if (index < 0) return 0;
        if (static_cast<SizeT>(index) >= size) return static_cast<IndexT>(size - 1);
        return index;
    }
    
    // Fast inline range check
    template<typename T>
    [[nodiscard]] inline constexpr bool IsInRange(T value, T min, T max) noexcept {
        return value >= min && value <= max;
    }

    // ========================================================================
    // Validation Result Cache
    // ========================================================================
    
    // Cache validation results to avoid repeated expensive checks
    class ValidationCache {
    public:
        enum class ValidationStatus : uint8_t {
            Unknown = 0,
            Valid = 1,
            Invalid = 2,
            Repaired = 3
        };
        
        struct CacheEntry {
            ValidationStatus status;
            std::chrono::steady_clock::time_point timestamp;
            uint32_t accessCount;
        };
        
        // Get singleton instance
        static ValidationCache& GetInstance() {
            static ValidationCache instance;
            return instance;
        }
        
        // Check if resource is cached
        [[nodiscard]] bool IsCached(const std::string& resourcePath) const {
            std::shared_lock lock(m_mutex);
            return m_cache.find(resourcePath) != m_cache.end();
        }
        
        // Get cached validation status
        [[nodiscard]] ValidationStatus GetStatus(const std::string& resourcePath) {
            std::shared_lock lock(m_mutex);
            auto it = m_cache.find(resourcePath);
            if (it != m_cache.end()) {
                // Update access count (requires upgrade to unique lock)
                lock.unlock();
                std::unique_lock ulock(m_mutex);
                it->second.accessCount++;
                return it->second.status;
            }
            return ValidationStatus::Unknown;
        }
        
        // Cache validation result
        void CacheResult(const std::string& resourcePath, ValidationStatus status) {
            std::unique_lock lock(m_mutex);
            
            // Limit cache size to prevent unbounded growth
            if (m_cache.size() >= m_maxCacheSize) {
                EvictOldestEntry();
            }
            
            CacheEntry entry;
            entry.status = status;
            entry.timestamp = std::chrono::steady_clock::now();
            entry.accessCount = 1;
            
            m_cache[resourcePath] = entry;
        }
        
        // Clear cache
        void Clear() {
            std::unique_lock lock(m_mutex);
            m_cache.clear();
        }
        
        // Get cache statistics
        struct CacheStats {
            size_t entryCount;
            size_t validCount;
            size_t invalidCount;
            size_t repairedCount;
        };
        
        [[nodiscard]] CacheStats GetStats() const {
            std::shared_lock lock(m_mutex);
            CacheStats stats{};
            stats.entryCount = m_cache.size();
            
            for (const auto& [path, entry] : m_cache) {
                switch (entry.status) {
                    case ValidationStatus::Valid:
                        stats.validCount++;
                        break;
                    case ValidationStatus::Invalid:
                        stats.invalidCount++;
                        break;
                    case ValidationStatus::Repaired:
                        stats.repairedCount++;
                        break;
                    default:
                        break;
                }
            }
            
            return stats;
        }
        
    private:
        ValidationCache() = default;
        ~ValidationCache() = default;
        ValidationCache(const ValidationCache&) = delete;
        ValidationCache& operator=(const ValidationCache&) = delete;
        
        void EvictOldestEntry() {
            // Find entry with oldest timestamp
            auto oldest = m_cache.begin();
            for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
                if (it->second.timestamp < oldest->second.timestamp) {
                    oldest = it;
                }
            }
            
            if (oldest != m_cache.end()) {
                m_cache.erase(oldest);
            }
        }
        
        mutable std::shared_mutex m_mutex;
        std::unordered_map<std::string, CacheEntry> m_cache;
        static constexpr size_t m_maxCacheSize = 10000;  // Limit cache size
    };

    // ========================================================================
    // Fast-Path Optimization Helpers
    // ========================================================================
    
    // Fast-path check for valid data - returns true if data is valid and can skip validation
    class FastPathChecker {
    public:
        // Check if mesh likely needs validation (fast heuristic)
        [[nodiscard]] static inline bool MeshLikelyValid(const RE::NiAVObject* mesh) noexcept {
            // Fast null check
            if (!mesh) return false;
            
            // Check if it's a NiNode (scene graph) - these are always valid
            if (const_cast<RE::NiAVObject*>(mesh)->AsNode()) {
                return true;
            }
            
            // For geometry, do basic pointer checks
            auto geometry = const_cast<RE::NiAVObject*>(mesh)->AsNiGeometry();
            if (!geometry) return true;  // Not geometry, let it through
            
            auto geometryData = geometry->GetRuntimeData().m_spModelData.get();
            if (!geometryData) return false;
            
            // Fast checks: has vertices and vertex data pointer
            return geometryData->vertices > 0 && geometryData->vertex != nullptr;
        }
        
        // Check if FormID likely valid (fast heuristic)
        [[nodiscard]] static inline bool FormIDLikelyValid(RE::FormID formID) noexcept {
            // Fast checks: not zero, not max value
            if (formID == 0 || formID == 0xFFFFFFFF) return false;
            
            // Check if mod index is reasonable (0-255 for ESM/ESP, 0xFE for ESL)
            uint8_t modIndex = (formID >> 24) & 0xFF;
            return modIndex <= 0xFE;
        }
        
        // Check if pointer likely valid (fast heuristic)
        [[nodiscard]] static inline bool PointerLikelyValid(const void* ptr) noexcept {
            // Fast null check
            if (!ptr) return false;
            
            // Check if pointer is in reasonable address range
            // Windows user-mode addresses are typically in range 0x00010000 to 0x7FFFFFFF
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
            return addr >= 0x00010000 && addr <= 0x7FFFFFFFFFFFFFFF;
        }
    };

    // ========================================================================
    // Performance Metrics
    // ========================================================================
    
    // Track performance metrics for hot paths
    class PerformanceMetrics {
    public:
        struct Metrics {
            uint64_t totalCalls = 0;
            uint64_t fastPathHits = 0;
            uint64_t slowPathHits = 0;
            uint64_t cacheHits = 0;
            uint64_t cacheMisses = 0;
            std::chrono::nanoseconds totalTime{0};
            std::chrono::nanoseconds fastPathTime{0};
            std::chrono::nanoseconds slowPathTime{0};
        };
        
        static PerformanceMetrics& GetInstance() {
            static PerformanceMetrics instance;
            return instance;
        }
        
        void RecordFastPath(std::chrono::nanoseconds duration) {
            std::unique_lock lock(m_mutex);
            m_metrics.totalCalls++;
            m_metrics.fastPathHits++;
            m_metrics.totalTime += duration;
            m_metrics.fastPathTime += duration;
        }
        
        void RecordSlowPath(std::chrono::nanoseconds duration) {
            std::unique_lock lock(m_mutex);
            m_metrics.totalCalls++;
            m_metrics.slowPathHits++;
            m_metrics.totalTime += duration;
            m_metrics.slowPathTime += duration;
        }
        
        void RecordCacheHit() {
            std::unique_lock lock(m_mutex);
            m_metrics.cacheHits++;
        }
        
        void RecordCacheMiss() {
            std::unique_lock lock(m_mutex);
            m_metrics.cacheMisses++;
        }
        
        [[nodiscard]] Metrics GetMetrics() const {
            std::shared_lock lock(m_mutex);
            return m_metrics;
        }
        
        void Reset() {
            std::unique_lock lock(m_mutex);
            m_metrics = Metrics{};
        }
        
    private:
        PerformanceMetrics() = default;
        ~PerformanceMetrics() = default;
        PerformanceMetrics(const PerformanceMetrics&) = delete;
        PerformanceMetrics& operator=(const PerformanceMetrics&) = delete;
        
        mutable std::shared_mutex m_mutex;
        Metrics m_metrics;
    };

    // ========================================================================
    // RAII Performance Timer
    // ========================================================================
    
    // Automatic performance timing for scopes
    class ScopedTimer {
    public:
        explicit ScopedTimer(bool isFastPath) 
            : m_isFastPath(isFastPath)
            , m_start(std::chrono::high_resolution_clock::now()) 
        {}
        
        ~ScopedTimer() {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start);
            
            if (m_isFastPath) {
                PerformanceMetrics::GetInstance().RecordFastPath(duration);
            } else {
                PerformanceMetrics::GetInstance().RecordSlowPath(duration);
            }
        }
        
    private:
        bool m_isFastPath;
        std::chrono::high_resolution_clock::time_point m_start;
    };

    // ========================================================================
    // Lazy Symbol Resolution Cache
    // ========================================================================
    
    // Cache resolved symbols to avoid repeated lookups
    class SymbolCache {
    public:
        static SymbolCache& GetInstance() {
            static SymbolCache instance;
            return instance;
        }
        
        // Get or resolve symbol address
        template<typename T>
        T* GetSymbol(const std::string& symbolName, std::function<T*()> resolver) {
            std::shared_lock lock(m_mutex);
            
            auto it = m_cache.find(symbolName);
            if (it != m_cache.end()) {
                return reinterpret_cast<T*>(it->second);
            }
            
            // Upgrade to unique lock for resolution
            lock.unlock();
            std::unique_lock ulock(m_mutex);
            
            // Double-check after acquiring write lock
            it = m_cache.find(symbolName);
            if (it != m_cache.end()) {
                return reinterpret_cast<T*>(it->second);
            }
            
            // Resolve symbol
            T* address = resolver();
            if (address) {
                m_cache[symbolName] = reinterpret_cast<uintptr_t>(address);
            }
            
            return address;
        }
        
        // Clear cache
        void Clear() {
            std::unique_lock lock(m_mutex);
            m_cache.clear();
        }
        
    private:
        SymbolCache() = default;
        ~SymbolCache() = default;
        SymbolCache(const SymbolCache&) = delete;
        SymbolCache& operator=(const SymbolCache&) = delete;
        
        mutable std::shared_mutex m_mutex;
        std::unordered_map<std::string, uintptr_t> m_cache;
    };

}  // namespace Performance
