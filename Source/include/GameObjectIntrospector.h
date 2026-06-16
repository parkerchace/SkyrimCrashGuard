// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <typeinfo>

// Forward declarations for CommonLibSSE types
namespace RE {
    class TESForm;
    class TESDataHandler;
}

/// Game Object Introspection System
/// Identifies and extracts information from game objects in memory
/// during crash analysis to determine root causes and involved mods.
namespace GameObjectIntrospection {

    /// Form information extracted from TESForm objects
    struct FormInfo {
        uint32_t formID;
        std::string editorID;
        uint8_t modIndex;
        std::string pluginName;
        bool isValid;
    };

    /// Complete game object information
    struct GameObjectInfo {
        void* address;
        std::string type;
        uint32_t formID;
        std::string editorID;
        std::string modName;
        bool isValid;
        std::string additionalInfo;  // Type-specific details
    };

    /// Main introspection class for identifying game objects
    class GameObjectIntrospector {
    public:
        /// Initialize the introspector (sets up RTTI and data handler access)
        static bool Initialize();

        /// Check if the introspector is initialized
        static bool IsInitialized() { return s_initialized; }

        /// Identify game object at memory address
        /// Returns nullptr if address doesn't point to a valid game object
        static GameObjectInfo* IdentifyObject(void* address);

        /// Extract FormID and EditorID from a TESForm pointer
        /// Returns false if form is invalid or extraction fails
        static bool ExtractFormInfo(RE::TESForm* form, FormInfo& outInfo);

        /// Find which mod owns this object
        /// Returns empty string if mod cannot be determined
        static std::string FindOwningMod(RE::TESForm* form);

        /// Validate pointer points to valid game object
        /// Performs memory and RTTI checks
        static bool IsValidGameObject(void* ptr);

        /// Scan memory region for game objects
        /// Used to find objects in registers, stack, or heap
        static std::vector<GameObjectInfo> ScanMemoryRegion(void* start, size_t size);

        /// Scan CPU registers for valid game object pointers
        /// Takes a CONTEXT structure from exception handling
        static std::vector<GameObjectInfo> ScanRegisters(const CONTEXT* context);

        /// Scan stack memory for game object pointers
        /// Scans from current stack pointer up to maxDepth bytes
        static std::vector<GameObjectInfo> ScanStack(void* stackPointer, size_t maxDepth = 8192);

        /// Validate that a pointer points to valid memory
        /// More thorough than IsReadableMemory - checks alignment and range
        static bool ValidatePointer(void* ptr);

        /// Check if pointer is a valid TESForm
        static bool IsValidTESForm(void* ptr);

        /// Check if pointer is a valid NiObject (3D scene graph)
        static bool IsValidNiObject(void* ptr);

        /// Check if pointer is a valid Actor
        static bool IsValidActor(void* ptr);

        /// Get object type name using RTTI
        /// Returns "unknown" if RTTI is not available
        static std::string GetObjectType(void* ptr);

    private:
        /// Validate memory is readable
        static bool IsReadableMemory(const void* ptr, size_t size = sizeof(void*));

        /// Check if pointer looks like a valid vtable pointer
        static bool IsValidVTable(void* ptr);

        /// Extract type name from RTTI type_info
        static std::string ExtractTypeName(const std::type_info* typeInfo);

        /// Get TESDataHandler for mod lookups
        static RE::TESDataHandler* GetDataHandler();

        /// Extract EditorID from form (handles different form types)
        static std::string ExtractEditorID(RE::TESForm* form);

        /// Get plugin name from mod index
        static std::string GetPluginName(uint8_t modIndex);

        /// Check if address is in valid game memory range
        static bool IsGameMemory(void* ptr);

        static bool s_initialized;
    };

}  // namespace GameObjectIntrospection

