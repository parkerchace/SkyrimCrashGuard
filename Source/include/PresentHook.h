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
