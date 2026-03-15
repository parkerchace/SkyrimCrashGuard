// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <atomic>
#include <memory>
#include <optional>

/// Lock-Free Data Structures for High-Performance Thread Safety
/// Provides lock-free alternatives for performance-critical operations
namespace ThreadSafety {

    /// Lock-free counter for statistics tracking
    class LockFreeCounter {
    public:
        LockFreeCounter() : value_(0) {}
        
        /// Increment counter atomically
        void increment() {
            value_.fetch_add(1, std::memory_order_relaxed);
        }
        
        /// Decrement counter atomically
        void decrement() {
            value_.fetch_sub(1, std::memory_order_relaxed);
        }
        
        /// Add value atomically
        void add(size_t delta) {
            value_.fetch_add(delta, std::memory_order_relaxed);
        }
        
        /// Get current value
        size_t get() const {
            return value_.load(std::memory_order_relaxed);
        }
        
        /// Reset counter to zero
        void reset() {
            value_.store(0, std::memory_order_relaxed);
        }
        
    private:
        std::atomic<size_t> value_;
    };

    /// Lock-free flag for boolean state
    class LockFreeFlag {
    public:
        LockFreeFlag(bool initial = false) : value_(initial) {}
        
        /// Set flag to true
        void set() {
            value_.store(true, std::memory_order_release);
        }
        
        /// Set flag to false
        void clear() {
            value_.store(false, std::memory_order_release);
        }
        
        /// Get current value
        bool get() const {
            return value_.load(std::memory_order_acquire);
        }
        
        /// Test and set atomically (returns previous value)
        bool test_and_set() {
            return value_.exchange(true, std::memory_order_acq_rel);
        }
        
    private:
        std::atomic<bool> value_;
    };

    /// Lock-free stack for simple LIFO operations
    template<typename T>
    class LockFreeStack {
    private:
        struct Node {
            T data;
            Node* next;
            
            Node(const T& value) : data(value), next(nullptr) {}
        };
        
    public:
        LockFreeStack() : head_(nullptr), size_(0) {}
        
        ~LockFreeStack() {
            while (pop()) {}
        }
        
        /// Push item onto stack
        void push(const T& value) {
            Node* newNode = new Node(value);
            newNode->next = head_.load(std::memory_order_relaxed);
            
            while (!head_.compare_exchange_weak(newNode->next, newNode,
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
                // Retry if CAS failed
            }
            
            size_.fetch_add(1, std::memory_order_relaxed);
        }
        
        /// Pop item from stack
        std::optional<T> pop() {
            Node* oldHead = head_.load(std::memory_order_relaxed);
            
            while (oldHead && !head_.compare_exchange_weak(oldHead, oldHead->next,
                                                          std::memory_order_acquire,
                                                          std::memory_order_relaxed)) {
                // Retry if CAS failed
            }
            
            if (oldHead) {
                T value = oldHead->data;
                delete oldHead;
                size_.fetch_sub(1, std::memory_order_relaxed);
                return value;
            }
            
            return std::nullopt;
        }
        
        /// Check if stack is empty
        bool empty() const {
            return head_.load(std::memory_order_relaxed) == nullptr;
        }
        
        /// Get approximate size (may not be exact due to concurrent operations)
        size_t size() const {
            return size_.load(std::memory_order_relaxed);
        }
        
    private:
        std::atomic<Node*> head_;
        std::atomic<size_t> size_;
    };

    /// Lock contention profiler for identifying bottlenecks
    class LockContentionProfiler {
    public:
        struct ContentionStats {
            std::string lockName;
            size_t acquisitionCount;
            size_t contentionCount;
            std::chrono::microseconds totalWaitTime;
            std::chrono::microseconds maxWaitTime;
            std::chrono::microseconds avgWaitTime;
            float contentionRate;  // contentionCount / acquisitionCount
        };

        /// Initialize profiler
        static void Initialize();
        
        /// Shutdown profiler
        static void Shutdown();
        
        /// Record lock acquisition
        static void RecordAcquisition(const std::string& lockName, 
                                     std::chrono::microseconds waitTime);
        
        /// Record lock contention
        static void RecordContention(const std::string& lockName);
        
        /// Get contention statistics for a lock
        static ContentionStats GetStats(const std::string& lockName);
        
        /// Get all contention statistics
        static std::vector<ContentionStats> GetAllStats();
        
        /// Get top N most contended locks
        static std::vector<ContentionStats> GetTopContendedLocks(size_t count = 10);
        
        /// Reset statistics
        static void ResetStats();
        
        /// Enable/disable profiling
        static void SetEnabled(bool enabled);
        
        /// Export statistics to file
        static bool ExportStats(const std::string& filename);

    private:
        struct LockStats {
            LockFreeCounter acquisitionCount;
            LockFreeCounter contentionCount;
            std::atomic<uint64_t> totalWaitTimeMicros;
            std::atomic<uint64_t> maxWaitTimeMicros;
        };

        static bool s_initialized;
        static bool s_enabled;
        static std::unordered_map<std::string, LockStats> s_lockStats;
        static std::mutex s_statsMutex;  // Only for map modifications, not for counters
    };

    /// Scoped lock with contention profiling
    template<typename MutexType>
    class ProfiledLock {
    public:
        ProfiledLock(MutexType& mutex, const std::string& lockName)
            : mutex_(mutex), lockName_(lockName), locked_(false) {
            
            auto startTime = std::chrono::steady_clock::now();
            
            // Try to acquire lock
            if (!mutex_.try_lock()) {
                // Contention detected
                LockContentionProfiler::RecordContention(lockName);
                mutex_.lock();  // Block until acquired
            }
            
            locked_ = true;
            
            auto endTime = std::chrono::steady_clock::now();
            auto waitTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            
            LockContentionProfiler::RecordAcquisition(lockName, waitTime);
        }

        ~ProfiledLock() {
            if (locked_) {
                mutex_.unlock();
            }
        }

        // Delete copy/move
        ProfiledLock(const ProfiledLock&) = delete;
        ProfiledLock& operator=(const ProfiledLock&) = delete;
        ProfiledLock(ProfiledLock&&) = delete;
        ProfiledLock& operator=(ProfiledLock&&) = delete;

    private:
        MutexType& mutex_;
        std::string lockName_;
        bool locked_;
    };

}  // namespace ThreadSafety
