// Copyright (C) 2024-2026 Parker Chace
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of Skyrim Crash Guard.
//
// Skyrim Crash Guard is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version.
//
// Skyrim Crash Guard is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <https://www.gnu.org/licenses/>.

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
