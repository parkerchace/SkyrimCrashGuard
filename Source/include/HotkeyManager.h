// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "PCH.h"
#include <vector>
#include <string>
#include <chrono>
#include <functional>

namespace CrashGuard {

    // ========================================================================
    // Input Device Types
    // ========================================================================
    
    enum class InputDevice {
        Keyboard,
        Gamepad
    };

    // ========================================================================
    // Key/Button Definitions
    // ========================================================================
    
    // Virtual key codes for keyboard (Windows VK_* codes)
    enum class KeyboardKey : int {
        None = 0,
        F1 = VK_F1,
        F2 = VK_F2,
        F3 = VK_F3,
        F4 = VK_F4,
        F5 = VK_F5,
        F6 = VK_F6,
        F7 = VK_F7,
        F8 = VK_F8,
        F9 = VK_F9,
        F10 = VK_F10,
        F11 = VK_F11,
        F12 = VK_F12,
        Escape = VK_ESCAPE,
        Tab = VK_TAB,
        Shift = VK_SHIFT,
        Ctrl = VK_CONTROL,
        Alt = VK_MENU,
        Space = VK_SPACE,
        Enter = VK_RETURN,
        Backspace = VK_BACK,
        Delete = VK_DELETE,
        Home = VK_HOME,
        End = VK_END,
        PageUp = VK_PRIOR,
        PageDown = VK_NEXT,
        Insert = VK_INSERT,
        // Add more as needed
    };

    // Gamepad buttons (Xbox controller layout)
    enum class GamepadButton : int {
        None = 0,
        A = 0x1000,           // XINPUT_GAMEPAD_A
        B = 0x2000,           // XINPUT_GAMEPAD_B
        X = 0x4000,           // XINPUT_GAMEPAD_X
        Y = 0x8000,           // XINPUT_GAMEPAD_Y
        DPadUp = 0x0001,      // XINPUT_GAMEPAD_DPAD_UP
        DPadDown = 0x0002,    // XINPUT_GAMEPAD_DPAD_DOWN
        DPadLeft = 0x0004,    // XINPUT_GAMEPAD_DPAD_LEFT
        DPadRight = 0x0008,   // XINPUT_GAMEPAD_DPAD_RIGHT
        Start = 0x0010,       // XINPUT_GAMEPAD_START
        Back = 0x0020,        // XINPUT_GAMEPAD_BACK
        LeftThumb = 0x0040,   // XINPUT_GAMEPAD_LEFT_THUMB
        RightThumb = 0x0080,  // XINPUT_GAMEPAD_RIGHT_THUMB
        LeftShoulder = 0x0100,   // XINPUT_GAMEPAD_LEFT_SHOULDER (L1/LB)
        RightShoulder = 0x0200,  // XINPUT_GAMEPAD_RIGHT_SHOULDER (R1/RB)
        LeftTrigger = 0x10000,   // Custom: Left trigger (L2/LT)
        RightTrigger = 0x20000,  // Custom: Right trigger (R2/RT)
    };

    // ========================================================================
    // Hotkey Binding Structure
    // ========================================================================
    
    struct HotkeyBinding {
        InputDevice device;
        std::vector<int> keys;  // Multiple keys for combinations (e.g., Ctrl+Shift+F11)
        float holdDuration;     // Seconds to hold (0 = instant press)
        std::string name;       // User-friendly name
        
        HotkeyBinding() 
            : device(InputDevice::Keyboard)
            , holdDuration(0.0f) 
        {}
        
        // Check if this binding matches current input state
        bool IsPressed(const std::vector<int>& pressedKeys, float heldTime) const;
        
        // Get human-readable string representation
        std::string ToString() const;
        
        // Parse from string (e.g., "Keyboard:Ctrl+Shift+F11:0.0")
        static HotkeyBinding FromString(const std::string& str);
    };

    // ========================================================================
    // Hotkey Manager
    // ========================================================================
    
    class HotkeyManager {
    public:
        static HotkeyManager& GetSingleton() {
            static HotkeyManager instance;
            return instance;
        }
        
        // Initialize hotkey system
        bool Initialize();
        
        // Shutdown hotkey system
        void Shutdown();
        
        // Update hotkey state (call every frame)
        void Update();
        
        // Register a hotkey action
        void RegisterHotkey(const std::string& actionName, 
                          const HotkeyBinding& binding,
                          std::function<void()> callback);
        
        // Unregister a hotkey action
        void UnregisterHotkey(const std::string& actionName);
        
        // Get current binding for an action
        HotkeyBinding GetBinding(const std::string& actionName) const;
        
        // Set binding for an action
        void SetBinding(const std::string& actionName, const HotkeyBinding& binding);
        
        // Check if a specific key/button is currently pressed
        bool IsKeyPressed(int key) const;
        
        // Check if a specific key/button was just pressed this frame
        bool IsKeyJustPressed(int key) const;
        
        // Get all registered action names
        std::vector<std::string> GetActionNames() const;
        
        // Get current hold time for a specific device
        float GetCurrentHoldTime(InputDevice device) const;
        
        // Load bindings from config
        void LoadBindings();
        
        // Save bindings to config
        void SaveBindings();
        
        // Get default bindings
        static HotkeyBinding GetDefaultKeyboardBinding();
        static HotkeyBinding GetDefaultGamepadBinding();
        
    private:
        HotkeyManager() = default;
        ~HotkeyManager() = default;
        HotkeyManager(const HotkeyManager&) = delete;
        HotkeyManager& operator=(const HotkeyManager&) = delete;
        
        // Input state tracking
        struct InputState {
            std::vector<int> pressedKeys;
            std::vector<int> justPressedKeys;
            std::chrono::steady_clock::time_point pressStartTime;
            float currentHoldTime;
        };
        
        // Hotkey action
        struct HotkeyAction {
            HotkeyBinding binding;
            std::function<void()> callback;
            bool wasTriggered;
            std::chrono::steady_clock::time_point lastTriggerTime;
        };
        
        // Update keyboard input state
        void UpdateKeyboardState();
        
        // Update gamepad input state
        void UpdateGamepadState();
        
        // Check if binding is triggered
        bool CheckBinding(const HotkeyBinding& binding, const InputState& state);
        
        // Get gamepad state (XInput)
        bool GetGamepadState(int userIndex, void* state);
        
        InputState m_keyboardState;
        InputState m_gamepadState;
        std::unordered_map<std::string, HotkeyAction> m_actions;
        bool m_initialized = false;
    };

    // ========================================================================
    // Helper Functions
    // ========================================================================
    
    // Convert key code to string
    std::string KeyToString(int key, InputDevice device);
    
    // Convert string to key code
    int StringToKey(const std::string& str, InputDevice device);
    
    // Get all available keys for a device
    std::vector<std::pair<std::string, int>> GetAvailableKeys(InputDevice device);

}  // namespace CrashGuard
