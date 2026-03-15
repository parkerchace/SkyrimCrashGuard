// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

// ═══════════════════════════════════════════════════════════════════════
// RootCauseAnalyzer.cpp — Root Cause Analysis for Crash Recovery
// ═══════════════════════════════════════════════════════════════════════
//
// Analyzes crash context to determine the underlying cause:
// - Exception code analysis
// - Crash address analysis
// - Call stack signature matching
// - Crash category classification
// - Grid boundary crash detection
// - Confidence scoring
// - Mod identification
// - Suggested fix generation
// ═══════════════════════════════════════════════════════════════════════

#include "RootCauseAnalyzer.h"
#include "GameObjectIntrospector.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <unordered_map>
#include <cmath>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace RootCauseAnalysis {

// ═══════════════════════════════════════════════════════════════════════
// § 1  Call Stack Signature Patterns
// ═══════════════════════════════════════════════════════════════════════

struct SignaturePattern {
    const char* pattern;
    CrashCategory category;
    const char* description;
};

static const SignaturePattern s_signatures[] = {
    // Mesh loading signatures
    { "BSResourceNiBinaryStream", CrashCategory::Mesh, "Mesh file loading" },
    { "NiStream::Load", CrashCategory::Mesh, "NIF stream loading" },
    { "NiAVObject::Load", CrashCategory::Mesh, "Mesh object loading" },
    { "BSGeometry::Load", CrashCategory::Mesh, "Geometry loading" },
    { "NiSkinInstance", CrashCategory::Mesh, "Mesh skinning" },
    
    // Animation signatures
    { "hkbCharacter", CrashCategory::Animation, "Animation character system" },
    { "hkbBehaviorGraph", CrashCategory::Animation, "Animation behavior graph" },
    { "BSAnimationGraphManager", CrashCategory::Animation, "Animation graph manager" },
    { "AnimationFileManager", CrashCategory::Animation, "Animation file loading" },
    { "PlayAnimation", CrashCategory::Animation, "Animation playback" },
    
    // Script signatures
    { "BSScript::Internal", CrashCategory::Script, "Papyrus script execution" },
    { "VirtualMachine", CrashCategory::Script, "Script VM" },
    { "IFunction::Call", CrashCategory::Script, "Script function call" },
    { "ObjectBindPolicy", CrashCategory::Script, "Script object binding" },
    
    // AI signatures
    { "AIProcess", CrashCategory::AI, "AI processing" },
    { "PathingRequest", CrashCategory::AI, "AI pathfinding" },
    { "TESPackage", CrashCategory::AI, "AI package execution" },
    { "MovementController", CrashCategory::AI, "AI movement" },
    
    // Cell loading signatures
    { "TESObjectCELL::Load", CrashCategory::Cell, "Cell loading" },
    { "TESObjectREFR::Load3D", CrashCategory::Cell, "Reference 3D loading" },
    { "GridCellArray", CrashCategory::Cell, "Grid cell management" },
    { "LoadedAreaBound", CrashCategory::Cell, "Cell boundary management" },
    
    // Memory signatures
    { "malloc", CrashCategory::Memory, "Memory allocation" },
    { "ScrapHeap", CrashCategory::Memory, "Game heap allocation" },
    { "MemoryManager", CrashCategory::Memory, "Memory management" },
};

// ═══════════════════════════════════════════════════════════════════════
// § 2  Exception Code Analysis
// ═══════════════════════════════════════════════════════════════════════

static bool IsNullPointerException(DWORD exceptionCode, void* crashAddress) {
    if (exceptionCode != EXCEPTION_ACCESS_VIOLATION) {
        return false;
    }
    
    // Null or near-null address (< 64KB)
    uintptr_t addr = reinterpret_cast<uintptr_t>(crashAddress);
    return addr < 0x10000;
}

static bool IsStackOverflowException(DWORD exceptionCode) {
    return exceptionCode == EXCEPTION_STACK_OVERFLOW;
}

static bool IsIllegalInstructionException(DWORD exceptionCode) {
    return exceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION;
}

// ═══════════════════════════════════════════════════════════════════════
// § 3  Private Helper Implementations
// ═══════════════════════════════════════════════════════════════════════

bool RootCauseAnalyzer::IsNullPointerCrash(const VEH::CrashContext& context) {
    return IsNullPointerException(context.exceptionCode, context.crashAddress);
}

bool RootCauseAnalyzer::IsMeshCrash(const VEH::CrashContext& context) {
    // Check call stack for mesh-related signatures
    for (const auto& frame : context.callStack) {
        for (const auto& sig : s_signatures) {
            if (sig.category == CrashCategory::Mesh) {
                if (frame.functionName.find(sig.pattern) != std::string::npos ||
                    frame.moduleName.find(sig.pattern) != std::string::npos) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool RootCauseAnalyzer::IsAnimationCrash(const VEH::CrashContext& context) {
    for (const auto& frame : context.callStack) {
        for (const auto& sig : s_signatures) {
            if (sig.category == CrashCategory::Animation) {
                if (frame.functionName.find(sig.pattern) != std::string::npos ||
                    frame.moduleName.find(sig.pattern) != std::string::npos) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool RootCauseAnalyzer::IsScriptCrash(const VEH::CrashContext& context) {
    for (const auto& frame : context.callStack) {
        for (const auto& sig : s_signatures) {
            if (sig.category == CrashCategory::Script) {
                if (frame.functionName.find(sig.pattern) != std::string::npos ||
                    frame.moduleName.find(sig.pattern) != std::string::npos) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool RootCauseAnalyzer::IsAICrash(const VEH::CrashContext& context) {
    for (const auto& frame : context.callStack) {
        for (const auto& sig : s_signatures) {
            if (sig.category == CrashCategory::AI) {
                if (frame.functionName.find(sig.pattern) != std::string::npos ||
                    frame.moduleName.find(sig.pattern) != std::string::npos) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool RootCauseAnalyzer::IsCellCrash(const VEH::CrashContext& context) {
    for (const auto& frame : context.callStack) {
        for (const auto& sig : s_signatures) {
            if (sig.category == CrashCategory::Cell) {
                if (frame.functionName.find(sig.pattern) != std::string::npos ||
                    frame.moduleName.find(sig.pattern) != std::string::npos) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool RootCauseAnalyzer::IsMemoryCrash(const VEH::CrashContext& context) {
    // Check for stack overflow
    if (IsStackOverflowException(context.exceptionCode)) {
        return true;
    }
    
    // Check call stack for memory-related signatures
    for (const auto& frame : context.callStack) {
        for (const auto& sig : s_signatures) {
            if (sig.category == CrashCategory::Memory) {
                if (frame.functionName.find(sig.pattern) != std::string::npos ||
                    frame.moduleName.find(sig.pattern) != std::string::npos) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool RootCauseAnalyzer::IsGridBoundaryCrash(const VEH::CrashContext& context) {
    // Grid boundary crashes typically involve AI and cell loading
    bool hasAI = IsAICrash(context);
    bool hasCell = IsCellCrash(context);
    
    // If both AI and cell signatures present, likely grid boundary
    if (hasAI && hasCell) {
        return true;
    }
    
    // Check for grid-specific signatures
    for (const auto& frame : context.callStack) {
        if (frame.functionName.find("GridCellArray") != std::string::npos ||
            frame.functionName.find("LoadedAreaBound") != std::string::npos ||
            frame.functionName.find("PathingRequest") != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

std::string RootCauseAnalyzer::AnalyzeCallStackSignature(
    const std::vector<VEH::StackFrame>& callStack) {
    
    std::string signature;
    
    // Build signature from top 3 frames
    size_t frameCount = std::min(callStack.size(), size_t(3));
    for (size_t i = 0; i < frameCount; ++i) {
        if (!callStack[i].functionName.empty()) {
            if (!signature.empty()) {
                signature += " -> ";
            }
            signature += callStack[i].functionName;
        }
    }
    
    return signature;
}

bool RootCauseAnalyzer::IsNearCellBoundary(void* actorPtr, float& outDistance) {
    // Cell boundaries are at multiples of 4096 units
    constexpr float CELL_SIZE = 4096.0f;
    constexpr float BOUNDARY_THRESHOLD = 512.0f;
    
    if (!actorPtr) {
        outDistance = 0.0f;
        return false;
    }
    
    // Use SEH to safely access actor data
    __try {
        auto* actor = static_cast<RE::Actor*>(actorPtr);
        if (!actor) {
            outDistance = 0.0f;
            return false;
        }
        
        auto pos = actor->GetPosition();
        
        // Calculate distance to nearest cell boundary on X and Y axes
        float posModX = std::fmod(std::abs(pos.x), CELL_SIZE);
        float posModY = std::fmod(std::abs(pos.y), CELL_SIZE);
        
        float distX = std::min(posModX, CELL_SIZE - posModX);
        float distY = std::min(posModY, CELL_SIZE - posModY);
        
        outDistance = std::min(distX, distY);
        return outDistance < BOUNDARY_THRESHOLD;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        outDistance = 0.0f;
        return false;
    }
}

// C2712 workaround: SEH wrapper for safe actor data access
static const char* GetAIPackageInfo_SEH(void* actorPtr) {
    __try {
        auto* actor = static_cast<RE::Actor*>(actorPtr);
        if (!actor) {
            return "Invalid actor";
        }
        
        auto* currentPackage = actor->GetCurrentPackage();
        if (!currentPackage) {
            return "No active package";
        }
        
        const char* name = currentPackage->GetName();
        if (name && name[0]) {
            return name;  // Return raw pointer - caller will copy if needed
        }
        
        return "Unnamed package";
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return "Error reading package";
    }
}

std::string RootCauseAnalyzer::GetAIPackageInfo(void* actorPtr) {
    if (!actorPtr) {
        return "No actor";
    }
    
    const char* result = GetAIPackageInfo_SEH(actorPtr);
    return std::string(result ? result : "Unknown");
}

// C2712 workaround: SEH wrapper for cell check
struct CellCheckResult { bool isUnloaded; const char* info; RE::FormID formID; };

static CellCheckResult IsTargetInUnloadedCell_SEH(void* actorPtr) {
    CellCheckResult result{ false, nullptr, 0 };
    
    __try {
        auto* actor = static_cast<RE::Actor*>(actorPtr);
        if (!actor) {
            return result;
        }
        
        auto* parentCell = actor->GetParentCell();
        if (!parentCell) {
            result.isUnloaded = true;
            result.info = "Actor has no parent cell";
            return result;
        }
        
        // Check if cell is attached (loaded)
        if (!parentCell->IsAttached()) {
            result.isUnloaded = true;
            result.info = parentCell->GetName();
            result.formID = parentCell->GetFormID();
            return result;
        }
        
        return result;  // Cell is loaded and attached
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result.info = "Error checking cell state";
        return result;
    }
}

bool RootCauseAnalyzer::IsTargetInUnloadedCell(void* actorPtr, std::string& outCellInfo) {
    if (!actorPtr) {
        return false;
    }
    
    CellCheckResult result = IsTargetInUnloadedCell_SEH(actorPtr);
    
    if (result.isUnloaded) {
        if (result.info && result.info[0]) {
            outCellInfo = result.info;
        } else if (result.formID != 0) {
            outCellInfo = fmt::format("Cell {:#x} (unloaded)", result.formID);
        } else {
            outCellInfo = "Unknown cell";
        }
        return true;
    }
    
    return false;
}

// ═══════════════════════════════════════════════════════════════════════
// § 4  Public API Implementation
// ═══════════════════════════════════════════════════════════════════════

CrashCategory RootCauseAnalyzer::ClassifyCrash(const VEH::CrashContext& context) {
    // Check in priority order
    
    // Grid boundary crashes are specific and should be checked first
    if (IsGridBoundaryCrash(context)) {
        return CrashCategory::GridBoundary;
    }
    
    // Check for specific crash types
    if (IsMeshCrash(context)) {
        return CrashCategory::Mesh;
    }
    
    if (IsAnimationCrash(context)) {
        return CrashCategory::Animation;
    }
    
    if (IsScriptCrash(context)) {
        return CrashCategory::Script;
    }
    
    if (IsAICrash(context)) {
        return CrashCategory::AI;
    }
    
    if (IsCellCrash(context)) {
        return CrashCategory::Cell;
    }
    
    if (IsMemoryCrash(context)) {
        return CrashCategory::Memory;
    }
    
    // Default to Unknown
    return CrashCategory::Unknown;
}

// Inner function that does the actual register/stack scanning.
// Separated from SEH wrapper because __try cannot coexist with
// C++ objects needing unwinding (MSVC C2712).
static void ScanObjectsInner(const CONTEXT* ctx,
                             std::vector<GameObjectIntrospection::GameObjectInfo>& outObjects)
{
    auto regObjects = GameObjectIntrospection::GameObjectIntrospector::ScanRegisters(ctx);
    outObjects.insert(outObjects.end(), regObjects.begin(), regObjects.end());

    if (ctx->Rsp != 0) {
        auto stackObjects = GameObjectIntrospection::GameObjectIntrospector::ScanStack(
            reinterpret_cast<void*>(ctx->Rsp), 512);
        outObjects.insert(outObjects.end(), stackObjects.begin(), stackObjects.end());
    }
}

// SEH-safe wrapper: catches any access violation from pointer probing.
// outObjects is populated by ScanObjectsInner; if it crashes, we keep
// whatever was written before the fault.
static bool ScanObjectsSEH(const CONTEXT* ctx,
                           std::vector<GameObjectIntrospection::GameObjectInfo>* outObjects)
{
    __try {
        ScanObjectsInner(ctx, *outObjects);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

std::vector<GameObjectIntrospection::GameObjectInfo> 
RootCauseAnalyzer::IdentifyInvolvedObjects(const VEH::CrashContext& context) {
    
    // RE-ENABLED: This function is now ONLY called from a worker thread
    // (post-recovery), NOT from inside the VEH handler.  On a worker thread:
    //  - Secondary AVs in ScanRegisters/ScanStack are caught by SEH wrappers
    //  - They do NOT trigger recursive VEH handler entry
    //  - The game is already running normally after recovery
    //
    // The function identifies which TESForm objects were in CPU registers
    // or on the stack at crash time.  This is diagnostic — it tells us
    // which mod/actor/item caused the crash.
    
    std::vector<GameObjectIntrospection::GameObjectInfo> objects;

    // Guard: if introspector didn't initialize, bail out
    if (!GameObjectIntrospection::GameObjectIntrospector::IsInitialized()) {
        return objects;
    }

    bool ok = ScanObjectsSEH(&context.cpuContext, &objects);
    if (!ok) {
        auto log = spdlog::default_logger();
        if (log) log->warn("[RootCause] IdentifyInvolvedObjects: SEH caught exception during scan");
    }

    if (!objects.empty()) {
        auto log = spdlog::default_logger();
        if (log) log->info("[RootCause] IdentifyInvolvedObjects: found {} game objects", objects.size());
    }

    return objects;
}

GridBoundaryInfo RootCauseAnalyzer::DetectGridBoundaryCrash(
    const VEH::CrashContext& context) {
    
    GridBoundaryInfo info{};
    info.isGridBoundaryCrash = false;
    info.actorPtr = nullptr;
    info.distanceToNearestBoundary = 0.0f;
    info.targetCellLoaded = true;
    
    // First check if this looks like a grid boundary crash
    if (!IsGridBoundaryCrash(context)) {
        return info;
    }
    
    // Try to find an Actor in the crash context
    auto objects = IdentifyInvolvedObjects(context);
    
    for (const auto& obj : objects) {
        // Check if this is an Actor type
        if (obj.type.find("Actor") != std::string::npos) {
            info.actorPtr = obj.address;
            info.actorEditorID = obj.editorID;
            info.actorModName = obj.modName;
            
            // Check if actor is near cell boundary
            float distance = 0.0f;
            if (IsNearCellBoundary(obj.address, distance)) {
                info.distanceToNearestBoundary = distance;
                
                // Get AI package information
                info.aiPackageInfo = GetAIPackageInfo(obj.address);
                
                // Check if AI target is in unloaded cell
                std::string cellInfo;
                if (IsTargetInUnloadedCell(obj.address, cellInfo)) {
                    info.targetCellInfo = cellInfo;
                    info.targetCellLoaded = false;
                    info.isGridBoundaryCrash = true;
                }
            }
            
            break;  // Found an actor, stop searching
        }
    }
    
    return info;
}

std::vector<std::string> RootCauseAnalyzer::RankSuspectedMods(
    const std::vector<GameObjectIntrospection::GameObjectInfo>& objects,
    const VEH::CrashContext& context) {
    
    // Count mod occurrences
    std::unordered_map<std::string, int> modCounts;
    
    for (const auto& obj : objects) {
        if (!obj.modName.empty() && obj.modName != "Skyrim.esm") {
            modCounts[obj.modName]++;
        }
    }
    
    // Convert to vector and sort by count
    std::vector<std::pair<std::string, int>> modPairs(modCounts.begin(), modCounts.end());
    std::sort(modPairs.begin(), modPairs.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Extract just the mod names
    std::vector<std::string> rankedMods;
    for (const auto& pair : modPairs) {
        rankedMods.push_back(pair.first);
    }
    
    return rankedMods;
}

std::vector<std::string> RootCauseAnalyzer::GenerateSuggestedFixes(
    const RootCauseResult& result) {
    
    std::vector<std::string> fixes;
    
    switch (result.category) {
    case CrashCategory::Mesh:
        fixes.push_back("Validate mesh files in NifSkope for corruption");
        fixes.push_back("Reinstall or update the mod containing the mesh");
        fixes.push_back("Check for missing mesh dependencies");
        if (!result.involvedMods.empty()) {
            fixes.push_back("Report issue to " + result.involvedMods[0] + " mod author");
        }
        break;
        
    case CrashCategory::Animation:
        fixes.push_back("Verify animation files are compatible with your skeleton");
        fixes.push_back("Check for missing animation dependencies (FNIS/Nemesis)");
        fixes.push_back("Reinstall animation mod");
        if (!result.involvedMods.empty()) {
            fixes.push_back("Check " + result.involvedMods[0] + " for known animation issues");
        }
        break;
        
    case CrashCategory::Script:
        fixes.push_back("Check Papyrus logs for script errors");
        fixes.push_back("Verify script dependencies are installed");
        fixes.push_back("Clean save file with script cleaner");
        if (!result.involvedMods.empty()) {
            fixes.push_back("Update or reinstall " + result.involvedMods[0]);
        }
        break;
        
    case CrashCategory::AI:
        fixes.push_back("Check for AI package conflicts");
        fixes.push_back("Verify navmesh is not corrupted");
        fixes.push_back("Reduce AI processing load (fewer NPCs)");
        break;
        
    case CrashCategory::Cell:
        fixes.push_back("Verify cell edits are not conflicting");
        fixes.push_back("Check for missing cell dependencies");
        fixes.push_back("Rebuild cell precombines if using mods that edit cells");
        break;
        
    case CrashCategory::Memory:
        fixes.push_back("Increase system memory allocation");
        fixes.push_back("Reduce texture resolution");
        fixes.push_back("Disable memory-intensive mods");
        fixes.push_back("Use memory management tools (SSE Engine Fixes)");
        break;
        
    case CrashCategory::GridBoundary:
        if (result.gridBoundaryInfo.isGridBoundaryCrash) {
            fixes.push_back("Actor near cell boundary: " + result.gridBoundaryInfo.actorEditorID);
            fixes.push_back("AI package targeting unloaded cell: " + 
                          result.gridBoundaryInfo.targetCellInfo);
            fixes.push_back("Adjust AI package radius to stay within loaded cells");
            fixes.push_back("Add cell boundary markers to prevent AI crossing");
            if (!result.gridBoundaryInfo.actorModName.empty()) {
                fixes.push_back("Report grid boundary issue to " + 
                              result.gridBoundaryInfo.actorModName + " mod author");
            }
        }
        break;
        
    case CrashCategory::Unknown:
        fixes.push_back("Enable detailed logging for more information");
        fixes.push_back("Check for mod conflicts using xEdit");
        fixes.push_back("Verify game files integrity");
        fixes.push_back("Update SKSE and all SKSE plugins");
        break;
    }
    
    return fixes;
}

float RootCauseAnalyzer::CalculateConfidence(const RootCauseResult& result) {
    float confidence = 0.0f;
    
    // Base confidence on category
    if (result.category != CrashCategory::Unknown) {
        confidence += 0.3f;
    }
    
    // Increase confidence if we have involved mods
    if (!result.involvedMods.empty()) {
        confidence += 0.3f;
    }
    
    // Increase confidence if we have a detailed description
    if (!result.description.empty() && result.description != "Unknown") {
        confidence += 0.2f;
    }
    
    // Grid boundary crashes have high confidence if detected
    if (result.category == CrashCategory::GridBoundary && 
        result.gridBoundaryInfo.isGridBoundaryCrash) {
        confidence += 0.2f;
    }
    
    // Cap at 1.0
    return std::min(confidence, 1.0f);
}

RootCauseResult RootCauseAnalyzer::AnalyzeCrash(const VEH::CrashContext& context) {
    RootCauseResult result{};
    
    // Classify the crash
    result.category = ClassifyCrash(context);
    
    // Identify involved objects
    auto objects = IdentifyInvolvedObjects(context);
    
    // Rank suspected mods
    result.involvedMods = RankSuspectedMods(objects, context);
    
    // Detect grid boundary crashes
    result.gridBoundaryInfo = DetectGridBoundaryCrash(context);
    
    // Build description
    std::string categoryStr = CrashCategoryToString(result.category);
    result.description = categoryStr + " crash";
    
    // Add exception code info
    if (IsNullPointerException(context.exceptionCode, context.crashAddress)) {
        result.description += " (null pointer dereference)";
    } else if (IsStackOverflowException(context.exceptionCode)) {
        result.description += " (stack overflow)";
    } else if (IsIllegalInstructionException(context.exceptionCode)) {
        result.description += " (illegal instruction)";
    }
    
    // Add call stack signature
    std::string signature = AnalyzeCallStackSignature(context.callStack);
    if (!signature.empty()) {
        result.description += " in " + signature;
    }
    
    // Add involved object info
    if (!objects.empty() && !objects[0].editorID.empty()) {
        result.description += " involving " + objects[0].editorID;
        if (!objects[0].modName.empty()) {
            result.description += " from " + objects[0].modName;
        }
    }
    
    // Generate suggested fixes
    result.suggestedFixes = GenerateSuggestedFixes(result);
    
    // Calculate confidence
    result.confidence = CalculateConfidence(result);
    
    // Log the analysis
    auto log = spdlog::default_logger();
    if (log) {
        log->info("[RootCause] Category: {}, Confidence: {:.2f}", 
                 categoryStr, result.confidence);
        if (!result.involvedMods.empty()) {
            log->info("[RootCause] Suspected mods: {}", result.involvedMods[0]);
        }
        if (result.gridBoundaryInfo.isGridBoundaryCrash) {
            log->info("[RootCause] Grid boundary crash detected: {} at {:.1f} units from boundary",
                     result.gridBoundaryInfo.actorEditorID,
                     result.gridBoundaryInfo.distanceToNearestBoundary);
        }
    }
    
    return result;
}

const char* CrashCategoryToString(CrashCategory category) {
    switch (category) {
    case CrashCategory::Mesh:         return "Mesh";
    case CrashCategory::Animation:    return "Animation";
    case CrashCategory::Script:       return "Script";
    case CrashCategory::AI:           return "AI";
    case CrashCategory::Cell:         return "Cell";
    case CrashCategory::Memory:       return "Memory";
    case CrashCategory::GridBoundary: return "GridBoundary";
    case CrashCategory::Unknown:      return "Unknown";
    default:                          return "Invalid";
    }
}

}  // namespace RootCauseAnalysis
