// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <unordered_set>
#include <mutex>
#include <atomic>

namespace CrashGuard {

    /// Simple distance-based animation freezing system
    /// Freezes distant actor animations when performance drops
    /// Actors stay in current pose - no complex state management
    class AnimationOptimizer {
    public:
        static AnimationOptimizer& GetSingleton() {
            static AnimationOptimizer instance;
            return instance;
        }

        /// Initialize the animation optimizer
        void Initialize();

        /// Update animation states based on performance
        /// Call this every frame from main update loop
        void Update(float deltaTime);

        /// Freeze actor's animation at current pose
        void FreezeAnimation(RE::Actor* actor);

        /// Unfreeze actor's animation
        void UnfreezeAnimation(RE::Actor* actor);

        /// Unfreeze all frozen actors
        void UnfreezeAll();

        /// Check if actor is currently frozen
        bool IsFrozen(RE::Actor* actor) const;

        /// Get current freeze distance threshold
        float GetFreezeDistance() const { return m_currentFreezeDistance; }

        /// Get count of frozen actors
        uint32_t GetFrozenCount() const { return static_cast<uint32_t>(m_frozenActors.size()); }

        struct Stats {
            uint32_t totalFreezes;
            uint32_t totalUnfreezes;
            uint32_t currentFrozen;
            float currentFreezeDistance;
            float currentFPS;
        };

        Stats GetStats() const;
        void ResetStats();

    private:
        AnimationOptimizer() = default;
        ~AnimationOptimizer() = default;
        AnimationOptimizer(const AnimationOptimizer&) = delete;
        AnimationOptimizer& operator=(const AnimationOptimizer&) = delete;

        /// Calculate freeze distance based on current FPS
        float CalculateFreezeDistance(float fps) const;

        /// Check if we can freeze this actor
        bool CanFreezeActor(RE::Actor* actor) const;

        /// Get distance from player to actor
        float GetDistanceToPlayer(RE::Actor* actor) const;

        /// Check if player is talking to this actor
        bool IsPlayerTalkingTo(RE::Actor* actor) const;

        std::unordered_set<RE::Actor*> m_frozenActors;
        mutable std::mutex m_mutex;

        float m_currentFreezeDistance = 4096.0f;  // Default: 1 cell
        std::atomic<uint32_t> m_totalFreezes{0};
        std::atomic<uint32_t> m_totalUnfreezes{0};

        // Update throttling
        float m_timeSinceLastUpdate = 0.0f;
        static constexpr float UPDATE_INTERVAL = 0.5f;  // Update every 0.5 seconds
    };

}  // namespace CrashGuard
