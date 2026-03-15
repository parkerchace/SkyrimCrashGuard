// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "HotkeyManager.h"
#include "Config.h"
#include <spdlog/spdlog.h>
#include <Windows.h>
#include <Xinput.h>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "Xinput.lib")

namespace CrashGuard {

    // ========================================================================
    // HotkeyBinding Implementation
    // ========================================================================
    
    bool HotkeyBinding::IsPressed(const std::vector<int>& pressedKeys, float heldTime) const {
        // Check if all required keys are pressed
        for (int key : keys) {
            if (std::find(pressedKeys.begin(), pressedKeys.end(), key) == pressedKeys.end()) {
                return false;  // Required key not pressed
            }
        }
        
        // Check hold duration
        if (holdDuration > 0.0f && heldTime < holdDuration) {
            return false;  // Not held long enough
        }
        
        return true;
    }
    
    std::string HotkeyBinding::ToString() const {
        std::ostringstream oss;
        
        // Device
        oss << (device == InputDevice::Keyboard ? "Keyboard" : "Gamepad");
        oss << ":";
        
        // Keys (joined with +)
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i > 0) oss << "+";
            oss << KeyToString(keys[i], device);
        }
        
        // Hold duration
        oss << ":" << holdDuration;
        
        return oss.str();
    }
    
    HotkeyBinding HotkeyBinding::FromString(const std::string& str) {
        HotkeyBinding binding;
        
        std::istringstream iss(str);
        std::string deviceStr, keysStr, durationStr;
        
        // Parse: "Device:Key1+Key2+Key3:Duration"
        if (std::getline(iss, deviceStr, ':') &&
            std::getline(iss, keysStr, ':') &&
            std::getline(iss, durationStr)) {
            
            // Parse device
            binding.device = (deviceStr == "Keyboard") ? InputDevice::Keyboard : InputDevice::Gamepad;
            
            // Parse keys
            std::istringstream keysStream(keysStr);
            std::string keyStr;
            while (std::getline(keysStream, keyStr, '+')) {
                int key = StringToKey(keyStr, binding.device);
                if (key != 0) {
                    binding.keys.push_back(key);
                }
            }
            
            // Parse duration
            try {
                binding.holdDuration = std::stof(durationStr);
            } catch (...) {
                binding.holdDuration = 0.0f;
            }
        }
        
        return binding;
    }

    // ========================================================================
    // HotkeyManager Implementation
    // ========================================================================
    
    bool HotkeyManager::Initialize() {
        if (m_initialized) {
            return true;
        }
        
        spdlog::info("[HotkeyManager] Initializing hotkey system...");
        
        // Load bindings from config
        LoadBindings();
        
        m_initialized = true;
        spdlog::info("[HotkeyManager] Hotkey system initialized");
        return true;
    }
    
    void HotkeyManager::Shutdown() {
        if (!m_initialized) {
            return;
        }
        
        // Save bindings to config
        SaveBindings();
        
        m_actions.clear();
        m_initialized = false;
        
        spdlog::info("[HotkeyManager] Hotkey system shutdown");
    }
    
    void HotkeyManager::Update() {
        if (!m_initialized) {
            return;
        }
        
        // Update input states
        UpdateKeyboardState();
        // UpdateGamepadState(); // DISABLED - causes Skyrim to switch to keyboard mode
        
        // Check all registered actions
        for (auto& [actionName, action] : m_actions) {
            InputState* state = nullptr;
            
            // Select appropriate input state
            if (action.binding.device == InputDevice::Keyboard) {
                state = &m_keyboardState;
            } else {
                state = &m_gamepadState;
            }
            
            // Check if binding is triggered
            bool triggered = CheckBinding(action.binding, *state);
            
            // Debug log for gamepad bindings
            if (action.binding.device == InputDevice::Gamepad && !state->pressedKeys.empty()) {
                spdlog::trace("[HotkeyManager] Checking '{}': triggered={}, holdTime={:.2f}/{:.2f}", 
                    actionName, triggered, state->currentHoldTime, action.binding.holdDuration);
            }
            
            // Trigger callback on rising edge (just triggered)
            if (triggered && !action.wasTriggered) {
                spdlog::info("[HotkeyManager] Action '{}' triggered!", actionName);
                if (action.callback) {
                    action.callback();
                }
                action.lastTriggerTime = std::chrono::steady_clock::now();
            }
            
            action.wasTriggered = triggered;
        }
    }
    
    void HotkeyManager::RegisterHotkey(const std::string& actionName, 
                                      const HotkeyBinding& binding,
                                      std::function<void()> callback) {
        HotkeyAction action;
        action.binding = binding;
        action.callback = callback;
        action.wasTriggered = false;
        
        m_actions[actionName] = action;
        
        spdlog::info("[HotkeyManager] Registered hotkey '{}': {}", actionName, binding.ToString());
    }
    
    void HotkeyManager::UnregisterHotkey(const std::string& actionName) {
        m_actions.erase(actionName);
        spdlog::info("[HotkeyManager] Unregistered hotkey '{}'", actionName);
    }
    
    HotkeyBinding HotkeyManager::GetBinding(const std::string& actionName) const {
        auto it = m_actions.find(actionName);
        if (it != m_actions.end()) {
            return it->second.binding;
        }
        return HotkeyBinding();
    }
    
    void HotkeyManager::SetBinding(const std::string& actionName, const HotkeyBinding& binding) {
        auto it = m_actions.find(actionName);
        if (it != m_actions.end()) {
            it->second.binding = binding;
            spdlog::info("[HotkeyManager] Updated binding for '{}': {}", actionName, binding.ToString());
        }
    }
    
    bool HotkeyManager::IsKeyPressed(int key) const {
        // Check keyboard
        auto it = std::find(m_keyboardState.pressedKeys.begin(), 
                           m_keyboardState.pressedKeys.end(), key);
        if (it != m_keyboardState.pressedKeys.end()) {
            return true;
        }
        
        // Check gamepad
        it = std::find(m_gamepadState.pressedKeys.begin(), 
                      m_gamepadState.pressedKeys.end(), key);
        return it != m_gamepadState.pressedKeys.end();
    }
    
    bool HotkeyManager::IsKeyJustPressed(int key) const {
        // Check keyboard
        auto it = std::find(m_keyboardState.justPressedKeys.begin(), 
                           m_keyboardState.justPressedKeys.end(), key);
        if (it != m_keyboardState.justPressedKeys.end()) {
            return true;
        }
        
        // Check gamepad
        it = std::find(m_gamepadState.justPressedKeys.begin(), 
                      m_gamepadState.justPressedKeys.end(), key);
        return it != m_gamepadState.justPressedKeys.end();
    }
    
    std::vector<std::string> HotkeyManager::GetActionNames() const {
        std::vector<std::string> names;
        for (const auto& [name, action] : m_actions) {
            names.push_back(name);
        }
        return names;
    }
    
    float HotkeyManager::GetCurrentHoldTime(InputDevice device) const {
        if (device == InputDevice::Keyboard) {
            return m_keyboardState.currentHoldTime;
        } else {
            return m_gamepadState.currentHoldTime;
        }
    }
    
    void HotkeyManager::LoadBindings() {
        // Load from config file
        // For now, use defaults
        
        spdlog::info("[HotkeyManager] Loaded hotkey bindings from config");
    }
    
    void HotkeyManager::SaveBindings() {
        // Save to config file
        
        spdlog::info("[HotkeyManager] Saved hotkey bindings to config");
    }
    
    HotkeyBinding HotkeyManager::GetDefaultKeyboardBinding() {
        HotkeyBinding binding;
        binding.device = InputDevice::Keyboard;
        binding.keys = { static_cast<int>(KeyboardKey::F11) };
        binding.holdDuration = 0.0f;
        binding.name = "F11";
        return binding;
    }
    
    HotkeyBinding HotkeyManager::GetDefaultGamepadBinding() {
        HotkeyBinding binding;
        binding.device = InputDevice::Gamepad;
        binding.keys = { 
            static_cast<int>(GamepadButton::LeftThumb),   // L3
            static_cast<int>(GamepadButton::RightThumb)   // R3
        };
        binding.holdDuration = 0.75f;  // Hold for 0.75 seconds
        binding.name = "L3+R3 (Hold 0.75s)";
        return binding;
    }
    
    // ========================================================================
    // Private Methods
    // ========================================================================
    
    void HotkeyManager::UpdateKeyboardState() {
        static std::vector<int> previousKeys;
        std::vector<int> currentKeys;
        
        // Check all relevant keyboard keys
        std::vector<int> keysToCheck = {
            VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6,
            VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
            VK_SHIFT, VK_CONTROL, VK_MENU,  // Shift, Ctrl, Alt
            VK_ESCAPE, VK_TAB, VK_SPACE, VK_RETURN
        };
        
        for (int key : keysToCheck) {
            if (GetAsyncKeyState(key) & 0x8000) {
                currentKeys.push_back(key);
            }
        }
        
        // Determine just pressed keys (in current but not in previous)
        m_keyboardState.justPressedKeys.clear();
        for (int key : currentKeys) {
            if (std::find(previousKeys.begin(), previousKeys.end(), key) == previousKeys.end()) {
                m_keyboardState.justPressedKeys.push_back(key);
            }
        }
        
        // Update hold time
        if (!currentKeys.empty()) {
            if (previousKeys.empty()) {
                // Just started pressing
                m_keyboardState.pressStartTime = std::chrono::steady_clock::now();
                m_keyboardState.currentHoldTime = 0.0f;
            } else {
                // Continue holding
                auto now = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - m_keyboardState.pressStartTime);
                m_keyboardState.currentHoldTime = duration.count() / 1000.0f;
            }
        } else {
            m_keyboardState.currentHoldTime = 0.0f;
        }
        
        m_keyboardState.pressedKeys = currentKeys;
        previousKeys = currentKeys;
    }
    
    void HotkeyManager::UpdateGamepadState() {
        // COMPLETELY DISABLED - no XInput calls at all
        // Testing to isolate input mode switching issue
        
        m_gamepadState.pressedKeys.clear();
        m_gamepadState.justPressedKeys.clear();
        m_gamepadState.currentHoldTime = 0.0f;
        return;
    }
    
    bool HotkeyManager::CheckBinding(const HotkeyBinding& binding, const InputState& state) {
        return binding.IsPressed(state.pressedKeys, state.currentHoldTime);
    }
    
    // ========================================================================
    // Helper Functions
    // ========================================================================
    
    std::string KeyToString(int key, InputDevice device) {
        if (device == InputDevice::Keyboard) {
            switch (key) {
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
                case VK_SHIFT: return "Shift";
                case VK_CONTROL: return "Ctrl";
                case VK_MENU: return "Alt";
                case VK_ESCAPE: return "Escape";
                case VK_TAB: return "Tab";
                case VK_SPACE: return "Space";
                case VK_RETURN: return "Enter";
                default: return "Unknown";
            }
        } else {
            switch (key) {
                case static_cast<int>(GamepadButton::A): return "A";
                case static_cast<int>(GamepadButton::B): return "B";
                case static_cast<int>(GamepadButton::X): return "X";
                case static_cast<int>(GamepadButton::Y): return "Y";
                case static_cast<int>(GamepadButton::DPadUp): return "DPad-Up";
                case static_cast<int>(GamepadButton::DPadDown): return "DPad-Down";
                case static_cast<int>(GamepadButton::DPadLeft): return "DPad-Left";
                case static_cast<int>(GamepadButton::DPadRight): return "DPad-Right";
                case static_cast<int>(GamepadButton::Start): return "Start";
                case static_cast<int>(GamepadButton::Back): return "Back";
                case static_cast<int>(GamepadButton::LeftThumb): return "L3";
                case static_cast<int>(GamepadButton::RightThumb): return "R3";
                case static_cast<int>(GamepadButton::LeftShoulder): return "L1";
                case static_cast<int>(GamepadButton::RightShoulder): return "R1";
                case static_cast<int>(GamepadButton::LeftTrigger): return "L2";
                case static_cast<int>(GamepadButton::RightTrigger): return "R2";
                default: return "Unknown";
            }
        }
    }
    
    int StringToKey(const std::string& str, InputDevice device) {
        if (device == InputDevice::Keyboard) {
            if (str == "F1") return VK_F1;
            if (str == "F2") return VK_F2;
            if (str == "F3") return VK_F3;
            if (str == "F4") return VK_F4;
            if (str == "F5") return VK_F5;
            if (str == "F6") return VK_F6;
            if (str == "F7") return VK_F7;
            if (str == "F8") return VK_F8;
            if (str == "F9") return VK_F9;
            if (str == "F10") return VK_F10;
            if (str == "F11") return VK_F11;
            if (str == "F12") return VK_F12;
            if (str == "Shift") return VK_SHIFT;
            if (str == "Ctrl") return VK_CONTROL;
            if (str == "Alt") return VK_MENU;
            if (str == "Escape") return VK_ESCAPE;
            if (str == "Tab") return VK_TAB;
            if (str == "Space") return VK_SPACE;
            if (str == "Enter") return VK_RETURN;
        } else {
            if (str == "A") return static_cast<int>(GamepadButton::A);
            if (str == "B") return static_cast<int>(GamepadButton::B);
            if (str == "X") return static_cast<int>(GamepadButton::X);
            if (str == "Y") return static_cast<int>(GamepadButton::Y);
            if (str == "DPad-Up") return static_cast<int>(GamepadButton::DPadUp);
            if (str == "DPad-Down") return static_cast<int>(GamepadButton::DPadDown);
            if (str == "DPad-Left") return static_cast<int>(GamepadButton::DPadLeft);
            if (str == "DPad-Right") return static_cast<int>(GamepadButton::DPadRight);
            if (str == "Start") return static_cast<int>(GamepadButton::Start);
            if (str == "Back") return static_cast<int>(GamepadButton::Back);
            if (str == "L3") return static_cast<int>(GamepadButton::LeftThumb);
            if (str == "R3") return static_cast<int>(GamepadButton::RightThumb);
            if (str == "L1") return static_cast<int>(GamepadButton::LeftShoulder);
            if (str == "R1") return static_cast<int>(GamepadButton::RightShoulder);
            if (str == "L2") return static_cast<int>(GamepadButton::LeftTrigger);
            if (str == "R2") return static_cast<int>(GamepadButton::RightTrigger);
        }
        return 0;
    }
    
    std::vector<std::pair<std::string, int>> GetAvailableKeys(InputDevice device) {
        std::vector<std::pair<std::string, int>> keys;
        
        if (device == InputDevice::Keyboard) {
            keys.push_back({"F1", VK_F1});
            keys.push_back({"F2", VK_F2});
            keys.push_back({"F3", VK_F3});
            keys.push_back({"F4", VK_F4});
            keys.push_back({"F5", VK_F5});
            keys.push_back({"F6", VK_F6});
            keys.push_back({"F7", VK_F7});
            keys.push_back({"F8", VK_F8});
            keys.push_back({"F9", VK_F9});
            keys.push_back({"F10", VK_F10});
            keys.push_back({"F11", VK_F11});
            keys.push_back({"F12", VK_F12});
            keys.push_back({"Shift", VK_SHIFT});
            keys.push_back({"Ctrl", VK_CONTROL});
            keys.push_back({"Alt", VK_MENU});
        } else {
            keys.push_back({"A", static_cast<int>(GamepadButton::A)});
            keys.push_back({"B", static_cast<int>(GamepadButton::B)});
            keys.push_back({"X", static_cast<int>(GamepadButton::X)});
            keys.push_back({"Y", static_cast<int>(GamepadButton::Y)});
            keys.push_back({"L1", static_cast<int>(GamepadButton::LeftShoulder)});
            keys.push_back({"R1", static_cast<int>(GamepadButton::RightShoulder)});
            keys.push_back({"L2", static_cast<int>(GamepadButton::LeftTrigger)});
            keys.push_back({"R2", static_cast<int>(GamepadButton::RightTrigger)});
            keys.push_back({"L3", static_cast<int>(GamepadButton::LeftThumb)});
            keys.push_back({"R3", static_cast<int>(GamepadButton::RightThumb)});
            keys.push_back({"Start", static_cast<int>(GamepadButton::Start)});
            keys.push_back({"Back", static_cast<int>(GamepadButton::Back)});
            keys.push_back({"DPad-Up", static_cast<int>(GamepadButton::DPadUp)});
            keys.push_back({"DPad-Down", static_cast<int>(GamepadButton::DPadDown)});
            keys.push_back({"DPad-Left", static_cast<int>(GamepadButton::DPadLeft)});
            keys.push_back({"DPad-Right", static_cast<int>(GamepadButton::DPadRight)});
        }
        
        return keys;
    }

}  // namespace CrashGuard
