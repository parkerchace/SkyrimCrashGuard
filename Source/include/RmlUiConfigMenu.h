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
