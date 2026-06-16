// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "PerformanceMetrics.h"
#include <Windows.h>
#include <Psapi.h>

namespace CrashGuard {

    void PerformanceMonitor::Update() {
        UpdateFPS();
        UpdateMemory();
    }

    void PerformanceMonitor::UpdateFPS() {
        auto now = std::chrono::high_resolution_clock::now();
        
        if (m_lastFrameTime.time_since_epoch().count() > 0) {
            // Calculate frame time in milliseconds (use duration_cast to milliseconds directly)
            auto duration = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(now - m_lastFrameTime);
            float frameTimeMs = duration.count();
            
            // Sanity check - frame time should be between 1ms (1000 FPS) and 1000ms (1 FPS)
            if (frameTimeMs < 1.0f || frameTimeMs > 1000.0f) {
                // Skip this frame if timing is unrealistic
                m_lastFrameTime = now;
                return;
            }
            
            m_frameTimes.push_back(frameTimeMs);
            if (m_frameTimes.size() > MAX_FRAME_SAMPLES) {
                m_frameTimes.pop_front();
            }
            
            // Calculate current FPS
            m_metrics.frameTimeMs = frameTimeMs;
            m_metrics.currentFPS = frameTimeMs > 0.0f ? 1000.0f / frameTimeMs : 0.0f;
            
            // Calculate averages
            if (!m_frameTimes.empty()) {
                float sum = 0.0f;
                float min = m_frameTimes[0];
                float max = m_frameTimes[0];
                
                for (float ft : m_frameTimes) {
                    sum += ft;
                    if (ft < min) min = ft;
                    if (ft > max) max = ft;
                }
                
                m_metrics.averageFrameTimeMs = sum / m_frameTimes.size();
                m_metrics.averageFPS = 1000.0f / m_metrics.averageFrameTimeMs;
                m_metrics.minFPS = max > 0.0f ? 1000.0f / max : 0.0f;
                m_metrics.maxFPS = min > 0.0f ? 1000.0f / min : 0.0f;
            }
        }
        
        m_lastFrameTime = now;
    }

    void PerformanceMonitor::UpdateMemory() {
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            m_metrics.memoryUsageMB = pmc.WorkingSetSize / (1024 * 1024);
            m_metrics.peakMemoryMB = pmc.PeakWorkingSetSize / (1024 * 1024);
        }
        
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus)) {
            m_metrics.availableMemoryMB = memStatus.ullAvailPhys / (1024 * 1024);
        }
    }

}  // namespace CrashGuard
