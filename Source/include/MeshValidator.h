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
        /// Set all normals to a safe upward fallback value (0,0,1).
        /// Per-vertex normals from triangle data are not available in this
        /// CommonLibSSE-NG version (NiTriShapeData fields are not exposed).
        static bool RecalculateNormals(RE::NiGeometry* geometry);

        /// Generate planar UV coordinates using XY-plane projection
        static bool GeneratePlanarUVs(RE::NiGeometry* geometry);

        /// Clamp bone weight values to valid range [0,1]
        static bool ClampBoneIndices(RE::NiSkinInstance* skin);

        /// Replace NaN/Inf values with valid numbers so the renderer won't crash
        static bool ReplaceInvalidValues(RE::NiGeometry* geometry);

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

        static bool s_initialized;
    };

}  // namespace MeshValidation
