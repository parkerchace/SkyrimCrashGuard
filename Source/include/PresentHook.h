// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

// Forward declare DirectX types to avoid including headers here
struct IDXGISwapChain;

namespace CrashGuard {

    class PresentHook {
    public:
        static bool Install();
        static void Uninstall();

    private:
        static int __stdcall Present_Hook(IDXGISwapChain* swapChain, unsigned int syncInterval, unsigned int flags);
        
        using Present_t = int(__stdcall*)(IDXGISwapChain*, unsigned int, unsigned int);
        static Present_t s_originalPresent;
    };

}
