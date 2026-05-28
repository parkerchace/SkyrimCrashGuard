// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace CrashGuard {

/// Identifies each distinct recovery layer / outcome.
/// Values must stay stable  -  they are stored in LayerEvent::id.
enum class LayerID : uint8_t {
    // ── Universal recovery block (before Handler) ──
    UR_ZeroedReg,    ///< Zeroed destination GP register, advanced RIP
    UR_ZeroedXMM,    ///< Zeroed destination XMM register, advanced RIP
    UR_WriteSkip,    ///< Skipped write instruction (game code path)
    UR_FlagsSkip,    ///< Skipped flags-only instruction (TEST/CMP), no dest reg
    UR_FuncReturn,   ///< L5 function return from universal block
    UR_DeepWalk,     ///< L6 deep stack walk from universal block

    // ── Execute-AV path ──
    ExecAV_Return,   ///< Popped garbage call target, returned RAX=0 to caller

    // ── Handler layers (H_) ──
    H_KnownSite,     ///< L1  pre-analysed known crash site
    H_Pattern,       ///< L1b instruction-pattern match
    H_Learned,       ///< L2  previously decoded + cached fix
    H_RegFixup,      ///< L3  redirected faulting register to safety buffer
    H_InstrSkip,     ///< L4  decoded, zeroed dest, advanced RIP
    H_FuncReturn,    ///< L5  synthetic function return
    H_DeepWalk,      ///< L6  deep stack walk for return address

    // ── Terminal outcomes ──
    Unrecovered,     ///< All layers exhausted  -  crash propagates
    CascadeLimit,    ///< Cascade/cooldown gate refused recovery
    CooldownBlocked, ///< Recovery cooldown refused recovery
};

/// One step in the recovery chain.
struct LayerEvent {
    LayerID     id;
    std::string detail;   ///< e.g. "zeroed RCX, skip 3 bytes"
    bool        handled;  ///< true if this layer succeeded (last event that handled == true wins)
};

/// Full trace captured for one exception.
struct LayerTrace {
    std::string             crashAddr;    ///< formatted hex address
    std::string             exceptionDesc;///< e.g. "Access Violation (read 0x0)"
    std::vector<LayerEvent> events;

    bool empty() const { return events.empty(); }
};

// ─── Display helpers (inline, header-only) ───────────────────────────────

inline const char* GetLayerDisplayName(LayerID id) {
    switch (id) {
    case LayerID::UR_ZeroedReg:    return "Read as Zero";
    case LayerID::UR_ZeroedXMM:    return "Float Read as Zero";
    case LayerID::UR_WriteSkip:    return "Write Dropped";
    case LayerID::UR_FlagsSkip:    return "Check Skipped";
    case LayerID::UR_FuncReturn:   return "Function Returned";
    case LayerID::UR_DeepWalk:     return "Stack Recovered";
    case LayerID::ExecAV_Return:   return "Bad Call Intercepted";
    case LayerID::H_KnownSite:     return "Known Fix  (L1)";
    case LayerID::H_Pattern:       return "Pattern Fix  (L1b)";
    case LayerID::H_Learned:       return "Cached Fix  (L2)";
    case LayerID::H_RegFixup:      return "Pointer Fix  (L3)";
    case LayerID::H_InstrSkip:     return "Instruction Fix  (L4)";
    case LayerID::H_FuncReturn:    return "Function Return  (L5)";
    case LayerID::H_DeepWalk:      return "Stack Walk  (L6)";
    case LayerID::Unrecovered:     return "Not Recovered";
    case LayerID::CascadeLimit:    return "Rate Limited";
    case LayerID::CooldownBlocked: return "Cooldown Active";
    default:                       return "Unknown";
    }
}

inline const char* GetLayerShortDesc(LayerID id) {
    switch (id) {
    case LayerID::UR_ZeroedReg:    return "The game read from a missing object. CrashGuard answered with zero so the game could continue as if the object returned nothing.";
    case LayerID::UR_ZeroedXMM:    return "The game read a floating-point value from a missing object. CrashGuard answered with zero so the calculation can complete safely.";
    case LayerID::UR_WriteSkip:    return "The game tried to write to a missing or deleted object. CrashGuard quietly dropped the write  -  the game never knew it failed.";
    case LayerID::UR_FlagsSkip:    return "The game checked a condition on a missing object (TEST/CMP instruction  -  no data to store, just sets a flag). CrashGuard skipped the check; the game continues on the false/not-found branch.";
    case LayerID::UR_FuncReturn:   return "No register to fix. CrashGuard returned safely from the entire crashing function with an empty result, as if it ran and found nothing.";
    case LayerID::UR_DeepWalk:     return "The function couldn't return cleanly. CrashGuard scanned the call stack for a safe return point and jumped there  -  bypassing the corrupt code entirely.";
    case LayerID::ExecAV_Return:   return "The game tried to call a function that doesn't exist (null or corrupted vtable). CrashGuard intercepted the call, returned an empty result, and the caller continued normally.";
    case LayerID::H_KnownSite:     return "This exact crash address was pre-analysed. CrashGuard applied a known, tested fix for it instantly.";
    case LayerID::H_Pattern:       return "CrashGuard decoded the crashing instruction and matched it to a known recovery pattern  -  fixed without needing to know the exact game version.";
    case LayerID::H_Learned:       return "CrashGuard has seen this crash before this session and cached the fix. Recovery is instant  -  no re-analysis needed.";
    case LayerID::H_RegFixup:      return "The crash happened because a pointer register held a bad address. CrashGuard redirected it to a safe dummy area so the instruction could complete without harm.";
    case LayerID::H_InstrSkip:     return "CrashGuard decoded the failing instruction, zeroed the destination register, and moved past it  -  the game continues with a safe empty value.";
    case LayerID::H_FuncReturn:    return "CrashGuard couldn't fix the specific instruction. Instead it returned cleanly from the whole function with an empty result (RAX = 0).";
    case LayerID::H_DeepWalk:      return "The stack was too corrupted for a simple function return. CrashGuard scanned deeper and found a real return address, then jumped there safely.";
    case LayerID::Unrecovered:     return "All six recovery layers were tried and none could fix this crash. CrashGuard passed the exception to the OS  -  Skyrim would crash to desktop here.";
    case LayerID::CascadeLimit:    return "The same code crashed too many times in rapid succession. CrashGuard stopped trying to prevent an infinite loop  -  this is a sign of a deeper instability.";
    case LayerID::CooldownBlocked: return "Too many recoveries happened in a very short time. CrashGuard paused briefly to prevent the handler from spinning and eating CPU.";
    default:                       return "";
    }
}

/// One step in the full call chain that CrashGuard executes when handling a crash.
/// Shown sequentially in the code pane — from Windows calling the handler
/// all the way to EXCEPTION_CONTINUE_EXECUTION being returned.
struct JourneyStep {
    const char* file;         ///< Source file name (no path)
    int         lineFrom;     ///< First line of the relevant snippet
    int         lineTo;       ///< Last line (0 = same as lineFrom)
    const char* heading;      ///< Label: what this step does
    const char* explanation;  ///< One-sentence plain-English description
    const char* code;         ///< Curated code from that location
};

/// Returns the complete code journey for the given layer: every source
/// location touched from exception entry to EXCEPTION_CONTINUE_EXECUTION.
/// outCount receives the number of steps in the returned array.
inline const JourneyStep* GetLayerJourney(LayerID id, int* outCount) {

    // ── Shared preamble steps ────────────────────────────────────────────
    static const JourneyStep kEntry = {
        "VEH.cpp", 3213, 3246,
        "Exception caught by VEH",
        "Windows delivers the exception to CrashGuard's handler before any game code sees it.",
        "static LONG CALLBACK OrchestratedRecovery(\n"
        "        PEXCEPTION_POINTERS info) {\n"
        "    DWORD     code = info->ExceptionRecord->ExceptionCode;\n"
        "    uintptr_t rip  = info->ContextRecord->Rip;\n"
        "    // Fast-reject non-AV, reentrancy guard, module filter..."
    };
    static const JourneyStep kGuard = {
        "VEH.cpp", 3255, 3267,
        "Universal recovery block entered",
        "Confirms this is a read/write access violation from game/mod code, not the plugin itself.",
        "if (code == EXCEPTION_ACCESS_VIOLATION &&\n"
        "    (!IsSelfAddr(rip) || t_testMode)) {\n"
        "    bool isRead  = (accessType == 0);\n"
        "    bool isWrite = (accessType == 1);\n"
        "    if (isRead || isWrite) {    // execute-AV handled separately"
    };
    static const JourneyStep kZydis = {
        "VEH.cpp", 3278, 3290,
        "Zydis decodes the faulting instruction",
        "Zydis disassembles up to 15 bytes at RIP to identify the opcode, operands, and destination register.",
        "ZydisDecoderDecodeFull(\n"
        "    &s_decoder,\n"
        "    reinterpret_cast<void*>(rip), 15,\n"
        "    &instr, operands);\n"
        "// Find write-destination register in operands:\n"
        "for (int i = 0; i < instr.operand_count; ++i)\n"
        "    if (operands[i].actions & ZYDIS_OPERAND_ACTION_WRITE)\n"
        "        destReg = operands[i].reg.value;"
    };
    static const JourneyStep kReturn = {
        "VEH.cpp", 3520, 3520,
        "Execution resumed",
        "EXCEPTION_CONTINUE_EXECUTION tells Windows to resume the game at the (now advanced) instruction pointer.",
        "return EXCEPTION_CONTINUE_EXECUTION;\n"
        "// Windows restores the saved CONTEXT (with our edits)\n"
        "// and jumps to ctx->Rip — the instruction after the fault."
    };

    // ── Execute-AV preamble ───────────────────────────────────────────────
    static const JourneyStep kExecAVDetect = {
        "VEH.cpp", 3525, 3543,
        "Execute-AV detected",
        "The CPU tried to execute code at an invalid address (null vtable or corrupted function pointer). RIP itself is the garbage address.",
        "// accessType == 8 means the CPU tried to EXECUTE the faulting address\n"
        "if (ExceptionInformation[0] == 8) {\n"
        "    isExecuteAV = true;\n"
        "    // Check the RETURN ADDRESS on the stack instead of RIP\n"
        "    uintptr_t retAddr = *reinterpret_cast<uint64_t*>(ctx->Rsp);\n"
        "    if (IsSelfAddr(retAddr) || !IsRecoverableAddr(retAddr))\n"
        "        return EXCEPTION_CONTINUE_SEARCH; // not ours"
    };
    static const JourneyStep kExecAVRecover = {
        "VEH.cpp", 1732, 1845,
        "ExecuteAV_Recovery: pop and return",
        "The return address (left on the stack by the CALL instruction) is popped into RIP. RAX is set to zero. The caller resumes as if the function returned null.",
        "static bool ExecuteAV_Recovery(PCONTEXT ctx) {\n"
        "    uint64_t retAddr =\n"
        "        *reinterpret_cast<uint64_t*>(ctx->Rsp);\n"
        "    // Verify retAddr is in executable memory and\n"
        "    // preceded by a CALL opcode (E8 or FF /2).\n"
        "    ctx->Rip = retAddr;   // jump to caller\n"
        "    ctx->Rsp += 8;        // pop the return addr\n"
        "    ctx->Rax = 0;         // return value = null\n"
        "    s_stats.funcReturn++;"
    };

    // ── Universal block: zeroed register ─────────────────────────────────
    static const JourneyStep kURZeroReg = {
        "VEH.cpp", 3316, 3322,
        "Destination register zeroed",
        "The register that was about to receive the bad read is set to zero. RIP advances past the instruction so the game resumes at the next opcode.",
        "int ctxIdx = RegToCtx(destReg);\n"
        "if (ctxIdx != kNONE) {\n"
        "    // Write 0 into the CONTEXT register slot\n"
        "    *reinterpret_cast<uintptr_t*>(\n"
        "        reinterpret_cast<uint8_t*>(ctx) + ctxIdx) = 0;\n"
        "    ctx->Rip += instr.length;  // skip past the fault\n"
        "    recoveryMethod = \"zeroed register\";"
    };

    // ── Universal block: zeroed XMM ───────────────────────────────────────
    static const JourneyStep kURZeroXMM = {
        "VEH.cpp", 3293, 3297,
        "XMM register zeroed",
        "The floating-point destination register is zeroed. Game math involving this value continues with 0.0 instead of crashing.",
        "if (IsXMMRegister(destReg)) {\n"
        "    ZeroXMMRegister(ctx, destReg);\n"
        "    ctx->Rip += instr.length;\n"
        "    recoveryMethod = \"zeroed XMM\";"
    };
    static const JourneyStep kURZeroXMMImpl = {
        "VEH.cpp", 1061, 1079,
        "ZeroXMMRegister writes zeros to the XMM slot",
        "Looks up the register's offset in the CONTEXT struct and memsets 16 bytes to zero.",
        "static void ZeroXMMRegister(PCONTEXT ctx, ZydisRegister r) {\n"
        "    // Map Zydis register enum -> CONTEXT.Xmm0..Xmm15 offset\n"
        "    int off = XMMToCtx(r);\n"
        "    if (off != kNONE)\n"
        "        memset(reinterpret_cast<uint8_t*>(ctx) + off,\n"
        "               0, sizeof(M128A));"
    };

    // ── Universal block: write skip ───────────────────────────────────────
    static const JourneyStep kURWriteSkip = {
        "VEH.cpp", 3435, 3439,
        "Write instruction skipped",
        "The write is silently dropped by advancing RIP past the instruction. Game state is unchanged; one data write is lost.",
        "// Game code write AV — just skip the store\n"
        "ctx->Rip += instr.length;\n"
        "recoveryMethod = \"skipped write\";"
    };

    // ── Universal block: flags skip (TEST/CMP, no dest) ──────────────────
    static const JourneyStep kURNoDestCheck = {
        "VEH.cpp", 3341, 3345,
        "No destination register found",
        "TEST and CMP instructions only write to CPU flags, not a register. There is nothing to zero — a different path is needed.",
        "// destReg == NONE: instruction has no writable register operand\n"
        "// (e.g. TEST BYTE PTR [rax+0x109], 0x04)\n"
        "// Cannot zero a register; fall through to instruction-skip."
    };
    static const JourneyStep kURFlagsSkip = {
        "VEH.cpp", 3388, 3391,
        "RIP advanced past the TEST/CMP instruction",
        "The instruction pointer is moved past the faulting instruction. The CPU flags remain unchanged; the game takes the false/not-found branch of whatever condition follows.",
        "// Last resort in test mode — advance past the opcode\n"
        "ctx->Rip += instr.length;\n"
        "recoveryMethod = \"skipped instruction (no output reg)\";"
    };

    // ── Universal block: func return / deep walk ──────────────────────────
    static const JourneyStep kURFuncReturn = {
        "VEH.cpp", 1704, 1715,
        "L5_FuncReturn: synthetic function return",
        "Reads the return address from the top of the stack and jumps there with RAX=0, as if the crashing function returned null cleanly.",
        "static bool L5_FuncReturn(PCONTEXT ctx) {\n"
        "    uintptr_t ret =\n"
        "        *reinterpret_cast<uintptr_t*>(ctx->Rsp);\n"
        "    if (!IsExec(reinterpret_cast<void*>(ret)))\n"
        "        return false;\n"
        "    ctx->Rip = ret;\n"
        "    ctx->Rsp += 8;\n"
        "    ctx->Rax = 0;"
    };
    static const JourneyStep kURDeepWalk = {
        "VEH.cpp", 1903, 1930,
        "L6_DeepWalk: scans the call stack",
        "Walks up the stack 8 bytes at a time looking for an address preceded by a CALL opcode signature, then jumps there.",
        "static bool L6_DeepWalk(PCONTEXT ctx) {\n"
        "    for (uintptr_t sp = ctx->Rsp;\n"
        "         sp < stackTop; sp += 8) {\n"
        "        uintptr_t candidate =\n"
        "            *reinterpret_cast<uintptr_t*>(sp);\n"
        "        if (IsExec(candidate) &&\n"
        "            IsCallSite(candidate - 5)) {\n"
        "            ctx->Rip = candidate;\n"
        "            ctx->Rsp = sp + 8;\n"
        "            ctx->Rax = 0;"
    };

    // ── Handler layers ────────────────────────────────────────────────────
    static const JourneyStep kHandlerEntry = {
        "VEH.cpp", 3600, 3620,
        "Handler layers invoked",
        "Universal recovery did not fire (address is inside a known DLL, or it's a module-filter exception). The 6-layer Handler is called next.",
        "// Universal block skipped — call Handler() / OrchestratedRecovery\n"
        "// L1 KnownSite -> L1b Pattern -> L2 Learned ->\n"
        "// L3 RegFixup  -> L4 InstrSkip -> L5 Return -> L6 DeepWalk"
    };
    static const JourneyStep kHL1 = {
        "VEH.cpp", 1082, 1100,
        "L1_KnownSite: pre-analysed fix",
        "A table of manually reverse-engineered crash sites is checked first. If the RIP matches, the stored fix is applied in microseconds.",
        "static bool L1_KnownSite(PCONTEXT ctx, uintptr_t rip) {\n"
        "    for (const auto& site : s_knownSites) {\n"
        "        if (site.rip == rip) {\n"
        "            site.apply(ctx);\n"
        "            s_stats.knownSite++;\n"
        "            return true;"
    };
    static const JourneyStep kHL1b = {
        "VEH.cpp", 1203, 1250,
        "L1b_InstructionPattern: Zydis pattern match",
        "Decodes the faulting instruction and matches it against known crash patterns (null CALL, null JMP, XMM read, etc.) — version-independent.",
        "static bool L1b_InstructionPattern(\n"
        "        PEXCEPTION_POINTERS info) {\n"
        "    ZydisDecodedInstruction instr;\n"
        "    ZydisDecoderDecodeFull(&s_decoder, rip, 15,\n"
        "                           &instr, operands);\n"
        "    // P1: CALL [base+off] bad base\n"
        "    // P2: JMP  [base+off] bad base\n"
        "    // P3: MOV/MOVZX read  bad base -> zero dest\n"
        "    // P4: MOV write        bad base -> skip"
    };
    static const JourneyStep kHL2 = {
        "VEH.cpp", 1504, 1520,
        "L2_LearnedSite: in-memory cache hit",
        "On the second crash at the same RIP address, the cached fix (register offset + instruction length) is applied instantly — no re-decoding needed.",
        "static bool L2_LearnedSite(PCONTEXT ctx, uintptr_t rip) {\n"
        "    size_t n = s_learnedCount.load();\n"
        "    for (size_t i = 0; i < n; ++i) {\n"
        "        if (s_learned[i].rip == rip) {\n"
        "            // Apply cached fix\n"
        "            *RegPtr(ctx, s_learned[i].destCtx) = 0;\n"
        "            ctx->Rip += s_learned[i].instrLen;\n"
        "            s_stats.learnedSite++;"
    };
    static const JourneyStep kHL3 = {
        "VEH.cpp", 1537, 1615,
        "L3_RegFixup: redirect faulting pointer",
        "The base register holding the bad address is redirected to a static 4 KB dummy buffer so the instruction can complete without writing anywhere harmful.",
        "static bool L3_RegFixup(PEXCEPTION_POINTERS info) {\n"
        "    // Find the base register of the faulting memory operand\n"
        "    ZydisRegister base = GetBaseReg(instr, operands);\n"
        "    if (base == ZYDIS_REGISTER_NONE) return false;\n"
        "    *RegPtr(ctx, RegToCtx(base)) =\n"
        "        reinterpret_cast<uintptr_t>(&s_safetyBuffer);\n"
        "    s_stats.regFixup++;"
    };
    static const JourneyStep kHL4 = {
        "VEH.cpp", 1623, 1695,
        "L4_InstrSkip: decode, zero dest, advance RIP",
        "If L3 failed, Zydis decodes the instruction, zeroes the destination register, and advances RIP. The fix is cached in s_learned[] for instant re-use.",
        "static bool L4_InstrSkip(PCONTEXT ctx) {\n"
        "    ZeroDestReg(ctx, instr);\n"
        "    ctx->Rip += instr.length;\n"
        "    // Cache for L2 next time\n"
        "    s_learned[slot].rip.store(rip);\n"
        "    s_learned[slot].instrLen = instr.length;\n"
        "    s_learned[slot].destCtx  = destOff;\n"
        "    s_stats.instrSkip++;"
    };
    static const JourneyStep kHL5 = {
        "VEH.cpp", 1704, 1715,
        "L5_FuncReturn: synthetic function return",
        "Pops the return address from the stack, sets RAX=0, and jumps there — the crashing function appears to have returned null to its caller.",
        "static bool L5_FuncReturn(PCONTEXT ctx) {\n"
        "    uintptr_t ret =\n"
        "        *reinterpret_cast<uintptr_t*>(ctx->Rsp);\n"
        "    ctx->Rip = ret;\n"
        "    ctx->Rsp += 8;\n"
        "    ctx->Rax = 0;\n"
        "    s_stats.funcReturn++;"
    };
    static const JourneyStep kHL6 = {
        "VEH.cpp", 1903, 1930,
        "L6_DeepWalk: stack scan for return site",
        "All six layers failed to fix the instruction directly. CrashGuard scans the call stack for a CALL-preceded address and jumps there as a last resort.",
        "static bool L6_DeepWalk(PCONTEXT ctx) {\n"
        "    // Walk up the stack 8 bytes at a time\n"
        "    for (uintptr_t sp = ctx->Rsp; sp < top; sp += 8) {\n"
        "        uintptr_t candidate = *(uintptr_t*)sp;\n"
        "        if (IsExec(candidate) &&\n"
        "            IsCallSite(candidate - 5)) {\n"
        "            ctx->Rip = candidate;\n"
        "            ctx->Rsp = sp + 8;\n"
        "            ctx->Rax = 0;\n"
        "            s_stats.deepWalk++;"
    };

    // ── Terminal outcomes ─────────────────────────────────────────────────
    static const JourneyStep kUnrecovered = {
        "VEH.cpp", 4010, 4015,
        "All layers exhausted — EXCEPTION_CONTINUE_SEARCH",
        "Every recovery attempt failed. CrashGuard passes the exception to the next handler (CrashLogger, OS debugger, or crash-to-desktop).",
        "s_stats.unrecoverable++;\n"
        "return EXCEPTION_CONTINUE_SEARCH;\n"
        "// Windows walks to next VEH/SEH handler.\n"
        "// CrashLogger (if installed) will log the callstack.\n"
        "// Otherwise the game shows the crash dialog or CTDs."
    };
    static const JourneyStep kCascade = {
        "VEH.cpp", 3600, 3610,
        "Cascade limit gate",
        "The same address has crashed more than cascadeLimit times in a short window. Refusing to recover prevents an infinite exception loop.",
        "if (!t_testMode && hits > cascadeLimit)\n"
        "    return EXCEPTION_CONTINUE_SEARCH;\n"
        "// This protects against code that crashes again\n"
        "// immediately after recovery (broken loop, etc.)."
    };
    static const JourneyStep kCooldown = {
        "VEH.cpp", 3590, 3598,
        "Recovery cooldown gate",
        "Too many recoveries happened within the cooldown window. Rate-limiting prevents the handler from spinning at full CPU speed.",
        "if (!t_testMode && !CheckRecoveryCooldown())\n"
        "    return EXCEPTION_CONTINUE_SEARCH;\n"
        "// Cooldown: max N recoveries per time window.\n"
        "// Prevents the handler from burning a full CPU core\n"
        "// if a broken mod crashes every microsecond."
    };

    // ── Journey arrays ─────────────────────────────────────────────────────
    static const JourneyStep kJourney_UR_ZeroedReg[] = {
        kEntry, kGuard, kZydis, kURZeroReg, kReturn };
    static const JourneyStep kJourney_UR_ZeroedXMM[] = {
        kEntry, kGuard, kZydis, kURZeroXMM, kURZeroXMMImpl, kReturn };
    static const JourneyStep kJourney_UR_WriteSkip[] = {
        kEntry, kGuard, kZydis, kURWriteSkip, kReturn };
    static const JourneyStep kJourney_UR_FlagsSkip[] = {
        kEntry, kGuard, kZydis, kURNoDestCheck, kURFlagsSkip, kReturn };
    static const JourneyStep kJourney_UR_FuncReturn[] = {
        kEntry, kGuard, kZydis, kURNoDestCheck, kURFuncReturn, kReturn };
    static const JourneyStep kJourney_UR_DeepWalk[] = {
        kEntry, kGuard, kZydis, kURNoDestCheck, kURDeepWalk, kReturn };
    static const JourneyStep kJourney_ExecAV[] = {
        kEntry, kExecAVDetect, kExecAVRecover, kReturn };
    static const JourneyStep kJourney_H_KnownSite[] = {
        kEntry, kHandlerEntry, kHL1, kReturn };
    static const JourneyStep kJourney_H_Pattern[] = {
        kEntry, kHandlerEntry, kHL1b, kHL4, kReturn };
    static const JourneyStep kJourney_H_Learned[] = {
        kEntry, kHandlerEntry, kHL2, kReturn };
    static const JourneyStep kJourney_H_RegFixup[] = {
        kEntry, kHandlerEntry, kHL3, kReturn };
    static const JourneyStep kJourney_H_InstrSkip[] = {
        kEntry, kHandlerEntry, kHL4, kReturn };
    static const JourneyStep kJourney_H_FuncReturn[] = {
        kEntry, kHandlerEntry, kHL5, kReturn };
    static const JourneyStep kJourney_H_DeepWalk[] = {
        kEntry, kHandlerEntry, kHL6, kReturn };
    static const JourneyStep kJourney_Unrecovered[] = { kEntry, kUnrecovered };
    static const JourneyStep kJourney_CascadeLimit[] = { kEntry, kCascade };
    static const JourneyStep kJourney_CooldownBlocked[] = { kEntry, kCooldown };

#define CG_JOURNEY(arr) do { *outCount = (int)(sizeof(arr)/sizeof(arr[0])); return arr; } while(0)

    switch (id) {
    case LayerID::UR_ZeroedReg:    CG_JOURNEY(kJourney_UR_ZeroedReg);
    case LayerID::UR_ZeroedXMM:    CG_JOURNEY(kJourney_UR_ZeroedXMM);
    case LayerID::UR_WriteSkip:    CG_JOURNEY(kJourney_UR_WriteSkip);
    case LayerID::UR_FlagsSkip:    CG_JOURNEY(kJourney_UR_FlagsSkip);
    case LayerID::UR_FuncReturn:   CG_JOURNEY(kJourney_UR_FuncReturn);
    case LayerID::UR_DeepWalk:     CG_JOURNEY(kJourney_UR_DeepWalk);
    case LayerID::ExecAV_Return:   CG_JOURNEY(kJourney_ExecAV);
    case LayerID::H_KnownSite:     CG_JOURNEY(kJourney_H_KnownSite);
    case LayerID::H_Pattern:       CG_JOURNEY(kJourney_H_Pattern);
    case LayerID::H_Learned:       CG_JOURNEY(kJourney_H_Learned);
    case LayerID::H_RegFixup:      CG_JOURNEY(kJourney_H_RegFixup);
    case LayerID::H_InstrSkip:     CG_JOURNEY(kJourney_H_InstrSkip);
    case LayerID::H_FuncReturn:    CG_JOURNEY(kJourney_H_FuncReturn);
    case LayerID::H_DeepWalk:      CG_JOURNEY(kJourney_H_DeepWalk);
    case LayerID::Unrecovered:     CG_JOURNEY(kJourney_Unrecovered);
    case LayerID::CascadeLimit:    CG_JOURNEY(kJourney_CascadeLimit);
    case LayerID::CooldownBlocked: CG_JOURNEY(kJourney_CooldownBlocked);
    default:
        *outCount = 0;
        return nullptr;
    }
#undef CG_JOURNEY
}

/// Compact source location (file + line) used by the UI for labels.
struct LayerCodeLocation {
    const char* file;  ///< Filename (no path). nullptr if unknown.
    int         line;  ///< 1-based line number. 0 if unknown.
};

/// Returns the source location of the most-specific step in the journey
/// (the layer action itself, not the entry or return).
inline LayerCodeLocation GetLayerCodeLocation(LayerID id) {
    int count = 0;
    const JourneyStep* j = GetLayerJourney(id, &count);
    if (!j || count == 0) return { nullptr, 0 };
    // Return the location of the most-specific step (last non-return step)
    int specific = (count >= 2) ? count - 2 : 0;
    return { j[specific].file, j[specific].lineFrom };
}

}  // namespace CrashGuard
