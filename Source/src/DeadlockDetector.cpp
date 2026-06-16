// Copyright (C) 2026 Parker Chace
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
    std::thread DeadlockDetector::s_watchdogThread;
    std::atomic<bool> DeadlockDetector::s_watchdogRunning{false};

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

        // Start the watchdog thread. It wakes every half-timeout period and calls
        // CheckForDeadlock(). If the check itself can't acquire the mutex (because
        // a monitored thread is holding it), the watchdog skips that beat rather
        // than blocking, so the watchdog can't participate in the deadlock it's
        // trying to detect.
        s_watchdogRunning = true;
        s_watchdogThread = std::thread([]() {
            spdlog::debug("[DeadlockDetector] Watchdog started (period: {}ms)",
                         s_deadlockTimeout.count() / 2);
            while (s_watchdogRunning.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(s_deadlockTimeout / 2);

                if (!s_enabled || !s_initialized) continue;

                // Non-blocking try: if the detector mutex is already held we skip
                // this beat to avoid the watchdog itself blocking.
                std::unique_lock<std::mutex> lk(s_detectorMutex, std::try_to_lock);
                if (!lk.owns_lock()) continue;

                // Snapshot the lock table while holding the mutex, then release
                // before calling BreakDeadlock (which re-acquires internally).
                auto now = std::chrono::steady_clock::now();
                DeadlockInfo info;
                info.detected = false;

                for (const auto& [tid, locks] : s_threadLocks) {
                    for (const auto& acq : locks) {
                        if (!acq.isAcquired) {
                            auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now - acq.acquisitionTime);
                            if (wait > acq.maxWaitTime) {
                                info.detected = true;
                                info.involvedThreads.push_back(tid);
                                info.involvedLocks.push_back(acq.lockName);
                                info.waitDuration = wait;
                                info.description = fmt::format(
                                    "Thread {} waiting for '{}' for {}ms (limit {}ms)",
                                    std::hash<std::thread::id>{}(tid),
                                    acq.lockName, wait.count(), acq.maxWaitTime.count());
                                break;
                            }
                        }
                    }
                    if (info.detected) break;
                }
                lk.unlock();

                if (info.detected) {
                    spdlog::warn("[DeadlockDetector] Watchdog: {}", info.description);
                    BreakDeadlock(info);
                }
            }
            spdlog::debug("[DeadlockDetector] Watchdog stopped");
        });

        s_initialized = true;
        spdlog::info("DeadlockDetector initialized with {}ms timeout, watchdog active",
                     s_deadlockTimeout.count());
    }

    void DeadlockDetector::Shutdown() {
        if (!s_initialized) {
            return;
        }

        spdlog::info("DeadlockDetector shutting down");
        spdlog::info("  Total deadlocks detected: {}", s_deadlockCount);
        spdlog::info("  Deadlocks broken: {}", s_brokenDeadlockCount);

        // Stop and join the watchdog thread before clearing state
        s_watchdogRunning = false;
        if (s_watchdogThread.joinable()) {
            s_watchdogThread.join();
        }

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

        // Log the deadlock details so they appear in the crash log.
        // Forcing a lock release from a DLL plugin is not safe — it could corrupt
        // the game's internal state and cause a worse crash than the deadlock itself.
        // We detect, log, and return false. The watchdog thread will keep watching.
        LogDeadlock(deadlock);
        return false;
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
        // Detect circular lock waits by building two maps:
        //   lockOwners   — which thread currently holds each lock
        //   threadWaiting — which lock each thread is blocked on
        // Then we follow the wait chain from any blocked thread:
        // if we ever revisit a thread we've already seen, that loop IS a deadlock.
        // This is O(n) per detection pass and safe to call from the watchdog thread.
        
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

}  // namespace ThreadSafety
