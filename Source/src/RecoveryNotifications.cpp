// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "RecoveryNotifications.h"
#include "ImGuiConfigMenu.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace CrashGuard {

    void RecoveryNotifications::AddRecovery(
        const std::string& severity,
        const std::string& rootCause,
        const std::string& strategy,
        const std::vector<std::string>& actions,
        const std::vector<std::string>& suspectedMods,
        bool successful
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Update statistics
        m_totalRecoveries++;
        if (successful) {
            m_successfulRecoveries++;
        } else {
            m_failedRecoveries++;
        }
        
        // Create toast notification
        RecoveryToast toast;
        toast.severity = severity;
        toast.summary = CreateSummary(rootCause, strategy);
        toast.strategy = strategy;
        toast.timestamp = std::chrono::steady_clock::now();
        toast.visible = true;
        
        // Set display time based on severity
        if (severity == "Safe") {
            toast.displayTime = TOAST_DURATION_SAFE;
        } else if (severity == "Warning") {
            toast.displayTime = TOAST_DURATION_WARNING;
        } else {
            toast.displayTime = TOAST_DURATION_CRITICAL;
        }
        
        // Add to active toasts (limit to MAX_ACTIVE_TOASTS)
        if (m_activeToasts.size() >= MAX_ACTIVE_TOASTS) {
            m_activeToasts.erase(m_activeToasts.begin());
        }
        m_activeToasts.push_back(toast);
        
        // Create detailed history entry
        RecoveryEntry entry;
        entry.severity = severity;
        entry.timestamp = FormatTimestamp(toast.timestamp);
        entry.rootCause = rootCause;
        entry.strategy = strategy;
        entry.actions = actions;
        entry.suspectedMods = suspectedMods;
        entry.successful = successful;
        
        // Add to history (limit to MAX_HISTORY)
        m_history.insert(m_history.begin(), entry);
        if (m_history.size() > MAX_HISTORY) {
            m_history.resize(MAX_HISTORY);
        }
        
        // DON'T auto-open F11 menu - just show toast and log
        spdlog::info("[RecoveryNotifications] Crash recovery: {} - {}", severity, rootCause);
    }

    void RecoveryNotifications::RenderToasts() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        UpdateToasts();
        
        if (m_activeToasts.empty()) {
            return;
        }
        
        ImGuiIO& io = ImGui::GetIO();
        float padding = 10.0f;
        float toastWidth = 400.0f;
        float toastSpacing = 10.0f;
        float yOffset = padding;
        
        for (auto& toast : m_activeToasts) {
            if (!toast.visible) continue;
            
            // Calculate fade alpha based on remaining time
            float alpha = 1.0f;
            if (toast.displayTime < 1.0f) {
                alpha = toast.displayTime; // Fade out in last second
            }
            
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - padding, yOffset), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowSize(ImVec2(toastWidth, 0), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.85f * alpha);
            
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | 
                                    ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoFocusOnAppearing |
                                    ImGuiWindowFlags_NoNav |
                                    ImGuiWindowFlags_NoMove;
            
            std::string windowName = "RecoveryToast##" + std::to_string((uintptr_t)&toast);
            
            if (ImGui::Begin(windowName.c_str(), nullptr, flags)) {
                // Color based on severity
                ImVec4 severityColor;
                if (toast.severity == "Safe") {
                    severityColor = ImVec4(0.2f, 1.0f, 0.2f, alpha);
                } else if (toast.severity == "Warning") {
                    severityColor = ImVec4(1.0f, 0.8f, 0.2f, alpha);
                } else {
                    severityColor = ImVec4(1.0f, 0.2f, 0.2f, alpha);
                }
                
                // Header with severity
                ImGui::PushStyleColor(ImGuiCol_Text, severityColor);
                ImGui::Text("Crash Recovered - %s", toast.severity.c_str());
                ImGui::PopStyleColor();
                
                ImGui::Separator();
                
                // Summary
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, alpha));
                ImGui::TextWrapped("%s", toast.summary.c_str());
                ImGui::PopStyleColor();
                
                // Strategy
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, alpha));
                ImGui::Text("Strategy: %s", toast.strategy.c_str());
                ImGui::PopStyleColor();
                
                // Hint to open F11 menu
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, alpha * 0.8f));
                ImGui::Text("Press F11 for details");
                ImGui::PopStyleColor();
                
                yOffset += ImGui::GetWindowHeight() + toastSpacing;
            }
            ImGui::End();
        }
    }

    std::vector<RecoveryEntry> RecoveryNotifications::GetRecentRecoveries(size_t maxCount) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_history.size() <= maxCount) {
            return m_history;
        }
        
        return std::vector<RecoveryEntry>(m_history.begin(), m_history.begin() + maxCount);
    }

    void RecoveryNotifications::ClearHistory() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_history.clear();
        m_activeToasts.clear();
    }

    size_t RecoveryNotifications::GetTotalRecoveries() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_totalRecoveries;
    }

    size_t RecoveryNotifications::GetSuccessfulRecoveries() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_successfulRecoveries;
    }

    size_t RecoveryNotifications::GetFailedRecoveries() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_failedRecoveries;
    }

    void RecoveryNotifications::UpdateToasts() {
        ImGuiIO& io = ImGui::GetIO();
        float deltaTime = io.DeltaTime;
        
        // Update display times and remove expired toasts
        for (auto it = m_activeToasts.begin(); it != m_activeToasts.end();) {
            it->displayTime -= deltaTime;
            
            if (it->displayTime <= 0.0f) {
                it->visible = false;
                it = m_activeToasts.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::string RecoveryNotifications::FormatTimestamp(const std::chrono::steady_clock::time_point& time) const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - time);
        
        // Convert to system time for actual timestamp
        auto systemNow = std::chrono::system_clock::now();
        auto systemTime = systemNow - elapsed;
        auto timeT = std::chrono::system_clock::to_time_t(systemTime);
        
        std::tm tm;
        localtime_s(&tm, &timeT);
        
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    std::string RecoveryNotifications::CreateSummary(const std::string& rootCause, const std::string& strategy) const {
        std::ostringstream oss;
        
        // Truncate root cause if too long
        std::string shortCause = rootCause;
        if (shortCause.length() > 60) {
            shortCause = shortCause.substr(0, 57) + "...";
        }
        
        oss << shortCause << " (using " << strategy << ")";
        return oss.str();
    }

    void RecoveryNotifications::AddResourceWarning(
        const std::string& resourceType,
        const std::string& message,
        bool isCritical
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Create toast
        RecoveryToast toast;
        toast.severity = isCritical ? "Critical" : "Warning";
        toast.summary = resourceType + ": " + message;
        toast.strategy = "Resource Management";
        toast.timestamp = std::chrono::steady_clock::now();
        toast.displayTime = isCritical ? TOAST_DURATION_CRITICAL : TOAST_DURATION_WARNING;
        toast.visible = true;
        
        // Add to active toasts
        if (m_activeToasts.size() >= MAX_ACTIVE_TOASTS) {
            m_activeToasts.erase(m_activeToasts.begin());
        }
        m_activeToasts.push_back(toast);
        
        // DON'T auto-open F11 menu - just show toast
        spdlog::info("[RecoveryNotifications] Resource warning: {} - {}", resourceType, message);
    }

}
