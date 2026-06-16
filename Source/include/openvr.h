// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

// Stub OpenVR header for compilation
// This allows the project to compile without full OpenVR SDK
// VR functionality is not used in this plugin

#pragma once

#include <cstdint>

// Minimal OpenVR stubs to satisfy CommonLibSSE includes
namespace vr {
    // Basic VR types that CommonLibSSE expects
    typedef uint32_t TrackedDeviceIndex_t;
    typedef uint64_t VROverlayHandle_t;
    
    enum EVREye {
        Eye_Left = 0,
        Eye_Right = 1
    };
    
    // Interface stubs
    class IVRSystem {};
    class IVRCompositor {};
    class IVROverlay {};
    class IVRRenderModels {};
    class IVRSettings {};
}
