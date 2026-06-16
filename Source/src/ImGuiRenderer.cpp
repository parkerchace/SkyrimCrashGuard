// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "PCH.h"
#include "ImGuiRenderer.h"
#include "ImGuiConfigMenu.h"
#include "InputDiagnostics.h"
#include "InputBlocker.h"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <spdlog/spdlog.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dxgi.h>

// Forward declare ImGui Win32 handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace CrashGuard {

    // Static members
    void* ImGuiRenderer::s_originalWndProc = nullptr;

    // WndProc hook
    long long __stdcall ImGuiRenderer::WndProcHandler(HWND hWnd, unsigned int msg, unsigned long long wParam, long long lParam) {
        auto& menu = ImGuiConfigMenu::GetSingleton();
        bool crashGuardMenuVisible = menu.IsVisible();
        
        // CRITICAL: Block mouse wheel when looking at containers (for QuickLootIE compatibility)
        bool cameraZoomBlocked = InputBlocker::GetSingleton().IsCameraZoomBlocked();
        if (cameraZoomBlocked && msg == WM_MOUSEWHEEL) {
            // Completely consume the mouse wheel message
            // QuickLootIE handles mouse wheel through Skyrim's DirectInput, not WndProc
            // So blocking WndProc prevents camera zoom while allowing QuickLootIE to scroll
            return 0;
        }
        
        // CRITICAL: Only let ImGui process messages when CrashGuard menu is visible
        // This prevents ImGui from consuming mouse wheel when menu is closed
        if (crashGuardMenuVisible) {
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        }
        
        // If CrashGuard menu is visible, block keyboard input from reaching Skyrim
        if (crashGuardMenuVisible) {
            // Only block keyboard input from reaching Skyrim
            if (msg >= WM_KEYFIRST && msg <= WM_KEYLAST) {
                return true;  // Consume keyboard messages
            }
            // Let mouse messages through - we're polling directly anyway
        }

        // Pass to original WndProc (normal game operation)
        if (s_originalWndProc) {
            return CallWindowProcA((WNDPROC)s_originalWndProc, hWnd, msg, wParam, lParam);
        }
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    bool ImGuiRenderer::Initialize() {
        if (m_initialized) {
            return true;
        }

        spdlog::info("ImGuiRenderer: Starting initialization");

        // Get D3D11 device and context from BSRenderManager (replaces BSGraphics::Renderer in v3.6.0+)
        auto render_manager = RE::BSRenderManager::GetSingleton();
        if (!render_manager) {
            spdlog::error("ImGuiRenderer: Failed to get BSRenderManager");
            return false;
        }

        auto& render_data = render_manager->GetRuntimeData();
        m_device = render_data.forwarder;
        m_swapChain = render_data.swapChain;

        if (!m_device || !m_swapChain) {
            spdlog::error("ImGuiRenderer: Failed to get D3D11 device or swap chain");
            return false;
        }

        m_device->GetImmediateContext(&m_context);
        if (!m_context) {
            spdlog::error("ImGuiRenderer: Failed to get D3D11 context");
            return false;
        }

        // Get window handle
        DXGI_SWAP_CHAIN_DESC desc;
        m_swapChain->GetDesc(&desc);
        m_hwnd = desc.OutputWindow;

        if (!m_hwnd) {
            spdlog::error("ImGuiRenderer: Failed to get window handle");
            return false;
        }

        spdlog::info("ImGuiRenderer: Got D3D11 device, context, swap chain, and HWND");

        // Create ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        
        // Set style
        ImGui::StyleColorsDark();

        // Initialize ImGui Win32 backend - this handles WndProc internally
        if (!ImGui_ImplWin32_Init(m_hwnd)) {
            spdlog::error("ImGuiRenderer: Failed to initialize ImGui Win32 backend");
            ImGui::DestroyContext();
            return false;
        }

        // Initialize ImGui DX11 backend
        if (!ImGui_ImplDX11_Init(m_device, m_context)) {
            spdlog::error("ImGuiRenderer: Failed to initialize ImGui DX11 backend");
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            return false;
        }

        spdlog::info("ImGuiRenderer: ImGui backends initialized");

        // Install WndProc hook AFTER ImGui_ImplWin32_Init
        s_originalWndProc = (void*)SetWindowLongPtrA(m_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcHandler);
        if (!s_originalWndProc) {
            spdlog::error("ImGuiRenderer: Failed to install WndProc hook");
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            return false;
        }

        spdlog::info("ImGuiRenderer: WndProc hook installed successfully");

        m_initialized = true;
        spdlog::info("ImGuiRenderer: Initialization complete");

        return true;
    }

    void ImGuiRenderer::Shutdown() {
        if (!m_initialized) {
            return;
        }

        // Restore original WndProc
        if (s_originalWndProc && m_hwnd) {
            SetWindowLongPtrA(m_hwnd, GWLP_WNDPROC, (LONG_PTR)s_originalWndProc);
            s_originalWndProc = nullptr;
        }

        // Shutdown ImGui
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (m_context) {
            m_context->Release();
            m_context = nullptr;
        }

        m_initialized = false;
        spdlog::info("ImGuiRenderer: Shutdown complete");
    }

    void ImGuiRenderer::NewFrame() {
        if (!m_initialized) {
            return;
        }

        // Check if menu is visible
        auto& menu = ImGuiConfigMenu::GetSingleton();
        bool menuVisible = menu.IsVisible();

        // Start new ImGui frame
        ImGui_ImplDX11_NewFrame();
        
        // CRITICAL FIX: Manually update mouse state instead of relying on WndProc
        // This bypasses Skyrim's input mode switching and ControlMap interference
        ImGuiIO& io = ImGui::GetIO();
        
        // CRITICAL: Force input capture flags based on menu visibility
        // This ensures ImGui releases input when menu closes
        io.WantCaptureMouse = menuVisible;
        io.WantCaptureKeyboard = menuVisible;
        io.WantTextInput = menuVisible;
        
        if (menuVisible) {
            // Get cursor position in client coordinates
            POINT pt;
            if (GetCursorPos(&pt) && ScreenToClient(m_hwnd, &pt)) {
                // Mouse is within window
                io.MousePos = ImVec2((float)pt.x, (float)pt.y);
            } else {
                // Mouse is outside window
                io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
            }
            
            // Directly poll mouse button states (bypasses WndProc entirely)
            io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;  // Left button
            io.MouseDown[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;  // Right button
            io.MouseDown[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;  // Middle button
            
            // Set mouse cursor visibility
            io.MouseDrawCursor = true;
        } else {
            // Menu closed - clear all input states to prevent any lingering state
            io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
            io.MouseDown[0] = false;
            io.MouseDown[1] = false;
            io.MouseDown[2] = false;
            io.MouseDrawCursor = false;
            
            // Clear any keyboard state
            for (int i = 0; i < IM_ARRAYSIZE(io.KeysDown); i++) {
                io.KeysDown[i] = false;
            }
        }
        
        // Call Win32 NewFrame AFTER we've set mouse state
        ImGui_ImplWin32_NewFrame();
        
        ImGui::NewFrame();
    }

    void ImGuiRenderer::RenderDrawData() {
        if (!m_initialized) {
            return;
        }

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void ImGuiRenderer::EnsureInputReady() {
        // This function is no longer needed since we handle WndProc directly
        // Kept for API compatibility
    }

    void ImGuiRenderer::LogImGuiInputState() const {
        if (!m_initialized) {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        spdlog::debug("ImGui Input State:");
        spdlog::debug("  WantCaptureMouse: {}", io.WantCaptureMouse);
        spdlog::debug("  WantCaptureKeyboard: {}", io.WantCaptureKeyboard);
        spdlog::debug("  MouseDrawCursor: {}", io.MouseDrawCursor);
        spdlog::debug("  MousePos: ({}, {})", io.MousePos.x, io.MousePos.y);
        spdlog::debug("  MouseDown[0]: {}", io.MouseDown[0]);
        spdlog::debug("  MouseDown[1]: {}", io.MouseDown[1]);
    }

}
