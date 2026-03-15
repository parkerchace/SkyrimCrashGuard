// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RE {
    class Actor;
    class TESObjectREFR;
    class BGSSoundDescriptorForm;
    class NiAVObject;
    class NiPoint3;
}

namespace CrashGuard {

// ═══════════════════════════════════════════════════════════════════════════
// EngineOptimizer - Modern Game Engine Optimization Techniques
// ═══════════════════════════════════════════════════════════════════════════
// Implements optimization techniques inspired by:
// - Frostbite Engine (Star Wars Battlefront 2017) - Texture streaming, culling
// - CryEngine (Crysis) - LOD systems, visibility culling
// - IdTech (DOOM 2016) - Efficient occlusion culling, frame budgeting
// - Unreal Engine (Skyblivion approach) - Hierarchical LOD, async loading
// - Unity HDRP - Temporal coherence, batching
//
// These optimizations reduce CPU/GPU load for distant or non-visible objects
// while maintaining visual and gameplay fidelity for nearby elements.
// ═══════════════════════════════════════════════════════════════════════════

// LOD Levels for various systems
enum class MeshLODLevel : uint8_t {
    Full = 0,
    LOD1 = 1,
    LOD2 = 2,
    LOD3 = 3,
    Billboard = 4,
    Culled = 5
};

enum class AnimationLODLevel : uint8_t {
    FullRate = 0,     // Every frame
    HalfRate = 1,     // Every 2 frames
    QuarterRate = 2,  // Every 4 frames
    EighthRate = 3,   // Every 8 frames
    Frozen = 4        // No updates
};

enum class PhysicsLODLevel : uint8_t {
    Full = 0,
    Simplified = 1,
    Sleeping = 2,
    Disabled = 3
};

// HLOD Cluster for grouping distant objects
struct HLODCluster {
    RE::NiPoint3 center;
    float radius;
    std::vector<uint32_t> memberFormIDs;
    MeshLODLevel currentLOD;
    bool isActive;
};

// Frame budget tracking
struct FrameBudget {
    float targetMs = 11.1f;       // 90 FPS
    float lastFrameMs = 0.0f;
    float averageFrameMs = 0.0f;
    float peakFrameMs = 0.0f;
    bool budgetExceeded = false;
    int qualityLevel = 100;       // 0-100, adjusted dynamically
};

class EngineOptimizer {
public:
    static EngineOptimizer& GetInstance();

    // Lifecycle
    void Initialize();
    void Shutdown();
    void Update(float deltaTime);
    void OnFrameStart();
    void OnFrameEnd();

    // === Core Optimization Passes ===
    void OptimizeActors();
    void OptimizeParticles();
    void OptimizeShadows();
    void OptimizeScripts();
    
    // === Advanced Optimization Passes (CryEngine/IdTech/Unreal) ===
    void OptimizeMeshLOD();         // Object mesh detail reduction
    void OptimizeGrass();           // Grass and flora density
    void OptimizeTrees();           // Tree LOD and billboarding
    void OptimizeAnimations();      // Animation update frequency
    void OptimizePhysics();         // Physics simulation detail
    void OptimizeDecals();          // Decal culling
    void OptimizeLights();          // Dynamic light culling
    void OptimizeWeather();         // Weather effect optimization
    void OptimizeWater();           // Water rendering optimization
    void UpdateHLOD();              // Hierarchical LOD clusters
    void ManageFrameBudget();       // DOOM-style frame budgeting
    void ProcessAsyncLoading();     // Unreal-style async loading

    // === Decision Functions ===
    bool ShouldThrottleScript(RE::Actor* actor);
    bool ShouldCullParticles(RE::TESObjectREFR* ref);
    bool ShouldDisableShadow(RE::TESObjectREFR* ref);
    bool ShouldCullDecal(RE::TESObjectREFR* ref);
    bool ShouldCullLight(RE::TESObjectREFR* light);
    bool ShouldCullGrass(RE::TESObjectREFR* grass);
    bool ShouldUseTreeBillboard(RE::TESObjectREFR* tree);
    bool ShouldSleepPhysics(RE::TESObjectREFR* ref);
    
    // === LOD Level Functions ===
    int GetParticleLODLevel(float distance);
    float GetScriptInterval(float distance);
    MeshLODLevel GetMeshLODLevel(float distance);
    AnimationLODLevel GetAnimationLODLevel(float distance);
    PhysicsLODLevel GetPhysicsLODLevel(float distance);
    float GetGrassDensityMultiplier(float distance);
    
    // === Frame Budget Functions ===
    float GetRemainingFrameBudgetMs() const;
    int GetDynamicQualityLevel() const;
    void SetTargetFrameRate(float fps);
    bool IsBudgetExceeded() const { return m_frameBudget.budgetExceeded; }
    
    // === HLOD Functions ===
    void BuildHLODClusters();
    void UpdateHLODCluster(HLODCluster& cluster);
    
    // === Statistics ===
    struct Stats {
        std::atomic<uint32_t> actorsOptimized{0};
        std::atomic<uint32_t> particlesCulled{0};
        std::atomic<uint32_t> shadowsDisabled{0};
        std::atomic<uint32_t> scriptsThrottled{0};
        std::atomic<uint32_t> meshesLODed{0};
        std::atomic<uint32_t> grassCulled{0};
        std::atomic<uint32_t> treesBillboarded{0};
        std::atomic<uint32_t> animationsReduced{0};
        std::atomic<uint32_t> physicsSleeping{0};
        std::atomic<uint32_t> decalsCulled{0};
        std::atomic<uint32_t> lightsCulled{0};
        std::atomic<uint32_t> drawCallsSaved{0};
        std::atomic<uint32_t> hLODClustersActive{0};
        std::atomic<uint64_t> estimatedCPUSavedUs{0};
        std::atomic<uint64_t> estimatedGPUSavedUs{0};
        std::atomic<uint64_t> bytesStreamed{0};
        std::atomic<float> averageFrameTimeMs{0.0f};
    };
    const Stats& GetStats() const { return m_stats; }
    void ResetStats();

    // State queries
    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    const FrameBudget& GetFrameBudget() const { return m_frameBudget; }

private:
    EngineOptimizer() = default;
    ~EngineOptimizer() = default;
    EngineOptimizer(const EngineOptimizer&) = delete;
    EngineOptimizer& operator=(const EngineOptimizer&) = delete;

    // Internal helpers
    float GetDistanceToPlayer(RE::TESObjectREFR* ref);
    float GetDistanceToPlayer(const RE::NiPoint3& pos);
    bool IsInPlayerFOV(RE::TESObjectREFR* ref, float fovAngle = 90.0f);
    bool IsInPlayerFOV(const RE::NiPoint3& pos, float fovAngle = 90.0f);
    void ApplyActorLOD(RE::Actor* actor, int lodLevel);
    void ApplyParticleLOD(RE::NiAVObject* particleSystem, int lodLevel);
    void ApplyMeshLOD(RE::TESObjectREFR* ref, MeshLODLevel level);
    void ApplyAnimationLOD(RE::Actor* actor, AnimationLODLevel level);
    void ApplyPhysicsLOD(RE::TESObjectREFR* ref, PhysicsLODLevel level);
    
    // Temporal coherence - cache last frame's visibility decisions
    std::unordered_map<uint32_t, bool> m_lastFrameVisibility;
    std::unordered_map<uint32_t, MeshLODLevel> m_lastFrameMeshLOD;
    
    // Script throttling tracking
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> m_lastScriptRun;
    std::mutex m_scriptMutex;

    // Animation LOD tracking
    std::unordered_map<uint32_t, uint32_t> m_animationFrameCounter;
    std::unordered_map<uint32_t, AnimationLODLevel> m_animationLODLevel;
    std::mutex m_animMutex;
    
    // Physics LOD tracking
    std::unordered_map<uint32_t, PhysicsLODLevel> m_physicsLODLevel;
    std::mutex m_physicsMutex;

    // HLOD clusters
    std::vector<HLODCluster> m_hLODClusters;
    std::mutex m_hLODMutex;
    float m_hLODRebuildTimer = 0.0f;
    
    // Frame budget
    FrameBudget m_frameBudget;
    std::chrono::high_resolution_clock::time_point m_frameStartTime;
    std::deque<float> m_frameTimeHistory;  // For averaging
    static constexpr size_t FRAME_HISTORY_SIZE = 60;
    
    // Async loading queue
    std::vector<uint32_t> m_asyncLoadQueue;
    std::mutex m_asyncLoadMutex;

    // Timing
    float m_timeSinceLastUpdate = 0.0f;
    float m_updateInterval = 0.1f;  // 100ms between optimization passes
    float m_frameCount = 0;

    // State
    bool m_enabled = true;
    bool m_initialized = false;
    Stats m_stats;
};

}  // namespace CrashGuard
