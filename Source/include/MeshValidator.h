// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

/// Mesh Validation and Repair System
/// Validates and repairs 3D mesh files to prevent crashes from corrupted geometry
namespace MeshValidation {

    /// Validation result structure
    struct ValidationResult {
        bool isValid;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        bool canRepair;
    };

    /// Main mesh validator class
    class MeshValidator {
    public:
        /// Initialize the mesh validator
        static bool Initialize();

        /// Validate mesh before loading
        static ValidationResult ValidateMesh(const RE::NiAVObject* mesh);

        /// Attempt procedural repair of mesh
        static RE::NiAVObject* RepairMesh(RE::NiAVObject* mesh, const ValidationResult& issues);

        /// Get fallback placeholder mesh
        static RE::NiAVObject* GetPlaceholderMesh();

    private:
        // Validation functions
        /// Validate vertex data (positions, count)
        static bool ValidateVertexData(const RE::NiGeometry* geometry, std::vector<std::string>& errors);

        /// Validate normal vectors
        static bool ValidateNormals(const RE::NiGeometry* geometry, std::vector<std::string>& errors);

        /// Validate UV coordinates
        static bool ValidateUVCoords(const RE::NiGeometry* geometry, std::vector<std::string>& errors);

        /// Validate bone weights and indices
        static bool ValidateBoneWeights(const RE::NiSkinInstance* skin, std::vector<std::string>& errors);

        /// Validate triangle data
        static bool ValidateTriangles(const RE::NiGeometry* geometry, std::vector<std::string>& errors);

        // Repair functions
        /// Recalculate normals from vertices
        static bool RecalculateNormals(RE::NiGeometry* geometry);

        /// Generate planar UV coordinates
        static bool GeneratePlanarUVs(RE::NiGeometry* geometry);

        /// Remove degenerate triangles
        static bool RemoveDegenerateTriangles(RE::NiGeometry* geometry);

        /// Clamp bone indices to valid range
        static bool ClampBoneIndices(RE::NiSkinInstance* skin);

        /// Replace NaN/Inf values with valid numbers
        static bool ReplaceInvalidValues(RE::NiGeometry* geometry);

        // Placeholder mesh generation
        /// Create simple cube mesh as fallback
        static RE::NiAVObject* CreatePlaceholderCube();

        // Helper functions
        /// Check if a float value is valid (not NaN or Inf)
        static bool IsValidFloat(float value);

        /// Check if a vector is valid (all components finite)
        static bool IsValidVector(const RE::NiPoint3& vec);

        /// Check if a vector is unit length
        static bool IsUnitVector(const RE::NiPoint3& vec, float epsilon = 0.01f);

        /// Normalize a vector
        static RE::NiPoint3 Normalize(const RE::NiPoint3& vec);

        /// Calculate triangle area
        static float CalculateTriangleArea(const RE::NiPoint3& v0, const RE::NiPoint3& v1, const RE::NiPoint3& v2);

        /// Check if triangle is degenerate (zero area or duplicate vertices)
        static bool IsDegenerate(const RE::NiPoint3& v0, const RE::NiPoint3& v1, const RE::NiPoint3& v2);

        /// Get geometry data from NiAVObject
        static RE::NiGeometry* GetGeometry(RE::NiAVObject* object);

        /// Get geometry data (const version)
        static const RE::NiGeometry* GetGeometry(const RE::NiAVObject* object);

        // Cached placeholder mesh
        static RE::NiAVObject* s_placeholderMesh;
        static bool s_initialized;
    };

}  // namespace MeshValidation
