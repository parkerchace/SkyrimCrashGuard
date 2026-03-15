// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

// Forward declare DirectX types to avoid including headers here
struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct HWND__;
typedef HWND__* HWND;

namespace CrashGuard {
    
    class ImGuiRenderer {
    public:
        static ImGuiRenderer& GetInstance() {
            static ImGuiRenderer instance;
            return instance;
        }

        bool Initialize();
        void Shutdown();
        void NewFrame();
        void RenderDrawData();
        void EnsureInputReady();  // Ensures WndProc hook is installed before ImGui processes input
        void LogImGuiInputState() const;  // Diagnostic logging of ImGui input state
        
        bool IsInitialized() const { return m_initialized; }
        
        // Get HWND for hook installation
        HWND GetHWND() const { return m_hwnd; }
        
        // WndProc hook - using void* to avoid including Windows.h
        static long long __stdcall WndProcHandler(HWND hWnd, unsigned int msg, unsigned long long wParam, long long lParam);
        static void* s_originalWndProc;


    private:
        ImGuiRenderer() = default;
        ~ImGuiRenderer() = default;
        ImGuiRenderer(const ImGuiRenderer&) = delete;
        ImGuiRenderer& operator=(const ImGuiRenderer&) = delete;

        void UpdateGamepadInput();

        bool m_initialized = false;
        ID3D11Device* m_device = nullptr;
        ID3D11DeviceContext* m_context = nullptr;
        IDXGISwapChain* m_swapChain = nullptr;
        HWND m_hwnd = nullptr;
        bool m_cursorVisible = false;
        int m_originalCursorCount = 0;
        bool m_wasUsingGamepad = false;  // Track if gamepad was active before menu opened
    };

}
