// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT

#pragma once

#include <RE/Skyrim.h>
#include <atomic>
#include <vector>
#include <mutex>
#include <queue>

namespace CrashGuard {

/// Hooks actor spawn functions to throttle spawning based on threshold
/// Queues excess spawns and releases them gradually as NPCs are cleared
class ActorSpawnHook {
public:
    static ActorSpawnHook& GetSingleton() {
        static ActorSpawnHook instance;
        return instance;
    }

    /// Initialize spawn hooks
    void Initialize();
    
    /// Update spawn queue - called each frame
    void Update(float deltaTime);
    
    /// Check if an actor should be allowed to spawn immediately
    bool ShouldAllowSpawn();
    
    /// Queue an actor for deferred spawning
    void QueueActor(RE::TESObjectREFR* actor);
    
    /// Process queued actors and spawn them if threshold allows
    void ProcessQueue();
    
    /// Get queue size
    uint32_t GetQueueSize() const { return static_cast<uint32_t>(m_spawnQueue.size()); }
    
    /// Clear the spawn queue
    void ClearQueue();

private:
    ActorSpawnHook() = default;
    ~ActorSpawnHook() = default;
    ActorSpawnHook(const ActorSpawnHook&) = delete;
    ActorSpawnHook& operator=(const ActorSpawnHook&) = delete;

    /// Hook for InitializeActorInstant - called when actors are spawned
    static void Hook_InitializeActorInstant(RE::Actor* actor);
    static inline REL::Relocation<decltype(Hook_InitializeActorInstant)> _InitializeActorInstant;
    
    /// Hook for PlaceObjectAtMe - console command and script spawning
    static RE::TESObjectREFR* Hook_PlaceAtMe(RE::TESObjectREFR* a_this, 
                                              RE::TESBoundObject* a_baseObject,
                                              uint32_t a_count,
                                              bool a_forcePersist,
                                              bool a_initiallyDisabled);
    static inline REL::Relocation<decltype(Hook_PlaceAtMe)> _PlaceAtMe;

    struct QueuedActor {
        RE::FormID formID;
        RE::NiPoint3 position;
        RE::NiPoint3 rotation;
        RE::TESObjectCELL* cell;
        float queueTime;
    };
    
    std::queue<QueuedActor> m_spawnQueue;
    std::mutex m_queueMutex;
    
    float m_timeSinceLastSpawn = 0.0f;
    float m_spawnInterval = 1.0f; // Time between spawning queued actors
    
    bool m_initialized = false;
};

}  // namespace CrashGuard
