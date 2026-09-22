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

#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include "LayerTrace.h"

namespace CrashGuard {

    /// Brief recovery notification for ImGui overlay
    struct RecoveryToast {
        std::string severity;           // "Safe", "Warning", "Critical"
        std::string summary;            // Brief one-line description (fallback)
        std::string strategy;           // Recovery strategy used
        std::chrono::steady_clock::time_point timestamp;
        float displayTime;              // Seconds to display (3-5s)
        bool visible;

        // Rich crash context (populated when VEH recovers a real crash)
        std::string crashAddr;          // e.g. "SkyrimSE.exe+0x14F400E"
        std::string moduleName;         // DLL where crash occurred
        std::string decodedInstruction; // Zydis Intel-syntax string
        uint64_t    accessAddress = 0;  // ExceptionInformation[1]
        int         accessType    = -1; // 0=read, 1=write, 8=execute
        std::string affectedRegister;   // register zeroed, or empty

        RecoveryToast() : displayTime(0.0f), visible(false) {}
    };

    /// Detailed recovery entry for F11 menu
    struct RecoveryEntry {
        std::string severity;
        std::string timestamp;          ///< Formatted time string
        std::string rootCause;
        std::string strategy;
        std::vector<std::string> actions;
        std::vector<std::string> suspectedMods;
        bool successful;

        /// VEH layer that handled this crash (set from universal recovery block)
        LayerID     layerUsed  = LayerID::Unrecovered;
        /// Formatted crash address, e.g. "SkyrimSE.exe+0x14F400E"
        std::string crashAddr;

        /// Module (DLL/exe) where the crash occurred, e.g. "hdtSMP64.dll"
        std::string moduleName;
        /// Zydis-formatted faulting instruction, e.g. "test byte ptr [r14+0x109], 0x08"
        std::string decodedInstruction;
        /// Address that was read/written/executed (ExceptionInformation[1])
        uint64_t    accessAddress = 0;
        /// 0=read, 1=write, 8=execute, -1=unknown
        int         accessType = -1;
        /// Register that was zeroed or otherwise modified, e.g. "rax", "xmm3" - empty if none
        std::string affectedRegister;
        /// True when CrashGuard's own test suite raised this fault on purpose. Such
        /// entries are not game crashes and carry no mod attribution.
        bool        selfTest = false;

        RecoveryEntry() : successful(false), layerUsed(LayerID::Unrecovered) {}
    };

    /// Manages real-time recovery notifications
    class RecoveryNotifications {
    public:
        static RecoveryNotifications& GetSingleton() {
            static RecoveryNotifications instance;
            return instance;
        }

        /// Add a new recovery notification (called from VEH)
        void AddRecovery(
            const std::string& severity,
            const std::string& rootCause,
            const std::string& strategy,
            const std::vector<std::string>& actions,
            const std::vector<std::string>& suspectedMods,
            bool successful,
            LayerID     layerUsed          = LayerID::Unrecovered,
            std::string crashAddr          = {},
            std::string moduleName         = {},
            std::string decodedInstruction = {},
            uint64_t    accessAddress      = 0,
            int         accessType         = -1,
            std::string affectedRegister   = {},
            bool        selfTest           = false
        );
        
        /// Add a resource warning notification (auto-opens menu if critical)
        void AddResourceWarning(
            const std::string& resourceType,
            const std::string& message,
            bool isCritical
        );

        /// Render toast notifications (top-right overlay)
        void RenderToasts();

        /// Get recent recoveries for F11 menu
        std::vector<RecoveryEntry> GetRecentRecoveries(size_t maxCount = 50) const;

        /// Clear all recovery history
        void ClearHistory();

        /// Get statistics
        size_t GetTotalRecoveries() const;
        size_t GetSuccessfulRecoveries() const;
        size_t GetFailedRecoveries() const;

    private:
        RecoveryNotifications() = default;
        ~RecoveryNotifications() = default;
        RecoveryNotifications(const RecoveryNotifications&) = delete;
        RecoveryNotifications& operator=(const RecoveryNotifications&) = delete;

        void UpdateToasts();
        std::string FormatTimestamp(const std::chrono::steady_clock::time_point& time) const;
        std::string CreateSummary(const std::string& rootCause, const std::string& strategy) const;

        mutable std::mutex m_mutex;
        std::vector<RecoveryToast> m_activeToasts;
        std::vector<RecoveryEntry> m_history;
        
        size_t m_totalRecoveries = 0;
        size_t m_successfulRecoveries = 0;
        size_t m_failedRecoveries = 0;
        
        static constexpr size_t MAX_ACTIVE_TOASTS = 3;
        static constexpr size_t MAX_HISTORY = 100;
        static constexpr float TOAST_DURATION_SAFE = 3.0f;
        static constexpr float TOAST_DURATION_WARNING = 4.0f;
        static constexpr float TOAST_DURATION_CRITICAL = 5.0f;
    };

}
