// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "ImGuiConfigMenu.h"
#include "Config.h"
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
#include "NPCManager.h"
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
        m_autoManageNPCs = config.autoManageNPCs;
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
        config.autoManageNPCs = m_autoManageNPCs;
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
        if (Config::Get().allowBuiltinActions || bmConditional.IsRunning() || bmConditional.IsInteractiveRunning() || !bmConditional.GetSnapshots().empty()) {
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

                if (ImGui::BeginTabItem("NPC Tools", nullptr, m_activeTab == Tab::NPCTools ? flags : 0)) {
                    m_activeTab = Tab::NPCTools;
                    m_forceTabSwitch = false;
                    
                    // Use child window with proper sizing to leave room for bottom buttons
                    ImGui::BeginChild("NPCToolsContent", ImVec2(0, -60), false);
                    RenderNPCToolsTab();
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
            
            if (ImGui::CollapsingHeader("Advanced Features")) {
                ImGui::Indent();
                ImGui::Spacing();

                if (ImGui::Checkbox("Pattern Learning", &m_patternLearning)) {
                    SaveSettings();
                }
                ImGui::TextWrapped("Learn from crash patterns to automatically prevent recurring issues");
                ImGui::Spacing();

                if (ImGui::Checkbox("User Notifications", &m_notifications)) {
                    SaveSettings();
                }
                ImGui::TextWrapped("Show on-screen notifications when crashes are prevented");
                
                ImGui::Unindent();
            }

            ImGui::Spacing();
            
            if (ImGui::CollapsingHeader("Input Conflict Prevention")) {
                ImGui::Indent();
                ImGui::Spacing();
                
                ImGui::TextWrapped("Auto-detected menus with input blocking:");
                ImGui::Spacing();
                
                // Get auto-detected menus from MenuInputObserver
                auto& menuObserver = MenuInputObserver::GetSingleton();
                const auto& detectedMenus = menuObserver.GetDetectedMenus();
                
                if (detectedMenus.empty()) {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No custom menus detected yet");
                    ImGui::TextWrapped("(Menus are detected when you open them)");
                } else {
                    ImGui::BeginChild("DetectedMenusList", ImVec2(0, 180), true);
                    
                    for (const auto& menuName : detectedMenus) {
                        ImGui::BulletText("%s", menuName.c_str());
                    }
                    
                    ImGui::EndChild();
                    
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), 
                        "[OK] %zu custom menu(s) protected", detectedMenus.size());
                }
                
                ImGui::Spacing();
                ImGui::TextWrapped("These menus have automatic input blocking to prevent conflicts with camera zoom, favorites, combat, and other game controls.");
                
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
                m_patternLearning = true;
                m_notifications = true;
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
        
        if (m_scriptMonitoring) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[X]");
            ImGui::SameLine();
            ImGui::Text("Script Monitoring");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ ]");
            ImGui::SameLine();
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
        
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[X]");
        ImGui::SameLine();
        ImGui::Text("Deadlock Detection");
        
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
        ImGui::SetNextWindowBgAlpha(0.35f); // More transparent
        
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | 
                                ImGuiWindowFlags_AlwaysAutoResize |
                                ImGuiWindowFlags_NoSavedSettings |
                                ImGuiWindowFlags_NoFocusOnAppearing |
                                ImGuiWindowFlags_NoNav |
                                ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("CrashGuardNotifications", nullptr, flags)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 0.9f));
            
            // Show countdown
            int countdown = (int)std::ceil(m_notificationTimer);
            ImGui::Text("Skyrim Crash Guard Active");
            
            // Show keyboard binding
            auto& hotkeyMgr = HotkeyManager::GetSingleton();
            auto keyboardBinding = hotkeyMgr.GetBinding("ToggleMenu_Keyboard");
            
            if (!keyboardBinding.keys.empty()) {
                std::string kbText = "Press ";
                for (size_t i = 0; i < keyboardBinding.keys.size(); ++i) {
                    if (i > 0) kbText += "+";
                    kbText += KeyToString(keyboardBinding.keys[i], InputDevice::Keyboard);
                }
                kbText += " for menu";
                ImGui::Text("%s", kbText.c_str());
            }
            
            ImGui::Text("Countdown: %d", countdown);
            
            ImGui::PopStyleColor();
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
                ImGui::SetTooltip("Maximum number of recovery attempts during a cascade of crashes. Prevents infinite recovery loops.");
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
                ImGui::SetTooltip("Enable runtime patches to fix known game engine bugs and vulnerabilities.");
            }
            
            ImGui::Unindent();
        }
        
        // Proactive Validation
        if (ImGui::CollapsingHeader("Proactive Validation")) {
            ImGui::Indent();
            
            if (config.enableMeshValidation != m_savedValues.enableMeshValidation) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Mesh Validation", &config.enableMeshValidation);
            if (config.enableMeshValidation != m_savedValues.enableMeshValidation) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Validate mesh files before loading to prevent crashes from corrupted or invalid mesh data.");
            }
            
            if (config.enableAnimationValidation != m_savedValues.enableAnimationValidation) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Animation Validation", &config.enableAnimationValidation);
            if (config.enableAnimationValidation != m_savedValues.enableAnimationValidation) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Validate animation files before loading to prevent crashes from corrupted or invalid animation data.");
            }
            
            if (config.enableScriptMonitoring != m_savedValues.enableScriptMonitoring) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Script Monitoring", &config.enableScriptMonitoring);
            if (config.enableScriptMonitoring != m_savedValues.enableScriptMonitoring) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Monitor Papyrus scripts for timeouts and errors to prevent script-related crashes.");
            }
            
            if (config.enableCellValidation != m_savedValues.enableCellValidation) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Cell Validation", &config.enableCellValidation);
            if (config.enableCellValidation != m_savedValues.enableCellValidation) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Validate cell data before loading to prevent crashes from corrupted or invalid cell data.");
            }
            
            ImGui::Unindent();
        }
        
        // Safety Checks
        if (ImGui::CollapsingHeader("Safety Checks")) {
            ImGui::Indent();
            
            if (config.enableNullChecks != m_savedValues.enableNullChecks) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Null Pointer Checks", &config.enableNullChecks);
            if (config.enableNullChecks != m_savedValues.enableNullChecks) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Check for null pointers before dereferencing to prevent null pointer crashes.");
            }
            
            if (config.enableBoundsChecks != m_savedValues.enableBoundsChecks) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Bounds Checks", &config.enableBoundsChecks);
            if (config.enableBoundsChecks != m_savedValues.enableBoundsChecks) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Check array and buffer bounds before access to prevent buffer overflow crashes.");
            }
            
            if (config.enableFormIDChecks != m_savedValues.enableFormIDChecks) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("FormID Validation", &config.enableFormIDChecks);
            if (config.enableFormIDChecks != m_savedValues.enableFormIDChecks) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Validate FormIDs before use to prevent crashes from invalid or missing game objects.");
            }
            
            ImGui::Unindent();
        }
        
        // State Management
        if (ImGui::CollapsingHeader("State Management")) {
            ImGui::Indent();
            
            if (config.enableStateSnapshots != m_savedValues.enableStateSnapshots) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("State Snapshots", &config.enableStateSnapshots);
            if (config.enableStateSnapshots != m_savedValues.enableStateSnapshots) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enable state snapshots for rollback recovery. Allows restoring game state after a crash.");
            }
            
            int tempMaxSnapshots = config.maxSnapshotsPerSession;
            if (config.maxSnapshotsPerSession != m_savedValues.maxSnapshotsPerSession) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            if (ImGui::SliderInt("Max Snapshots Per Session", &tempMaxSnapshots, 10, 500)) {
                if (ValidateIntRange(tempMaxSnapshots, 10, 500, "Max Snapshots Per Session")) {
                    config.maxSnapshotsPerSession = tempMaxSnapshots;
                } else {
                    tempMaxSnapshots = config.maxSnapshotsPerSession;
                }
            }
            if (config.maxSnapshotsPerSession != m_savedValues.maxSnapshotsPerSession) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Maximum number of state snapshots to keep in memory. Higher values use more memory but provide more recovery options.");
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
                ImGui::SetTooltip("Display on-screen notifications when crashes are prevented or recovered.");
            }
            
            if (config.autoRecoverSafe != m_savedValues.autoRecoverSafe) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Auto-Recover Safe", &config.autoRecoverSafe);
            if (config.autoRecoverSafe != m_savedValues.autoRecoverSafe) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Automatically recover from crashes classified as 'safe' without user intervention.");
            }
            
            if (config.autoRecoverWarning != m_savedValues.autoRecoverWarning) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
            ImGui::Checkbox("Auto-Recover Warning", &config.autoRecoverWarning);
            if (config.autoRecoverWarning != m_savedValues.autoRecoverWarning) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Automatically recover from crashes classified as 'warning' level. May cause instability.");
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
                ImGui::SetTooltip("Maximum time a script can run before being considered timed out. Prevents infinite script loops.");
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
                ImGui::SetTooltip("Maximum number of recovery attempts for a single crash before giving up.");
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
                ImGui::SetTooltip("Aggregate similar crash patterns in logs to reduce redundancy and improve readability.");
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

    void ImGuiConfigMenu::RenderNPCToolsTab() {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "NPC Tools & Actor Management");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextWrapped("Simple NPC management: prevents spawning over threshold and cleans up dead bodies.");
        ImGui::Spacing();
        ImGui::Spacing();

        auto& config = Config::GetMutable();
        auto& npcMgr = NPCManager::GetSingleton();
        
        // Get current stats
        auto stats = npcMgr.GetStats();
        uint32_t maxActors = config.maxActors > 0 ? static_cast<uint32_t>(config.maxActors) : 1u;
        float usage = static_cast<float>(stats.activeNPCs) / static_cast<float>(maxActors);
        
        // Status display - larger and more prominent
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Current Status");
        ImGui::Spacing();
        
        ImGui::Text("Active NPCs:");
        ImGui::SameLine(150);
        ImGui::Text("%u / %u", stats.activeNPCs, maxActors);
        
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, 
            usage > 0.9f ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : 
            usage > 0.75f ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) : 
            ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
        ImGui::ProgressBar(usage, ImVec2(-1, 30), "");
        ImGui::PopStyleColor();
        
        // Color code the usage
        if (usage > 0.9f) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "WARNING: Near limit!");
        } else if (usage > 0.75f) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Approaching limit");
        } else {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Normal");
        }
        
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Settings in collapsible section
        if (ImGui::CollapsingHeader("NPC Management Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            ImGui::Spacing();
            
            ImGui::Text("Max NPCs Threshold:");
            ImGui::SetNextItemWidth(400);
            int actorLimit = config.maxActors;
            if (ImGui::SliderInt("##NPCLimit", &actorLimit, 50, 2000)) {
                config.maxActors = actorLimit;
            }
            ImGui::TextDisabled("Prevents new NPCs from spawning when this limit is reached");

            ImGui::Spacing();
            ImGui::Spacing();
            
            if (ImGui::Checkbox("Auto-manage NPCs", &config.autoManageNPCs)) {
                // persisted on Save
            }
            ImGui::TextDisabled("Automatically clean up dead bodies when near limit");
            
            ImGui::Spacing();
            
            if (ImGui::Checkbox("Show toast notifications", &config.npcToolsToasts)) {
                // persisted on Save
            }
            ImGui::TextDisabled("Show notifications when NPCs are prevented from spawning");
            
            ImGui::Unindent();
        }
        
        ImGui::Spacing();
        
        // Manual actions
        if (ImGui::CollapsingHeader("Manual Actions")) {
            ImGui::Indent();
            ImGui::Spacing();
            
            ImGui::TextWrapped("Use these buttons to manually manage NPCs in the current area.");
            ImGui::Spacing();
            
            if (ImGui::Button("Clean Up Dead Bodies", ImVec2(250, 35))) {
                npcMgr.CleanupDeadBodies();
            }
            ImGui::TextDisabled("Remove dead NPC bodies from the current cell");
            
            ImGui::Spacing();
            
            if (ImGui::Button("Force Audit", ImVec2(250, 35))) {
                npcMgr.ForceAudit();
            }
            ImGui::TextDisabled("Recount active NPCs and update statistics");
            
            ImGui::Unindent();
        }
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
        const auto& current = Config::Get();
        
        // Compare all fields
        return current.enabled != m_savedValues.enabled ||
               current.logLevel != m_savedValues.logLevel ||
               current.vehEnabled != m_savedValues.vehEnabled ||
               current.cascadeLimit != m_savedValues.cascadeLimit ||
               current.patchesEnabled != m_savedValues.patchesEnabled ||
               current.enableMeshValidation != m_savedValues.enableMeshValidation ||
               current.enableAnimationValidation != m_savedValues.enableAnimationValidation ||
               current.enableScriptMonitoring != m_savedValues.enableScriptMonitoring ||
               current.enableCellValidation != m_savedValues.enableCellValidation ||
               current.enableNullChecks != m_savedValues.enableNullChecks ||
               current.enableBoundsChecks != m_savedValues.enableBoundsChecks ||
               current.enableFormIDChecks != m_savedValues.enableFormIDChecks ||
               current.enableStateSnapshots != m_savedValues.enableStateSnapshots ||
               current.maxSnapshotsPerSession != m_savedValues.maxSnapshotsPerSession ||
               current.enableLearning != m_savedValues.enableLearning ||
               current.patternDatabasePath != m_savedValues.patternDatabasePath ||
               current.showNotifications != m_savedValues.showNotifications ||
               current.autoRecoverSafe != m_savedValues.autoRecoverSafe ||
               current.autoRecoverWarning != m_savedValues.autoRecoverWarning ||
               current.notificationTimeoutSeconds != m_savedValues.notificationTimeoutSeconds ||
               current.scriptTimeoutMs != m_savedValues.scriptTimeoutMs ||
               current.maxRecoveryAttempts != m_savedValues.maxRecoveryAttempts ||
               current.enableDetailedLogging != m_savedValues.enableDetailedLogging ||
               current.logOnlyFailures != m_savedValues.logOnlyFailures ||
               current.logSuccessfulRecoveries != m_savedValues.logSuccessfulRecoveries ||
               current.aggregatePatterns != m_savedValues.aggregatePatterns ||
               current.maxLogSizeMB != m_savedValues.maxLogSizeMB ||
               current.maxLogFiles != m_savedValues.maxLogFiles;
    }

    void ImGuiConfigMenu::RenderRecentRecoveriesTab() {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Recent Crash Recoveries");
        ImGui::Separator();
        ImGui::TextWrapped("View all crash recoveries including silent auto-recoveries. Toast notifications appear at top-right for 3-5 seconds.");
        
        ImGui::Spacing();
        
        auto& recoverySystem = RecoveryNotifications::GetSingleton();
        
        // Statistics
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Statistics");
        ImGui::Spacing();
        ImGui::Indent();
        ImGui::Text("Total Recoveries: %zu", recoverySystem.GetTotalRecoveries());
        ImGui::Text("Successful: %zu", recoverySystem.GetSuccessfulRecoveries());
        ImGui::Text("Failed: %zu", recoverySystem.GetFailedRecoveries());
        ImGui::Unindent();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Clear history button
        if (ImGui::Button("Clear History")) {
            recoverySystem.ClearHistory();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Recovery history
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Recovery History (Most Recent First)");
        ImGui::Spacing();
        
        auto recoveries = recoverySystem.GetRecentRecoveries(50);
        
        if (recoveries.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No recoveries recorded yet.");
        } else {
            ImGui::BeginChild("RecoveryHistory", ImVec2(0, 400), true);
            
            for (size_t i = 0; i < recoveries.size(); ++i) {
                const auto& entry = recoveries[i];
                
                ImGui::PushID((int)i);
                
                // Severity color
                ImVec4 severityColor;
                if (entry.severity == "Safe") {
                    severityColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
                } else if (entry.severity == "Warning") {
                    severityColor = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                } else {
                    severityColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
                }
                
                // Header with severity and timestamp
                ImGui::TextColored(severityColor, "[%s]", entry.severity.c_str());
                ImGui::SameLine();
                ImGui::Text("%s", entry.timestamp.c_str());
                ImGui::SameLine();
                if (entry.successful) {
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[OK] Success");
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "[FAIL] Failed");
                }
                
                ImGui::Indent();
                
                // Root cause
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Cause:");
                ImGui::SameLine();
                ImGui::TextWrapped("%s", entry.rootCause.c_str());
                
                // Strategy
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Strategy:");
                ImGui::SameLine();
                ImGui::Text("%s", entry.strategy.c_str());
                
                // Actions
                if (!entry.actions.empty()) {
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Actions:");
                    ImGui::Indent();
                    for (const auto& action : entry.actions) {
                        ImGui::BulletText("%s", action.c_str());
                    }
                    ImGui::Unindent();
                }
                
                // Suspected mods
                if (!entry.suspectedMods.empty()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Suspected Mods:");
                    ImGui::Indent();
                    for (const auto& mod : entry.suspectedMods) {
                        ImGui::BulletText("%s", mod.c_str());
                    }
                    ImGui::Unindent();
                }
                
                ImGui::Unindent();
                
                // Separator between entries
                if (i < recoveries.size() - 1) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                }
                
                ImGui::PopID();
            }
            
            ImGui::EndChild();
        }
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
        
        auto& npcMgr = NPCManager::GetSingleton();
        npcMgr.ForceAudit();
        auto stats = npcMgr.GetStats();
        uint32_t currentActors = stats.activeNPCs;
        uint32_t maxActors = Config::Get().maxActors > 0 ? static_cast<uint32_t>(Config::Get().maxActors) : 0u;
        float actorUsage = (maxActors > 0) ? (static_cast<float>(currentActors) / maxActors * 100.0f) : 0.0f;

        ImGui::Text("Active Actors:");
        ImGui::SameLine(200);
        ImGui::Text("%u / %u (%.1f%%)", currentActors, maxActors, actorUsage);
        
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
        // Consolidated recovery view: history (left) + severity guide + stats (right)
        if (ImGui::BeginTable("RecoveryTable", 2, ImGuiTableFlags_SizingStretchProp)) {
            // Left column: Recent recoveries
            ImGui::TableNextColumn();
            RenderRecentRecoveriesTab();

            // Right column: Severity guide and recovery statistics
            ImGui::TableNextColumn();
            ImGui::BeginChild("RecoveryRight", ImVec2(0, 0), false);
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.2f, 1.0f), "Severity Guide & Recovery Statistics");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            RenderSeverityGuideTab();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            RenderRecoveryStatisticsTab();

            ImGui::EndChild();
            ImGui::EndTable();
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


}
