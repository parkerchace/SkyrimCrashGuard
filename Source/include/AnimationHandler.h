// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <string>
#include <unordered_set>
#include <chrono>

/// Animation Handler for Layer 1 Proactive Validation
/// Validates animation files and bone mappings before playback
/// Implements requirements 7.1, 7.2, 7.4, 7.6, 36.1, 36.2
namespace AnimationValidation {

    /// Animation validation result
    struct AnimationValidationResult {
        bool isValid;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        bool canRetry;
    };

    /// Animation blacklist entry
    struct BlacklistEntry {
        std::string animationPath;
        std::string reason;
        std::chrono::steady_clock::time_point blacklistedAt;
        uint32_t failureCount;
    };

    /// Main animation handler class
    class AnimationHandler {
    public:
        /// Initialize the animation handler
        static bool Initialize();

        /// Validate animation before playback
        /// Requirements: 7.1, 7.2
        static bool ValidateAnimation(const char* animPath, RE::Actor* actor);

        /// Play animation with fallback
        /// Requirements: 7.4, 36.3, 36.4
        static bool PlayAnimationSafe(RE::Actor* actor, const char* animPath);

        /// Stop animation and reset to safe pose
        /// Requirements: 7.4, 7.6
        static void ResetToSafePose(RE::Actor* actor);

        /// Check if animation is blacklisted
        /// Requirements: 36.1
        static bool IsBlacklisted(const char* animPath);

        /// Blacklist problematic animation
        /// Requirements: 36.2
        static void BlacklistAnimation(const char* animPath, const std::string& reason);

        /// Get statistics
        static size_t GetBlacklistSize();
        static size_t GetValidationCount();
        static size_t GetFailureCount();

        /// Clear blacklist (for testing)
        static void ClearBlacklist();

    private:
        /// Validate bone mappings between animation and actor
        /// Requirements: 7.2
        static bool ValidateBoneMappings(const char* animPath, RE::Actor* actor);

        /// Validate animation file data
        /// Requirements: 7.1
        static bool ValidateAnimationData(const char* animPath);

        /// Get default idle animation path
        /// Requirements: 7.6
        static const char* GetDefaultIdleAnimation();

        /// Check if file exists and is readable
        static bool IsFileAccessible(const char* filePath);

        /// Extract skeleton from actor
        static RE::NiNode* GetActorSkeleton(RE::Actor* actor);

        /// Validate animation file format
        static AnimationValidationResult ValidateAnimationFile(const char* animPath);

        // State tracking
        static bool s_initialized;
        static std::unordered_set<std::string> s_blacklistedAnimations;
        static std::vector<BlacklistEntry> s_blacklistEntries;
        static size_t s_validationCount;
        static size_t s_failureCount;
        static std::shared_mutex s_blacklistMutex;  // Upgraded to shared_mutex for read-heavy operations (Requirements: 31.1, 31.2, 31.3)
    };

}  // namespace AnimationValidation