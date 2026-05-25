// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "DynamicFixApplicator.h"
#include "MeshValidator.h"
#include "ScriptMonitor.h"
#include "CellManager.h"
#include <spdlog/spdlog.h>
#include <Zydis/Zydis.h>

namespace DynamicFix {

    // Convert recovery strategy to string
    const char* RecoveryStrategyToString(RecoveryStrategy strategy) {
        switch (strategy) {
            case RecoveryStrategy::MeshRepair: return "MeshRepair";
            case RecoveryStrategy::MeshFallback: return "MeshFallback";
            case RecoveryStrategy::AnimationRetry: return "AnimationRetry";
            case RecoveryStrategy::AnimationFallback: return "AnimationFallback";
            case RecoveryStrategy::ScriptSkip: return "ScriptSkip";
            case RecoveryStrategy::ScriptTerminate: return "ScriptTerminate";
            case RecoveryStrategy::CellReload: return "CellReload";
            case RecoveryStrategy::CellTeleport: return "CellTeleport";
            case RecoveryStrategy::MemoryFree: return "MemoryFree";
            case RecoveryStrategy::InstructionPatch: return "InstructionPatch";
            case RecoveryStrategy::StateRollback: return "StateRollback";
            case RecoveryStrategy::NullPointerFix: return "NullPointerFix";
            case RecoveryStrategy::MissingResourceFix: return "MissingResourceFix";
            case RecoveryStrategy::Unknown: return "Unknown";
            default: return "Invalid";
        }
    }

    // Apply fix based on root cause analysis
    RecoveryResult DynamicFixApplicator::ApplyFix(
        VEH::CrashContext& context,
        const RootCauseAnalysis::RootCauseResult& rootCause)
    {
        RecoveryResult result;
        result.success = false;
        result.strategyUsed = RecoveryStrategy::Unknown;

        spdlog::info("DynamicFixApplicator: Applying fix for crash category: {}",
                    RootCauseAnalysis::CrashCategoryToString(rootCause.category));

        // Select strategy based on root cause category
        switch (rootCause.category) {
            case RootCauseAnalysis::CrashCategory::Mesh:
                result.strategyUsed = RecoveryStrategy::MeshFallback;
                result.success = FixMissingResource(context);
                if (result.success) {
                    result.actionsPerformed.push_back("Applied mesh fallback");
                }
                break;

            case RootCauseAnalysis::CrashCategory::Animation:
                result.strategyUsed = RecoveryStrategy::AnimationRetry;
                result.success = FixAnimationError(context);
                if (result.success) {
                    result.actionsPerformed.push_back("Retried animation with defaults");
                } else {
                    // Fallback to animation reset
                    result.strategyUsed = RecoveryStrategy::AnimationFallback;
                    result.success = RetryWithDefaults(context);
                    if (result.success) {
                        result.actionsPerformed.push_back("Reset to default animation");
                    }
                }
                break;

            case RootCauseAnalysis::CrashCategory::Script:
                result.strategyUsed = RecoveryStrategy::ScriptSkip;
                result.success = FixScriptError(context);
                if (result.success) {
                    result.actionsPerformed.push_back("Skipped problematic script statement");
                }
                break;

            case RootCauseAnalysis::CrashCategory::Memory:
            case RootCauseAnalysis::CrashCategory::Unknown:
                // Try null pointer fix first
                if (context.exceptionCode == EXCEPTION_ACCESS_VIOLATION) {
                    result.strategyUsed = RecoveryStrategy::NullPointerFix;
                    result.success = FixNullPointer(context);
                    if (result.success) {
                        result.actionsPerformed.push_back("Fixed null pointer dereference");
                    } else {
                        // Try instruction patching as fallback
                        result.strategyUsed = RecoveryStrategy::InstructionPatch;
                        result.success = PatchInstruction(context);
                        if (result.success) {
                            result.actionsPerformed.push_back("Patched instruction at crash site");
                        }
                    }
                }
                break;

            case RootCauseAnalysis::CrashCategory::Cell:
                // Check for interior cell lighting crashes first
                if (rootCause.interiorCellLightingInfo.isInteriorCellLightingCrash) {
                    spdlog::info("DynamicFixApplicator: Detected interior cell lighting crash");
                    
                    // For shadow/lighting crashes, use instruction patching to skip the problematic lighting update
                    if (rootCause.interiorCellLightingInfo.isShadowRelated ||
                        rootCause.interiorCellLightingInfo.isParticleLightingRelated) {
                        result.strategyUsed = RecoveryStrategy::InstructionPatch;
                        result.success = PatchInstruction(context);
                        if (result.success) {
                            result.actionsPerformed.push_back(
                                "Patched interior cell lighting crash: " +
                                rootCause.interiorCellLightingInfo.lightingSystemType);
                            result.actionsPerformed.push_back(
                                "Applied recovery: " +
                                rootCause.interiorCellLightingInfo.suggestedRecoveryAction);
                        }
                    }
                } else {
                    // Standard cell loading recovery
                    result.strategyUsed = RecoveryStrategy::CellReload;
                    result.success = FixMissingResource(context);
                    if (result.success) {
                        result.actionsPerformed.push_back("Reloaded cell with validation");
                    }
                }
                break;

            case RootCauseAnalysis::CrashCategory::AI:
            case RootCauseAnalysis::CrashCategory::GridBoundary:
                // For AI/grid boundary crashes, try instruction patching
                result.strategyUsed = RecoveryStrategy::InstructionPatch;
                result.success = PatchInstruction(context);
                if (result.success) {
                    result.actionsPerformed.push_back("Patched AI/grid boundary crash");
                }
                break;
        }

        if (!result.success) {
            result.failureReason = "No applicable fix strategy succeeded";
            spdlog::warn("DynamicFixApplicator: Failed to apply fix for category: {}",
                        RootCauseAnalysis::CrashCategoryToString(rootCause.category));
        } else {
            spdlog::info("DynamicFixApplicator: Successfully applied fix using strategy: {}",
                        RecoveryStrategyToString(result.strategyUsed));
        }

        return result;
    }

    // Fix null pointer dereference
    bool DynamicFixApplicator::FixNullPointer(VEH::CrashContext& context) {
        spdlog::debug("DynamicFixApplicator: Attempting null pointer fix");

        // Check if this is actually a null pointer dereference
        if (context.exceptionCode != EXCEPTION_ACCESS_VIOLATION) {
            return false;
        }

        // Try to allocate a safe default
        if (AllocateSafeDefault(context)) {
            spdlog::info("DynamicFixApplicator: Allocated safe default for null pointer");
            return true;
        }

        // If allocation fails, try instruction patching
        return PatchInstruction(context);
    }

    // Fix missing resource
    bool DynamicFixApplicator::FixMissingResource(VEH::CrashContext& context) {
        spdlog::debug("DynamicFixApplicator: Attempting missing resource fix");

        // Try to load fallback resource
        if (LoadFallbackResource(context)) {
            spdlog::info("DynamicFixApplicator: Loaded fallback resource");
            return true;
        }

        return false;
    }

    // Fix script error
    bool DynamicFixApplicator::FixScriptError(VEH::CrashContext& context) {
        spdlog::debug("DynamicFixApplicator: Attempting script error fix");

        // Try to skip the problematic script statement
        if (SkipScriptStatement(context)) {
            spdlog::info("DynamicFixApplicator: Skipped problematic script statement");
            return true;
        }

        return false;
    }

    // Fix animation error
    bool DynamicFixApplicator::FixAnimationError(VEH::CrashContext& context) {
        spdlog::debug("DynamicFixApplicator: Attempting animation error fix");

        // Try to retry with default parameters
        if (RetryWithDefaults(context)) {
            spdlog::info("DynamicFixApplicator: Retried animation with defaults");
            return true;
        }

        return false;
    }

    // Allocate safe default value for null pointer
    bool DynamicFixApplicator::AllocateSafeDefault(VEH::CrashContext& context) {
        // For null pointer dereferences, we can try to set the target register to zero
        // This is a simple fix that works for many cases
        
        // Determine which register was being dereferenced
        // For now, we'll use instruction patching instead
        return false;  // Defer to instruction patching
    }

    // Load fallback resource
    bool DynamicFixApplicator::LoadFallbackResource(VEH::CrashContext& context) {
        // This would require context about what resource was being loaded
        // For now, we'll rely on the proactive validation layers (L1)
        // to handle resource loading with fallbacks
        return false;
    }

    // Skip problematic script statement
    bool DynamicFixApplicator::SkipScriptStatement(VEH::CrashContext& context) {
        // To skip a script statement, we need to advance the instruction pointer
        // past the current instruction
        
        // Analyze the instruction to determine its length
        ZydisDecodedInstruction instruction;
        
        if (!AnalyzeInstruction(context.crashAddress, &instruction)) {
            return false;
        }

        // Calculate next instruction address
        void* nextInstruction = static_cast<char*>(context.crashAddress) + instruction.length;
        
        // Update instruction pointer
        return UpdateInstructionPointer(context, nextInstruction);
    }

    // Retry animation with default parameters
    bool DynamicFixApplicator::RetryWithDefaults(VEH::CrashContext& context) {
        // This would require identifying the actor and resetting to default idle
        // For now, we'll skip the current instruction
        return SkipScriptStatement(context);
    }

    // Patch instruction at crash site
    bool DynamicFixApplicator::PatchInstruction(VEH::CrashContext& context) {
        spdlog::debug("DynamicFixApplicator: Attempting instruction patching at address: {:p}",
                     context.crashAddress);

        // Analyze the instruction at the crash site
        ZydisDecodedInstruction instruction;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
        
        if (!AnalyzeInstruction(context.crashAddress, &instruction)) {
            spdlog::warn("DynamicFixApplicator: Failed to analyze instruction");
            return false;
        }

        bool patchSuccess = false;

        // Determine patch strategy based on instruction type
        switch (instruction.mnemonic) {
            case ZYDIS_MNEMONIC_MOV:
            case ZYDIS_MNEMONIC_MOVZX:
            case ZYDIS_MNEMONIC_MOVSX:
                // Check if this is a memory read or write
                if (instruction.operand_count >= 2) {
                    // Decode operands to check types
                    ZydisDecoder decoder;
                    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
                    ZydisDecoderDecodeFull(&decoder, context.crashAddress, 15,
                                          &instruction, operands);

                    // MOV from memory (null pointer read)
                    if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
                        spdlog::info("DynamicFixApplicator: Detected null pointer read (MOV)");
                        // Patch to NOP and zero the destination register
                        patchSuccess = PatchToNOP(context.crashAddress, instruction.length);
                        
                        if (patchSuccess && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
                            // Zero out the destination register in the context
                            ZydisRegister reg = operands[0].reg.value;
                            switch (reg) {
                                case ZYDIS_REGISTER_RAX: context.cpuContext.Rax = 0; break;
                                case ZYDIS_REGISTER_RBX: context.cpuContext.Rbx = 0; break;
                                case ZYDIS_REGISTER_RCX: context.cpuContext.Rcx = 0; break;
                                case ZYDIS_REGISTER_RDX: context.cpuContext.Rdx = 0; break;
                                case ZYDIS_REGISTER_RSI: context.cpuContext.Rsi = 0; break;
                                case ZYDIS_REGISTER_RDI: context.cpuContext.Rdi = 0; break;
                                case ZYDIS_REGISTER_R8:  context.cpuContext.R8 = 0; break;
                                case ZYDIS_REGISTER_R9:  context.cpuContext.R9 = 0; break;
                                case ZYDIS_REGISTER_R10: context.cpuContext.R10 = 0; break;
                                case ZYDIS_REGISTER_R11: context.cpuContext.R11 = 0; break;
                                case ZYDIS_REGISTER_R12: context.cpuContext.R12 = 0; break;
                                case ZYDIS_REGISTER_R13: context.cpuContext.R13 = 0; break;
                                case ZYDIS_REGISTER_R14: context.cpuContext.R14 = 0; break;
                                case ZYDIS_REGISTER_R15: context.cpuContext.R15 = 0; break;
                                default: break;
                            }
                            spdlog::debug("DynamicFixApplicator: Zeroed destination register");
                        }
                    }
                    // MOV to memory (null pointer write)
                    else if (operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
                        spdlog::info("DynamicFixApplicator: Detected null pointer write (MOV)");
                        // Patch to NOP (skip the write)
                        patchSuccess = PatchToNOP(context.crashAddress, instruction.length);
                    }
                }
                break;

            case ZYDIS_MNEMONIC_CALL:
                spdlog::info("DynamicFixApplicator: Detected invalid call");
                // Patch to return 0
                patchSuccess = PatchToReturn(context.crashAddress, 0);
                break;

            case ZYDIS_MNEMONIC_JMP:
            case ZYDIS_MNEMONIC_JZ:
            case ZYDIS_MNEMONIC_JNZ:
                spdlog::info("DynamicFixApplicator: Detected invalid jump");
                // Patch to NOP and continue to next instruction
                patchSuccess = PatchToNOP(context.crashAddress, instruction.length);
                break;

            case ZYDIS_MNEMONIC_LEA:
                // Load Effective Address - if loading from null, zero the register
                if (instruction.operand_count >= 2) {
                    ZydisDecoder decoder;
                    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
                    ZydisDecoderDecodeFull(&decoder, context.crashAddress, 15,
                                          &instruction, operands);
                    
                    if (operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
                        spdlog::info("DynamicFixApplicator: Detected LEA from invalid memory");
                        patchSuccess = PatchToNOP(context.crashAddress, instruction.length);
                    }
                }
                break;

            case ZYDIS_MNEMONIC_PUSH:
            case ZYDIS_MNEMONIC_POP:
                // Stack operations - skip if invalid
                spdlog::info("DynamicFixApplicator: Detected invalid stack operation");
                patchSuccess = PatchToNOP(context.crashAddress, instruction.length);
                break;

            default:
                // For other instructions, try to skip them
                spdlog::debug("DynamicFixApplicator: Unknown instruction type, attempting to skip");
                patchSuccess = PatchToNOP(context.crashAddress, instruction.length);
                break;
        }

        if (patchSuccess) {
            // Update instruction pointer to skip the patched instruction
            void* nextInstruction = static_cast<char*>(context.crashAddress) + instruction.length;
            UpdateInstructionPointer(context, nextInstruction);
            spdlog::info("DynamicFixApplicator: Successfully patched instruction");
        }

        return patchSuccess;
    }

    // Analyze instruction at crash address
    bool DynamicFixApplicator::AnalyzeInstruction(void* address, void* outInstruction) {
        ZydisDecoder decoder;
        ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

        ZydisDecodedInstruction* instruction = static_cast<ZydisDecodedInstruction*>(outInstruction);
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

        // Try to decode up to 15 bytes
        ZyanStatus status = ZydisDecoderDecodeFull(&decoder, address, 15,
                                                   instruction, operands);

        if (!ZYAN_SUCCESS(status)) {
            spdlog::warn("DynamicFixApplicator: Failed to decode instruction at {:p}", address);
            return false;
        }

        // Log instruction details for debugging
        char buffer[256];
        ZydisFormatter formatter;
        ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);
        ZydisFormatterFormatInstruction(&formatter, instruction, operands,
                                       instruction->operand_count_visible,
                                       buffer, sizeof(buffer), 
                                       reinterpret_cast<ZyanU64>(address), nullptr);
        
        spdlog::debug("DynamicFixApplicator: Analyzed instruction: {}", buffer);

        return true;
    }

    // Patch instruction to NOP (no operation)
    bool DynamicFixApplicator::PatchToNOP(void* address, size_t length) {
        DWORD oldProtection;
        
        // Make memory writable
        if (!MakeMemoryWritable(address, length, oldProtection)) {
            return false;
        }

        // Fill with NOP instructions (0x90)
        memset(address, 0x90, length);

        // Restore original protection
        RestoreMemoryProtection(address, length, oldProtection);

        // Flush instruction cache
        FlushInstructionCache(address, length);

        return true;
    }

    // Patch instruction to return with value
    bool DynamicFixApplicator::PatchToReturn(void* address, uint64_t returnValue) {
        DWORD oldProtection;
        size_t patchSize = 6;  // XOR EAX, EAX (2 bytes) + RET (1 byte) + padding

        // Make memory writable
        if (!MakeMemoryWritable(address, patchSize, oldProtection)) {
            return false;
        }

        // Write patch: XOR EAX, EAX; RET
        unsigned char* patchBytes = static_cast<unsigned char*>(address);
        patchBytes[0] = 0x31;  // XOR
        patchBytes[1] = 0xC0;  // EAX, EAX
        patchBytes[2] = 0xC3;  // RET
        
        // Fill remaining bytes with NOP
        for (size_t i = 3; i < patchSize; ++i) {
            patchBytes[i] = 0x90;  // NOP
        }

        // Restore original protection
        RestoreMemoryProtection(address, patchSize, oldProtection);

        // Flush instruction cache
        FlushInstructionCache(address, patchSize);

        return true;
    }

    // Update instruction pointer after patch
    bool DynamicFixApplicator::UpdateInstructionPointer(VEH::CrashContext& context, void* newAddress) {
        // Update RIP (instruction pointer) in the CPU context
        #ifdef _M_X64
            context.cpuContext.Rip = reinterpret_cast<DWORD64>(newAddress);
        #else
            context.cpuContext.Eip = reinterpret_cast<DWORD>(newAddress);
        #endif

        spdlog::debug("DynamicFixApplicator: Updated instruction pointer to {:p}", newAddress);
        return true;
    }

    // Make memory page writable for patching
    bool DynamicFixApplicator::MakeMemoryWritable(void* address, size_t size, DWORD& oldProtection) {
        if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtection)) {
            spdlog::error("DynamicFixApplicator: Failed to make memory writable at {:p}", address);
            return false;
        }
        return true;
    }

    // Restore original memory protection
    bool DynamicFixApplicator::RestoreMemoryProtection(void* address, size_t size, DWORD oldProtection) {
        DWORD dummy;
        if (!VirtualProtect(address, size, oldProtection, &dummy)) {
            spdlog::warn("DynamicFixApplicator: Failed to restore memory protection at {:p}", address);
            return false;
        }
        return true;
    }

    // Flush instruction cache after patching
    void DynamicFixApplicator::FlushInstructionCache(void* address, size_t size) {
        ::FlushInstructionCache(GetCurrentProcess(), address, size);
    }

}  // namespace DynamicFix
