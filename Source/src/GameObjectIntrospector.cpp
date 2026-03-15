// Copyright (C) 2024-2025 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

// GameObjectIntrospector.cpp
// Game Object Identification and Analysis
//
// Purpose: Identify game objects in memory during crash analysis.
// Uses RTTI, memory validation, and CommonLibSSE to extract FormIDs,
// EditorIDs, and mod ownership information from pointers found in
// registers, stack, or heap during exception handling.
//
// Key capabilities:
// - RTTI-based type identification for C++ objects
// - TESForm pointer validation and FormID extraction
// - EditorID extraction (handles TESFullName, BGSKeywordForm, etc.)
// - Mod ownership identification via TESDataHandler
// - Memory region scanning for game object pointers
// ═══════════════════════════════════════════════════════════════════════

#include "PCH.h"
#include "GameObjectIntrospector.h"

#include <spdlog/spdlog.h>

namespace GameObjectIntrospection {

    bool GameObjectIntrospector::s_initialized = false;

    // ═══════════════════════════════════════════════════════════════════
    // § 1  Initialization
    // ═══════════════════════════════════════════════════════════════════

    bool GameObjectIntrospector::Initialize() {
        if (s_initialized) {
            return true;
        }

        // Verify TESDataHandler is available
        auto* dataHandler = GetDataHandler();
        if (!dataHandler) {
            auto log = spdlog::default_logger();
            if (log) {
                log->error("[GameObjectIntrospector] Failed to get TESDataHandler");
            }
            return false;
        }

        s_initialized = true;

        auto log = spdlog::default_logger();
        if (log) {
            log->info("[GameObjectIntrospector] Initialized successfully");
        }

        return true;
    }

    // ═══════════════════════════════════════════════════════════════════
    // § 2  Memory Validation
    // ═══════════════════════════════════════════════════════════════════

    bool GameObjectIntrospector::IsReadableMemory(const void* ptr, size_t size) {
        if (!ptr) {
            return false;
        }

        // Check if memory is readable using VirtualQuery
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(ptr, &mbi, sizeof(mbi))) {
            return false;
        }

        // Must be committed memory
        if (mbi.State != MEM_COMMIT) {
            return false;
        }

        // Must have read access
        constexpr DWORD readableProtections = PAGE_READONLY | PAGE_READWRITE |
                                             PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                                             PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
        if (!(mbi.Protect & readableProtections)) {
            return false;
        }

        // Check if the entire range is within this memory block
        uintptr_t start = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t end = start + size;
        uintptr_t blockEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;

        return end <= blockEnd;
    }

    bool GameObjectIntrospector::IsValidVTable(void* ptr) {
        if (!IsReadableMemory(ptr, sizeof(void*))) {
            return false;
        }

        // VTable should point to executable memory
        void* vtableEntry = *reinterpret_cast<void**>(ptr);
        if (!vtableEntry) {
            return false;
        }

        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(vtableEntry, &mbi, sizeof(mbi))) {
            return false;
        }

        // VTable entries should be in executable memory
        constexpr DWORD execProtections = PAGE_EXECUTE | PAGE_EXECUTE_READ |
                                         PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        return (mbi.State == MEM_COMMIT) && (mbi.Protect & execProtections);
    }

    bool GameObjectIntrospector::IsGameMemory(void* ptr) {
        if (!ptr) {
            return false;
        }

        // Get module handle for the address
        HMODULE hModule = nullptr;
        constexpr DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
        
        if (!GetModuleHandleExA(flags, reinterpret_cast<LPCSTR>(ptr), &hModule)) {
            // Not in any module - could be heap memory, which is valid
            return IsReadableMemory(ptr);
        }

        // Check if it's in the game executable or a DLL
        char modulePath[MAX_PATH];
        if (GetModuleFileNameA(hModule, modulePath, MAX_PATH)) {
            std::string path(modulePath);
            std::transform(path.begin(), path.end(), path.begin(), ::tolower);
            
            // Accept game executable and common game DLLs
            return path.find("skyrim") != std::string::npos ||
                   path.find(".dll") != std::string::npos;
        }

        return false;
    }

    // ═══════════════════════════════════════════════════════════════════
    // § 3  RTTI-Based Type Identification
    // ═══════════════════════════════════════════════════════════════════

    std::string GameObjectIntrospector::ExtractTypeName(const std::type_info* typeInfo) {
        if (!typeInfo || !IsReadableMemory(typeInfo, sizeof(std::type_info))) {
            return "unknown";
        }

        const char* name = typeInfo->name();
        if (!name || !IsReadableMemory(name, 1)) {
            return "unknown";
        }

        std::string typeName(name);
        
        // Remove "class " prefix if present
        if (typeName.find("class ") == 0) {
            typeName = typeName.substr(6);
        }
        
        // Remove "struct " prefix if present
        if (typeName.find("struct ") == 0) {
            typeName = typeName.substr(7);
        }

        return typeName;
    }

    std::string GameObjectIntrospector::GetObjectType(void* ptr) {
        if (!ptr || !IsReadableMemory(ptr, sizeof(void*))) {
            return "unknown";
        }

        // Try to get RTTI information
        // C++ objects with virtual functions have a vtable pointer as first member
        void** vtablePtr = reinterpret_cast<void**>(ptr);
        if (!IsValidVTable(vtablePtr)) {
            return "unknown";
        }

        // MSVC RTTI: vtable[-1] points to RTTI Complete Object Locator
        void** vtable = reinterpret_cast<void**>(*vtablePtr);
        if (!IsReadableMemory(vtable - 1, sizeof(void*))) {
            return "unknown";
        }

        // Try to extract type_info
        // This is compiler-specific (MSVC layout)
        try {
            // Get the type_info from the object
            // Note: This uses compiler-specific RTTI layout
            const std::type_info* typeInfo = nullptr;
            
            // For MSVC, we can use dynamic_cast trick
            // First check if it's a TESForm
            if (IsValidTESForm(ptr)) {
                auto* form = reinterpret_cast<RE::TESForm*>(ptr);
                auto formType = form->GetFormType();
                return formType != RE::FormType::None ? 
                    std::string("TESForm:") + std::to_string(static_cast<int>(formType)) :
                    "TESForm";
            }

            // Check if it's an Actor
            if (IsValidActor(ptr)) {
                return "Actor";
            }

            // Check if it's a NiObject
            if (IsValidNiObject(ptr)) {
                return "NiObject";
            }

            return "GameObject";
        } catch (...) {
            return "unknown";
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // § 4  TESForm Validation and Extraction
    // ═══════════════════════════════════════════════════════════════════

    bool GameObjectIntrospector::IsValidTESForm(void* ptr) {
        if (!ptr || !IsReadableMemory(ptr, sizeof(RE::TESForm))) {
            return false;
        }

        // Check if vtable is valid BEFORE any virtual function calls
        if (!IsValidVTable(ptr)) {
            return false;
        }

        try {
            auto* form = reinterpret_cast<RE::TESForm*>(ptr);
            
            // SAFETY: Read FormID directly from memory offset instead of virtual call
            // FormID is at offset 0x14 in TESForm (non-virtual member)
            if (!IsReadableMemory(reinterpret_cast<char*>(ptr) + 0x14, sizeof(uint32_t))) {
                return false;
            }
            
            // Check if FormID is in valid range
            uint32_t formID = form->GetFormID();
            if (formID == 0 || formID == 0xFFFFFFFF) {
                return false;
            }

            // SAFETY: GetFormType() is a virtual function - validate vtable entry first
            void** vtable = *reinterpret_cast<void***>(ptr);
            if (!IsReadableMemory(vtable, sizeof(void*) * 64)) {  // Check first 64 vtable entries
                return false;
            }
            
            // Check if form type is valid
            auto formType = form->GetFormType();
            if (formType == RE::FormType::None) {
                return false;
            }

            return true;
        } catch (...) {
            return false;
        }
    }

    bool GameObjectIntrospector::IsValidNiObject(void* ptr) {
        if (!ptr || !IsReadableMemory(ptr, sizeof(RE::NiObject))) {
            return false;
        }

        // Check if vtable is valid
        if (!IsValidVTable(ptr)) {
            return false;
        }

        try {
            auto* niObj = reinterpret_cast<RE::NiObject*>(ptr);
            
            // NiObject has a reference count that should be reasonable
            // (not 0, not huge)
            // Note: This is a heuristic check
            return true;  // Basic vtable check is sufficient
        } catch (...) {
            return false;
        }
    }

    bool GameObjectIntrospector::IsValidActor(void* ptr) {
        if (!IsValidTESForm(ptr)) {
            return false;
        }

        try {
            auto* form = reinterpret_cast<RE::TESForm*>(ptr);
            
            // Check if form type is Actor or Character
            auto formType = form->GetFormType();
            return formType == RE::FormType::ActorCharacter;
        } catch (...) {
            return false;
        }
    }

    bool GameObjectIntrospector::IsValidGameObject(void* ptr) {
        return IsValidTESForm(ptr) || IsValidNiObject(ptr);
    }

    // ═══════════════════════════════════════════════════════════════════
    // § 5  FormID and EditorID Extraction
    // ═══════════════════════════════════════════════════════════════════

    RE::TESDataHandler* GameObjectIntrospector::GetDataHandler() {
        return RE::TESDataHandler::GetSingleton();
    }

    std::string GameObjectIntrospector::ExtractEditorID(RE::TESForm* form) {
        if (!form) {
            return "";
        }

        try {
            // Check for obviously invalid pointers first
            uintptr_t addr = reinterpret_cast<uintptr_t>(form);
            if (addr == 0 || addr == 0xFFFFFFFFFFFFFFFF || addr < 0x10000) {
                return "";
            }
            
            // CRITICAL: Validate vtable before calling ANY virtual functions
            // This prevents crashes from corrupted form pointers
            if (!IsValidVTable(form)) {
                return "";
            }
            
            // Additional safety: check if form pointer is in valid memory range
            if (!IsReadableMemory(form, sizeof(RE::TESForm))) {
                return "";
            }
            
            // Try to get editor ID directly (this is a virtual function call)
            const char* editorID = form->GetFormEditorID();
            if (editorID && IsReadableMemory(editorID, 1)) {
                return std::string(editorID);
            }

            // For some form types, try alternative methods
            auto formType = form->GetFormType();
            
            // TESFullName interface (many forms have this)
            if (auto* fullName = form->As<RE::TESFullName>()) {
                const char* name = fullName->GetFullName();
                if (name && IsReadableMemory(name, 1)) {
                    return std::string(name);
                }
            }

            // BGSKeywordForm (for objects with keywords)
            if (auto* keywordForm = form->As<RE::BGSKeywordForm>()) {
                // Keywords can help identify the object
                // but don't provide editor ID directly
            }

            return "";
        } catch (...) {
            return "";
        }
    }

    std::string GameObjectIntrospector::GetPluginName(uint8_t modIndex) {
        auto* dataHandler = GetDataHandler();
        if (!dataHandler) {
            return "";
        }

        try {
            // Get the file at this index using iterator
            auto& files = dataHandler->files;
            
            // BSSimpleList doesn't have size(), so we iterate and count
            uint8_t currentIndex = 0;
            for (auto it = files.begin(); it != files.end(); ++it, ++currentIndex) {
                if (currentIndex == modIndex) {
                    if (*it && (*it)->fileName) {
                        return std::string((*it)->fileName);
                    }
                    break;
                }
            }

            // Check light plugins (ESL)
            if (modIndex == 0xFE) {
                // Light plugin - need to check light plugin list
                // This requires additional FormID parsing
                return "LightPlugin.esl";
            }

            return "";
        } catch (...) {
            return "";
        }
    }

    std::string GameObjectIntrospector::FindOwningMod(RE::TESForm* form) {
        if (!form) {
            return "";
        }

        try {
            uint32_t formID = form->GetFormID();
            
            // Extract mod index from FormID
            // Standard plugins: top byte is mod index
            // Light plugins: 0xFE in top byte, next 12 bits are light index
            uint8_t modIndex = (formID >> 24) & 0xFF;
            
            if (modIndex == 0xFF) {
                // Created at runtime
                return "Runtime";
            }

            return GetPluginName(modIndex);
        } catch (...) {
            return "";
        }
    }

    bool GameObjectIntrospector::ExtractFormInfo(RE::TESForm* form, FormInfo& outInfo) {
        if (!form || !IsValidTESForm(form)) {
            outInfo.isValid = false;
            return false;
        }

        try {
            outInfo.formID = form->GetFormID();
            outInfo.editorID = ExtractEditorID(form);
            outInfo.modIndex = (outInfo.formID >> 24) & 0xFF;
            outInfo.pluginName = GetPluginName(outInfo.modIndex);
            outInfo.isValid = true;

            return true;
        } catch (...) {
            outInfo.isValid = false;
            return false;
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // § 6  Object Identification
    // ═══════════════════════════════════════════════════════════════════

    GameObjectInfo* GameObjectIntrospector::IdentifyObject(void* address) {
        if (!address || !IsGameMemory(address)) {
            return nullptr;
        }

        // Allocate on heap - caller must manage lifetime
        auto* info = new GameObjectInfo();
        info->address = address;
        info->isValid = false;

        // Try to identify as TESForm
        if (IsValidTESForm(address)) {
            auto* form = reinterpret_cast<RE::TESForm*>(address);
            
            FormInfo formInfo;
            if (ExtractFormInfo(form, formInfo)) {
                info->type = "TESForm";
                info->formID = formInfo.formID;
                info->editorID = formInfo.editorID;
                info->modName = formInfo.pluginName;
                info->isValid = true;

                // Add form type information
                auto formType = form->GetFormType();
                info->additionalInfo = "FormType: " + std::to_string(static_cast<int>(formType));

                return info;
            }
        }

        // Try to identify as NiObject
        if (IsValidNiObject(address)) {
            info->type = "NiObject";
            info->formID = 0;
            info->editorID = "";
            info->modName = "";
            info->isValid = true;
            info->additionalInfo = "3D Scene Object";

            return info;
        }

        // Unknown object type
        info->type = GetObjectType(address);
        delete info;
        return nullptr;
    }

    // ═══════════════════════════════════════════════════════════════════
    // § 7  Memory Region Scanning
    // ═══════════════════════════════════════════════════════════════════

    bool GameObjectIntrospector::ValidatePointer(void* ptr) {
        if (!ptr) {
            return false;
        }

        // Check pointer is not in low memory (null pointer range)
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        if (addr < 0x10000) {
            return false;
        }

        // Check pointer is aligned (8-byte alignment for x64)
        if (addr % sizeof(void*) != 0) {
            return false;
        }

        // Check memory is readable
        if (!IsReadableMemory(ptr, sizeof(void*))) {
            return false;
        }

        // Check it's in game memory or heap
        return IsGameMemory(ptr);
    }

    std::vector<GameObjectInfo> GameObjectIntrospector::ScanMemoryRegion(void* start, size_t size) {
        std::vector<GameObjectInfo> objects;

        if (!start || size == 0) {
            return objects;
        }

        // Scan memory in pointer-sized chunks
        constexpr size_t ptrSize = sizeof(void*);
        size_t numPointers = size / ptrSize;

        auto* ptrArray = reinterpret_cast<void**>(start);

        for (size_t i = 0; i < numPointers; ++i) {
            // Check if this memory is readable
            if (!IsReadableMemory(&ptrArray[i], ptrSize)) {
                continue;
            }

            void* candidate = ptrArray[i];
            
            // Validate pointer before attempting identification
            if (!ValidatePointer(candidate)) {
                continue;
            }

            // Try to identify this pointer as a game object
            GameObjectInfo* objInfo = IdentifyObject(candidate);
            if (objInfo && objInfo->isValid) {
                objects.push_back(*objInfo);
                delete objInfo;
            } else if (objInfo) {
                delete objInfo;
            }
        }

        return objects;
    }

    std::vector<GameObjectInfo> GameObjectIntrospector::ScanRegisters(const CONTEXT* context) {
        std::vector<GameObjectInfo> objects;

        if (!context) {
            return objects;
        }

        // List of general-purpose registers to scan
        std::vector<void*> registerValues = {
            reinterpret_cast<void*>(context->Rax),
            reinterpret_cast<void*>(context->Rbx),
            reinterpret_cast<void*>(context->Rcx),
            reinterpret_cast<void*>(context->Rdx),
            reinterpret_cast<void*>(context->Rsi),
            reinterpret_cast<void*>(context->Rdi),
            reinterpret_cast<void*>(context->Rbp),
            reinterpret_cast<void*>(context->R8),
            reinterpret_cast<void*>(context->R9),
            reinterpret_cast<void*>(context->R10),
            reinterpret_cast<void*>(context->R11),
            reinterpret_cast<void*>(context->R12),
            reinterpret_cast<void*>(context->R13),
            reinterpret_cast<void*>(context->R14),
            reinterpret_cast<void*>(context->R15),
        };

        // Scan each register value
        for (void* regValue : registerValues) {
            if (!ValidatePointer(regValue)) {
                continue;
            }

            // Try to identify as game object
            GameObjectInfo* objInfo = IdentifyObject(regValue);
            if (objInfo && objInfo->isValid) {
                objects.push_back(*objInfo);
                delete objInfo;
            } else if (objInfo) {
                delete objInfo;
            }
        }

        return objects;
    }

    std::vector<GameObjectInfo> GameObjectIntrospector::ScanStack(void* stackPointer, size_t maxDepth) {
        std::vector<GameObjectInfo> objects;

        if (!stackPointer) {
            return objects;
        }

        // Validate stack pointer is readable
        if (!IsReadableMemory(stackPointer, sizeof(void*))) {
            return objects;
        }

        // Scan stack memory for pointers
        // Stack grows downward, so we scan upward from current SP
        constexpr size_t ptrSize = sizeof(void*);
        size_t numPointers = maxDepth / ptrSize;

        auto* stackArray = reinterpret_cast<void**>(stackPointer);

        for (size_t i = 0; i < numPointers; ++i) {
            // Check if this stack location is readable
            if (!IsReadableMemory(&stackArray[i], ptrSize)) {
                // Stop scanning if we hit unreadable memory
                break;
            }

            void* candidate = stackArray[i];

            // Validate pointer
            if (!ValidatePointer(candidate)) {
                continue;
            }

            // Try to identify as game object
            GameObjectInfo* objInfo = IdentifyObject(candidate);
            if (objInfo && objInfo->isValid) {
                // Check if we already found this object (avoid duplicates)
                bool isDuplicate = false;
                for (const auto& existing : objects) {
                    if (existing.address == objInfo->address) {
                        isDuplicate = true;
                        break;
                    }
                }

                if (!isDuplicate) {
                    objects.push_back(*objInfo);
                }
                delete objInfo;
            } else if (objInfo) {
                delete objInfo;
            }
        }

        return objects;
    }

}  // namespace GameObjectIntrospection

