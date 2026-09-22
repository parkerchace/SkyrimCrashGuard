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

#include <chrono>
#include <deque>
#include <atomic>

namespace CrashGuard {

    struct PerformanceMetrics {
        // FPS tracking
        float currentFPS = 0.0f;
        float averageFPS = 0.0f;
        float minFPS = 0.0f;
        float maxFPS = 0.0f;
        
        // Frame time tracking
        float frameTimeMs = 0.0f;
        float averageFrameTimeMs = 0.0f;
        
        // Memory tracking
        size_t memoryUsageMB = 0;
        size_t peakMemoryMB = 0;
        size_t availableMemoryMB = 0;
        
        // Crash Guard stats
        uint32_t crashesPrevented = 0;
        uint32_t meshesValidated = 0;
        uint32_t animationsValidated = 0;
        uint32_t scriptsMonitored = 0;
        uint32_t cellsValidated = 0;
        
        // Recovery stats
        uint32_t recoveryAttempts = 0;
        uint32_t successfulRecoveries = 0;
        uint32_t failedRecoveries = 0;
        
        // Pattern learning
        uint32_t patternsLearned = 0;
        uint32_t patternsApplied = 0;
    };

    class PerformanceMonitor {
    public:
        static PerformanceMonitor& GetSingleton() {
            static PerformanceMonitor instance;
            return instance;
        }

        void Update();
        const PerformanceMetrics& GetMetrics() const { return m_metrics; }
        
        void IncrementCrashesPrevented() { m_metrics.crashesPrevented++; }
        void IncrementMeshesValidated() { m_metrics.meshesValidated++; }
        void IncrementAnimationsValidated() { m_metrics.animationsValidated++; }
        void IncrementScriptsMonitored() { m_metrics.scriptsMonitored++; }
        void IncrementCellsValidated() { m_metrics.cellsValidated++; }
        void IncrementRecoveryAttempt() { m_metrics.recoveryAttempts++; }
        void IncrementSuccessfulRecovery() { m_metrics.successfulRecoveries++; }
        void IncrementFailedRecovery() { m_metrics.failedRecoveries++; }
        void IncrementPatternsLearned() { m_metrics.patternsLearned++; }
        void IncrementPatternsApplied() { m_metrics.patternsApplied++; }

    private:
        PerformanceMonitor() = default;
        
        void UpdateFPS();
        void UpdateMemory();
        
        PerformanceMetrics m_metrics;
        std::deque<float> m_frameTimes;
        std::chrono::high_resolution_clock::time_point m_lastFrameTime;
        
        static constexpr size_t MAX_FRAME_SAMPLES = 120;
    };

    struct OverlaySettings {
        bool enabled = false;
        bool showFPS = true;
        bool showFrameTime = true;
        bool showMemory = true;
        bool showCrashStats = true;
        bool showRecoveryStats = false;
        bool showPatternStats = false;
        
        // Position (0-3: TopLeft, TopRight, BottomLeft, BottomRight)
        int position = 1;  // TopRight
        
        // Transparency
        float backgroundAlpha = 0.35f;
        float textAlpha = 0.9f;
        
        // Size
        float scale = 1.0f;
    };

}  // namespace CrashGuard
