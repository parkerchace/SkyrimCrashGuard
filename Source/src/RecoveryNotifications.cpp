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
        bool successful,
        LayerID     layerUsed,
        std::string crashAddr,
        std::string moduleName,
        std::string decodedInstruction,
        uint64_t    accessAddress,
        int         accessType,
        std::string affectedRegister
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
        toast.severity           = severity;
        toast.summary            = CreateSummary(rootCause, strategy);
        toast.strategy           = strategy;
        toast.timestamp          = std::chrono::steady_clock::now();
        toast.visible            = true;
        // Rich crash context
        toast.crashAddr          = crashAddr;
        toast.moduleName         = moduleName;
        toast.decodedInstruction = decodedInstruction;
        toast.accessAddress      = accessAddress;
        toast.accessType         = accessType;
        toast.affectedRegister   = affectedRegister;

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
        entry.successful         = successful;
        entry.layerUsed          = layerUsed;
        entry.crashAddr          = crashAddr;
        entry.moduleName         = moduleName;
        entry.decodedInstruction = decodedInstruction;
        entry.accessAddress      = accessAddress;
        entry.accessType         = accessType;
        entry.affectedRegister   = affectedRegister;
        
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

        if (m_activeToasts.empty()) return;

        ImGuiIO& io       = ImGui::GetIO();
        float    padding  = 12.0f;
        float    width    = 360.0f;
        float    spacing  = 8.0f;
        float    yOffset  = padding;

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration    |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings  |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav            |
            ImGuiWindowFlags_NoMove;

        for (auto& toast : m_activeToasts) {
            if (!toast.visible) continue;

            float alpha = (toast.displayTime < 1.0f) ? toast.displayTime : 1.0f;

            ImGui::SetNextWindowPos(
                ImVec2(io.DisplaySize.x - padding, yOffset),
                ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowSize(ImVec2(width, 0), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.88f * alpha);

            std::string wname = "##toast" + std::to_string((uintptr_t)&toast);
            if (ImGui::Begin(wname.c_str(), nullptr, flags)) {

                // ── Severity header ─────────────────────────────────────
                ImVec4 sevCol = (toast.severity == "Safe")    ? ImVec4(0.30f, 1.00f, 0.30f, alpha)
                              : (toast.severity == "Warning") ? ImVec4(1.00f, 0.80f, 0.25f, alpha)
                                                               : ImVec4(1.00f, 0.28f, 0.28f, alpha);
                ImGui::PushStyleColor(ImGuiCol_Text, sevCol);
                ImGui::TextUnformatted("[+] CrashGuard intercepted a crash");
                ImGui::PopStyleColor();

                ImGui::Separator();

                bool hasRich = !toast.crashAddr.empty();

                if (hasRich) {
                    // ── Crash address + module ────────────────────────────
                    bool isMod = !toast.moduleName.empty() &&
                                 toast.moduleName != "SkyrimSE.exe" &&
                                 toast.moduleName != "SkyrimVR.exe" &&
                                 toast.moduleName != "Skyrim.exe";
                    ImVec4 addrCol = isMod
                        ? ImVec4(1.00f, 0.68f, 0.18f, alpha)
                        : ImVec4(0.52f, 0.52f, 0.52f, alpha);
                    ImGui::TextColored(addrCol, "%s", toast.crashAddr.c_str());

                    // ── Decoded instruction ───────────────────────────────
                    if (!toast.decodedInstruction.empty()) {
                        ImGui::PushStyleColor(ImGuiCol_Text,
                            ImVec4(0.58f, 0.90f, 0.48f, alpha));
                        ImGui::TextUnformatted(toast.decodedInstruction.c_str());
                        ImGui::PopStyleColor();
                    }

                    // ── Access + resolution (compact one-liner) ───────────
                    if (toast.accessType >= 0) {
                        char accessBuf[48] = {};
                        char resolveBuf[40] = {};

                        if (toast.accessAddress == 0) {
                            snprintf(accessBuf, sizeof(accessBuf),
                                toast.accessType == 8 ? "Execute 0x0" :
                                toast.accessType == 1 ? "Write null"  : "Read null");
                        } else if (toast.accessAddress < 0x10000) {
                            snprintf(accessBuf, sizeof(accessBuf),
                                toast.accessType == 1
                                    ? "Write null+0x%llX"
                                    : "Read null+0x%llX",
                                (unsigned long long)toast.accessAddress);
                        } else {
                            snprintf(accessBuf, sizeof(accessBuf),
                                toast.accessType == 1
                                    ? "Write 0x%llX"
                                    : "Read 0x%llX",
                                (unsigned long long)toast.accessAddress);
                        }

                        if (!toast.affectedRegister.empty())
                            snprintf(resolveBuf, sizeof(resolveBuf),
                                "%s zeroed", toast.affectedRegister.c_str());
                        else if (toast.accessType == 1)
                            snprintf(resolveBuf, sizeof(resolveBuf), "write dropped");
                        else if (toast.accessType == 8)
                            snprintf(resolveBuf, sizeof(resolveBuf), "call returned");
                        else
                            snprintf(resolveBuf, sizeof(resolveBuf), "skipped");

                        ImGui::PushStyleColor(ImGuiCol_Text,
                            ImVec4(0.42f, 0.42f, 0.42f, alpha));
                        ImGui::Text("%s  ->  %s", accessBuf, resolveBuf);
                        ImGui::PopStyleColor();
                    }
                } else {
                    // ── Fallback for resource warnings etc. ───────────────
                    ImGui::PushStyleColor(ImGuiCol_Text,
                        ImVec4(0.74f, 0.74f, 0.74f, alpha));
                    ImGui::TextWrapped("%s", toast.summary.c_str());
                    ImGui::PopStyleColor();
                }

                // ── F11 hint ──────────────────────────────────────────────
                ImGui::PushStyleColor(ImGuiCol_Text,
                    ImVec4(0.30f, 0.30f, 0.30f, alpha * 0.9f));
                ImGui::TextUnformatted("F11 for details");
                ImGui::PopStyleColor();

                yOffset += ImGui::GetWindowHeight() + spacing;
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
