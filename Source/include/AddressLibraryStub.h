// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

namespace CrashGuard {

/**
 * @brief Stub address library system
 * 
 * Creates a minimal in-memory address library database that satisfies
 * CommonLibSSE-NG's requirements without requiring external files.
 * 
 * This allows the plugin to work independently while maintaining
 * compatibility with the CommonLibSSE-NG framework.
 */
class AddressLibraryStub {
public:
    /**
     * @brief Initialize the stub address library
     * 
     * This MUST be called before any CommonLibSSE-NG code runs,
     * ideally in a static initializer or DllMain.
     * 
     * @return true if successful, false otherwise
     */
    static bool Initialize();

    /**
     * @brief Check if the stub is initialized
     */
    static bool IsInitialized();

private:
    static bool _initialized;
};

/**
 * @brief Static initializer to set up stub before CommonLibSSE-NG loads
 */
struct AddressLibraryStubInitializer {
    AddressLibraryStubInitializer() {
        AddressLibraryStub::Initialize();
    }
};

// Global static initializer - runs before main() and before CommonLibSSE-NG
static AddressLibraryStubInitializer g_addressLibraryStubInit;

} // namespace CrashGuard
