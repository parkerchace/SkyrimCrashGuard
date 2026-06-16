// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_set>
#include <unordered_map>
#include <mutex>

/// Resource Loading Validation System
/// Validates resource files (NIF, DDS, KF, BSA) before loading to prevent crashes
/// Implements requirements 30.1-30.7
namespace ResourceValidation {

    /// Validation result structure
    struct ValidationResult {
        bool isValid;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::string resourcePath;
    };

    /// NIF file header structure (simplified)
    struct NifHeader {
        char headerString[40];  // "Gamebryo File Format" or "NetImmerse File Format"
        uint32_t version;
        uint8_t endianType;
        uint32_t userVersion;
        uint32_t numBlocks;
        uint16_t userVersion2;
    };

    /// DDS file header structures
    struct DDSPixelFormat {
        uint32_t size;
        uint32_t flags;
        uint32_t fourCC;
        uint32_t rgbBitCount;
        uint32_t rBitMask;
        uint32_t gBitMask;
        uint32_t bBitMask;
        uint32_t aBitMask;
    };

    struct DDSHeader {
        uint32_t magic;  // 'DDS '
        uint32_t size;
        uint32_t flags;
        uint32_t height;
        uint32_t width;
        uint32_t pitchOrLinearSize;
        uint32_t depth;
        uint32_t mipMapCount;
        uint32_t reserved1[11];
        DDSPixelFormat pixelFormat;
        uint32_t caps;
        uint32_t caps2;
        uint32_t caps3;
        uint32_t caps4;
        uint32_t reserved2;
    };

    /// KF animation header structure (simplified)
    struct KfHeader {
        char headerString[40];
        uint32_t version;
        uint8_t endianType;
        uint32_t userVersion;
        uint32_t numBlocks;
    };

    /// BSA archive header structure
    struct BSAHeader {
        uint32_t magic;  // 'BSA\0'
        uint32_t version;
        uint32_t offset;
        uint32_t archiveFlags;
        uint32_t folderCount;
        uint32_t fileCount;
        uint32_t totalFolderNameLength;
        uint32_t totalFileNameLength;
        uint32_t fileFlags;
    };

    /// Main resource validator class
    class ResourceValidator {
    public:
        /// Initialize the resource validator
        static bool Initialize();

        /// Shutdown and cleanup
        static void Shutdown();

        // NIF Validation (Requirement 30.1)
        /// Validate NIF file format before parsing
        static ValidationResult ValidateNifFile(const std::string& filepath);

        /// Validate NIF file from memory buffer
        static ValidationResult ValidateNifBuffer(const uint8_t* data, size_t size);

        // DDS Validation (Requirement 30.2)
        /// Validate DDS texture file
        static ValidationResult ValidateDdsFile(const std::string& filepath);

        /// Validate DDS texture from memory buffer
        static ValidationResult ValidateDdsBuffer(const uint8_t* data, size_t size);

        // KF Validation (Requirement 30.3)
        /// Validate KF animation file
        static ValidationResult ValidateKfFile(const std::string& filepath);

        /// Validate KF animation from memory buffer
        static ValidationResult ValidateKfBuffer(const uint8_t* data, size_t size);

        // BSA Validation (Requirement 30.4)
        /// Validate BSA archive file
        static ValidationResult ValidateBsaFile(const std::string& filepath);

        /// Validate BSA archive from memory buffer
        static ValidationResult ValidateBsaBuffer(const uint8_t* data, size_t size);

        // Resource Failure Caching (Requirements 30.6, 30.7)
        /// Check if resource is in failure cache
        static bool IsResourceCached(const std::string& filepath);

        /// Add resource to failure cache
        static void CacheFailedResource(const std::string& filepath, const std::string& reason);

        /// Get failure reason for cached resource
        static std::string GetCachedFailureReason(const std::string& filepath);

        /// Clear failure cache
        static void ClearFailureCache();

        /// Get all failed resources
        static std::vector<std::string> GetFailedResources();

    private:
        // NIF validation helpers
        static bool ValidateNifHeader(const NifHeader& header, std::vector<std::string>& errors);
        static bool ValidateNifVersion(uint32_t version, std::vector<std::string>& errors);
        static bool ValidateNifBlockStructure(const uint8_t* data, size_t size, const NifHeader& header, std::vector<std::string>& errors);

        // DDS validation helpers
        static bool ValidateDdsHeader(const DDSHeader& header, std::vector<std::string>& errors);
        static bool ValidateDdsDimensions(uint32_t width, uint32_t height, std::vector<std::string>& errors);
        static bool ValidateDdsFormat(const DDSPixelFormat& format, std::vector<std::string>& errors);
        static bool ValidateDdsMipmaps(const DDSHeader& header, std::vector<std::string>& errors);

        // KF validation helpers
        static bool ValidateKfHeader(const KfHeader& header, std::vector<std::string>& errors);
        static bool ValidateKfVersion(uint32_t version, std::vector<std::string>& errors);
        static bool ValidateKfBoneMappings(const uint8_t* data, size_t size, const KfHeader& header, std::vector<std::string>& errors);
        static bool ValidateKfKeyframeData(const uint8_t* data, size_t size, const KfHeader& header, std::vector<std::string>& errors);

        // BSA validation helpers
        static bool ValidateBsaHeader(const BSAHeader& header, std::vector<std::string>& errors);
        static bool ValidateBsaFileTable(const uint8_t* data, size_t size, const BSAHeader& header, std::vector<std::string>& errors);
        static bool ValidateBsaCompression(const BSAHeader& header, std::vector<std::string>& errors);

        // File I/O helpers
        static bool ReadFileToBuffer(const std::string& filepath, std::vector<uint8_t>& buffer);
        static bool FileExists(const std::string& filepath);

        // Failure cache
        struct FailedResource {
            std::string filepath;
            std::string reason;
            std::chrono::steady_clock::time_point timestamp;
        };

        static std::unordered_map<std::string, FailedResource> s_failureCache;
        static std::mutex s_cacheMutex;
        static bool s_initialized;
    };

}  // namespace ResourceValidation
