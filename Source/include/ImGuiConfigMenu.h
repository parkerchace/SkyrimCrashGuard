// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once
#include <imgui.h>
#include "Config.h"
#include "PerformanceMetrics.h"
#include "CrashTestSuite.h"
// #include "DebugVisualization.h"  // Temporarily disabled - missing file

namespace CrashGuard {
    class ImGuiConfigMenu {
    public:
        static ImGuiConfigMenu& GetSingleton() {
            static ImGuiConfigMenu instance;
            return instance;
        }

        void Initialize();
        void Render();
        void Toggle();
        bool IsVisible() const { return m_visible; }
        int GetMenuHotkey() const { return m_menuHotkey; }
        
        /// Open menu and switch to specific tab
        enum class Tab {
            Overview,           // System status, integrations, quick stats
            Settings,           // Core toggles
            ResourceMonitor,    // Resource management + memory pressure
            CrashHistory,       // Recent recoveries
            Performance,        // Metrics + overlay settings
            Advanced,           // All TOML config
            Debug,             // Debug visualization + hotkeys
            SeverityGuide,     // Severity classification guide
            RecoveryStats,     // Recovery statistics
            Recovery,          // Consolidated recovery UI (history, severity guide, stats)
            Diagnostics        // VEH layer stats + crash recovery test suite
        };
        void OpenToTab(Tab tab);

    private:
        ImGuiConfigMenu() = default;
        ~ImGuiConfigMenu() = default;
        ImGuiConfigMenu(const ImGuiConfigMenu&) = delete;
        ImGuiConfigMenu& operator=(const ImGuiConfigMenu&) = delete;

        bool m_visible = false;
        
        // Active tab selection
        Tab m_activeTab = Tab::Settings;
        bool m_forceTabSwitch = false;
        
        // Settings (in-memory, synced with Config)
        bool m_enabled = true;
        bool m_meshValidation = true;
        bool m_animationValidation = true;
        bool m_scriptMonitoring = true;
        bool m_cellValidation = true;
        bool m_patternLearning = true;
        bool m_notifications = true;
        
        // Hotkey settings
        int m_menuHotkey = VK_F11;
        
        // Notification countdown
        float m_notificationTimer = 11.0f;
        bool m_notificationVisible = true;
        
        // Save status tracking
        bool m_showSaveMessage = false;
        bool m_saveSuccess = false;
        float m_saveMessageTimer = 0.0f;
        std::string m_saveErrorMessage;
        
        // Validation error tracking
        bool m_showValidationError = false;
        float m_validationErrorTimer = 0.0f;
        std::string m_validationErrorMessage;
        
        // Saved values — used for per-field orange highlighting in the UI
        Config::Settings m_savedValues;

        // Hash of m_savedValues — used by HasUnsavedChanges() to detect changes
        // across ALL config fields without a per-field comparison chain.
        uint64_t m_savedHash = 0;
        
        // Performance overlay settings
        OverlaySettings m_overlaySettings;

        void LoadSettings();
        void SaveSettings();
        void SaveTomlFile();
        
        void RenderMainWindow();
        void RenderSettingsTab();
        void RenderAdvancedConfigTab();
        void RenderPerformanceTab();
        void RenderDebugVisualizationTab();
        void RenderHotkeysTab();
        void RenderStatisticsTab();
        void RenderResourceManagementTab();
        void RenderRecentRecoveriesTab();
        void RenderNotificationArea();
        void RenderPerformanceOverlay();
        void RenderDebugMarkerCostPopups();
        void RenderHotkeyButton(const char* label, int* hotkey);
        const char* GetKeyName(int vkCode);
        
        // New reorganized tabs
        void RenderOverviewTab();
        void RenderResourceMonitorTab();
        void RenderCrashHistoryTab();
        void RenderRecoveryTab();
        void RenderDebugTab();
        void RenderSeverityGuideTab();
        void RenderRecoveryStatisticsTab();
        void RenderDiagnosticsTab();
        
        // Helper for severity guide
        void RenderSeverityLevel(
            const char* name,
            const ImVec4& color,
            const char* icon,
            const char* description,
            const char* riskLevel,
            const char* behavior,
            const std::vector<std::string>& examples);
        
        // Validation helpers
        bool ValidateIntRange(int value, int min, int max, const char* paramName);
        bool ValidatePath(const std::string& path);
        void ShowValidationError(const std::string& message);
        bool HasUnsavedChanges() const;
    };
}
