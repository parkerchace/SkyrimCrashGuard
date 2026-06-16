// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "ImGuiConfigMenu.h"
#include "Config.h"
#include "VEH.h"
#include "CrashTestSuite.h"
#include "HotkeyManager.h"
#include "PerformanceMetrics.h"
#include "BenchmarkManager.h"
#include "MenuInputManager.h"
#include "MenuInputObserver.h"
#include "InputDiagnostics.h"
#include "ImGuiRenderer.h"
#include "RecoveryNotifications.h"
#include "RecoveryStatistics.h"
#include "MemoryPressureDetector.h"
#include "CrashLoggerDetector.h"
#include "StateManager.h"
#include "Plugin.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <ctime>

namespace CrashGuard {

    void ImGuiConfigMenu::Initialize() {
        LoadSettings();
        m_notificationTimer = 11.0f;
        m_notificationVisible = true;
        
        // Store initial saved values for change detection
        m_savedValues = Config::Get();
        m_savedHash   = m_savedValues.ComputeHash();
    }

    void ImGuiConfigMenu::Toggle() {
        // Respect TOML disable setting
        if (Config::Get().disableImGuiMenu) {
            m_visible = false;
            return;
        }
        
        m_visible = !m_visible;
        if (m_visible) {
            // Log menu open event
            InputDiagnostics::LogMenuOpen();
            
            // Hide notification immediately when user opens menu
            m_notificationVisible = false;
            
            // Enable menu input mode (allows mouse in gamepad mode)
            MenuInputManager::GetSingleton().EnableMenuInput();
            
            // Log ControlMap state after enabling menu input
            InputDiagnostics::LogControlMapState();
        } else {
            // Log menu close event
            InputDiagnostics::LogMenuClose();
            
            // Restore game input mode
            MenuInputManager::GetSingleton().RestoreGameInput();
            
            // Log ControlMap state after restoring game input
            InputDiagnostics::LogControlMapState();
        }
    }
    
    void ImGuiConfigMenu::OpenToTab(Tab tab) {
        m_activeTab = tab;
        m_forceTabSwitch = true;
        
        if (!m_visible) {
            Toggle();  // Open menu if not already open
        }
    }

    void ImGuiConfigMenu::LoadSettings() {
        const auto& config = Config::Get();
        m_enabled = config.enabled;
        m_meshValidation = config.enableMeshValidation;
        m_animationValidation = config.enableAnimationValidation;
        m_scriptMonitoring = config.enableScriptMonitoring;
        m_cellValidation = config.enableCellValidation;
        m_patternLearning = config.enableLearning;
        m_notifications = config.showNotifications;
    }

    void ImGuiConfigMenu::SaveSettings() {
        auto& config = Config::GetMutable();
        config.enabled = m_enabled;
        config.enableMeshValidation = m_meshValidation;
        config.enableAnimationValidation = m_animationValidation;
        config.enableScriptMonitoring = m_scriptMonitoring;
        config.enableCellValidation = m_cellValidation;
        config.enableLearning = m_patternLearning;
        config.showNotifications = m_notifications;
    }

    void ImGuiConfigMenu::SaveTomlFile() {
        // Don't call SaveSettings() here - Advanced tab already modifies Config directly
        // SaveSettings() would overwrite Advanced tab changes with Settings tab values
        
        try {
            bool success = Config::Save("Data/SKSE/Plugins/SkyrimCrashGuard.toml");
            
            m_showSaveMessage = true;
            m_saveSuccess = success;
            m_saveMessageTimer = 3.0f; // Show message for 3 seconds
            
            if (success) {
                // Update saved values snapshot on successful save
                m_savedValues = Config::Get();
                m_savedHash   = m_savedValues.ComputeHash();
                spdlog::info("Configuration saved to TOML file");
            } else {
                m_saveErrorMessage = "Failed to write to TOML file. Check file permissions.";
                spdlog::error("Failed to save configuration to TOML file");
            }
        } catch (const std::exception& e) {
            m_showSaveMessage = true;
            m_saveSuccess = false;
            m_saveMessageTimer = 3.0f;
            m_saveErrorMessage = std::string("Exception while saving: ") + e.what();
            spdlog::error("Exception while saving configuration: {}", e.what());
        } catch (...) {
            m_showSaveMessage = true;
            m_saveSuccess = false;
            m_saveMessageTimer = 3.0f;
            m_saveErrorMessage = "Unknown error occurred while saving configuration.";
            spdlog::error("Unknown exception while saving configuration");
        }
    }

    void ImGuiConfigMenu::Render() {
        // Update performance metrics
        PerformanceMonitor::GetSingleton().Update();

        // Update benchmark state machine only when benchmarks are enabled or active
        auto& bmConditional = BenchmarkManager::GetSingleton();
        if (bmConditional.IsRunning() || bmConditional.IsInteractiveRunning() || !bmConditional.GetSnapshots().empty()) {
            bmConditional.Update();
        }
        
        // Respect TOML disable setting - skip all menu rendering
        if (Config::Get().disableImGuiMenu) {
            m_visible = false;
            m_notificationVisible = false;
            return;
        }
        
        if (m_visible) {
            RenderMainWindow();
        }
        
        // Update notification timer
        if (m_notificationVisible && m_notificationTimer > 0.0f) {
            ImGuiIO& io = ImGui::GetIO();
            m_notificationTimer -= io.DeltaTime;
            if (m_notificationTimer <= 0.0f) {
                m_notificationVisible = false;
            }
        }
        
        // Update save message timer
        if (m_showSaveMessage && m_saveMessageTimer > 0.0f) {
            ImGuiIO& io = ImGui::GetIO();
            m_saveMessageTimer -= io.DeltaTime;
            if (m_saveMessageTimer <= 0.0f) {
                m_showSaveMessage = false;
            }
        }
        
        // Update validation error timer
        if (m_showValidationError && m_validationErrorTimer > 0.0f) {
            ImGuiIO& io = ImGui::GetIO();
            m_validationErrorTimer -= io.DeltaTime;
            if (m_validationErrorTimer <= 0.0f) {
                m_showValidationError = false;
            }
        }
        
        // Render notification if visible
        if (m_notificationVisible) {
            RenderNotificationArea();
        }
        
        // Always render performance overlay
        RenderPerformanceOverlay();
    }

    void ImGuiConfigMenu::RenderMainWindow() {
        // Larger window with better proportions
        ImGui::SetNextWindowSize(ImVec2(1000, 850), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Crash Guard Configuration", &m_visible)) {
            // Larger font scale for better readability
            ImGui::SetWindowFontScale(1.15f);
            ImGui::Separator();

            if (ImGui::BeginTabBar("CrashGuardTabs")) {
                // Force tab selection if requested
                ImGuiTabItemFlags flags = m_forceTabSwitch ? ImGuiTabItemFlags_SetSelected : 0;
                
                if (ImGui::BeginTabItem("Overview", nullptr, m_activeTab == Tab::Overview ? flags : 0)) {
                    m_activeTab = Tab::Overview;
                    m_forceTabSwitch = false;
                    
                    // Use child window with proper sizing to leave room for bottom buttons
                    ImGui::BeginChild("OverviewContent", ImVec2(0, -60), false);
                    RenderOverviewTab();
                    ImGui::EndChild();
                    
                    ImGui::EndTabItem();
                }
                
                if (ImGui::BeginTabItem("Configuration", nullptr, m_activeTab == Tab::Settings ? flags : 0)) {
                    m_activeTab = Tab::Settings;
                    m_forceTabSwitch = false;
                    
                    // Use child window with proper sizing to leave room for bottom buttons
                    ImGui::BeginChild("ConfigurationContent", ImVec2(0, -60), false);
                    RenderSettingsTab();
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    RenderAdvancedConfigTab();
                    ImGui::EndChild();
                    
                    ImGui::EndTabItem();
                }
                
                if (ImGui::BeginTabItem("Resource Monitor", nullptr, m_activeTab == Tab::ResourceMonitor ? flags : 0)) {
                    m_activeTab = Tab::ResourceMonitor;
                    m_forceTabSwitch = false;
                    
                    // Use child window with proper sizing to leave room for bottom buttons
                    ImGui::BeginChild("ResourceMonitorContent", ImVec2(0, -60), false);
                    RenderResourceMonitorTab();
                    ImGui::EndChild();
                    
                    ImGui::EndTabItem();
                }
                
                if (ImGui::BeginTabItem("Recovery", nullptr, m_activeTab == Tab::Recovery ? flags : 0)) {
                    m_activeTab = Tab::Recovery;
                    m_forceTabSwitch = false;
                    
                    // Use child window with proper sizing to leave room for bottom buttons
                    ImGui::BeginChild("RecoveryContent", ImVec2(0, -60), false);
                    RenderRecoveryTab();
                    ImGui::EndChild();
                    
                    ImGui::EndTabItem();
                }
                
                if (ImGui::BeginTabItem("Debug", nullptr, m_activeTab == Tab::Debug ? flags : 0)) {
                    m_activeTab = Tab::Debug;
                    m_forceTabSwitch = false;

                    // Use child window with proper sizing to leave room for bottom buttons
                    ImGui::BeginChild("DebugContent", ImVec2(0, -60), false);
                    RenderDebugTab();
                    ImGui::EndChild();

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Diagnostics", nullptr, m_activeTab == Tab::Diagnostics ? flags : 0)) {
                    m_activeTab = Tab::Diagnostics;
                    m_forceTabSwitch = false;

                    ImGui::BeginChild("DiagnosticsContent", ImVec2(0, -60), false);
                    RenderDiagnosticsTab();
                    ImGui::EndChild();

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            // Bottom section - always visible
            ImGui::Spacing();
            ImGui::Separator();
            
            // Display save status message
            if (m_showSaveMessage) {
                ImGui::Spacing();
                if (m_saveSuccess) {
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[OK] Configuration saved successfully!");
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "[ERROR] %s", m_saveErrorMessage.c_str());
                }
                ImGui::Spacing();
            }
            
            if (ImGui::Button("Save to TOML", ImVec2(120, 30))) {
                SaveTomlFile();
            }
            ImGui::SameLine();
            if (ImGui::Button("Close", ImVec2(80, 30))) {
                m_visible = false;
            }
            ImGui::SameLine();
            ImGui::Text("See Debug tab to change menu bindings");
        }
        ImGui::End();
    }

    void ImGuiConfigMenu::RenderSettingsTab() {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Core Settings");
            ImGui::Separator();
            ImGui::Spacing();
            
            // Main enable/disable toggle - prominent
            if (ImGui::Checkbox("Enable CrashGuard", &m_enabled)) {
                SaveSettings();
            }
            ImGui::TextWrapped("Master switch for all crash protection features");

            ImGui::Spacing();
            ImGui::Spacing();
            
            // Collapsible sections for better organization
            if (ImGui::CollapsingHeader("Validation Systems", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();
                ImGui::Spacing();
                
                if (ImGui::Checkbox("Mesh Validation", &m_meshValidation)) {
                    SaveSettings();
                }
                ImGui::TextWrapped("Validate mesh files before loading to prevent corrupted mesh crashes");
                ImGui::Spacing();

                if (ImGui::Checkbox("Animation Validation", &m_animationValidation)) {
                    SaveSettings();
                }
                ImGui::TextWrapped("Validate animation files before loading to prevent animation crashes");
                ImGui::Spacing();

                if (ImGui::Checkbox("Script Monitoring", &m_scriptMonitoring)) {
                    SaveSettings();
                }
                ImGui::TextWrapped("Monitor Papyrus scripts for timeouts and errors");
                ImGui::Spacing();

                if (ImGui::Checkbox("Cell Validation", &m_cellValidation)) {
                    SaveSettings();
                }
                ImGui::TextWrapped("Validate cell data before loading to prevent world space crashes");
                
                ImGui::Unindent();
            }

            ImGui::Spacing();
            ImGui::Spacing();
            
            if (ImGui::Button("Reset to Defaults", ImVec2(200, 30))) {
                m_enabled = true;
                m_meshValidation = true;
                m_animationValidation = true;
                m_scriptMonitoring = true;
                m_cellValidation = true;
                SaveSettings();
            }
    }

    void ImGuiConfigMenu::RenderStatisticsTab() {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "System Status");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Protection Systems
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Active Protection Systems");
        ImGui::Spacing();
        
        ImGui::Indent();
        
        // Show which systems are enabled
        if (m_meshValidation) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[X]");
            ImGui::SameLine();
            ImGui::Text("Mesh Validation");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ ]");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Mesh Validation (Disabled)");
        }
        
        if (m_animationValidation) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[X]");
            ImGui::SameLine();
            ImGui::Text("Animation Validation");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ ]");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Animation Validation (Disabled)");
        }
        
        // Script hook is not installed (vtable offset not yet validated).
        // ScriptMonitor initializes data structures only — no live Papyrus interception.
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ ]");
        ImGui::SameLine();
        if (m_scriptMonitoring) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Script Monitoring (no VM hook installed)");
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Script Monitoring (Disabled)");
        }
        
        if (m_cellValidation) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[X]");
            ImGui::SameLine();
            ImGui::Text("Cell Validation");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ ]");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Cell Validation (Disabled)");
        }
        
        if (m_patternLearning) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[X]");
            ImGui::SameLine();
            ImGui::Text("Pattern Learning");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ ]");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Pattern Learning (Disabled)");
        }
        
        ImGui::Unindent();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Thread Safety
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Thread Safety Features");
        ImGui::Spacing();
        
        ImGui::Indent();
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[X]");
        ImGui::SameLine();
        ImGui::Text("Shared Mutex System");
        
        // DeadlockDetector::Initialize() creates data structures only.
        // No watchdog thread runs. AttemptBreakDeadlock() always returns false.
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ ]");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Deadlock Detection (data structures only)");

        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[X]");
        ImGui::SameLine();
        ImGui::Text("Lock-Free Structures");
        ImGui::Unindent();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // External Integration Status
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "External Tool Integration");
        ImGui::Spacing();
        
        ImGui::Indent();
        
        // CrashLogger integration
        if (CrashLoggerDetector::Detector::IsCrashLoggerPresent()) {
            auto crashLoggerInfo = CrashLoggerDetector::Detector::GetCrashLoggerInfo();
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[X]");
            ImGui::SameLine();
            ImGui::Text("CrashLogger");
            if (crashLoggerInfo.has_value()) {
                ImGui::Indent();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Version: %s", crashLoggerInfo->version.c_str());
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Status: Active - Post-mortem analysis enabled");
                ImGui::Unindent();
            }
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ ]");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "CrashLogger (Not Detected)");
        }
        
        ImGui::Spacing();
        
        // Trainwreck integration
        if (CrashLoggerDetector::Detector::IsTrainwreckPresent()) {
            auto trainwreckInfo = CrashLoggerDetector::Detector::GetTrainwreckInfo();
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[X]");
            ImGui::SameLine();
            ImGui::Text("Trainwreck");
            if (trainwreckInfo.has_value()) {
                ImGui::Indent();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Version: %s", trainwreckInfo->version.c_str());
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Status: Active - Coordinated crash reporting");
                ImGui::Unindent();
            }
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ ]");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Trainwreck (Not Detected)");
        }
        
        ImGui::Spacing();
        
        ImGui::TextWrapped("CrashGuard automatically integrates with CrashLogger and Trainwreck when detected. "
                          "These tools provide complementary crash analysis - CrashGuard prevents crashes proactively, "
                          "while CrashLogger/Trainwreck provide detailed post-mortem analysis when crashes do occur.");
        
        ImGui::Unindent();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Info message
        ImGui::TextWrapped("Crash Guard is actively monitoring your game. Enable or disable specific protection systems in the Settings tab.");
    }

    void ImGuiConfigMenu::RenderHotkeysTab() {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Hotkey Configuration");
        ImGui::Separator();
        ImGui::TextWrapped("Configure keyboard and controller hotkeys for opening the Crash Guard menu.");
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Keyboard binding section
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Keyboard - Menu Toggle Key");
        ImGui::Indent();
        
        auto& hotkeyMgr = HotkeyManager::GetSingleton();
        auto keyboardBinding = hotkeyMgr.GetBinding("ToggleMenu_Keyboard");
        
        ImGui::Text("Current: %s", keyboardBinding.ToString().c_str());
        ImGui::TextWrapped("Press a single key (F1-F12) or a combination (Ctrl+F1, Shift+F2, etc.)");
        
        if (ImGui::Button("Change Hotkey", ImVec2(200, 0))) {
            ImGui::OpenPopup("KeyboardBindingPopup");
        }
        
        // Keyboard binding popup
        if (ImGui::BeginPopup("KeyboardBindingPopup")) {
            ImGui::Text("Press a key...");
            ImGui::Separator();
            
            static std::vector<int> pressedKeys;
            static float waitTimer = 0.0f;
            static bool hasNonModifier = false;
            
            pressedKeys.clear();
            hasNonModifier = false;
            
            // Check for modifier keys
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) pressedKeys.push_back(VK_CONTROL);
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) pressedKeys.push_back(VK_SHIFT);
            if (GetAsyncKeyState(VK_MENU) & 0x8000) pressedKeys.push_back(VK_MENU);
            
            // Check for function keys
            for (int vk = VK_F1; vk <= VK_F12; vk++) {
                if (GetAsyncKeyState(vk) & 0x8000) {
                    pressedKeys.push_back(vk);
                    hasNonModifier = true;
                }
            }
            
            if (!pressedKeys.empty()) {
                HotkeyBinding newBinding;
                newBinding.device = InputDevice::Keyboard;
                newBinding.keys = pressedKeys;
                newBinding.holdDuration = 0.0f;
                
                ImGui::Text("New binding: %s", newBinding.ToString().c_str());
                
                // Auto-apply logic:
                // - If only modifiers pressed, wait for a non-modifier key
                // - If a non-modifier key is pressed, apply after short delay
                if (hasNonModifier) {
                    ImGuiIO& io = ImGui::GetIO();
                    waitTimer += io.DeltaTime;
                    
                    // Auto-apply after 1 second
                    if (waitTimer >= 1.0f) {
                        hotkeyMgr.SetBinding("ToggleMenu_Keyboard", newBinding);
                        waitTimer = 0.0f;
                        ImGui::CloseCurrentPopup();
                    } else {
                        ImGui::Text("Applying in %.1f...", 1.0f - waitTimer);
                    }
                } else {
                    // Only modifiers, show hint
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Press a function key (F1-F12)");
                    waitTimer = 0.0f;
                }
                
                if (ImGui::Button("Apply Now")) {
                    hotkeyMgr.SetBinding("ToggleMenu_Keyboard", newBinding);
                    waitTimer = 0.0f;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
            } else {
                waitTimer = 0.0f;
            }
            
            if (ImGui::Button("Cancel")) {
                waitTimer = 0.0f;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
        
        ImGui::Unindent();
        
        ImGui::Spacing();
        ImGui::Separator();
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Reset to defaults
        if (ImGui::Button("Reset Keyboard to Default (F11)")) {
            hotkeyMgr.SetBinding("ToggleMenu_Keyboard", HotkeyManager::GetDefaultKeyboardBinding());
        }
        
        ImGui::Spacing();
        ImGui::TextWrapped("Note: Controller combo translates to F11 keypress, so Skyrim's input mode is preserved.");
    }

    void ImGuiConfigMenu::RenderHotkeyButton(const char* label, int* hotkey) {
        static bool isCapturing = false;
        static int* capturingHotkey = nullptr;
        
        if (isCapturing && capturingHotkey == hotkey) {
            if (ImGui::Button("Press any key...", ImVec2(150, 0))) {
                isCapturing = false;
                capturingHotkey = nullptr;
            }
            
            // Check for key press
            for (int vk = 0x08; vk <= 0xFE; vk++) {
                // Skip mouse buttons and some special keys
                if (vk >= VK_LBUTTON && vk <= VK_XBUTTON2) continue;
                if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU) continue;
                
                if (GetAsyncKeyState(vk) & 0x8000) {
                    *hotkey = vk;
                    isCapturing = false;
                    capturingHotkey = nullptr;
                    SaveSettings();
                    break;
                }
            }
        } else {
            if (ImGui::Button(GetKeyName(*hotkey), ImVec2(150, 0))) {
                isCapturing = true;
                capturingHotkey = hotkey;
            }
        }
    }

    void ImGuiConfigMenu::RenderNotificationArea() {
        // Only show if notifications are enabled, menu is not visible, timer hasn't expired
        if (!m_notifications || m_visible || !m_notificationVisible) {
            return;
        }

        // Top-right notification area - small and transparent
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 10, 10), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.40f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("CrashGuardNotifications", nullptr, flags)) {

            // ── Header ────────────────────────────────────────────────────────
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 0.9f), "[+] Skyrim Crash Guard Active");

            // ── Key binding hint ──────────────────────────────────────────────
            auto& hotkeyMgr = HotkeyManager::GetSingleton();
            auto  keyboardBinding = hotkeyMgr.GetBinding("ToggleMenu_Keyboard");
            if (!keyboardBinding.keys.empty()) {
                std::string kbText = "Press ";
                for (size_t i = 0; i < keyboardBinding.keys.size(); ++i) {
                    if (i > 0) kbText += "+";
                    kbText += KeyToString(keyboardBinding.keys[i], InputDevice::Keyboard);
                }
                kbText += " for menu";
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 0.8f), "%s", kbText.c_str());
            }

            // ── Most recent crash recovery (if any occurred this session) ─────
            auto& notif = RecoveryNotifications::GetSingleton();
            if (notif.GetTotalRecoveries() > 0) {
                auto recent = notif.GetRecentRecoveries(1);
                if (!recent.empty()) {
                    const auto& last = recent[0];
                    ImGui::Separator();

                    // Show crash source module — highlight mod DLLs in amber
                    bool isMod = !last.moduleName.empty() &&
                                 last.moduleName != "SkyrimSE.exe" &&
                                 last.moduleName != "SkyrimVR.exe" &&
                                 last.moduleName != "Skyrim.exe";
                    if (!last.crashAddr.empty()) {
                        ImVec4 addrCol = isMod
                            ? ImVec4(1.0f, 0.68f, 0.18f, 0.9f)
                            : ImVec4(0.55f, 0.55f, 0.55f, 0.9f);
                        ImGui::TextColored(addrCol, "%s", last.crashAddr.c_str());
                    }
                    if (!last.decodedInstruction.empty()) {
                        ImGui::TextColored(ImVec4(0.58f, 0.88f, 0.48f, 0.9f),
                            "%s", last.decodedInstruction.c_str());
                    }
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 0.8f),
                        "Recovered via: %s", last.strategy.c_str());
                    ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 0.8f),
                        "Total this session: %zu", notif.GetTotalRecoveries());
                }
            }

            // ── Countdown ─────────────────────────────────────────────────────
            int countdown = (int)std::ceil(m_notificationTimer);
            ImGui::TextColored(ImVec4(0.35f, 0.35f, 0.35f, 0.7f), "(%ds)", countdown);
        }
        ImGui::End();
    }

    const char* ImGuiConfigMenu::GetKeyName(int vkCode) {
        static char keyName[32];
        
        // Special keys
        switch (vkCode) {
            case VK_F1: return "F1";
            case VK_F2: return "F2";
            case VK_F3: return "F3";
            case VK_F4: return "F4";
            case VK_F5: return "F5";
            case VK_F6: return "F6";
            case VK_F7: return "F7";
            case VK_F8: return "F8";
            case VK_F9: return "F9";
            case VK_F10: return "F10";
            case VK_F11: return "F11";
            case VK_F12: return "F12";
            case VK_INSERT: return "Insert";
            case VK_DELETE: return "Delete";
            case VK_HOME: return "Home";
            case VK_END: return "End";
            case VK_PRIOR: return "Page Up";
            case VK_NEXT: return "Page Down";
            case VK_ESCAPE: return "Escape";
            case VK_TAB: return "Tab";
            case VK_RETURN: return "Enter";
            case VK_SPACE: return "Space";
            case VK_BACK: return "Backspace";
            default:
                // Try to get the key name from Windows
                UINT scanCode = MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC);
                if (GetKeyNameTextA(scanCode << 16, keyName, sizeof(keyName)) > 0) {
                    return keyName;
                }
                return "Unknown";
        }
    }

    void ImGuiConfigMenu::RenderAdvancedConfigTab() {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Advanced Configuration");
        ImGui::Separator();
        ImGui::TextWrapped("Edit all TOML configuration parameters. Changes are saved to memory immediately and to file when you click 'Save to TOML'.");
        
        // Display validation error if present
        if (m_showValidationError) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "[ERROR] Validation Error: %s", m_validationErrorMessage.c_str());
            ImGui::Spacing();
        }
        
        // Display unsaved changes indicator
        if (HasUnsavedChanges()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[!] You have unsaved changes");
            ImGui::Spacing();
        }
        
        ImGui::Spacing();
        
        auto& config = Config::GetMutable();
        
        // General
        if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            
            // Highlight if changed
            if (config.enabled != m_savedValues.enabled) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Plugin Enabled", &config.enabled);
            if (config.enabled != m_savedValues.enabled) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enable or disable CrashGuard entirely. When disabled, no crash prevention features will be active.");
            }
            
            // Log level with validation
            int tempLogLevel = config.logLevel;
            if (config.logLevel != m_savedValues.logLevel) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            if (ImGui::SliderInt("Log Level", &tempLogLevel, 0, 4)) {
                if (ValidateIntRange(tempLogLevel, 0, 4, "Log Level")) {
                    config.logLevel = tempLogLevel;
                } else {
                    // Revert to previous value
                    tempLogLevel = config.logLevel;
                }
            }
            if (config.logLevel != m_savedValues.logLevel) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Logging verbosity: 0=Off, 1=Errors/Warnings, 2=Info, 3=Debug, 4=Trace. Higher levels produce more detailed logs.");
            }
            
            ImGui::Unindent();
        }
        
        // VEH
        if (ImGui::CollapsingHeader("VEH (Vectored Exception Handler)")) {
            ImGui::Indent();
            
            if (config.vehEnabled != m_savedValues.vehEnabled) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("VEH Enabled", &config.vehEnabled);
            if (config.vehEnabled != m_savedValues.vehEnabled) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enable Vectored Exception Handler for catching and recovering from crashes. This is the core crash prevention mechanism.");
            }
            
            int tempCascadeLimit = config.cascadeLimit;
            if (config.cascadeLimit != m_savedValues.cascadeLimit) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            if (ImGui::SliderInt("Cascade Limit", &tempCascadeLimit, 1, 10)) {
                if (ValidateIntRange(tempCascadeLimit, 1, 10, "Cascade Limit")) {
                    config.cascadeLimit = tempCascadeLimit;
                } else {
                    tempCascadeLimit = config.cascadeLimit;
                }
            }
            if (config.cascadeLimit != m_savedValues.cascadeLimit) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "How many crashes CrashGuard will recover in a single 100ms window before "
                    "it backs off and lets the exception propagate normally.\n\n"
                    "Default: 3  (industry-standard safety margin)\n"
                    "  - 1-2 crashes: common in normal play (NPC AI null, streaming hiccup)\n"
                    "  - 3 crashes:   a bad frame with multiple simultaneous mod conflicts\n"
                    "  - 4+ crashes:  runaway loop or heap corruption - recovery would mask\n"
                    "                 the real problem and corrupt save state\n\n"
                    "Raise this only if you see 'cascade limit' in the recovery log AND the "
                    "additional crashes are genuine recoverable AV faults, not heap damage.");
            }

            ImGui::Unindent();
        }

        // Patches
        if (ImGui::CollapsingHeader("Patches")) {
            ImGui::Indent();
            
            if (config.patchesEnabled != m_savedValues.patchesEnabled) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Patches Enabled", &config.patchesEnabled);
            if (config.patchesEnabled != m_savedValues.patchesEnabled) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Registers pre-analysed crash sites with the VEH recovery chain (L1 Known Fix).\n\n"
                    "For a small set of well-documented crash addresses - for example\n"
                    "SkyrimSE.exe+0x14F400E (shadow frustum null actor) - CrashGuard stores the\n"
                    "exact recovery action ahead of time. When VEH intercepts a crash at one of\n"
                    "those addresses it skips instruction decode entirely and applies the fix\n"
                    "instantly.\n\n"
                    "This does NOT write to the game executable, inject code, or modify memory.\n"
                    "It only registers address-to-action mappings inside CrashGuard's own tables.\n"
                    "Disable only if you suspect a specific L1 fix is causing instability.");
            }

            ImGui::Unindent();
        }

        // Learning
        if (ImGui::CollapsingHeader("Learning")) {
            ImGui::Indent();
            
            if (config.enableLearning != m_savedValues.enableLearning) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Learning Enabled", &config.enableLearning);
            if (config.enableLearning != m_savedValues.enableLearning) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enable pattern learning to automatically recognize and prevent recurring crash patterns.");
            }
            
            char pathBuf[256];
            strncpy_s(pathBuf, config.patternDatabasePath.c_str(), sizeof(pathBuf) - 1);
            std::string originalPath = config.patternDatabasePath;
            
            if (config.patternDatabasePath != m_savedValues.patternDatabasePath) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            if (ImGui::InputText("Pattern Database Path", pathBuf, sizeof(pathBuf))) {
                std::string newPath = pathBuf;
                if (ValidatePath(newPath)) {
                    config.patternDatabasePath = newPath;
                } else {
                    // Revert to previous value
                    strncpy_s(pathBuf, originalPath.c_str(), sizeof(pathBuf) - 1);
                }
            }
            if (config.patternDatabasePath != m_savedValues.patternDatabasePath) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Path to the pattern database file where learned crash patterns are stored.");
            }
            
            ImGui::Unindent();
        }
        
        // Notifications
        if (ImGui::CollapsingHeader("Notifications")) {
            ImGui::Indent();
            
            if (config.showNotifications != m_savedValues.showNotifications) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Show Notifications", &config.showNotifications);
            if (config.showNotifications != m_savedValues.showNotifications) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Show a brief on-screen toast when CrashGuard intercepts a crash.\n"
                    "The toast displays the crash address, the responsible module (mod DLL\n"
                    "or SkyrimSE.exe), and which recovery layer handled it.\n\n"
                    "Safe and Warning level crashes are always auto-recovered silently by\n"
                    "the VEH handler - CrashGuard cannot pause to ask the user mid-frame.\n"
                    "This toggle only controls whether the notification appears afterward.");
            }

            int tempNotificationTimeout = config.notificationTimeoutSeconds;
            if (config.notificationTimeoutSeconds != m_savedValues.notificationTimeoutSeconds) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            if (ImGui::SliderInt("Notification Timeout (seconds)", &tempNotificationTimeout, 5, 120)) {
                if (ValidateIntRange(tempNotificationTimeout, 5, 120, "Notification Timeout")) {
                    config.notificationTimeoutSeconds = tempNotificationTimeout;
                } else {
                    tempNotificationTimeout = config.notificationTimeoutSeconds;
                }
            }
            if (config.notificationTimeoutSeconds != m_savedValues.notificationTimeoutSeconds) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("How long notifications remain visible on screen before fading out.");
            }
            
            ImGui::Unindent();
        }
        
        // Performance
        if (ImGui::CollapsingHeader("Performance")) {
            ImGui::Indent();
            
            int tempScriptTimeout = config.scriptTimeoutMs;
            if (config.scriptTimeoutMs != m_savedValues.scriptTimeoutMs) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            if (ImGui::SliderInt("Script Timeout (ms)", &tempScriptTimeout, 1000, 30000)) {
                if (ValidateIntRange(tempScriptTimeout, 1000, 30000, "Script Timeout")) {
                    config.scriptTimeoutMs = tempScriptTimeout;
                } else {
                    tempScriptTimeout = config.scriptTimeoutMs;
                }
            }
            if (config.scriptTimeoutMs != m_savedValues.scriptTimeoutMs) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Detection threshold for long-running Papyrus script stacks (in ms).\n\n"
                    "CrashGuard's ScriptMonitor tracks stack depth and call frequency via\n"
                    "Papyrus log analysis - it does NOT hook the VM or kill scripts.\n"
                    "When a script stack stays active longer than this value it is flagged\n"
                    "in the log as a potential runaway. No forced termination occurs.\n\n"
                    "Default 5000ms is appropriate for most heavily-scripted mod setups.\n"
                    "Lower this if you want earlier warnings; raise it if you see false\n"
                    "positives from legitimate long-running scripts (e.g. RaceMenu).");
            }

            int tempMaxRecoveryAttempts = config.maxRecoveryAttempts;
            if (config.maxRecoveryAttempts != m_savedValues.maxRecoveryAttempts) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            if (ImGui::SliderInt("Max Recovery Attempts", &tempMaxRecoveryAttempts, 1, 10)) {
                if (ValidateIntRange(tempMaxRecoveryAttempts, 1, 10, "Max Recovery Attempts")) {
                    config.maxRecoveryAttempts = tempMaxRecoveryAttempts;
                } else {
                    tempMaxRecoveryAttempts = config.maxRecoveryAttempts;
                }
            }
            if (config.maxRecoveryAttempts != m_savedValues.maxRecoveryAttempts) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Maximum number of layer escalations for a SINGLE crash event.\n\n"
                    "Different from Cascade Limit (which counts crashes-per-window):\n"
                    "  - Cascade Limit: stop if N crashes arrive within 100ms\n"
                    "  - Max Recovery Attempts: stop escalating layers for one crash\n\n"
                    "The 6-layer VEH chain (zero reg -> skip write -> skip instr -> func\n"
                    "return -> deep walk) tries layers in order until one succeeds.\n"
                    "This setting caps how many layers are tried before giving up.\n"
                    "Default 3 means at most 3 layers are attempted per crash.");
            }

            ImGui::Unindent();
        }

        // Logging
        if (ImGui::CollapsingHeader("Logging")) {
            ImGui::Indent();
            
            if (config.enableDetailedLogging != m_savedValues.enableDetailedLogging) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Detailed Logging", &config.enableDetailedLogging);
            if (config.enableDetailedLogging != m_savedValues.enableDetailedLogging) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enable detailed logging with additional diagnostic information. Increases log file size.");
            }
            
            if (config.logOnlyFailures != m_savedValues.logOnlyFailures) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Log Only Failures", &config.logOnlyFailures);
            if (config.logOnlyFailures != m_savedValues.logOnlyFailures) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Only log failures and errors, not successful operations. Reduces log file size.");
            }
            
            if (config.logSuccessfulRecoveries != m_savedValues.logSuccessfulRecoveries) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Log Successful Recoveries", &config.logSuccessfulRecoveries);
            if (config.logSuccessfulRecoveries != m_savedValues.logSuccessfulRecoveries) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Log successful crash recoveries. Useful for debugging but increases log file size.");
            }
            
            if (config.aggregatePatterns != m_savedValues.aggregatePatterns) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Aggregate Patterns", &config.aggregatePatterns);
            if (config.aggregatePatterns != m_savedValues.aggregatePatterns) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Group identical crash-site events in the log file instead of writing\n"
                    "a separate line for every occurrence.\n\n"
                    "When enabled, repeated crashes at the same address are collapsed into\n"
                    "a single entry prefixed with [AGGREGATED] and a hit count, e.g.:\n"
                    "  [AGGREGATED x7] SkyrimSE.exe+0x14F400E zeroed RAX\n\n"
                    "Disable if you need every individual event timestamped separately\n"
                    "(e.g. for correlating crashes with specific in-game actions).");
            }

            int tempMaxLogSize = config.maxLogSizeMB;
            if (config.maxLogSizeMB != m_savedValues.maxLogSizeMB) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            if (ImGui::SliderInt("Max Log Size (MB)", &tempMaxLogSize, 1, 100)) {
                if (ValidateIntRange(tempMaxLogSize, 1, 100, "Max Log Size")) {
                    config.maxLogSizeMB = tempMaxLogSize;
                } else {
                    tempMaxLogSize = config.maxLogSizeMB;
                }
            }
            if (config.maxLogSizeMB != m_savedValues.maxLogSizeMB) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Maximum size of a single log file before rotation. Older logs are archived.");
            }
            
            int tempMaxLogFiles = config.maxLogFiles;
            if (config.maxLogFiles != m_savedValues.maxLogFiles) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            if (ImGui::SliderInt("Max Log Files", &tempMaxLogFiles, 1, 10)) {
                if (ValidateIntRange(tempMaxLogFiles, 1, 10, "Max Log Files")) {
                    config.maxLogFiles = tempMaxLogFiles;
                } else {
                    tempMaxLogFiles = config.maxLogFiles;
                }
            }
            if (config.maxLogFiles != m_savedValues.maxLogFiles) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Maximum number of log files to keep. Oldest files are deleted when limit is reached.");
            }
            
            ImGui::Unindent();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Remember to click 'Save to TOML' to persist changes!");
    }

    // Small helper popup for editing marker cost (Nanite override)
    void ImGuiConfigMenu::RenderDebugMarkerCostPopups() {
        // This function is intentionally empty here; popup handling is done inline
    }

    void ImGuiConfigMenu::RenderPerformanceOverlay() {
        const auto& config = Config::Get();
        if (!config.overlayEnabled || m_visible) return;
        
        const auto& metrics = PerformanceMonitor::GetSingleton().GetMetrics();
        
        // Calculate position
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 windowPos;
        ImVec2 windowPosPivot;
        
        float padding = 10.0f;
        switch (config.overlayPosition) {
            case 0: // Top Left
                windowPos = ImVec2(padding, padding);
                windowPosPivot = ImVec2(0.0f, 0.0f);
                break;
            case 1: // Top Right
                windowPos = ImVec2(io.DisplaySize.x - padding, padding);
                windowPosPivot = ImVec2(1.0f, 0.0f);
                break;
            case 2: // Bottom Left
                windowPos = ImVec2(padding, io.DisplaySize.y - padding);
                windowPosPivot = ImVec2(0.0f, 1.0f);
                break;
            case 3: // Bottom Right
                windowPos = ImVec2(io.DisplaySize.x - padding, io.DisplaySize.y - padding);
                windowPosPivot = ImVec2(1.0f, 1.0f);
                break;
        }
        
        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPosPivot);
        ImGui::SetNextWindowBgAlpha(config.overlayBackgroundAlpha);
        
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | 
                                ImGuiWindowFlags_AlwaysAutoResize |
                                ImGuiWindowFlags_NoSavedSettings |
                                ImGuiWindowFlags_NoFocusOnAppearing |
                                ImGuiWindowFlags_NoNav |
                                ImGuiWindowFlags_NoMove;
        
        ImGui::SetNextWindowSize(ImVec2(0, 0));
        
        if (ImGui::Begin("PerformanceOverlay", nullptr, flags)) {
            ImGui::SetWindowFontScale(config.overlayScale);
            ImVec4 textColor = ImVec4(1.0f, 1.0f, 1.0f, config.overlayTextAlpha);
            
            if (config.overlayShowFPS) {
                ImGui::TextColored(textColor, "FPS: %.1f", metrics.currentFPS);
            }
            
            if (config.overlayShowFrameTime) {
                ImGui::TextColored(textColor, "Frame: %.2f ms", metrics.frameTimeMs);
            }
            
            if (config.overlayShowMemory) {
                ImGui::TextColored(textColor, "Memory: %zu MB", metrics.memoryUsageMB);
            }
            
            if (config.overlayShowCrashStats) {
                ImGui::TextColored(textColor, "Crashes Prevented: %u", metrics.crashesPrevented);
            }
            
            if (config.overlayShowRecoveryStats) {
                ImGui::TextColored(textColor, "Recoveries: %u/%u", 
                                  metrics.successfulRecoveries, metrics.recoveryAttempts);
            }
            
            if (config.overlayShowPatternStats) {
                ImGui::TextColored(textColor, "Patterns: %u learned, %u applied", 
                                  metrics.patternsLearned, metrics.patternsApplied);
            }
            
            const auto& stateManager = StateManager::GetInstance();
            bool saveSafe = stateManager.IsSavingSafe();
            ImVec4 safetyColor = saveSafe ? ImVec4(0.2f, 1.0f, 0.2f, config.overlayTextAlpha) 
                                          : ImVec4(1.0f, 0.3f, 0.3f, config.overlayTextAlpha);
            ImGui::TextColored(safetyColor, "Save Safety: %s", saveSafe ? "OK" : "CORRUPTED");
        }
        ImGui::End();
    }

    bool ImGuiConfigMenu::ValidateIntRange(int value, int min, int max, const char* paramName) {
        if (value < min || value > max) {
            std::string errorMsg = std::string(paramName) + " must be between " + 
                                  std::to_string(min) + " and " + std::to_string(max);
            ShowValidationError(errorMsg);
            return false;
        }
        return true;
    }

    bool ImGuiConfigMenu::ValidatePath(const std::string& path) {
        // Basic path validation - check for invalid characters
        if (path.empty()) {
            ShowValidationError("Path cannot be empty");
            return false;
        }
        
        // Check for invalid path characters
        const std::string invalidChars = "<>|\"?*";
        for (char c : invalidChars) {
            if (path.find(c) != std::string::npos) {
                ShowValidationError("Path contains invalid characters: " + std::string(1, c));
                return false;
            }
        }
        
        // Check path length (Windows MAX_PATH is 260)
        if (path.length() > 250) {
            ShowValidationError("Path is too long (max 250 characters)");
            return false;
        }
        
        return true;
    }

    void ImGuiConfigMenu::ShowValidationError(const std::string& message) {
        m_showValidationError = true;
        m_validationErrorMessage = message;
        m_validationErrorTimer = 3.0f;
        spdlog::warn("Validation error: {}", message);
    }

    bool ImGuiConfigMenu::HasUnsavedChanges() const {
        // Compare a hash of every field in the current config against the hash
        // we saved when the user last clicked "Save" (or when the menu first opened).
        // This automatically covers ALL fields — including ones added in the future —
        // without needing a separate comparison line for each one.
        return Config::Get().ComputeHash() != m_savedHash;
    }

    void ImGuiConfigMenu::RenderRecentRecoveriesTab() {

        auto& recoverySystem = RecoveryNotifications::GetSingleton();
        auto  recoveries     = recoverySystem.GetRecentRecoveries(100);

        // ── Header + stats summary ───────────────────────────────────────────
        ImGui::TextColored(ImVec4(1.f, 0.82f, 0.22f, 1.f), "Crash Recovery History");
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.f);
        if (ImGui::Button("Clear", ImVec2(0, 20)))
            recoverySystem.ClearHistory();
        ImGui::Spacing();

        {
            size_t tot = recoverySystem.GetTotalRecoveries();
            size_t ok  = recoverySystem.GetSuccessfulRecoveries();
            size_t bad = recoverySystem.GetFailedRecoveries();
            ImGui::TextColored(ImVec4(0.44f, 0.44f, 0.44f, 1.f),
                "Session: %zu recovered,  %zu failed", ok, bad);
            (void)tot;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (recoveries.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.42f, 1.f),
                "No crashes recovered yet this session.\n"
                "When CrashGuard intercepts a crash in-game, it will appear here.");
            return;
        }

        // Persistent selection
        static int s_selRec = 0;
        if (s_selRec >= (int)recoveries.size()) s_selRec = 0;

        const float totalH  = ImGui::GetContentRegionAvail().y;
        const float listW   = 260.f;
        const float detailW = ImGui::GetContentRegionAvail().x - listW - 8.f;

        // ── Left: recovery list ──────────────────────────────────────────────
        ImGui::BeginChild("RecList", ImVec2(listW, totalH), true);

        for (int i = 0; i < (int)recoveries.size(); ++i) {
            const auto& e = recoveries[i];
            const bool  sel = (s_selRec == i);

            // Full-row selectable
            char rowId[24]; snprintf(rowId, sizeof(rowId), "##rec%d", i);
            if (sel) {
                ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.18f, 0.30f, 0.36f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.22f, 0.38f, 0.44f, 1.f));
            }
            if (ImGui::Selectable(rowId, sel, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 17)))
                s_selRec = i;
            if (sel) ImGui::PopStyleColor(2);

            // Status badge
            ImGui::SameLine(5.f);
            if (e.successful)
                ImGui::TextColored(ImVec4(0.22f, 1.f, 0.22f, 1.f), "+");
            else
                ImGui::TextColored(ImVec4(1.f, 0.28f, 0.28f, 1.f), "x");

            // Crash address or root cause (truncated)
            ImGui::SameLine(20.f);
            const std::string& label = e.crashAddr.empty() ? e.rootCause : e.crashAddr;
            ImVec4 labelCol = sel ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImVec4(0.72f, 0.72f, 0.72f, 1.f);
            // Truncate to fit
            char truncated[36] = {};
            snprintf(truncated, sizeof(truncated), "%s", label.c_str());
            ImGui::TextColored(labelCol, "%s", truncated);

            // Timestamp
            ImGui::SameLine(listW - 52.f);
            ImGui::TextColored(ImVec4(0.34f, 0.34f, 0.34f, 1.f), "%s", e.timestamp.c_str());
        }

        ImGui::EndChild();  // RecList

        // ── Right: detail ────────────────────────────────────────────────────
        ImGui::SameLine();
        ImGui::BeginChild("RecDetail", ImVec2(detailW, totalH), true);

        const auto& e = recoveries[s_selRec];

        ImGui::Spacing();

        // ── Crash address (heading) ────────────────────────────────────
        ImGui::SetWindowFontScale(1.16f);
        if (!e.crashAddr.empty())
            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 1.f), "%s", e.crashAddr.c_str());
        else
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.f), "Unknown address");
        ImGui::SetWindowFontScale(1.f);

        // ── Module — highlight non-game DLLs (likely the responsible mod) ──
        if (!e.moduleName.empty()) {
            bool isGameExe = (e.moduleName == "SkyrimSE.exe" ||
                              e.moduleName == "SkyrimVR.exe"  ||
                              e.moduleName == "Skyrim.exe");
            ImGui::SameLine(0.f, 8.f);
            if (isGameExe)
                ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.f), "(%s)", e.moduleName.c_str());
            else
                ImGui::TextColored(ImVec4(1.f, 0.70f, 0.20f, 1.f), "  mod: %s", e.moduleName.c_str());
        }

        ImGui::Spacing();

        // ── Outcome + timestamp ────────────────────────────────────────
        if (e.successful)
            ImGui::TextColored(ImVec4(0.22f, 1.f, 0.22f, 1.f), "Recovered");
        else
            ImGui::TextColored(ImVec4(1.f, 0.30f, 0.30f, 1.f), "Not recovered");
        ImGui::SameLine(100.f);
        ImGui::TextColored(ImVec4(0.40f, 0.40f, 0.40f, 1.f), "%s", e.timestamp.c_str());

        // ── Decoded instruction ────────────────────────────────────────
        if (!e.decodedInstruction.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.44f, 0.44f, 0.44f, 1.f), "Instruction:");
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.90f, 0.60f, 1.f));
            ImGui::TextUnformatted(e.decodedInstruction.c_str());
            ImGui::PopStyleColor();
        }

        // ── Access details ─────────────────────────────────────────────
        if (e.accessType >= 0) {
            ImGui::Spacing();
            const char* typeLbl = (e.accessType == 0) ? "Read from"
                                : (e.accessType == 1) ? "Write to"
                                : (e.accessType == 8) ? "Execute"
                                : "Access";

            bool isNullRegion = (e.accessAddress < 0x10000);
            char addrBuf[64];
            if (e.accessAddress == 0)
                snprintf(addrBuf, sizeof(addrBuf), "null (0x0)");
            else if (isNullRegion)
                snprintf(addrBuf, sizeof(addrBuf), "null + 0x%llX",
                         (unsigned long long)e.accessAddress);
            else
                snprintf(addrBuf, sizeof(addrBuf), "0x%llX",
                         (unsigned long long)e.accessAddress);

            ImGui::TextColored(ImVec4(0.44f, 0.44f, 0.44f, 1.f), "%s", typeLbl);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.f, 0.60f, 0.60f, 1.f), "%s", addrBuf);
        }

        // ── Register affected ──────────────────────────────────────────
        if (!e.affectedRegister.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.44f, 0.44f, 0.44f, 1.f), "Register zeroed:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.80f, 0.80f, 1.f, 1.f), "%s", e.affectedRegister.c_str());
        }

        // ── Suspected mods ─────────────────────────────────────────────
        if (!e.suspectedMods.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.f, 0.68f, 0.22f, 1.f), "Suspected mods:");
            for (const auto& mod : e.suspectedMods)
                ImGui::BulletText("%s", mod.c_str());
        }

        // ── Recovery layer section ─────────────────────────────────────
        using LID = CrashGuard::LayerID;
        const bool hasLayer = (e.layerUsed != LID::Unrecovered);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (!hasLayer) {
            ImGui::TextColored(ImVec4(1.f, 0.38f, 0.38f, 1.f), "Not recovered");
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.f));
            ImGui::TextWrapped("%s", CrashGuard::GetLayerShortDesc(LID::Unrecovered));
            ImGui::PopStyleColor();
        } else {
            // Layer badge + name
            ImGui::TextColored(ImVec4(0.38f, 0.78f, 1.f, 1.f), "How CrashGuard fixed it:");
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.22f, 1.f, 0.22f, 1.f), "[OK]");
            ImGui::SameLine(50.f);
            ImGui::SetWindowFontScale(1.08f);
            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 1.f), "%s",
                CrashGuard::GetLayerDisplayName(e.layerUsed));
            ImGui::SetWindowFontScale(1.f);

            // Source location
            auto loc = CrashGuard::GetLayerCodeLocation(e.layerUsed);
            if (loc.file && loc.line > 0) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.36f, 0.36f, 0.40f, 1.f),
                    "  (%s : %d)", loc.file, loc.line);
            }

            ImGui::Spacing();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Full code journey
            int jCount = 0;
            const CrashGuard::JourneyStep* jSteps =
                CrashGuard::GetLayerJourney(e.layerUsed, &jCount);
            for (int ji = 0; ji < jCount; ++ji) {
                const auto& js = jSteps[ji];
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.34f, 0.34f, 0.38f, 1.f));
                if (js.lineTo > 0 && js.lineTo != js.lineFrom)
                    ImGui::Text("  %s : %d-%d", js.file, js.lineFrom, js.lineTo);
                else
                    ImGui::Text("  %s : %d", js.file, js.lineFrom);
                ImGui::PopStyleColor();
                ImGui::SameLine(0.f, 6.f);
                ImGui::TextColored(ImVec4(0.88f, 0.88f, 0.88f, 1.f), "%s", js.heading);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.50f, 1.f));
                ImGui::TextWrapped("  %s", js.explanation);
                ImGui::PopStyleColor();
                ImGui::Spacing();
                if (js.code && js.code[0]) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.88f, 0.60f, 1.f));
                    ImGui::TextUnformatted(js.code);
                    ImGui::PopStyleColor();
                }
                if (ji < jCount - 1) {
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.18f, 0.18f, 0.20f, 1.f));
                    ImGui::Separator();
                    ImGui::PopStyleColor();
                    ImGui::Spacing();
                }
            }
        }

        ImGui::EndChild();  // RecDetail
  }

    void ImGuiConfigMenu::RenderResourceManagementTab() {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Resource Management Systems");
        ImGui::Separator();
        ImGui::TextWrapped("Real-time monitoring and management of game resources to prevent crashes.");
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Memory Pressure Detector
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Memory Pressure Monitor");
        ImGui::Spacing();
        
        auto& memDetector = MemoryPressureDetector::GetSingleton();
        auto memStats = memDetector.GetStats();
        
        ImGui::Indent();
        
        // Memory usage bar
        float memUsagePercent = memStats.usagePercent;
        ImVec4 barColor;
        if (memUsagePercent < 70.0f) {
            barColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f); // Green
        } else if (memUsagePercent < 85.0f) {
            barColor = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); // Yellow
        } else if (memUsagePercent < 95.0f) {
            barColor = ImVec4(1.0f, 0.5f, 0.2f, 1.0f); // Orange
        } else {
            barColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // Red
        }
        
        ImGui::Text("System RAM Usage:");
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
        ImGui::ProgressBar(memUsagePercent / 100.0f, ImVec2(-1, 0), "");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("%.1f%%", memUsagePercent);
        
        ImGui::Text("Total RAM: %.2f GB", memStats.totalRAM / (1024.0 * 1024.0 * 1024.0));
        ImGui::Text("Available: %.2f GB", memStats.availableRAM / (1024.0 * 1024.0 * 1024.0));
        ImGui::Text("Used: %.2f GB", memStats.usedRAM / (1024.0 * 1024.0 * 1024.0));
        
        ImGui::Spacing();
        ImGui::Text("Process Memory: %.2f MB", memStats.processMemory / (1024.0 * 1024.0));
        ImGui::Text("Peak Memory: %.2f MB", memStats.peakProcessMemory / (1024.0 * 1024.0));
        
        ImGui::Spacing();
        
        // Pressure level indicator
        const char* pressureLevels[] = {"Normal", "Elevated", "High", "CRITICAL"};
        ImVec4 pressureColors[] = {
            ImVec4(0.2f, 1.0f, 0.2f, 1.0f),  // Green
            ImVec4(1.0f, 0.8f, 0.2f, 1.0f),  // Yellow
            ImVec4(1.0f, 0.5f, 0.2f, 1.0f),  // Orange
            ImVec4(1.0f, 0.2f, 0.2f, 1.0f)   // Red
        };
        
        int pressureIdx = static_cast<int>(memStats.pressureLevel);
        ImGui::Text("Pressure Level:");
        ImGui::SameLine();
        ImGui::TextColored(pressureColors[pressureIdx], "%s", pressureLevels[pressureIdx]);
        
        if (memStats.allocationSpike) {
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "[!] Allocation spike detected!");
        }
        
        ImGui::Spacing();
        ImGui::TextWrapped("Recommendation: %s", memStats.recommendation.c_str());
        
        ImGui::Unindent();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextWrapped("Resource limiting and auto-culling UI is disabled while the subsystem is being reworked. Backups are in docs/backups.");
        ImGui::Spacing();
    }

    void ImGuiConfigMenu::RenderOverviewTab() {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Crash Guard Overview");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextWrapped("Quick system status and integration information.");
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // System Status - larger and more prominent
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "System Status");
        ImGui::Spacing();
        
        ImVec4 statusColor = m_enabled ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
        const char* statusText = m_enabled ? "[OK] Active" : "[OFF] Disabled";
        
        ImGui::Indent();
        ImGui::TextColored(statusColor, "%s", statusText);
        ImGui::SameLine();
        ImGui::Text("- Crash Guard Protection");
        ImGui::Unindent();
        
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Quick Stats - better layout
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Session Statistics");
        ImGui::Spacing();
        
        auto& recoverySystem = RecoveryNotifications::GetSingleton();
        
        if (ImGui::BeginTable("OverviewStats", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 200.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Total Recoveries:");
            ImGui::TableNextColumn();
            ImGui::Text("%zu", recoverySystem.GetTotalRecoveries());
            
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Successful:");
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%zu", recoverySystem.GetSuccessfulRecoveries());
            
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Failed:");
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%zu", recoverySystem.GetFailedRecoveries());
            
            ImGui::EndTable();
        }
        
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // External Tool Integration - cleaner layout
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "External Tool Integration");
        ImGui::Spacing();
        
        // CrashLogger
        if (CrashLoggerDetector::Detector::IsCrashLoggerPresent()) {
            auto crashLoggerInfo = CrashLoggerDetector::Detector::GetCrashLoggerInfo();
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[X]");
            ImGui::SameLine();
            ImGui::Text("CrashLogger");
            if (crashLoggerInfo.has_value()) {
                ImGui::Indent();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Version: %s", crashLoggerInfo->version.c_str());
                ImGui::Unindent();
            }
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ ]");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "CrashLogger (Not Detected)");
        }
        
        ImGui::Spacing();
        
        // Trainwreck
        if (CrashLoggerDetector::Detector::IsTrainwreckPresent()) {
            auto trainwreckInfo = CrashLoggerDetector::Detector::GetTrainwreckInfo();
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[X]");
            ImGui::SameLine();
            ImGui::Text("Trainwreck");
            if (trainwreckInfo.has_value()) {
                ImGui::Indent();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Version: %s", trainwreckInfo->version.c_str());
                ImGui::Unindent();
            }
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ ]");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Trainwreck (Not Detected)");
        }
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        ImGui::TextWrapped("Crash Guard works alongside CrashLogger and Trainwreck to provide comprehensive crash protection. Crash Guard prevents crashes proactively, while CrashLogger/Trainwreck provide detailed post-mortem analysis. Compatibility modes are automatically enabled when these tools are detected - no manual configuration needed.");
    }

    void ImGuiConfigMenu::RenderResourceMonitorTab() {
        auto& config = Config::GetMutable();
        
        // Performance Overlay Settings Section
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Performance Overlay Settings");
        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::CollapsingHeader("Overlay Configuration", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            ImGui::Spacing();
            
            ImGui::Checkbox("Enable Performance Overlay", &config.overlayEnabled);
            ImGui::TextWrapped("Show real-time performance metrics on screen during gameplay");
            
            if (config.overlayEnabled) {
                ImGui::Spacing();
                ImGui::Spacing();
                
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Display Options");
                ImGui::Spacing();
                
                ImGui::Checkbox("Show FPS", &config.overlayShowFPS);
                ImGui::Checkbox("Show Frame Time", &config.overlayShowFrameTime);
                ImGui::Checkbox("Show Memory Usage", &config.overlayShowMemory);
                ImGui::Checkbox("Show Crash Statistics", &config.overlayShowCrashStats);
                ImGui::Checkbox("Show Recovery Statistics", &config.overlayShowRecoveryStats);
                ImGui::Checkbox("Show Pattern Statistics", &config.overlayShowPatternStats);
                
                ImGui::Spacing();
                ImGui::Spacing();
                
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Appearance");
                ImGui::Spacing();
                
                const char* positions[] = { "Top Left", "Top Right", "Bottom Left", "Bottom Right" };
                ImGui::Text("Position:");
                ImGui::SetNextItemWidth(200);
                ImGui::Combo("##Position", &config.overlayPosition, positions, 4);
                
                ImGui::Spacing();
                
                ImGui::Text("Background Opacity:");
                ImGui::SetNextItemWidth(300);
                ImGui::SliderFloat("##BgAlpha", &config.overlayBackgroundAlpha, 0.0f, 1.0f, "%.2f");
                
                ImGui::Text("Text Opacity:");
                ImGui::SetNextItemWidth(300);
                ImGui::SliderFloat("##TextAlpha", &config.overlayTextAlpha, 0.0f, 1.0f, "%.2f");
                
                ImGui::Text("Scale:");
                ImGui::SetNextItemWidth(300);
                ImGui::SliderFloat("##Scale", &config.overlayScale, 0.5f, 2.0f, "%.2f");
            }
            
            ImGui::Unindent();
        }
        
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Performance Metrics Section
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Performance Metrics");
        ImGui::Separator();
        ImGui::Spacing();
        
        const auto& metrics = PerformanceMonitor::GetSingleton().GetMetrics();
        
        if (ImGui::CollapsingHeader("Frame Rate & Timing", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            ImGui::Spacing();
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Current FPS:");
            ImGui::SameLine(200);
            ImGui::Text("%.1f", metrics.currentFPS);
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Average FPS:");
            ImGui::SameLine(200);
            ImGui::Text("%.1f", metrics.averageFPS);
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Min / Max FPS:");
            ImGui::SameLine(200);
            ImGui::Text("%.1f / %.1f", metrics.minFPS, metrics.maxFPS);
            
            ImGui::Spacing();
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Frame Time:");
            ImGui::SameLine(200);
            ImGui::Text("%.2f ms", metrics.frameTimeMs);
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Average Frame Time:");
            ImGui::SameLine(200);
            ImGui::Text("%.2f ms", metrics.averageFrameTimeMs);
            
            ImGui::Unindent();
        }
        
        ImGui::Spacing();
        
        if (ImGui::CollapsingHeader("Memory Usage", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            ImGui::Spacing();
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Current Usage:");
            ImGui::SameLine(200);
            ImGui::Text("%zu MB", metrics.memoryUsageMB);
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Peak Usage:");
            ImGui::SameLine(200);
            ImGui::Text("%zu MB", metrics.peakMemoryMB);
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Available:");
            ImGui::SameLine(200);
            ImGui::Text("%zu MB", metrics.availableMemoryMB);
            
            ImGui::Unindent();
        }
        
        ImGui::Spacing();
        
        if (ImGui::CollapsingHeader("Crash Guard Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            ImGui::Spacing();
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Crashes Prevented:");
            ImGui::SameLine(200);
            ImGui::Text("%u", metrics.crashesPrevented);
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Meshes Validated:");
            ImGui::SameLine(200);
            ImGui::Text("%u", metrics.meshesValidated);
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Animations Validated:");
            ImGui::SameLine(200);
            ImGui::Text("%u", metrics.animationsValidated);
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Scripts Monitored:");
            ImGui::SameLine(200);
            ImGui::Text("%u", metrics.scriptsMonitored);
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Cells Validated:");
            ImGui::SameLine(200);
            ImGui::Text("%u", metrics.cellsValidated);
            
            ImGui::Spacing();
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Recovery Attempts:");
            ImGui::SameLine(200);
            ImGui::Text("%u", metrics.recoveryAttempts);
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Successful:");
            ImGui::SameLine(200);
            ImGui::Text("%u", metrics.successfulRecoveries);
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Failed:");
            ImGui::SameLine(200);
            ImGui::Text("%u", metrics.failedRecoveries);
            
            ImGui::Spacing();
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Patterns Learned:");
            ImGui::SameLine(200);
            ImGui::Text("%u", metrics.patternsLearned);
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Patterns Applied:");
            ImGui::SameLine(200);
            ImGui::Text("%u", metrics.patternsApplied);
            
            ImGui::Unindent();
        }
        
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Memory Pressure Section
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Memory Pressure Monitor");
        ImGui::Separator();
        ImGui::Spacing();
        
        auto& memDetector = MemoryPressureDetector::GetSingleton();
        auto memStats = memDetector.GetStats();
        
        // Memory usage bar
        float memUsagePercent = memStats.usagePercent;
        ImVec4 barColor;
        if (memUsagePercent < 70.0f) {
            barColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f); // Green
        } else if (memUsagePercent < 85.0f) {
            barColor = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); // Yellow
        } else if (memUsagePercent < 95.0f) {
            barColor = ImVec4(1.0f, 0.5f, 0.2f, 1.0f); // Orange
        } else {
            barColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // Red
        }
        
        ImGui::Text("System RAM Usage:");
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
        ImGui::ProgressBar(memUsagePercent / 100.0f, ImVec2(-1, 30), "");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("%.1f%%", memUsagePercent);
        
        ImGui::Spacing();
        
        if (ImGui::CollapsingHeader("Memory Details", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            ImGui::Spacing();
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Total RAM:");
            ImGui::SameLine(200);
            ImGui::Text("%.2f GB", memStats.totalRAM / (1024.0 * 1024.0 * 1024.0));
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Available:");
            ImGui::SameLine(200);
            ImGui::Text("%.2f GB", memStats.availableRAM / (1024.0 * 1024.0 * 1024.0));
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Used:");
            ImGui::SameLine(200);
            ImGui::Text("%.2f GB", memStats.usedRAM / (1024.0 * 1024.0 * 1024.0));
            
            ImGui::Spacing();
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Process Memory:");
            ImGui::SameLine(200);
            ImGui::Text("%.2f MB", memStats.processMemory / (1024.0 * 1024.0));
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Peak Memory:");
            ImGui::SameLine(200);
            ImGui::Text("%.2f MB", memStats.peakProcessMemory / (1024.0 * 1024.0));
            
            ImGui::Unindent();
        }
        
        ImGui::Spacing();
        
        // Pressure level indicator
        const char* pressureLevels[] = {"Normal", "Elevated", "High", "CRITICAL"};
        ImVec4 pressureColors[] = {
            ImVec4(0.2f, 1.0f, 0.2f, 1.0f),  // Green
            ImVec4(1.0f, 0.8f, 0.2f, 1.0f),  // Yellow
            ImVec4(1.0f, 0.5f, 0.2f, 1.0f),  // Orange
            ImVec4(1.0f, 0.2f, 0.2f, 1.0f)   // Red
        };
        
        int pressureIdx = static_cast<int>(memStats.pressureLevel);
        ImGui::Text("Pressure Level:");
        ImGui::SameLine();
        ImGui::TextColored(pressureColors[pressureIdx], "%s", pressureLevels[pressureIdx]);
        
        if (memStats.allocationSpike) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "[!] Allocation spike detected!");
        }
        
        ImGui::Spacing();
        ImGui::TextWrapped("Recommendation: %s", memStats.recommendation.c_str());
    }

    void ImGuiConfigMenu::RenderCrashHistoryTab() {
        // This is the old RecentRecoveries tab
        RenderRecentRecoveriesTab();
    }

    void ImGuiConfigMenu::RenderRecoveryTab() {
        // Full-width recovery history (master-detail).
        // The severity guide has been removed — it squished the detail panel
        // and added little value next to the real crash data already shown.
        {
            RenderRecentRecoveriesTab();

            // Stats are available in the Overview tab; nothing extra needed here.
            if (false) {
                RenderRecoveryStatisticsTab();
            }
        }
    }

    void ImGuiConfigMenu::RenderDebugTab() {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Debug Tools");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Hotkeys section
        if (ImGui::CollapsingHeader("Hotkey Configuration", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            ImGui::Spacing();
            
            ImGui::TextWrapped("Configure keyboard hotkeys for opening the Crash Guard menu.");
            ImGui::Spacing();
            
            auto& hotkeyMgr = HotkeyManager::GetSingleton();
            auto keyboardBinding = hotkeyMgr.GetBinding("ToggleMenu_Keyboard");
            
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Current Hotkey:");
            ImGui::SameLine();
            ImGui::Text("%s", keyboardBinding.ToString().c_str());
            
            ImGui::Spacing();
            
            if (ImGui::Button("Change Hotkey", ImVec2(200, 30))) {
                ImGui::OpenPopup("KeyboardBindingPopup");
            }
            
            // Keyboard binding popup (simplified)
            if (ImGui::BeginPopup("KeyboardBindingPopup")) {
                ImGui::Text("Press a key (F1-F12)...");
                ImGui::Separator();
                
                // Check for function keys
                for (int vk = VK_F1; vk <= VK_F12; vk++) {
                    if (GetAsyncKeyState(vk) & 0x8000) {
                        HotkeyBinding newBinding;
                        newBinding.device = InputDevice::Keyboard;
                        newBinding.keys = {vk};
                        newBinding.holdDuration = 0.0f;
                        
                        hotkeyMgr.SetBinding("ToggleMenu_Keyboard", newBinding);
                        ImGui::CloseCurrentPopup();
                        break;
                    }
                }
                
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                }
                
                ImGui::EndPopup();
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("Reset to Default (F11)", ImVec2(200, 30))) {
                hotkeyMgr.SetBinding("ToggleMenu_Keyboard", HotkeyManager::GetDefaultKeyboardBinding());
            }
            
            ImGui::Unindent();
        }
        
        ImGui::Spacing();
        ImGui::TextWrapped("Additional debug tools and diagnostics will be added here in future updates.");
    }

    void ImGuiConfigMenu::RenderSeverityGuideTab() {
        ImGui::BeginChild("SeverityGuide", ImVec2(0, 0), true);
        
        ImGui::TextWrapped(
            "CrashGuard classifies crashes into 4 severity levels to determine "
            "when user intervention is needed. This helps balance automatic "
            "recovery with user control for dangerous situations.");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // SAFE
        RenderSeverityLevel(
            "SAFE",
            ImVec4(0.3f, 1.0f, 0.3f, 1.0f),
            "[GREEN]",
            "Visual glitches, missing textures, UI errors",
            "No risk to save data or game state",
            "Auto-recovered silently",
            {
                "Rendering crashes",
                "Animation errors",
                "Missing mesh files",
                "UI layout issues"
            }
        );
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // WARNING
        RenderSeverityLevel(
            "WARNING",
            ImVec4(1.0f, 1.0f, 0.3f, 1.0f),
            "[YELLOW]",
            "Missing resources, null pointers, arithmetic errors",
            "Recoverable, no data loss expected",
            "Auto-recovered with toast notification",
            {
                "Null actor references",
                "Divide by zero",
                "Missing sound files",
                "Invalid spell effects"
            }
        );
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // CRITICAL
        RenderSeverityLevel(
            "CRITICAL",
            ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
            "[ORANGE]",
            "Save data or persistent state affected",
            "Risk of save corruption if continued",
            "USER CHOICE REQUIRED",
            {
                "Crashes in save/load functions",
                "Quest system errors",
                "Inventory corruption",
                "Player data corruption"
            }
        );
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // FATAL
        RenderSeverityLevel(
            "FATAL",
            ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
            "[RED]",
            "Stack corruption, unrecoverable state",
            "Cannot safely continue",
            "USER CHOICE REQUIRED (recommend load save)",
            {
                "Stack overflow",
                "Heap corruption",
                "Cascading failures",
                "Multiple simultaneous crashes"
            }
        );
        
        ImGui::EndChild();
    }

    void ImGuiConfigMenu::RenderSeverityLevel(
        const char* name,
        const ImVec4& color,
        const char* icon,
        const char* description,
        const char* riskLevel,
        const char* behavior,
        const std::vector<std::string>& examples) {
        
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted((std::string(icon) + " " + name).c_str());
        ImGui::PopStyleColor();
        
        ImGui::Indent();
        ImGui::TextWrapped("%s", description);
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Risk Level:");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", riskLevel);
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Behavior:");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", behavior);
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Examples:");
        for (const auto& example : examples) {
            ImGui::BulletText("%s", example.c_str());
        }
        
        ImGui::Unindent();
    }

    void ImGuiConfigMenu::RenderRecoveryStatisticsTab() {
        ImGui::BeginChild("RecoveryStats", ImVec2(0, 0), true);
        
        auto& stats = RecoveryStatistics::GetInstance();
        
        ImGui::TextUnformatted("Recovery Statistics (Current Session)");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Summary table
        if (ImGui::BeginTable("RecoveryStatsTable", 3, 
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            
            ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Behavior", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            
            // Safe
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Safe");
            ImGui::TableNextColumn();
            ImGui::Text("%u", stats.GetSafeCount());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Auto-recovered (silent)");
            
            // Warning
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "Warning");
            ImGui::TableNextColumn();
            ImGui::Text("%u", stats.GetWarningCount());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Auto-recovered (toast)");
            
            // Critical
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Critical");
            ImGui::TableNextColumn();
            ImGui::Text("%u", stats.GetCriticalCount());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("User prompted");
            
            // Fatal
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Fatal");
            ImGui::TableNextColumn();
            ImGui::Text("%u", stats.GetFatalCount());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("User prompted");
            
            ImGui::EndTable();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // User choice breakdown
        uint32_t totalUserChoices = stats.GetCriticalCount() + stats.GetFatalCount();
        if (totalUserChoices > 0) {
            ImGui::TextUnformatted("User Choices Made:");
            ImGui::Spacing();
            
            if (ImGui::BeginTable("UserChoicesTable", 2, 
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                
                ImGui::TableSetupColumn("Choice", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableHeadersRow();
                
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Continue Playing");
                ImGui::TableNextColumn();
                ImGui::Text("%u", stats.GetUserChoiceContinue());
                
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Load Last Save");
                ImGui::TableNextColumn();
                ImGui::Text("%u", stats.GetUserChoiceLoadSave());
                
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Teleport to Safe");
                ImGui::TableNextColumn();
                ImGui::Text("%u", stats.GetUserChoiceTeleport());
                
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("View Log");
                ImGui::TableNextColumn();
                ImGui::Text("%u", stats.GetUserChoiceViewLog());
                
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Let It Crash");
                ImGui::TableNextColumn();
                ImGui::Text("%u", stats.GetUserChoiceCrashAnyway());
                
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Timeout (Auto-selected)");
                ImGui::TableNextColumn();
                ImGui::Text("%u", stats.GetUserChoiceTimeout());
                
                ImGui::EndTable();
            }
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                "No Critical or Fatal crashes this session.");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("View Detailed Recovery History", ImVec2(-1, 0))) {
            // Switch to consolidated recovery tab
            m_activeTab = Tab::Recovery;
        }

        ImGui::EndChild();
    }

    // ─── Diagnostics Tab ──────────────────────────────────────────────────────
    // VEH layer statistics + animated crash-recovery test suite (14 tests).
    //
    // Animation design:
    //   Each recovery layer in the staircase reveals sequentially (0.30 s / layer).
    //   The newly-revealed layer flashes bright then settles to green (handled) or
    //   amber (tried, gave up).  Pending layers are ghosted.  The code-snippet pane
    //   on the right tracks the current animation frame automatically; hovering a
    //   specific layer overrides it.
    // ─────────────────────────────────────────────────────────────────────────
    void ImGuiConfigMenu::RenderDiagnosticsTab() {

        // ── VEH Recovery Statistics (collapsible) ──────────────────────────
        bool statsOpen = ImGui::TreeNodeEx("##stats_hdr",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed,
            "  Live Recovery Statistics");
        if (statsOpen) {
            ImGui::Spacing();
            auto stats = VEH::VEHExceptionHandler::GetLayerStats();
            if (ImGui::BeginTable("VEHStats", 2,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("What CrashGuard did",  ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Times fired",          ImGuiTableColumnFlags_WidthFixed, 90.f);
                ImGui::TableHeadersRow();
                auto row = [](const char* n, uint64_t v, bool hi = false) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (hi) ImGui::TextColored(ImVec4(0.9f, 1.f, 0.6f, 1.f), "%s", n);
                    else    ImGui::TextUnformatted(n);
                    ImGui::TableNextColumn();
                    if (v > 0) ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "%llu", (unsigned long long)v);
                    else       ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.f), "0");
                };
                row("Crashes prevented (all methods)",      stats.total,        true);
                row("  Pre-programmed known fix",           stats.knownSite);
                row("  Instruction decoded + skipped",      stats.instrPattern);
                row("  Remembered from earlier in session", stats.learnedSite);
                row("  Redirected bad pointer",             stats.regFixup);
                row("  Decoded + destination zeroed",       stats.instrSkip);
                row("  Returned from crashing function",    stats.funcReturn);
                row("  Found return point in stack",        stats.deepWalk);
                row("Not recovered (passed to OS)",         stats.unrecoverable);
                ImGui::EndTable();
            }
            ImGui::Spacing();

            // ── Pattern Learning explainer ──────────────────────────────
            bool learnOpen = ImGui::TreeNodeEx("##learn_hdr",
                ImGuiTreeNodeFlags_Framed,
                "  How CrashGuard Learns");
            if (learnOpen) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.7f, 1.f));
                ImGui::TextUnformatted("In-session memory  (always active)");
                ImGui::PopStyleColor();
                ImGui::TextWrapped(
                    "The first time CrashGuard fixes a crash it stores the instruction "
                    "address, decoded length, and which register was zeroed in a small "
                    "in-memory cache (up to 64 slots). If the exact same code crashes "
                    "again this session, recovery is instant  -  no Zydis decode, no "
                    "layer cascade. The 'Remembered from earlier in session' counter "
                    "above shows how often that fast path fired.");
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.7f, 1.f));
                ImGui::TextUnformatted("Between-session analytics  (file-based)");
                ImGui::PopStyleColor();
                ImGui::TextWrapped(
                    "CrashGuard also writes anonymised crash-site data to patterns.json. "
                    "This lets you see which crash addresses recur across play sessions "
                    "and is useful for identifying mods that crash consistently. "
                    "It does NOT change recovery speed or strategy  -  the file is "
                    "analytics only, not a lookup table used during recovery.");
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.f));
                ImGui::TextWrapped(
                    "In short: the in-memory cache is real and measurable (see counter). "
                    "The file log is a record, not a brain.");
                ImGui::PopStyleColor();
                ImGui::Spacing();
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Persistent state ────────────────────────────────────────────────
        static CrashTestSuite s_suite;
        static int  s_selTest = -1;
        static double s_animStart[CrashTestSuite::NUM_TESTS];
        static bool   s_animInit = false;
        if (!s_animInit) {
            for (auto& t : s_animStart) t = -999.0;
            s_animInit = true;
        }

        // Synthetic trace builder (used until real LayerTrace emission is wired in VEH.cpp)
        static auto buildTrace = [](int idx, const TestResult& r) -> CrashGuard::LayerTrace {
            using LID = CrashGuard::LayerID;
            CrashGuard::LayerTrace tr;
            if (!r.ran) return tr;
            tr.exceptionDesc = r.exceptionType;
            auto add = [&](LID id, const char* detail, bool ok) {
                tr.events.push_back({ id, std::string(detail), ok });
            };
            if (!r.vehIntercepted) {
                add(LID::Unrecovered, "CrashGuard did not intercept this exception.", false);
                return tr;
            }
            switch (idx) {
            case 0: add(LID::UR_ZeroedReg,  "Read from nullptr - answered with zero, execution continued.", true); break;
            case 1: add(LID::UR_WriteSkip,  "Write to nullptr - silently dropped, game never knew it failed.", true); break;
            case 2: add(LID::ExecAV_Return, "Call through null vtable - call cancelled, RAX set to 0, caller continued.", true); break;
            case 3:
                if (r.tierUsed == TestTier::Demo) {
                    // In Safe mode the kernel runs inside a __try/__except block.
                    // The SEH prologue adjusts RSP, so L5/L6 would read a wrong
                    // return address and jump to the wrong frame. Both are skipped;
                    // CrashGuard falls through to instruction-skip instead.
                    add(LID::UR_ZeroedReg,  "TEST instruction has no output register - no register to zero.", false);
                    add(LID::UR_FuncReturn, "L5 skipped: Safe mode __try prologue adjusted RSP - L5 would return to wrong frame.", false);
                    add(LID::UR_DeepWalk,   "L6 skipped: same reason - stack scan in Safe mode would find __try frame, not real call site.", false);
                    add(LID::UR_FlagsSkip,  "RIP advanced past the TEST instruction. Game takes the false/not-found branch.", true);
                } else {
                    // In Real Conditions / Live mode the stub runs in VirtualAlloc'd
                    // memory with no __try frame. RSP is clean - L5 reads the real
                    // return address back into ExecStub and jumps there cleanly.
                    add(LID::UR_ZeroedReg,  "TEST instruction has no output register - no register to zero.", false);
                    add(LID::UR_FuncReturn, "L5: stack is clean (no __try frame). Return address is valid. Returned cleanly from the crashing function.", true);
                }
                break;
            case 4: add(LID::UR_ZeroedReg,  "Read vtable pointer from null object - zeroed so the read returned null safely.", true); break;
            case 5: add(LID::UR_WriteSkip,  "Write to 0xDEADBEEF (poison sentinel address) - dropped silently.", true); break;
            case 6: add(LID::UR_ZeroedReg,  "Read at offset +0x2A0 on nullptr (partially loaded struct) - answered with zero.", true); break;
            case 7: add(LID::UR_ZeroedReg,  "10 sequential null reads in a loop - each intercepted and answered with zero.", true); break;
            case 8:
                if (r.tierUsed == TestTier::Demo) {
                    add(LID::UR_ZeroedReg, "Crash 1 of 2: read at +0x4 on nullptr - answered with zero.", true);
                    add(LID::UR_ZeroedReg, "Crash 2 of 2: read at +0x14 on nullptr - cascade limiter bypassed in Safe mode, caught.", true);
                } else {
                    // Real Conditions / Live: stubs placed at distinct page offsets (0 and 64)
                    // so each crash has a unique RIP. Cascade limiter is active and did not block.
                    add(LID::UR_ZeroedReg, "Crash 1 of 2: null read at stub offset 0 - answered with zero.", true);
                    add(LID::UR_ZeroedReg, "Crash 2 of 2: null write at stub offset 64 - distinct address, cascade limiter correctly allowed both.", true);
                }
                break;
            case 9:
                if (r.tierUsed == TestTier::Demo) {
                    add(LID::UR_ZeroedReg,  "TEST instruction has no output register - no register to zero.", false);
                    add(LID::UR_FuncReturn, "L5 skipped: Safe mode __try prologue adjusted RSP - L5 would return to wrong frame.", false);
                    add(LID::UR_DeepWalk,   "L6 skipped: same reason - stack scan in Safe mode would find __try frame, not real call site.", false);
                    add(LID::UR_FlagsSkip,  "RIP advanced past TEST BYTE PTR [r14+0x109],0x08 (mirrors SkyrimSE.exe+14F400E). Shadow system takes 'not visible' path. Crash pattern: null actor pointer in BSShadowFrustumLight during interior cell load.", true);
                } else {
                    add(LID::UR_ZeroedReg,  "TEST instruction has no output register - no register to zero.", false);
                    add(LID::UR_FuncReturn, "L5: stack is clean (no __try frame). Return address is valid. Returned cleanly from the crashing function (mirrors real interior-cell-load recovery).", true);
                }
                break;
            default: break;
            }
            return tr;
        };

        // ── Header ─────────────────────────────────────────────────────────
        ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.f), "Crash Recovery Tests");
        ImGui::Spacing();

        // ── Tier selector ───────────────────────────────────────────────────
        {
            TestTier curTier = CrashTestSuite::GetTestTier();

            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.f), "Test tier:");
            ImGui::SameLine();

            // Safe (formerly Demo) - bypassed internal mode
            ImGui::PushStyleColor(ImGuiCol_Text,
                curTier == TestTier::Demo
                    ? ImVec4(0.30f, 1.00f, 0.30f, 1.f)
                    : ImVec4(0.60f, 0.60f, 0.60f, 1.f));
            if (ImGui::RadioButton("Safe##tier", curTier == TestTier::Demo))
                CrashTestSuite::SetTestTier(TestTier::Demo);
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "Safe / bypass mode.\n"
                    "Crashes run inside CrashGuard.dll with cascade/cooldown bypassed\n"
                    "and a __try safety net. Verifies the mechanism works, but does not\n"
                    "reflect real in-game conditions (L5/L6 skipped for flag-only instructions).");

            ImGui::SameLine();

            // Demo (formerly Real Conditions) - real VEH behavior, with safety net
            ImGui::PushStyleColor(ImGuiCol_Text,
                curTier == TestTier::RealConditions
                    ? ImVec4(1.00f, 0.80f, 0.25f, 1.f)
                    : ImVec4(0.60f, 0.60f, 0.60f, 1.f));
            if (ImGui::RadioButton("Demo##tier", curTier == TestTier::RealConditions))
                CrashTestSuite::SetTestTier(TestTier::RealConditions);
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "Demo mode - real conditions, with safety net.\n"
                    "Crash kernels run in allocated memory outside CrashGuard.dll.\n"
                    "Real cascade and cooldown rules apply, no bypasses.\n"
                    "VEH behaves exactly as it does mid-game.\n"
                    "__try safety net present - game will NOT crash if VEH fails.");

            ImGui::SameLine();

            // Live
            ImGui::PushStyleColor(ImGuiCol_Text,
                curTier == TestTier::Live
                    ? ImVec4(1.00f, 0.32f, 0.32f, 1.f)
                    : ImVec4(0.60f, 0.60f, 0.60f, 1.f));
            if (ImGui::RadioButton("Live##tier", curTier == TestTier::Live))
                CrashTestSuite::SetTestTier(TestTier::Live);
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "No safety net.\n"
                    "If VEH fails to recover the crash, the game crashes to desktop.\n"
                    "Save your game before running any test in this mode.\n"
                    "Run All is disabled - run tests one at a time.");

            // Live warning inline
            if (curTier == TestTier::Live) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.32f, 0.32f, 1.f));
                ImGui::TextUnformatted("  Save your game. CTD if VEH fails.");
                ImGui::PopStyleColor();
            }
        }
        ImGui::Spacing();

        // ── Buttons: Run All + Clear ────────────────────────────────────────
        {
            const bool isLive = (CrashTestSuite::GetTestTier() == TestTier::Live);
            if (isLive) {
                // Grayed-out disabled button
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.16f, 0.16f, 0.16f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.16f, 0.16f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.16f, 0.16f, 0.16f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.35f, 0.35f, 0.35f, 1.f));
                ImGui::Button("  Run All  ", ImVec2(0, 22));
                ImGui::PopStyleColor(4);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Run All is disabled in Live mode.\nRun tests individually - one at a time.");
            } else {
                if (ImGui::Button("  Run All  ", ImVec2(0, 22))) {
                    for (int i = 0; i < CrashTestSuite::NUM_TESTS; ++i) {
                        s_suite.RunTest(i);
                        s_animStart[i] = ImGui::GetTime();
                    }
                    if (s_selTest < 0) s_selTest = 0;
                }
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.30f, 0.12f, 0.12f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.48f, 0.16f, 0.16f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.62f, 0.20f, 0.20f, 1.f));
            if (ImGui::Button("Clear", ImVec2(0, 22))) {
                s_suite   = CrashTestSuite{};
                s_selTest = -1;
                for (auto& t : s_animStart) t = -999.0;
            }
            ImGui::PopStyleColor(3);
        }
        ImGui::Spacing();

        // Layout split
        const float totalH  = ImGui::GetContentRegionAvail().y;
        const float listW   = 268.f;
        const float detailW = ImGui::GetContentRegionAvail().x - listW - 8.f;
        const int   VEH_N   = CrashTestSuite::SYSTEM_TEST_START;
        const int   SYS_N   = CrashTestSuite::NUM_TESTS - VEH_N;

        // ── Left: test list ─────────────────────────────────────────────────
        ImGui::BeginChild("DiagList", ImVec2(listW, totalH), true);

        auto renderGroup = [&](const char* label, int first, int count) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.82f, 0.82f, 0.30f, 1.f), "%s", label);
            ImGui::Separator();
            ImGui::Spacing();

            for (int i = first; i < first + count; ++i) {
                const TestResult& r  = s_suite.GetResult(i);
                const bool        sel = (s_selTest == i);

                // Full-row selectable (invisible, just provides highlight + click)
                char rowId[24]; snprintf(rowId, sizeof(rowId), "##row%d", i);
                if (sel) {
                    ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.18f, 0.32f, 0.18f, 1.f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.22f, 0.40f, 0.22f, 1.f));
                }
                if (ImGui::Selectable(rowId, sel, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 18)))
                    s_selTest = i;
                if (sel) ImGui::PopStyleColor(2);

                // Overlay: status glyph
                ImGui::SameLine(6.f);
                if      (!r.ran)   ImGui::TextColored(ImVec4(0.36f, 0.36f, 0.36f, 1.f), "o");
                else if (r.passed) ImGui::TextColored(ImVec4(0.22f, 1.f,   0.22f, 1.f), "+");
                else               ImGui::TextColored(ImVec4(1.f,   0.26f, 0.26f, 1.f), "x");

                // Name
                ImGui::SameLine(24.f);
                ImVec4 nameCol = sel      ? ImVec4(1.f,    1.f,    1.f,    1.f)
                               : !r.ran   ? ImVec4(0.58f,  0.58f,  0.58f,  1.f)
                               : r.passed ? ImVec4(0.80f,  1.f,    0.80f,  1.f)
                                          : ImVec4(1.f,    0.66f,  0.66f,  1.f);
                ImGui::TextColored(nameCol, "%s", r.name.c_str());

                // Elapsed time + tier badge (VEH tests only)
                if (r.ran) {
                    ImGui::SameLine(listW - 78.f);
                    ImGui::TextColored(ImVec4(0.38f, 0.38f, 0.38f, 1.f), "%.1fms", r.elapsedMs);

                    if (i < CrashTestSuite::SYSTEM_TEST_START) {
                        // S = Safe/bypass (green-ish), D = Demo/real conditions (amber), L = Live (red)
                        ImVec4 tCol;
                        char   tChar = 'S';
                        if      (r.tierUsed == TestTier::RealConditions) { tCol = {1.f, 0.80f, 0.25f, 0.9f}; tChar = 'D'; }
                        else if (r.tierUsed == TestTier::Live)            { tCol = {1.f, 0.32f, 0.32f, 0.9f}; tChar = 'L'; }
                        else                                               { tCol = {0.38f, 0.70f, 0.38f, 0.8f}; tChar = 'S'; }
                        ImGui::SameLine(listW - 22.f);
                        ImGui::TextColored(tCol, "%c", tChar);
                        if (ImGui::IsItemHovered()) {
                            const char* tip =
                                (r.tierUsed == TestTier::RealConditions)
                                    ? "Result from Demo tier\n(real cascade/cooldown, no bypasses, __try safety net)"
                                : (r.tierUsed == TestTier::Live)
                                    ? "Result from Live tier\n(no __try safety net - CTD if VEH fails)"
                                    : "Result from Safe tier\n(bypass mode: cascade/cooldown skipped, __try safety net)";
                            ImGui::SetTooltip("%s", tip);
                        }
                    }
                }
            }
            ImGui::Spacing();
        };

        renderGroup("Crash Recovery (VEH)", 0,     VEH_N);
        renderGroup("System Health",         VEH_N, SYS_N);
        ImGui::EndChild();  // DiagList

        // ── Right: detail panel ─────────────────────────────────────────────
        ImGui::SameLine();
        ImGui::BeginChild("DiagDetail", ImVec2(detailW, totalH), true);

        if (s_selTest < 0 || s_selTest >= CrashTestSuite::NUM_TESTS) {
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.42f, 0.42f, 0.42f, 1.f),
                "Select a test from the list to read what it does,\n"
                "then run it to see CrashGuard intercept the crash in real time.");
            ImGui::EndChild();
            return;
        }

        const TestResult& r  = s_suite.GetResult(s_selTest);
        const double      now = ImGui::GetTime();
        const float       age = (float)(now - s_animStart[s_selTest]);

        // Test name
        ImGui::Spacing();
        ImGui::SetWindowFontScale(1.18f);
        ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 1.f), "%s", r.name.c_str());
        ImGui::SetWindowFontScale(1.f);
        ImGui::Spacing();

        // Description
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.87f, 1.f, 1.f));
        ImGui::TextWrapped("%s", r.description.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.44f, 0.44f, 0.44f, 1.f), "Exception type: %s", r.exceptionType.c_str());
        ImGui::Spacing();

        // Run button — large + green before first run, subdued "Run Again" after
        if (!r.ran) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.34f, 0.10f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.52f, 0.15f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.20f, 0.70f, 0.20f, 1.f));
            char btnId[32]; snprintf(btnId, sizeof(btnId), "  Run Test  ##d%d", s_selTest);
            if (ImGui::Button(btnId, ImVec2(0, 26))) {
                s_suite.RunTest(s_selTest);
                s_animStart[s_selTest] = ImGui::GetTime();
            }
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.16f, 0.16f, 0.16f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.24f, 0.24f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.32f, 0.32f, 0.32f, 1.f));
            char btnId[32]; snprintf(btnId, sizeof(btnId), "Run Again##d%d", s_selTest);
            if (ImGui::Button(btnId)) {
                s_suite.RunTest(s_selTest);
                s_animStart[s_selTest] = ImGui::GetTime();
            }
            ImGui::PopStyleColor(3);
        }

        if (!r.ran) { ImGui::EndChild(); return; }

        // Result
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetWindowFontScale(1.24f);
        if (r.passed)
            ImGui::TextColored(ImVec4(0.22f, 1.f, 0.22f, 1.f), "PASS");
        else
            ImGui::TextColored(ImVec4(1.f, 0.28f, 0.28f, 1.f), "FAIL");
        ImGui::SetWindowFontScale(1.f);
        ImGui::SameLine(66.f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.f);
        ImGui::TextColored(ImVec4(0.44f, 0.44f, 0.44f, 1.f), "%.2f ms", r.elapsedMs);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text,
            r.passed ? ImVec4(0.80f, 1.f, 0.80f, 1.f) : ImVec4(1.f, 0.70f, 0.70f, 1.f));
        ImGui::TextWrapped("%s", r.resultMessage.c_str());
        ImGui::PopStyleColor();

        if (r.vehIntercepted && r.crashCountAfter > r.crashCountBefore) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.38f, 0.82f, 0.38f, 1.f),
                "Crash counter: %zu -> %zu",
                r.crashCountBefore, r.crashCountAfter);
        }

        // Recovery chain — only shown for VEH crash tests.
        // System health tests (indices >= SYSTEM_TEST_START) do not go through
        // the VEH recovery chain; showing "Not recovered" for them is misleading.
        if (s_selTest >= CrashTestSuite::SYSTEM_TEST_START) {
            ImGui::EndChild();
            return;
        }

        CrashGuard::LayerTrace trace = r.layerTrace.empty()
            ? buildTrace(s_selTest, r) : r.layerTrace;
        if (trace.empty()) { ImGui::EndChild(); return; }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.38f, 0.76f, 1.f, 1.f), "Recovery chain:");
        ImGui::Spacing();

        const int   nLayers  = (int)trace.events.size();
        static constexpr float STEP_DELAY = 0.30f;
        static constexpr float PULSE_DUR  = 0.55f;

        const int visUpTo = (age < 0.f) ? -1
            : std::min((int)(age / STEP_DELAY), nLayers - 1);

        const float availH  = ImGui::GetContentRegionAvail().y;
        const float stepsW  = detailW * 0.40f;
        const float codeW   = ImGui::GetContentRegionAvail().x - stepsW - 8.f;
        const float paneH   = std::max(availH, 10.f);

        // Steps pane
        ImGui::BeginChild("Steps", ImVec2(stepsW, paneH), false);
        for (int ei = 0; ei < nLayers; ++ei) {
            const auto& ev     = trace.events[ei];
            const float layAge = age - ei * STEP_DELAY;
            const bool  vis    = (layAge >= 0.f);
            const bool  pulse  = vis && (layAge < PULSE_DUR);
            const float flash  = pulse ? (1.f - layAge / PULSE_DUR) : 0.f;

            if (ei > 0)
                ImGui::TextColored({ 0.28f, 0.28f, 0.28f, vis ? 0.65f : 0.16f }, "  |");

            ImVec4 badgeCol, nameCol;
            if (!vis) {
                badgeCol = nameCol = { 0.24f, 0.24f, 0.24f, 0.32f };
            } else if (ev.handled) {
                badgeCol = nameCol = { 0.2f + flash*0.8f, 1.f, 0.2f + flash*0.3f, 1.f };
            } else {
                badgeCol = { 1.f, 0.55f + flash*0.45f, 0.1f + flash*0.2f, 1.f };
                nameCol  = { 0.70f, 0.50f, 0.20f, vis ? 1.f : 0.32f };
            }

            ImGui::TextColored(badgeCol, "  %s", ev.handled ? "[OK]" : "[ ]");
            ImGui::SameLine(56.f);
            ImGui::TextColored(nameCol, "%s", CrashGuard::GetLayerDisplayName(ev.id));

            if (vis && !ev.detail.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.44f, 0.44f, 0.44f, 1.f));
                ImGui::TextWrapped("       %s", ev.detail.c_str());
                ImGui::PopStyleColor();
                ImGui::Spacing();
            }
        }
        ImGui::EndChild();  // Steps

        // Code pane — full journey for the last revealed layer
        ImGui::SameLine();
        ImGui::BeginChild("Code", ImVec2(codeW, paneH), true);

        if (visUpTo >= 0 && visUpTo < nLayers) {
            const auto& ev    = trace.events[visUpTo];
            const float lAge  = age - visUpTo * STEP_DELAY;
            const float flash = (lAge >= 0.f && lAge < PULSE_DUR) ? (1.f - lAge / PULSE_DUR) : 0.f;

            // Header: layer name (crash-specific detail from ev.detail)
            ImVec4 hdrCol = ev.handled
                ? ImVec4{ 0.2f + flash*0.8f, 1.f, 0.2f + flash*0.3f, 1.f }
                : ImVec4{ 0.76f, 0.44f, 0.10f, 1.f };

            ImGui::SetWindowFontScale(1.06f);
            ImGui::TextColored(hdrCol, "%s", CrashGuard::GetLayerDisplayName(ev.id));
            ImGui::SetWindowFontScale(1.f);
            if (!ev.detail.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.86f, 1.f, 1.f));
                ImGui::TextWrapped("%s", ev.detail.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::Separator();
            ImGui::Spacing();

            // Full call journey: every source step touched for this layer
            int jCount = 0;
            const CrashGuard::JourneyStep* jSteps =
                CrashGuard::GetLayerJourney(ev.id, &jCount);

            for (int ji = 0; ji < jCount; ++ji) {
                const auto& js = jSteps[ji];

                // Step badge + file:line
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.34f, 0.34f, 0.38f, 1.f));
                if (js.lineTo > 0 && js.lineTo != js.lineFrom)
                    ImGui::Text("  %s : %d-%d", js.file, js.lineFrom, js.lineTo);
                else
                    ImGui::Text("  %s : %d", js.file, js.lineFrom);
                ImGui::PopStyleColor();

                // Step heading
                ImGui::SameLine(0.f, 6.f);
                ImGui::TextColored(ImVec4(0.90f, 0.90f, 0.90f, 1.f), "%s", js.heading);

                // One-sentence explanation
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.52f, 0.52f, 0.52f, 1.f));
                ImGui::TextWrapped("  %s", js.explanation);
                ImGui::PopStyleColor();
                ImGui::Spacing();

                // Code block
                if (js.code && js.code[0]) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.88f, 0.60f, 1.f));
                    ImGui::TextUnformatted(js.code);
                    ImGui::PopStyleColor();
                }

                if (ji < jCount - 1) {
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.20f, 0.20f, 0.22f, 1.f));
                    ImGui::Separator();
                    ImGui::PopStyleColor();
                    ImGui::Spacing();
                }
            }
        }

        ImGui::EndChild();  // Code
        ImGui::EndChild();  // DiagDetail

    }

}
