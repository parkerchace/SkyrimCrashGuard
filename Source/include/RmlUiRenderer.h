// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <RmlUi/Core.h>
#include <d3d11.h>
#include <dxgi.h>
#include <memory>

namespace CrashGuard {

    // DirectX 11 render interface for RmlUi
    class RmlUiRenderInterface : public Rml::RenderInterface {
    public:
        RmlUiRenderInterface(ID3D11Device* device, ID3D11DeviceContext* context);
        ~RmlUiRenderInterface() override;

        // RmlUi RenderInterface implementation
        void RenderGeometry(Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices,
                          Rml::TextureHandle texture, const Rml::Vector2f& translation) override;
        
        Rml::CompiledGeometryHandle CompileGeometry(Rml::Vertex* vertices, int num_vertices,
                                                   int* indices, int num_indices,
                                                   Rml::TextureHandle texture) override;
        
        void RenderCompiledGeometry(Rml::CompiledGeometryHandle geometry,
                                   const Rml::Vector2f& translation) override;
        
        void ReleaseCompiledGeometry(Rml::CompiledGeometryHandle geometry) override;
        
        void EnableScissorRegion(bool enable) override;
        void SetScissorRegion(int x, int y, int width, int height) override;
        
        bool LoadTexture(Rml::TextureHandle& texture_handle, Rml::Vector2i& texture_dimensions,
                        const Rml::String& source) override;
        
        bool GenerateTexture(Rml::TextureHandle& texture_handle, const Rml::byte* source,
                           const Rml::Vector2i& source_dimensions) override;
        
        void ReleaseTexture(Rml::TextureHandle texture) override;
        
        void SetTransform(const Rml::Matrix4f* transform) override;

        // Setup for rendering
        void BeginFrame();
        void EndFrame();

    private:
        struct CompiledGeometry;
        struct Texture;

        ID3D11Device* m_device;
        ID3D11DeviceContext* m_context;
        
        // Rendering resources
        ID3D11VertexShader* m_vertexShader;
        ID3D11PixelShader* m_pixelShader;
        ID3D11InputLayout* m_inputLayout;
        ID3D11Buffer* m_vertexBuffer;
        ID3D11Buffer* m_indexBuffer;
        ID3D11Buffer* m_constantBuffer;
        ID3D11BlendState* m_blendState;
        ID3D11RasterizerState* m_rasterizerState;
        ID3D11SamplerState* m_samplerState;
        
        UINT m_vertexBufferSize;
        UINT m_indexBufferSize;
        
        bool m_scissorEnabled;
        D3D11_RECT m_scissorRect;
        
        Rml::Matrix4f m_transform;
        Rml::Vector2f m_translation;
        
        bool CreateShaders();
        bool CreateBuffers();
        bool CreateStates();
        void UpdateBuffers(Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices);
    };

    // System interface for RmlUi (file loading, timing, etc.)
    class RmlUiSystemInterface : public Rml::SystemInterface {
    public:
        double GetElapsedTime() override;
        bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
    };

    // Main RmlUi renderer class
    class RmlUiRenderer {
    public:
        static RmlUiRenderer& GetSingleton() {
            static RmlUiRenderer instance;
            return instance;
        }

        bool Initialize();
        void Shutdown();
        void NewFrame();
        void Render();
        
        bool IsInitialized() const { return m_initialized; }
        Rml::Context* GetContext() const { return m_context; }
        HWND GetHWND() const { return m_hwnd; }
        
        // WndProc hook for input
        static LRESULT CALLBACK WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static WNDPROC s_originalWndProc;

    private:
        RmlUiRenderer() = default;
        ~RmlUiRenderer() = default;
        RmlUiRenderer(const RmlUiRenderer&) = delete;
        RmlUiRenderer& operator=(const RmlUiRenderer&) = delete;

        bool m_initialized = false;
        ID3D11Device* m_device = nullptr;
        ID3D11DeviceContext* m_context_dx = nullptr;
        IDXGISwapChain* m_swapChain = nullptr;
        HWND m_hwnd = nullptr;
        
        std::unique_ptr<RmlUiRenderInterface> m_renderInterface;
        std::unique_ptr<RmlUiSystemInterface> m_systemInterface;
        Rml::Context* m_context = nullptr;
        
        std::chrono::high_resolution_clock::time_point m_startTime;
    };

}
