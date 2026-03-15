// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <RmlUi/Core.h>
#include "Config.h"

namespace CrashGuard {

    // Event listener for menu interactions
    class ConfigMenuEventListener : public Rml::EventListener {
    public:
        ConfigMenuEventListener(class RmlUiConfigMenu* menu) : m_menu(menu) {}
        void ProcessEvent(Rml::Event& event) override;
        
    private:
        class RmlUiConfigMenu* m_menu;
    };

    class RmlUiConfigMenu {
    public:
        static RmlUiConfigMenu& GetSingleton() {
            static RmlUiConfigMenu instance;
            return instance;
        }

        void Initialize();
        void Toggle();
        bool IsVisible() const { return m_visible; }
        
        // Event handlers
        void OnSaveClicked();
        void OnCloseClicked();
        void OnResetClicked();
        void OnSettingChanged(const std::string& settingName, const std::string& value);

    private:
        RmlUiConfigMenu() = default;
        ~RmlUiConfigMenu() = default;
        RmlUiConfigMenu(const RmlUiConfigMenu&) = delete;
        RmlUiConfigMenu& operator=(const RmlUiConfigMenu&) = delete;

        bool m_visible = false;
        Rml::ElementDocument* m_document = nullptr;
        std::unique_ptr<ConfigMenuEventListener> m_eventListener;
        
        // Notification state
        float m_notificationTimer = 11.0f;
        bool m_notificationVisible = true;
        
        // Saved values for change detection
        Config::Settings m_savedValues;
        
        void LoadDocument();
        void UpdateFormValues();
        void SaveTomlFile();
        bool HasUnsavedChanges() const;
        void ShowNotification(const std::string& message, bool success);
    };

}
