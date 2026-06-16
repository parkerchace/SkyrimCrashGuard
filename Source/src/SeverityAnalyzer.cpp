// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "SeverityAnalyzer.h"
#include "RootCauseAnalyzer.h"
#include <algorithm>
#include <sstream>

namespace CrashGuard {

// Dangerous function patterns for call stack analysis
const std::vector<std::string> SeverityAnalyzer::s_saveFunctions = {
    "SaveGame", "LoadGame", "Save", "Load", "Serialize", "Deserialize",
    "WriteSave", "ReadSave", "SaveData", "LoadData"
};

const std::vector<std::string> SeverityAnalyzer::s_questFunctions = {
    "Quest", "QuestStage", "SetStage", "CompleteQuest", "StartQuest",
    "QuestObjective", "UpdateObjective", "QuestData"
};

const std::vector<std::string> SeverityAnalyzer::s_inventoryFunctions = {
    "Inventory", "Container", "AddItem", "RemoveItem", "EquipItem",
    "UnequipItem", "DropItem", "PickupItem", "ItemData"
};

const std::vector<std::string> SeverityAnalyzer::s_playerDataFunctions = {
    "PlayerCharacter", "ActorValue", "SetActorValue", "ModActorValue",
    "PlayerData", "CharacterData", "PlayerStats"
};

const std::vector<std::string> SeverityAnalyzer::s_renderingFunctions = {
    "Render", "Draw", "Display", "BSShader", "NiRenderer", "D3D11",
    "Present", "Mesh", "Texture", "Animation", "BSGraphics"
};

SeverityAnalysis SeverityAnalyzer::AnalyzeCrash(
    const VEH::CrashContext& context,
    const RootCauseAnalysis::RootCauseResult& rootCause) {
    
    SeverityAnalysis analysis;
    
    // Initialize with Unknown severity
    analysis.level = VEH::SeverityLevel::Unknown;
    analysis.confidenceScore = 0.0f;
    analysis.affectsSaveData = false;
    analysis.affectsQuestData = false;
    analysis.affectsInventory = false;
    analysis.isRecoverable = true;
    
    // Classify by multiple methods and take the highest severity
    VEH::SeverityLevel callStackSeverity = ClassifyByCallStack(context.callStack);
    VEH::SeverityLevel memoryRegionSeverity = ClassifyByMemoryRegion(context.crashAddress);
    VEH::SeverityLevel patternSeverity = ClassifyByPattern(context);
    
    // Take the highest severity level (Fatal > Critical > Warning > Safe)
    analysis.level = std::max({callStackSeverity, memoryRegionSeverity, patternSeverity});
    
    // Determine detection method based on which classification was most severe
    if (callStackSeverity == analysis.level) {
        analysis.detectionMethod = "Call stack analysis";
    } else if (memoryRegionSeverity == analysis.level) {
        analysis.detectionMethod = "Memory region analysis";
    } else if (patternSeverity == analysis.level) {
        analysis.detectionMethod = "Pattern matching";
    } else {
        analysis.detectionMethod = "Context analysis";
    }
    
    // Generate technical reason from root cause
    if (!rootCause.description.empty()) {
        analysis.technicalReason = rootCause.description;
    } else {
        analysis.technicalReason = "Unknown crash cause";
    }
    
    // Calculate confidence score (higher if multiple methods agree)
    int agreementCount = 0;
    if (callStackSeverity == analysis.level) agreementCount++;
    if (memoryRegionSeverity == analysis.level) agreementCount++;
    if (patternSeverity == analysis.level) agreementCount++;
    
    analysis.confidenceScore = std::min(1.0f, 0.3f + (agreementCount * 0.25f));
    
    // Set boolean flags based on severity and call stack
    for (const auto& frame : context.callStack) {
        for (const auto& saveFunc : s_saveFunctions) {
            if (frame.functionName.find(saveFunc) != std::string::npos) {
                analysis.affectsSaveData = true;
                break;
            }
        }
        for (const auto& questFunc : s_questFunctions) {
            if (frame.functionName.find(questFunc) != std::string::npos) {
                analysis.affectsQuestData = true;
                break;
            }
        }
        for (const auto& invFunc : s_inventoryFunctions) {
            if (frame.functionName.find(invFunc) != std::string::npos) {
                analysis.affectsInventory = true;
                break;
            }
        }
    }
    
    // Determine if recoverable based on severity
    analysis.isRecoverable = (analysis.level != VEH::SeverityLevel::Fatal);
    
    // Generate user-friendly explanation
    analysis.userExplanation = GenerateUserExplanation(analysis.level, analysis.technicalReason);
    
    // Generate recommendation (assuming recovery was attempted)
    analysis.recommendation = GenerateRecommendation(analysis.level, true);
    
    // Identify risks
    analysis.risks = IdentifyRisks(analysis.level, context);
    
    return analysis;
}

VEH::SeverityLevel SeverityAnalyzer::ClassifyByCallStack(
    const std::vector<VEH::StackFrame>& callStack) {
    
    // Scan call stack for dangerous function patterns
    for (const auto& frame : callStack) {
        const std::string& funcName = frame.functionName;
        
        // Check for save/load functions - Critical
        for (const auto& saveFunc : s_saveFunctions) {
            if (funcName.find(saveFunc) != std::string::npos) {
                return VEH::SeverityLevel::Critical;
            }
        }
        
        // Check for quest functions - Critical
        for (const auto& questFunc : s_questFunctions) {
            if (funcName.find(questFunc) != std::string::npos) {
                return VEH::SeverityLevel::Critical;
            }
        }
        
        // Check for inventory functions - Critical
        for (const auto& invFunc : s_inventoryFunctions) {
            if (funcName.find(invFunc) != std::string::npos) {
                return VEH::SeverityLevel::Critical;
            }
        }
        
        // Check for player data functions - Critical
        for (const auto& playerFunc : s_playerDataFunctions) {
            if (funcName.find(playerFunc) != std::string::npos) {
                return VEH::SeverityLevel::Critical;
            }
        }
        
        // Check for rendering functions - Safe
        for (const auto& renderFunc : s_renderingFunctions) {
            if (funcName.find(renderFunc) != std::string::npos) {
                return VEH::SeverityLevel::Safe;
            }
        }
    }
    
    // No dangerous functions found - default to Unknown
    return VEH::SeverityLevel::Unknown;
}

VEH::SeverityLevel SeverityAnalyzer::ClassifyByMemoryRegion(void* crashAddress) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(crashAddress);

    // Near-null: classic null pointer dereference
    if (addr < 0x10000) {
        return VEH::SeverityLevel::Warning;
    }

    // Query the memory region type via VirtualQuery.
    // This tells us whether the address is committed, reserved, free, etc.
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(crashAddress, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        if (mbi.State == MEM_FREE || mbi.State == MEM_RESERVE) {
            // Dereferencing an unmapped address — typical null/dangling pointer crash
            return VEH::SeverityLevel::Warning;
        }

        // Check if the crash is inside executable code (Type == MEM_IMAGE, PAGE_EXECUTE_*).
        // A crash *inside* code (e.g. jump to bad address) is harder to recover from.
        const DWORD execMask = PAGE_EXECUTE | PAGE_EXECUTE_READ |
                               PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if ((mbi.Protect & execMask) != 0) {
            return VEH::SeverityLevel::Fatal;
        }

        // Stack range check omitted: GetCurrentThreadStackLimits requires
        // _WIN32_WINNT >= 0x0602 which may not be defined in this SDK config.
        // Stack overflows are already caught by ClassifyByPattern via
        // EXCEPTION_STACK_OVERFLOW, and MEM_RESERVE above catches most
        // invalid stack accesses.
    }

    // Committed, non-executable, non-stack address — heap or mapped file.
    // Access violations here are Warning-level (dangling pointer, race, etc.)
    return VEH::SeverityLevel::Warning;
}

VEH::SeverityLevel SeverityAnalyzer::ClassifyByPattern(
    const VEH::CrashContext& context) {
    
    // Check exception code for known patterns
    switch (context.exceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:
            // Access violations could be Warning or Critical depending on context
            return VEH::SeverityLevel::Warning;
            
        case EXCEPTION_STACK_OVERFLOW:
            // Stack overflow is always Fatal
            return VEH::SeverityLevel::Fatal;
            
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            // Division by zero is typically Warning
            return VEH::SeverityLevel::Warning;
            
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            // Illegal instruction could be Fatal
            return VEH::SeverityLevel::Fatal;
            
        default:
            return VEH::SeverityLevel::Unknown;
    }
}

std::string SeverityAnalyzer::GenerateUserExplanation(
    VEH::SeverityLevel level,
    const std::string& technicalReason) {
    
    std::stringstream ss;
    
    switch (level) {
        case VEH::SeverityLevel::Safe:
            ss << "This crash is classified as Safe because it only affects visual elements. ";
            ss << "The crash occurred in rendering or UI code that doesn't affect your save data or game progress. ";
            ss << "It's safe to continue playing.";
            break;
            
        case VEH::SeverityLevel::Warning:
            ss << "This crash is classified as Warning because it involves missing resources or null pointers. ";
            ss << "While recoverable, it indicates a mod or game issue that should be addressed. ";
            ss << "Your save data is not at risk, but you may experience gameplay issues.";
            break;
            
        case VEH::SeverityLevel::Critical:
            ss << "This crash is classified as Critical because it affects save data or persistent game state. ";
            ss << "The crash occurred in code that handles saves, quests, inventory, or player data. ";
            ss << "Continuing may risk save file corruption or loss of progress. ";
            ss << "It's recommended to load your last save to avoid potential data loss.";
            break;
            
        case VEH::SeverityLevel::Fatal:
            ss << "This crash is classified as Fatal because it involves stack corruption or unrecoverable state. ";
            ss << "The game's memory has been severely damaged and cannot be safely recovered. ";
            ss << "Continuing is not recommended as it will likely cause more crashes or data corruption. ";
            ss << "Please load your last save or restart the game.";
            break;
            
        case VEH::SeverityLevel::Unknown:
        default:
            ss << "The severity of this crash could not be determined. ";
            ss << "CrashGuard will attempt recovery, but proceed with caution. ";
            ss << "Consider loading your last save if you experience further issues.";
            break;
    }
    
    return ss.str();
}

std::string SeverityAnalyzer::GenerateRecommendation(
    VEH::SeverityLevel level,
    bool recoverySuccessful) {
    
    if (!recoverySuccessful) {
        return "Recovery failed. Load your last save or restart the game.";
    }
    
    switch (level) {
        case VEH::SeverityLevel::Safe:
            return "Continue playing. This crash was safely recovered.";
            
        case VEH::SeverityLevel::Warning:
            return "Continue playing, but monitor for additional issues. Consider checking your mod load order.";
            
        case VEH::SeverityLevel::Critical:
            return "Load your last save to avoid potential data corruption. Do not save your game.";
            
        case VEH::SeverityLevel::Fatal:
            return "Load your last save immediately. Do not continue playing or save your game.";
            
        case VEH::SeverityLevel::Unknown:
        default:
            return "Proceed with caution. Consider loading your last save if you experience further issues.";
    }
}

std::vector<std::string> SeverityAnalyzer::IdentifyRisks(
    VEH::SeverityLevel level,
    const VEH::CrashContext& context) {
    
    std::vector<std::string> risks;
    
    switch (level) {
        case VEH::SeverityLevel::Safe:
            // No significant risks for Safe crashes
            break;
            
        case VEH::SeverityLevel::Warning:
            risks.push_back("Potential gameplay issues");
            risks.push_back("Mod conflicts");
            break;
            
        case VEH::SeverityLevel::Critical:
            risks.push_back("Save file corruption");
            risks.push_back("Quest progress loss");
            risks.push_back("Inventory data corruption");
            risks.push_back("Player data corruption");
            break;
            
        case VEH::SeverityLevel::Fatal:
            risks.push_back("Severe save file corruption");
            risks.push_back("Complete loss of game progress");
            risks.push_back("Game instability");
            risks.push_back("Cascading failures");
            break;
            
        case VEH::SeverityLevel::Unknown:
        default:
            risks.push_back("Unknown risks");
            risks.push_back("Potential data loss");
            break;
    }
    
    return risks;
}

}  // namespace CrashGuard
