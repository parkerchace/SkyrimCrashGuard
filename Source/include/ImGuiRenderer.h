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
