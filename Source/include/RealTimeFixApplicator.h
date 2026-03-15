// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

// CommonLibSSE must come before any Windows headers
#include <RE/Skyrim.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <shared_mutex>
#include <cstdint>

/// Real-Time Fix Applicator
/// Applies fixes to game objects in real-time without requiring a crash
namespace RealTimeFix {

    /// Fix application result
    struct FixResult {
        bool success;
        std::string action;
        std::string details;
        std::chrono::steady_clock::time_point timestamp;
    };

    /// Mesh replacement tracking
    struct MeshReplacementInfo {
        RE::NiAVObject* originalMesh;
        RE::NiAVObject* replacementMesh;
        std::string originalPath;
        std::string reason;
        std::chrono::steady_clock::time_point replacedAt;
        bool persistOnSave;
    };

    /// Animation replacement tracking
    struct AnimationReplacementInfo {
        RE::Actor* actor;
        std::string originalAnimation;
        std::string replacementAnimation;
        std::string reason;
        std::chrono::steady_clock::time_point replacedAt;
    };

    /// Script disable tracking
    struct ScriptDisableInfo {
        std::string scriptName;
        std::string reason;
        std::chrono::steady_clock::time_point disabledAt;
        uint32_t disableCount;
    };

    /// Texture replacement tracking
    struct TextureReplacementInfo {
        RE::NiAVObject* object;
        std::string originalTexturePath;
        std::string replacementTexturePath;
        std::string reason;
        std::chrono::steady_clock::time_point replacedAt;
    };

    /// Reference removal tracking
    struct ReferenceRemovalInfo {
        RE::FormID referenceFormID;
        RE::FormID cellFormID;
        std::string reason;
        std::chrono::steady_clock::time_point removedAt;
    };

    /// Main real-time fix applicator class
    class RealTimeFixApplicator {
    public:
        /// Initialize the real-time fix applicator
        static bool Initialize();

        /// Shutdown and cleanup
        static void Shutdown();

        /// Replace bad mesh with placeholder immediately
        static FixResult ReplaceMeshRealTime(RE::NiAVObject* badMesh, 
                                            const std::string& meshPath,
                                            const std::string& reason);

        /// Update scene graph after mesh replacement
        static bool UpdateSceneGraph(RE::NiAVObject* oldMesh, RE::NiAVObject* newMesh);

        /// Persist mesh replacement on save
        static bool PersistMeshReplacements();

        /// Switch to default idle on animation failure
        static FixResult SwitchAnimationRealTime(RE::Actor* actor,
                                                const std::string& failedAnimation,
                                                const std::string& reason);

        /// Update animation state
        static bool UpdateAnimationState(RE::Actor* actor, const std::string& newAnimation);

        /// Disable corrupted script immediately
        static FixResult DisableScriptRealTime(const std::string& scriptName,
                                              const std::string& reason);

        /// Log disabled script
        static void LogDisabledScript(const std::string& scriptName, const std::string& reason);

        /// Apply default texture for missing one
        static FixResult ReplaceTextureRealTime(RE::NiAVObject* object,
                                               const std::string& missingTexturePath,
                                               const std::string& reason);

        /// Update material properties
        static bool UpdateMaterialProperties(RE::NiAVObject* object, 
                                            const std::string& newTexturePath);

        /// Remove invalid reference from cell
        static FixResult RemoveReferenceRealTime(RE::TESObjectREFR* reference,
                                                RE::TESObjectCELL* cell,
                                                const std::string& reason);

        /// Update cell data after reference removal
        static bool UpdateCellData(RE::TESObjectCELL* cell, RE::TESObjectREFR* removedRef);

        // Statistics and tracking
        static size_t GetMeshReplacementCount();
        static size_t GetAnimationSwitchCount();
        static size_t GetScriptDisableCount();
        static size_t GetTextureReplacementCount();
        static size_t GetReferenceRemovalCount();

        /// Get all applied fixes for reporting
        static std::vector<FixResult> GetAppliedFixes();

        /// Clear fix history (for testing)
        static void ClearFixHistory();

    private:
        /// Find parent node in scene graph
        static RE::NiNode* FindParentNode(RE::NiAVObject* object);

        /// Detach object from parent
        static bool DetachFromParent(RE::NiAVObject* object);

        /// Attach object to parent
        static bool AttachToParent(RE::NiNode* parent, RE::NiAVObject* object);

        /// Update bounding volumes after mesh change
        static void UpdateBoundingVolumes(RE::NiNode* node);

        /// Get default texture path
        static std::string GetDefaultTexturePath();

        /// Get default idle animation
        static std::string GetDefaultIdleAnimation();

        /// Validate mesh replacement is safe
        static bool ValidateMeshReplacement(RE::NiAVObject* oldMesh, RE::NiAVObject* newMesh);

        /// Validate animation switch is safe
        static bool ValidateAnimationSwitch(RE::Actor* actor, const std::string& newAnimation);

        /// Validate reference removal is safe
        static bool ValidateReferenceRemoval(RE::TESObjectREFR* reference);

        /// Record fix for persistence
        static void RecordFix(const FixResult& result);

        // State tracking
        static bool s_initialized;
        static std::vector<MeshReplacementInfo> s_meshReplacements;
        static std::vector<AnimationReplacementInfo> s_animationReplacements;
        static std::unordered_set<std::string> s_disabledScripts;
        static std::vector<ScriptDisableInfo> s_scriptDisables;
        static std::vector<TextureReplacementInfo> s_textureReplacements;
        static std::vector<ReferenceRemovalInfo> s_referenceRemovals;
        static std::vector<FixResult> s_fixHistory;

        // Statistics
        static size_t s_meshReplacementCount;
        static size_t s_animationSwitchCount;
        static size_t s_scriptDisableCount;
        static size_t s_textureReplacementCount;
        static size_t s_referenceRemovalCount;

        // Thread safety
        static std::shared_mutex s_meshMutex;
        static std::shared_mutex s_animationMutex;
        static std::shared_mutex s_scriptMutex;
        static std::shared_mutex s_textureMutex;
        static std::shared_mutex s_referenceMutex;
        static std::shared_mutex s_historyMutex;
    };

}  // namespace RealTimeFix
