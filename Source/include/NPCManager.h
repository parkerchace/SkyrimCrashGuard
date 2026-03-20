// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT

#pragma once

#include <RE/Skyrim.h>
#include <atomic>
#include <vector>
#include <mutex>
#include <queue>

namespace CrashGuard {

/// Reactive NPC management system that:
/// - Counts NPCs every frame to detect massive spawns immediately
/// - Emergency mode at 2x threshold (100 NPCs with default 50 max)
/// - Ultra-fast deletion removes 500+ NPCs per frame during emergencies
/// - Uses per-cell baseline detection for modded playthroughs
/// - Disables NPCs temporarily instead of deleting (can restore later)
/// - Smart prioritization based on NPC burden/complexity
/// - Whitelist/blacklist system for fine control
/// NOTE: PlaceAtMe hook is DISABLED due to stack alignment crashes
class NPCManager {
public:
    struct DisabledNPC {
        RE::ActorHandle actorHandle;  // SAFETY: Use handle instead of raw pointer
        RE::TESObjectCELL* cell = nullptr;
        std::string name;
        uint32_t formID = 0;
        float disabledTime = 0.0f;
        int burden = 0; // Complexity score
        
        // Helper to safely get actor pointer with validation
        RE::Actor* GetActor() const {
            auto actorPtr = actorHandle.get();
            if (!actorPtr) return nullptr;
            auto actor = actorPtr.get();
            if (!actor || actor->IsDeleted() || actor->IsMarkedForDeletion()) {
                return nullptr;
            }
            return actor;
        }
        
        // Check if this cached reference is still valid
        bool IsValid() const {
            return GetActor() != nullptr;
        }
    };
    
    struct CellBaseline {
        RE::TESObjectCELL* cell = nullptr;
        uint32_t baselineCount = 0;
        float learnedTime = 0.0f;
        bool isLearned = false;
    };

    static NPCManager& GetSingleton() {
        static NPCManager instance;
        return instance;
    }

    void Initialize();
    void Update(float deltaTime);
    
    /// Get current active NPC count
    uint32_t GetActiveNPCCount();
    
    /// Check if we can spawn more NPCs (not used - hook disabled)
    bool CanSpawnNPC();
    
    /// Force a recount of NPCs
    void ForceAudit();
    
    /// Clean up non-essential dead bodies
    void CleanupDeadBodies();
    
    /// Remove excess NPCs when over threshold (reactive approach)
    void RemoveExcessNPCs(uint32_t excessCount);
    
    /// Restore disabled NPCs when room is available
    void RestoreDisabledNPCs(uint32_t count);
    
    /// Learn baseline NPC count for current cell
    void LearnCellBaseline();
    
    /// Get effective threshold for current cell
    uint32_t GetEffectiveThreshold() const;
    
    struct Stats {
        uint32_t activeNPCs = 0;
        uint32_t deadBodies = 0;
        uint32_t excessRemoved = 0;
        uint32_t bodiesRemoved = 0;
        uint32_t disabledNPCs = 0;      // Currently disabled
        uint32_t restoredNPCs = 0;      // Restored from disabled
        uint32_t cellBaseline = 0;      // Current cell baseline
        uint32_t effectiveThreshold = 0; // Actual threshold being used
    };
    
    Stats GetStats() const;
    
    /// Get number of queued spawn requests
    uint32_t GetQueuedSpawnCount() const;
    
    /// Check if spawn hooks are installed and working
    bool AreHooksInstalled() const { return m_hooksInstalled; }

private:
    NPCManager() = default;
    ~NPCManager() = default;
    NPCManager(const NPCManager&) = delete;
    NPCManager& operator=(const NPCManager&) = delete;

    void CountNPCs();
    bool IsEssentialActor(RE::Actor* actor);
    bool IsQuestActor(RE::Actor* actor);
    bool IsWhitelistedNPC(RE::Actor* actor);
    bool IsBlacklistedNPC(RE::Actor* actor);
    int CalculateNPCBurden(RE::Actor* actor);
    void InstallSpawnHooks();
    uint32_t GetEffectiveThresholdInternal() const;  // Internal version without mutex lock
    
    // SAFETY FEATURES: Validation helpers to prevent use-after-free
    bool IsValidActorPointer(RE::Actor* actor);
    bool IsValidVTable(RE::Actor* actor);
    void InvalidateCachedActors();
    void ValidateDisabledNPCs();  // Periodic validation of cached actors
    
    // PlaceAtMe hook (DISABLED - causes stack alignment crashes)
    // Kept for potential future implementation with different hooking method
    static RE::TESObjectREFR* PlaceAtMe_Hook(
        RE::TESObjectREFR* a_self,
        RE::TESBoundObject* a_baseObject,
        std::int32_t a_count,
        bool a_forcePersist,
        bool a_initiallyDisabled);
    
    std::atomic<uint32_t> m_activeNPCCount{0};
    std::atomic<uint32_t> m_deadBodyCount{0};
    std::atomic<uint32_t> m_excessRemoved{0};
    std::atomic<uint32_t> m_bodiesRemoved{0};
    std::atomic<uint32_t> m_restoredNPCs{0};
    std::atomic<uint32_t> m_spawnsReleased{0}; // Kept for compatibility
    std::atomic<bool> m_emergencyMode{false};  // Emergency mode flag - activates at 2x threshold
    std::atomic<bool> m_gameFullyLoaded{false}; // Tracks when game is fully loaded
    
    float m_timeSinceLastUpdate = 0.0f;
    float m_updateInterval = 0.5f;  // Check every 0.5 seconds (was 2.0s) for faster response
    float m_timeSinceLastValidation = 0.0f;  // SAFETY: Track validation timing
    float m_validationInterval = 5.0f;  // SAFETY: Validate cached actors every 5 seconds
    
    mutable std::mutex m_mutex;
    std::vector<DisabledNPC> m_disabledNPCs;
    std::unordered_map<RE::TESObjectCELL*, CellBaseline> m_cellBaselines;
    RE::TESObjectCELL* m_currentCell = nullptr;
    
    bool m_initialized = false;
    bool m_hooksInstalled = false;
};

}  // namespace CrashGuard
