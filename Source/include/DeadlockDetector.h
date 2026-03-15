// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <string>
#include <vector>

/// Deadlock Detection System for Thread Safety
/// Monitors lock acquisition times and detects potential deadlocks
namespace ThreadSafety {

    /// Lock acquisition record
    struct LockAcquisition {
        std::thread::id threadId;
        std::string lockName;
        std::chrono::steady_clock::time_point acquisitionTime;
        bool isAcquired;
        std::chrono::milliseconds maxWaitTime;
    };

    /// Deadlock detection result
    struct DeadlockInfo {
        bool detected;
        std::vector<std::thread::id> involvedThreads;
        std::vector<std::string> involvedLocks;
        std::chrono::milliseconds waitDuration;
        std::string description;
    };

    /// Deadlock detector class
    class DeadlockDetector {
    public:
        /// Initialize the deadlock detector
        static void Initialize();

        /// Shutdown the deadlock detector
        static void Shutdown();

        /// Record lock acquisition attempt
        static void RecordLockAttempt(const std::string& lockName, 
                                     std::chrono::milliseconds maxWaitTime = std::chrono::milliseconds(5000));

        /// Record successful lock acquisition
        static void RecordLockAcquired(const std::string& lockName);

        /// Record lock release
        static void RecordLockReleased(const std::string& lockName);

        /// Check for potential deadlocks
        static DeadlockInfo CheckForDeadlock();

        /// Break detected deadlock (if possible)
        static bool BreakDeadlock(const DeadlockInfo& deadlock);

        /// Get deadlock statistics
        static size_t GetDeadlockCount();
        static size_t GetBrokenDeadlockCount();

        /// Enable/disable deadlock detection
        static void SetEnabled(bool enabled);

        /// Set deadlock timeout threshold
        static void SetDeadlockTimeout(std::chrono::milliseconds timeout);

    private:
        /// Check if a specific thread is waiting too long
        static bool IsThreadWaitingTooLong(const LockAcquisition& acquisition);

        /// Find circular wait conditions
        static bool FindCircularWait(std::vector<LockAcquisition>& chain);

        /// Log deadlock occurrence
        static void LogDeadlock(const DeadlockInfo& deadlock);

        /// Attempt to break deadlock by releasing oldest lock
        static bool AttemptBreakDeadlock(const DeadlockInfo& deadlock);

        // State tracking
        static bool s_initialized;
        static bool s_enabled;
        static std::chrono::milliseconds s_deadlockTimeout;
        static std::unordered_map<std::thread::id, std::vector<LockAcquisition>> s_threadLocks;
        static std::mutex s_detectorMutex;
        static size_t s_deadlockCount;
        static size_t s_brokenDeadlockCount;
    };

    /// RAII wrapper for automatic deadlock detection
    template<typename MutexType>
    class DeadlockGuard {
    public:
        DeadlockGuard(MutexType& mutex, const std::string& lockName, 
                     std::chrono::milliseconds maxWaitTime = std::chrono::milliseconds(5000))
            : mutex_(mutex), lockName_(lockName), locked_(false) {
            
            if (DeadlockDetector::CheckForDeadlock().detected) {
                // Deadlock detected before attempting lock
                spdlog::error("Deadlock detected before acquiring lock: {}", lockName);
            }

            DeadlockDetector::RecordLockAttempt(lockName, maxWaitTime);
            
            // Attempt to acquire lock with timeout
            auto startTime = std::chrono::steady_clock::now();
            
            if constexpr (std::is_same_v<MutexType, std::mutex> || 
                         std::is_same_v<MutexType, std::shared_mutex>) {
                // For regular mutexes, use try_lock with timeout
                while (!mutex_.try_lock()) {
                    auto elapsed = std::chrono::steady_clock::now() - startTime;
                    if (elapsed > maxWaitTime) {
                        // Timeout - potential deadlock
                        auto deadlock = DeadlockDetector::CheckForDeadlock();
                        if (deadlock.detected) {
                            DeadlockDetector::BreakDeadlock(deadlock);
                        }
                        // Try one more time
                        mutex_.lock();
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            } else {
                // For other mutex types, just lock normally
                mutex_.lock();
            }
            
            locked_ = true;
            DeadlockDetector::RecordLockAcquired(lockName);
        }

        ~DeadlockGuard() {
            if (locked_) {
                mutex_.unlock();
                DeadlockDetector::RecordLockReleased(lockName_);
            }
        }

        // Delete copy/move
        DeadlockGuard(const DeadlockGuard&) = delete;
        DeadlockGuard& operator=(const DeadlockGuard&) = delete;
        DeadlockGuard(DeadlockGuard&&) = delete;
        DeadlockGuard& operator=(DeadlockGuard&&) = delete;

    private:
        MutexType& mutex_;
        std::string lockName_;
        bool locked_;
    };

}  // namespace ThreadSafety
