// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <RE/Skyrim.h>
#include <vector>
#include <string>
#include <chrono>

namespace CrashGuard {

    enum class DebugObjectType {
        None,
        BadMesh,
        BadAnimation,
        BadScript,
        BadCell,
        BadSound,
        BadTrigger,
        NullPointer,
        InvalidReference
    };

    enum class VisualizationMode {
        ByType = 0,
        ByEstimatedCost,
        ByEventCategory
    };

    struct DebugObject {
        RE::NiPointer<RE::NiAVObject> node;
        RE::NiPoint3 position;
        DebugObjectType type;
        std::string description;
        std::chrono::steady_clock::time_point timestamp;
        float lifetime;  // seconds
        RE::FormID formID;
        std::string modName;
        float costScore = 0.0f; // heuristic cost metric (higher = more expensive)
        
        bool IsExpired() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp).count();
            return elapsed >= lifetime;
        }
    };

    class DebugVisualization {
    public:
        static DebugVisualization& GetSingleton() {
            static DebugVisualization instance;
            return instance;
        }

        void Initialize();
        void Update();
        void Render();
        
        // Add debug markers
        void MarkBadMesh(RE::NiAVObject* object, const std::string& reason);
        void MarkBadAnimation(RE::Actor* actor, const std::string& animPath, const std::string& reason);
        void MarkBadScript(RE::TESObjectREFR* ref, const std::string& scriptName, const std::string& reason);
        void MarkBadCell(RE::TESObjectCELL* cell, const std::string& reason);
        void MarkBadSound(const RE::NiPoint3& position, const std::string& soundPath, const std::string& reason);
        void MarkBadTrigger(RE::TESObjectREFR* trigger, const std::string& reason);
        void MarkNullPointer(const RE::NiPoint3& position, const std::string& context);
        void MarkInvalidReference(RE::TESObjectREFR* ref, const std::string& reason);
        
        // Clear markers
        void ClearAll();
        void ClearExpired();
        
        // Settings
        void SetEnabled(bool enabled) { m_enabled = enabled; }
        bool IsEnabled() const { return m_enabled; }
        
        void SetDefaultLifetime(float seconds) { m_defaultLifetime = seconds; }
        float GetDefaultLifetime() const { return m_defaultLifetime; }
        
        void SetMaxDistance(float distance) { m_maxDistance = distance; }
        float GetMaxDistance() const { return m_maxDistance; }
        
        void SetShowLabels(bool show) { m_showLabels = show; }
        bool GetShowLabels() const { return m_showLabels; }
        
        const std::vector<DebugObject>& GetDebugObjects() const { return m_debugObjects; }

        // Lumen-style debug visualization (simulated)
        void SetLumenDebugMode(bool enabled) { m_lumenMode = enabled; }
        bool IsLumenDebugMode() const { return m_lumenMode; }
        void SetLumenProbeCount(int count) { m_lumenProbeCount = std::max(1, std::min(256, count)); }
        int GetLumenProbeCount() const { return m_lumenProbeCount; }
        void SetLumenProbeDistance(float dist) { m_lumenProbeDistance = std::max(100.0f, dist); }
        float GetLumenProbeDistance() const { return m_lumenProbeDistance; }
        void SetLumenShowRays(bool show) { m_lumenShowRays = show; }
        bool GetLumenShowRays() const { return m_lumenShowRays; }
        
        // Helper methods for UI
        RE::NiColor GetColorForType(DebugObjectType type);
        std::string GetIconForType(DebugObjectType type);
        RE::NiColor GetColorForObject(const DebugObject& obj, VisualizationMode mode);
        
        // Nanite-style visualization mode accessors
        void SetVisualizationMode(VisualizationMode m);
        VisualizationMode GetVisualizationMode() const;
        
        // Allow external subsystems to set/override the cost score for a marker
        // by FormID (applies to all markers with matching FormID).
        void SetMarkerCost(RE::FormID formID, float cost);

    private:
        DebugVisualization() = default;
        
        void RenderDebugObject(const DebugObject& obj);
        void RenderBoundingBox(const RE::NiPoint3& center, float size, const RE::NiColor& color);
        void RenderLabel(const RE::NiPoint3& worldPos, const std::string& text, const RE::NiColor& color);
        
        void EnforceCapacity();  // Remove oldest markers if at capacity
        
        std::vector<DebugObject> m_debugObjects;
        bool m_enabled = false;
        float m_defaultLifetime = 10.0f;  // seconds
        float m_maxDistance = 5000.0f;  // units
        bool m_showLabels = true;
        static constexpr size_t MAX_MARKERS = 1000;  // Memory safety limit
        // Lumen simulation settings
        bool m_lumenMode = false;
        int m_lumenProbeCount = 32;
        float m_lumenProbeDistance = 2000.0f;
        bool m_lumenShowRays = true;
        // Nanite-style visualization mode
        VisualizationMode m_visualizationMode = VisualizationMode::ByType;
    };

}  // namespace CrashGuard
