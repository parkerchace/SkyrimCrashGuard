// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <RE/Skyrim.h>

namespace CrashGuard {

/**
 * @brief Custom menu for CrashGuard configuration
 * 
 * This menu is opened when the player activates the CrashGuard configuration book.
 * It provides a UI for changing settings without console commands or MCM.
 */
class ConfigMenu : public RE::IMenu {
public:
    static constexpr const char* MENU_NAME = "CrashGuardConfigMenu";
    static constexpr const char* SWF_NAME = "CrashGuardConfig.swf";

    ConfigMenu();
    ~ConfigMenu() override = default;

    // IMenu interface
    static void Register();
    static void Open();
    static void Close();

    // Process menu messages from SWF
    RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override;

    // Handle Scaleform callbacks
    void AdvanceMovie(float a_interval, std::uint32_t a_currentTime) override;

private:
    // Send current config to SWF
    void SendConfigToUI();
    
    // Receive config changes from SWF
    void ReceiveConfigFromUI(const RE::FxDelegateArgs& a_params);
    
    // Scaleform callback handler
    class ConfigCallback : public RE::FxDelegateHandler {
    public:
        void Accept(CallbackProcessor* a_processor, RE::FxDelegateArgs& a_params) override;
    };

    std::unique_ptr<ConfigCallback> _callback;
};

/**
 * @brief Book activation handler
 * 
 * Intercepts book activation to open custom menu instead of book text
 */
class BookActivationHandler {
public:
    static void Install();
    
private:
    static void OnBookActivate(RE::TESObjectREFR* a_book, RE::Actor* a_actor);
    
    // Hook for book activation
    struct BookActivateHook {
        static void thunk(RE::TESObjectREFR* a_this, RE::Actor* a_actor, bool a_flag);
        static inline REL::Relocation<decltype(thunk)> func;
    };
};

} // namespace CrashGuard
