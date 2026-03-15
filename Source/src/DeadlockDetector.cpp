// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "PCH.h"
#include "DeadlockDetector.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace ThreadSafety {

    // Static member initialization
    bool DeadlockDetector::s_initialized = false;
    bool DeadlockDetector::s_enabled = true;
    std::chrono::milliseconds DeadlockDetector::s_deadlockTimeout = std::chrono::milliseconds(5000);
    std::unordered_map<std::thread::id, std::vector<LockAcquisition>> DeadlockDetector::s_threadLocks;
    std::mutex DeadlockDetector::s_detectorMutex;
    size_t DeadlockDetector::s_deadlockCount = 0;
    size_t DeadlockDetector::s_brokenDeadlockCount = 0;

    void DeadlockDetector::Initialize() {
        if (s_initialized) {
            return;
        }

        spdlog::info("╔════════════════════════════════════════╗");
        spdlog::info("║    Deadlock Detector Initializing     ║");
        spdlog::info("╚════════════════════════════════════════╝");

        s_threadLocks.clear();
        s_deadlockCount = 0;
        s_brokenDeadlockCount = 0;
        s_enabled = true;

        s_initialized = true;
        spdlog::info("DeadlockDetector initialized with {}ms timeout", s_deadlockTimeout.count());
    }

    void DeadlockDetector::Shutdown() {
        if (!s_initialized) {
            return;
        }

        spdlog::info("DeadlockDetector shutting down");
        spdlog::info("  Total deadlocks detected: {}", s_deadlockCount);
        spdlog::info("  Deadlocks broken: {}", s_brokenDeadlockCount);

        std::lock_guard<std::mutex> lock(s_detectorMutex);
        s_threadLocks.clear();
        s_initialized = false;
    }

    void DeadlockDetector::RecordLockAttempt(const std::string& lockName, 
                                            std::chrono::milliseconds maxWaitTime) {
        if (!s_initialized || !s_enabled) {
            return;
        }

        std::lock_guard<std::mutex> lock(s_detectorMutex);

        auto threadId = std::this_thread::get_id();
        
        LockAcquisition acquisition;
        acquisition.threadId = threadId;
        acquisition.lockName = lockName;
        acquisition.acquisitionTime = std::chrono::steady_clock::now();
        acquisition.isAcquired = false;
        acquisition.maxWaitTime = maxWaitTime;

        s_threadLocks[threadId].push_back(acquisition);

        spdlog::trace("Thread {} attempting to acquire lock: {}", 
                     std::hash<std::thread::id>{}(threadId), lockName);
    }

    void DeadlockDetector::RecordLockAcquired(const std::string& lockName) {
        if (!s_initialized || !s_enabled) {
            return;
        }

        std::lock_guard<std::mutex> lock(s_detectorMutex);

        auto threadId = std::this_thread::get_id();
        auto& locks = s_threadLocks[threadId];

        // Find the most recent attempt for this lock
        for (auto it = locks.rbegin(); it != locks.rend(); ++it) {
            if (it->lockName == lockName && !it->isAcquired) {
                it->isAcquired = true;
                
                auto waitTime = std::chrono::steady_clock::now() - it->acquisitionTime;
                auto waitMs = std::chrono::duration_cast<std::chrono::milliseconds>(waitTime);
                
                if (waitMs > std::chrono::milliseconds(100)) {
                    spdlog::debug("Thread {} acquired lock {} after {}ms wait",
                                std::hash<std::thread::id>{}(threadId), lockName, waitMs.count());
                }
                break;
            }
        }
    }

    void DeadlockDetector::RecordLockReleased(const std::string& lockName) {
        if (!s_initialized || !s_enabled) {
            return;
        }

        std::lock_guard<std::mutex> lock(s_detectorMutex);

        auto threadId = std::this_thread::get_id();
        auto& locks = s_threadLocks[threadId];

        // Remove the most recent acquisition of this lock
        for (auto it = locks.rbegin(); it != locks.rend(); ++it) {
            if (it->lockName == lockName && it->isAcquired) {
                // Convert reverse iterator to forward iterator for erase
                locks.erase(std::next(it).base());
                break;
            }
        }

        // Clean up empty thread entries
        if (locks.empty()) {
            s_threadLocks.erase(threadId);
        }
    }

    DeadlockInfo DeadlockDetector::CheckForDeadlock() {
        DeadlockInfo info;
        info.detected = false;

        if (!s_initialized || !s_enabled) {
            return info;
        }

        std::lock_guard<std::mutex> lock(s_detectorMutex);

        auto now = std::chrono::steady_clock::now();

        // Check for threads waiting too long
        for (const auto& [threadId, locks] : s_threadLocks) {
            for (const auto& acquisition : locks) {
                if (!acquisition.isAcquired && IsThreadWaitingTooLong(acquisition)) {
                    // Potential deadlock detected
                    info.detected = true;
                    info.involvedThreads.push_back(threadId);
                    info.involvedLocks.push_back(acquisition.lockName);
                    
                    auto waitTime = now - acquisition.acquisitionTime;
                    info.waitDuration = std::chrono::duration_cast<std::chrono::milliseconds>(waitTime);
                    
                    info.description = fmt::format(
                        "Thread {} waiting for lock '{}' for {}ms (timeout: {}ms)",
                        std::hash<std::thread::id>{}(threadId),
                        acquisition.lockName,
                        info.waitDuration.count(),
                        acquisition.maxWaitTime.count()
                    );

                    // Found one deadlock, that's enough for now
                    return info;
                }
            }
        }

        // Check for circular wait conditions
        std::vector<LockAcquisition> chain;
        if (FindCircularWait(chain)) {
            info.detected = true;
            
            for (const auto& acquisition : chain) {
                info.involvedThreads.push_back(acquisition.threadId);
                info.involvedLocks.push_back(acquisition.lockName);
            }
            
            info.description = "Circular wait condition detected between multiple threads";
        }

        return info;
    }

    bool DeadlockDetector::BreakDeadlock(const DeadlockInfo& deadlock) {
        if (!deadlock.detected) {
            return false;
        }

        spdlog::warn("Attempting to break deadlock: {}", deadlock.description);
        LogDeadlock(deadlock);

        // Attempt to break the deadlock
        bool broken = AttemptBreakDeadlock(deadlock);

        if (broken) {
            s_brokenDeadlockCount++;
            spdlog::info("Successfully broke deadlock");
        } else {
            spdlog::error("Failed to break deadlock - manual intervention may be required");
        }

        return broken;
    }

    size_t DeadlockDetector::GetDeadlockCount() {
        return s_deadlockCount;
    }

    size_t DeadlockDetector::GetBrokenDeadlockCount() {
        return s_brokenDeadlockCount;
    }

    void DeadlockDetector::SetEnabled(bool enabled) {
        s_enabled = enabled;
        spdlog::info("DeadlockDetector {}", enabled ? "enabled" : "disabled");
    }

    void DeadlockDetector::SetDeadlockTimeout(std::chrono::milliseconds timeout) {
        s_deadlockTimeout = timeout;
        spdlog::info("DeadlockDetector timeout set to {}ms", timeout.count());
    }

    bool DeadlockDetector::IsThreadWaitingTooLong(const LockAcquisition& acquisition) {
        auto now = std::chrono::steady_clock::now();
        auto waitTime = now - acquisition.acquisitionTime;
        auto waitMs = std::chrono::duration_cast<std::chrono::milliseconds>(waitTime);
        
        return waitMs > acquisition.maxWaitTime;
    }

    bool DeadlockDetector::FindCircularWait(std::vector<LockAcquisition>& chain) {
        // Simplified circular wait detection
        // In a full implementation, this would build a wait-for graph
        // and detect cycles using DFS or similar algorithm
        
        // For now, we check if multiple threads are waiting for locks
        // that are held by other waiting threads
        
        std::unordered_map<std::string, std::thread::id> lockOwners;
        std::unordered_map<std::thread::id, std::string> threadWaiting;

        // Build lock ownership and waiting maps
        for (const auto& [threadId, locks] : s_threadLocks) {
            for (const auto& acquisition : locks) {
                if (acquisition.isAcquired) {
                    lockOwners[acquisition.lockName] = threadId;
                } else {
                    threadWaiting[threadId] = acquisition.lockName;
                }
            }
        }

        // Check for circular dependencies
        for (const auto& [waitingThread, waitingLock] : threadWaiting) {
            auto ownerIt = lockOwners.find(waitingLock);
            if (ownerIt != lockOwners.end()) {
                auto owningThread = ownerIt->second;
                
                // Check if owning thread is also waiting
                auto ownerWaitingIt = threadWaiting.find(owningThread);
                if (ownerWaitingIt != threadWaiting.end()) {
                    // Potential circular wait
                    // Check if the lock the owner is waiting for is held by the waiting thread
                    auto ownerWaitingLock = ownerWaitingIt->second;
                    auto ownerWaitingLockOwner = lockOwners.find(ownerWaitingLock);
                    
                    if (ownerWaitingLockOwner != lockOwners.end() && 
                        ownerWaitingLockOwner->second == waitingThread) {
                        // Circular wait detected!
                        // Build the chain
                        for (const auto& [threadId, locks] : s_threadLocks) {
                            if (threadId == waitingThread || threadId == owningThread) {
                                for (const auto& acquisition : locks) {
                                    chain.push_back(acquisition);
                                }
                            }
                        }
                        return true;
                    }
                }
            }
        }

        return false;
    }

    void DeadlockDetector::LogDeadlock(const DeadlockInfo& deadlock) {
        s_deadlockCount++;

        spdlog::error("╔════════════════════════════════════════╗");
        spdlog::error("║         DEADLOCK DETECTED              ║");
        spdlog::error("╚════════════════════════════════════════╝");
        spdlog::error("Description: {}", deadlock.description);
        spdlog::error("Wait Duration: {}ms", deadlock.waitDuration.count());
        
        spdlog::error("Involved Threads ({}):", deadlock.involvedThreads.size());
        for (const auto& threadId : deadlock.involvedThreads) {
            spdlog::error("  - Thread {}", std::hash<std::thread::id>{}(threadId));
        }
        
        spdlog::error("Involved Locks ({}):", deadlock.involvedLocks.size());
        for (const auto& lockName : deadlock.involvedLocks) {
            spdlog::error("  - {}", lockName);
        }
        
        spdlog::error("Total deadlocks detected: {}", s_deadlockCount);
    }

    bool DeadlockDetector::AttemptBreakDeadlock(const DeadlockInfo& deadlock) {
        // In a real implementation, we would:
        // 1. Identify the oldest lock in the deadlock chain
        // 2. Force release that lock (if possible)
        // 3. Allow other threads to proceed
        // 4. Re-acquire the lock later
        
        // For now, we just log the attempt
        // Breaking deadlocks safely is very complex and risky
        // In most cases, it's better to prevent them in the first place
        
        spdlog::warn("Deadlock breaking is not fully implemented");
        spdlog::warn("Recommend restarting the application if deadlock persists");
        
        // Return false to indicate we couldn't break it
        return false;
    }

}  // namespace ThreadSafety
