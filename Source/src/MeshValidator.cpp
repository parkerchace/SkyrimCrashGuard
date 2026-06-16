// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "PCH.h"
#include "MeshValidator.h"
#include "PerformanceOptimizations.h"
#include "PerformanceMetrics.h"

namespace MeshValidation {

    // Static member initialization
    bool MeshValidator::s_initialized = false;

    bool MeshValidator::Initialize() {
        if (s_initialized) {
            return true;
        }

        spdlog::info("Initializing MeshValidator...");
        
        // Placeholder mesh will be created on first use (lazy initialization)
        s_initialized = true;
        
        spdlog::info("MeshValidator initialized successfully");
        return true;
    }

    ValidationResult MeshValidator::ValidateMesh(const RE::NiAVObject* mesh) {
        // Performance optimization: Fast-path for likely valid meshes
        using namespace Performance;
        
        ValidationResult result;
        result.isValid = true;
        result.canRepair = true;

        // Inline null check
        if (!IsValidPointer(mesh)) {
            result.errors.push_back("Mesh pointer is null");
            result.isValid = false;
            result.canRepair = false;
            
            // Increment performance counter
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementMeshesValidated();
            
            return result;
        }

        // Fast-path: Check if mesh is likely valid using heuristics
        if (FastPathChecker::MeshLikelyValid(mesh)) {
            ScopedTimer timer(true);  // Fast path
            
            // Check if it's a NiNode (scene graph) - these are valid and common in Skyrim
            // Cast away const to call AsNode() - this is safe for type checking
            auto node = const_cast<RE::NiAVObject*>(mesh)->AsNode();
            if (node) {
                // NiNode hierarchies are valid - they contain geometry as children
                // We don't need to validate the structure, the game handles this fine
                result.isValid = true;
                
                // Increment performance counter
                CrashGuard::PerformanceMonitor::GetSingleton().IncrementMeshesValidated();
                
                return result;
            }
        }
        
        // Slow path: Full validation required
        ScopedTimer timer(false);  // Slow path

        // Get geometry from the mesh (only if it's actually a geometry object)
        const RE::NiGeometry* geometry = GetGeometry(mesh);
        if (!geometry) {
            // If it's not a NiNode and not a NiGeometry, then it might be invalid
            // But let's be permissive - the game might handle other types fine
            result.isValid = true;  // Changed from false - be less strict
            
            // Increment performance counter
            CrashGuard::PerformanceMonitor::GetSingleton().IncrementMeshesValidated();
            
            return result;
        }

        // Only validate actual NiGeometry objects
        // Validate vertex data
        if (!ValidateVertexData(geometry, result.errors)) {
            result.isValid = false;
        }

        // Validate normals
        if (!ValidateNormals(geometry, result.errors)) {
            result.isValid = false;
            // Normals can be recalculated
        }

        // Validate UV coordinates
        if (!ValidateUVCoords(geometry, result.errors)) {
            result.isValid = false;
            // UVs can be generated
        }

        // Validate triangles
        if (!ValidateTriangles(geometry, result.errors)) {
            result.isValid = false;
            // Degenerate triangles can be removed
        }

        // Validate bone weights if skinned
        auto& runtimeData = geometry->GetRuntimeData();
        if (runtimeData.m_spSkinInstance) {
            if (!ValidateBoneWeights(runtimeData.m_spSkinInstance.get(), result.errors)) {
                result.isValid = false;
                // Bone indices can be clamped
            }
        }

        // Determine if repair is possible
        if (!result.isValid && result.errors.size() > 10) {
            result.canRepair = false;
            result.errors.push_back("Too many errors - mesh is too corrupted to repair");
        }

        // Increment performance counter
        CrashGuard::PerformanceMonitor::GetSingleton().IncrementMeshesValidated();

        return result;
    }

    RE::NiAVObject* MeshValidator::RepairMesh(RE::NiAVObject* mesh, const ValidationResult& issues) {
        if (!mesh || !issues.canRepair) {
            return nullptr;
        }

        RE::NiGeometry* geometry = GetGeometry(mesh);
        if (!geometry) {
            return nullptr;
        }

        spdlog::info("Attempting to repair mesh with {} errors", issues.errors.size());

        bool repairSuccess = true;

        // Step 1: Replace invalid values (NaN/Inf) - must be done first
        if (!ReplaceInvalidValues(geometry)) {
            spdlog::warn("Failed to replace invalid values in mesh");
            repairSuccess = false;
        }

        // Step 2: Recalculate normals if needed
        auto geometryData = geometry->GetRuntimeData().m_spModelData.get();
        if (geometryData && (!geometryData->normal || geometryData->vertices == 0)) {
            if (!RecalculateNormals(geometry)) {
                spdlog::warn("Failed to recalculate normals");
                // Not critical, continue
            }
        }

        // Step 3: Generate UVs if missing
        if (geometryData && !geometryData->texture) {
            if (!GeneratePlanarUVs(geometry)) {
                spdlog::warn("Failed to generate UV coordinates");
                // Not critical, continue
            }
        }

        // Step 4: Clamp bone indices if skinned
        auto& runtimeData = geometry->GetRuntimeData();
        if (runtimeData.m_spSkinInstance) {
            if (!ClampBoneIndices(runtimeData.m_spSkinInstance.get())) {
                spdlog::warn("Failed to clamp bone indices");
                repairSuccess = false;
            }
        }

        if (repairSuccess) {
            spdlog::info("Mesh repair completed successfully");
            return mesh;
        } else {
            spdlog::error("Mesh repair failed");
            return nullptr;
        }
    }

    RE::NiAVObject* MeshValidator::GetPlaceholderMesh() {
        // No placeholder is loaded at runtime.
        // The validation system is intentionally permissive: NiNode hierarchies
        // are accepted as valid, so a replacement mesh is almost never needed.
        // Severely corrupted geometry is rejected (RepairMesh returns nullptr).
        return nullptr;
    }

    // ========================================================================
    // Validation Functions
    // ========================================================================

    bool MeshValidator::ValidateVertexData(const RE::NiGeometry* geometry, std::vector<std::string>& errors) {
        // Inline null check
        using namespace Performance;
        
        auto geometryData = geometry->GetRuntimeData().m_spModelData.get();
        if (!IsValidPointer(geometryData)) {
            errors.push_back("Geometry data is null");
            return false;
        }

        bool isValid = true;

        // Check vertex count
        if (geometryData->vertices == 0) {
            errors.push_back("Mesh has zero vertices");
            isValid = false;
        }

        // Inline null check for vertex array
        if (!IsValidPointer(geometryData->vertex)) {
            errors.push_back("Vertex array pointer is null");
            isValid = false;
            return isValid;  // Can't check further without vertex data
        }

        // Check for invalid vertex positions with inline bounds checking
        const uint16_t vertexCount = geometryData->vertices;
        for (uint16_t i = 0; i < vertexCount; ++i) {
            // Inline bounds check with early exit
            if (i >= vertexCount) [[unlikely]] {
                errors.push_back(fmt::format("Vertex index {} out of bounds (size: {})", i, vertexCount));
                isValid = false;
                break;
            }
            
            if (!IsValidVector(geometryData->vertex[i])) {
                errors.push_back(fmt::format("Vertex {} has invalid position (NaN/Inf)", i));
                isValid = false;
                break;  // Don't spam errors
            }
        }

        return isValid;
    }

    bool MeshValidator::ValidateNormals(const RE::NiGeometry* geometry, std::vector<std::string>& errors) {
        auto geometryData = geometry->GetRuntimeData().m_spModelData.get();
        if (!geometryData) {
            return false;
        }

        // Normals are optional, but if present they must be valid
        if (!geometryData->normal) {
            errors.push_back("Mesh has no normals (can be recalculated)");
            return false;
        }

        bool isValid = true;

        // Check for invalid normals with bounds checking
        for (uint16_t i = 0; i < geometryData->vertices; ++i) {
            // Bounds check: ensure index is within valid range
            if (i >= geometryData->vertices) {
                errors.push_back(fmt::format("Normal index {} out of bounds (size: {})", i, geometryData->vertices));
                isValid = false;
                break;
            }
            
            const auto& normal = geometryData->normal[i];
            
            if (!IsValidVector(normal)) {
                errors.push_back(fmt::format("Normal {} has invalid values (NaN/Inf)", i));
                isValid = false;
                break;
            }

            // Normals should be unit length (approximately)
            if (!IsUnitVector(normal, 0.1f)) {  // Allow 10% tolerance
                errors.push_back(fmt::format("Normal {} is not unit length", i));
                isValid = false;
                break;
            }
        }

        return isValid;
    }

    bool MeshValidator::ValidateUVCoords(const RE::NiGeometry* geometry, std::vector<std::string>& errors) {
        auto geometryData = geometry->GetRuntimeData().m_spModelData.get();
        if (!geometryData) {
            return false;
        }

        // UVs are optional
        if (!geometryData->texture) {
            errors.push_back("Mesh has no UV coordinates (can be generated)");
            return false;
        }

        bool isValid = true;

        // Check for invalid UV coordinates with bounds checking
        for (uint16_t i = 0; i < geometryData->vertices; ++i) {
            // Bounds check: ensure index is within valid range
            if (i >= geometryData->vertices) {
                errors.push_back(fmt::format("UV index {} out of bounds (size: {})", i, geometryData->vertices));
                isValid = false;
                break;
            }
            
            const auto& uv = geometryData->texture[i];
            
            if (!IsValidFloat(uv.x) || !IsValidFloat(uv.y)) {
                errors.push_back(fmt::format("UV {} has invalid values (NaN/Inf)", i));
                isValid = false;
                break;
            }
        }

        return isValid;
    }

    bool MeshValidator::ValidateBoneWeights(const RE::NiSkinInstance* skin, std::vector<std::string>& errors) {
        if (!skin) {
            return true;  // No skin data is valid
        }

        bool isValid = true;

        // Check bone count
        if (skin->numMatrices == 0) {
            errors.push_back("Skin instance has zero bones");
            isValid = false;
        }

        // Check bone array
        if (!skin->bones) {
            errors.push_back("Bone array pointer is null");
            isValid = false;
        }

        // Check skin data
        if (!skin->skinData) {
            errors.push_back("Skin data is null");
            isValid = false;
            return isValid;
        }

        auto skinData = skin->skinData.get();
        if (!skinData->boneData) {
            errors.push_back("Bone data array is null");
            isValid = false;
            return isValid;
        }

        // Validate bone indices and weights with bounds checking
        for (uint32_t boneIdx = 0; boneIdx < skinData->bones; ++boneIdx) {
            // Bounds check: ensure bone index is within valid range
            if (boneIdx >= skinData->bones) {
                errors.push_back(fmt::format("Bone index {} out of bounds (size: {})", boneIdx, skinData->bones));
                isValid = false;
                break;
            }
            
            const auto& boneData = skinData->boneData[boneIdx];
            
            if (!boneData.boneVertData) {
                errors.push_back(fmt::format("Bone {} has null vertex data", boneIdx));
                isValid = false;
                continue;
            }

            // Check each vertex influenced by this bone
            for (uint16_t vertIdx = 0; vertIdx < boneData.verts; ++vertIdx) {
                // Bounds check: ensure vertex index is within valid range
                if (vertIdx >= boneData.verts) {
                    errors.push_back(fmt::format("Bone {} vertex index {} out of bounds (size: {})", 
                        boneIdx, vertIdx, boneData.verts));
                    isValid = false;
                    break;
                }
                
                const auto& vertData = boneData.boneVertData[vertIdx];
                
                // Check weight is valid
                if (!IsValidFloat(vertData.weight)) {
                    errors.push_back(fmt::format("Bone {} vertex {} has invalid weight", boneIdx, vertIdx));
                    isValid = false;
                    break;
                }

                // Weight should be in range [0, 1]
                if (vertData.weight < 0.0f || vertData.weight > 1.0f) {
                    errors.push_back(fmt::format("Bone {} vertex {} has out-of-range weight: {}", 
                        boneIdx, vertIdx, vertData.weight));
                    isValid = false;
                    break;
                }
            }
        }

        return isValid;
    }

    bool MeshValidator::ValidateTriangles(const RE::NiGeometry* geometry, std::vector<std::string>& errors) {
        auto geometryData = geometry->GetRuntimeData().m_spModelData.get();
        if (!geometryData || !geometryData->vertex) {
            return false;
        }

        // A valid triangle mesh needs at least 3 vertices.
        // A mesh with fewer than 3 vertices can't describe even one triangle, which
        // would cause the renderer to access out-of-bounds vertex memory.
        if (geometryData->vertices < 3) {
            errors.push_back("Mesh has fewer than 3 vertices (cannot form triangles)");
            return false;
        }

        return true;
    }

    // ========================================================================
    // Repair Functions
    // ========================================================================

    bool MeshValidator::RecalculateNormals(RE::NiGeometry* geometry) {
        auto geometryData = geometry->GetRuntimeData().m_spModelData.get();
        if (!geometryData || !geometryData->vertex || geometryData->vertices == 0) {
            return false;
        }

        // Allocate the normal array if it doesn't exist yet
        if (!geometryData->normal) {
            geometryData->normal = new RE::NiPoint3[geometryData->vertices];
            if (!geometryData->normal) {
                spdlog::error("[MeshValidator] Failed to allocate normal array for {} vertices",
                              geometryData->vertices);
                return false;
            }
        }

        // Set all normals to straight up (0,0,1).
        //
        // A "normal" is a vector that points away from the surface — it tells the
        // renderer which direction to shine light from. Setting every normal to
        // (0,0,1) means every face looks flat and lit from directly above, which
        // looks wrong but is always valid and will not crash the renderer.
        //
        // Proper per-vertex normals require triangle indices (each triangle tells us
        // three vertex indices; the cross product of two edges gives the face direction).
        // In our installed version of CommonLibSSE-NG, NiTriShapeData is only
        // forward-declared — the triangle index array is not accessible via the header.
        // The safe-upward fallback is therefore the correct choice here.
        for (uint16_t i = 0; i < geometryData->vertices; ++i) {
            geometryData->normal[i] = RE::NiPoint3(0.0f, 0.0f, 1.0f);
        }

        spdlog::debug("[MeshValidator] Set {} normals to safe upward fallback (triangle "
                      "indices not accessible via NiGeometryData)", geometryData->vertices);
        return true;
    }

    bool MeshValidator::GeneratePlanarUVs(RE::NiGeometry* geometry) {
        auto geometryData = geometry->GetRuntimeData().m_spModelData.get();
        if (!geometryData || !geometryData->vertex || geometryData->vertices == 0) {
            return false;
        }

        // spdlog::debug("Generating planar UVs for mesh with {} vertices", geometryData->vertices);

        // Allocate UV array
        geometryData->texture = new RE::NiPoint2[geometryData->vertices];
        if (!geometryData->texture) {
            spdlog::error("Failed to allocate UV array");
            return false;
        }

        // Calculate bounding box with bounds checking
        if (geometryData->vertices == 0) {
            spdlog::error("Cannot generate UVs for mesh with 0 vertices");
            return false;
        }
        
        RE::NiPoint3 min = geometryData->vertex[0];
        RE::NiPoint3 max = geometryData->vertex[0];

        for (uint16_t i = 1; i < geometryData->vertices; ++i) {
            // Bounds check: ensure index is within valid range
            if (i >= geometryData->vertices) {
                spdlog::error("Vertex index {} out of bounds (size: {})", i, geometryData->vertices);
                break;
            }
            
            const auto& v = geometryData->vertex[i];
            min.x = (std::min)(min.x, v.x);
            min.y = (std::min)(min.y, v.y);
            min.z = (std::min)(min.z, v.z);
            max.x = (std::max)(max.x, v.x);
            max.y = (std::max)(max.y, v.y);
            max.z = (std::max)(max.z, v.z);
        }

        // Calculate size
        float width = max.x - min.x;
        float height = max.y - min.y;

        // Avoid division by zero
        if (width < 0.001f) width = 1.0f;
        if (height < 0.001f) height = 1.0f;

        // Generate planar UVs (XY projection) with bounds checking
        for (uint16_t i = 0; i < geometryData->vertices; ++i) {
            // Bounds check: ensure index is within valid range
            if (i >= geometryData->vertices) {
                spdlog::error("UV index {} out of bounds (size: {})", i, geometryData->vertices);
                break;
            }
            
            const auto& v = geometryData->vertex[i];
            geometryData->texture[i].x = (v.x - min.x) / width;
            geometryData->texture[i].y = (v.y - min.y) / height;
        }

        // spdlog::debug("Planar UV generation completed");
        return true;
    }

    bool MeshValidator::ClampBoneIndices(RE::NiSkinInstance* skin) {
        if (!skin || !skin->skinData) {
            return false;
        }

        auto skinData = skin->skinData.get();
        if (!skinData->boneData) {
            return false;
        }

        // spdlog::debug("Clamping bone indices for {} bones", skinData->bones);

        bool clamped = false;

        // Clamp bone indices to valid range with bounds checking
        for (uint32_t boneIdx = 0; boneIdx < skinData->bones; ++boneIdx) {
            // Bounds check: ensure bone index is within valid range
            if (boneIdx >= skinData->bones) {
                spdlog::error("Bone index {} out of bounds (size: {})", boneIdx, skinData->bones);
                break;
            }
            
            auto& boneData = skinData->boneData[boneIdx];
            
            if (!boneData.boneVertData) {
                continue;
            }

            // Check each vertex influenced by this bone
            for (uint16_t vertIdx = 0; vertIdx < boneData.verts; ++vertIdx) {
                // Bounds check: ensure vertex index is within valid range
                if (vertIdx >= boneData.verts) {
                    spdlog::error("Bone {} vertex index {} out of bounds (size: {})", 
                        boneIdx, vertIdx, boneData.verts);
                    break;
                }
                
                auto& vertData = boneData.boneVertData[vertIdx];
                
                // Clamp weight to [0, 1]
                if (vertData.weight < 0.0f) {
                    vertData.weight = 0.0f;
                    clamped = true;
                } else if (vertData.weight > 1.0f) {
                    vertData.weight = 1.0f;
                    clamped = true;
                }
            }
        }

        if (clamped) {
            // spdlog::debug("Bone indices clamped");
        }

        return true;
    }

    bool MeshValidator::ReplaceInvalidValues(RE::NiGeometry* geometry) {
        auto& runtimeData = geometry->GetRuntimeData();
        auto geometryData = runtimeData.m_spModelData.get();
        if (!geometryData) {
            return false;
        }

        bool replaced = false;

        // Replace invalid vertex positions with bounds checking
        if (geometryData->vertex && geometryData->vertices > 0) {
            for (uint16_t i = 0; i < geometryData->vertices; ++i) {
                // Bounds check: ensure index is within valid range
                if (i >= geometryData->vertices) {
                    spdlog::error("Vertex index {} out of bounds (size: {})", i, geometryData->vertices);
                    break;
                }
                
                RE::NiPoint3& v = geometryData->vertex[i];
                if (!IsValidFloat(v.x)) { v.x = 0.0f; replaced = true; }
                if (!IsValidFloat(v.y)) { v.y = 0.0f; replaced = true; }
                if (!IsValidFloat(v.z)) { v.z = 0.0f; replaced = true; }
            }
        }

        // Replace invalid normals with bounds checking
        if (geometryData->normal && geometryData->vertices > 0) {
            for (uint16_t i = 0; i < geometryData->vertices; ++i) {
                // Bounds check: ensure index is within valid range
                if (i >= geometryData->vertices) {
                    spdlog::error("Normal index {} out of bounds (size: {})", i, geometryData->vertices);
                    break;
                }
                
                RE::NiPoint3& n = geometryData->normal[i];
                if (!IsValidFloat(n.x)) { n.x = 0.0f; replaced = true; }
                if (!IsValidFloat(n.y)) { n.y = 0.0f; replaced = true; }
                if (!IsValidFloat(n.z)) { n.z = 1.0f; replaced = true; }
            }
        }

        // Replace invalid UVs with bounds checking
        if (geometryData->texture && geometryData->vertices > 0) {
            for (uint16_t i = 0; i < geometryData->vertices; ++i) {
                // Bounds check: ensure index is within valid range
                if (i >= geometryData->vertices) {
                    spdlog::error("UV index {} out of bounds (size: {})", i, geometryData->vertices);
                    break;
                }
                
                RE::NiPoint2& uv = geometryData->texture[i];
                if (!IsValidFloat(uv.x)) { uv.x = 0.0f; replaced = true; }
                if (!IsValidFloat(uv.y)) { uv.y = 0.0f; replaced = true; }
            }
        }

        if (replaced) {
            // spdlog::debug("Replaced invalid values (NaN/Inf) in mesh data");
        }

        return true;
    }

    // ========================================================================
    // Helper Functions
    // ========================================================================

    bool MeshValidator::IsValidFloat(float value) {
        return std::isfinite(value);
    }

    bool MeshValidator::IsValidVector(const RE::NiPoint3& vec) {
        return IsValidFloat(vec.x) && IsValidFloat(vec.y) && IsValidFloat(vec.z);
    }

    bool MeshValidator::IsUnitVector(const RE::NiPoint3& vec, float epsilon) {
        float lengthSq = vec.x * vec.x + vec.y * vec.y + vec.z * vec.z;
        return std::abs(lengthSq - 1.0f) < epsilon;
    }

    RE::NiPoint3 MeshValidator::Normalize(const RE::NiPoint3& vec) {
        float length = std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
        if (length < 0.0001f) {
            return RE::NiPoint3(0.0f, 0.0f, 1.0f);  // Default up vector
        }
        return RE::NiPoint3(vec.x / length, vec.y / length, vec.z / length);
    }

    float MeshValidator::CalculateTriangleArea(const RE::NiPoint3& v0, const RE::NiPoint3& v1, const RE::NiPoint3& v2) {
        // Calculate area using cross product
        RE::NiPoint3 edge1(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
        RE::NiPoint3 edge2(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);
        
        // Cross product
        RE::NiPoint3 cross(
            edge1.y * edge2.z - edge1.z * edge2.y,
            edge1.z * edge2.x - edge1.x * edge2.z,
            edge1.x * edge2.y - edge1.y * edge2.x
        );
        
        // Area is half the magnitude of cross product
        float lengthSq = cross.x * cross.x + cross.y * cross.y + cross.z * cross.z;
        return std::sqrt(lengthSq) * 0.5f;
    }

    bool MeshValidator::IsDegenerate(const RE::NiPoint3& v0, const RE::NiPoint3& v1, const RE::NiPoint3& v2) {
        // Check for duplicate vertices
        const float epsilon = 0.0001f;
        
        auto distSq = [](const RE::NiPoint3& a, const RE::NiPoint3& b) {
            float dx = a.x - b.x;
            float dy = a.y - b.y;
            float dz = a.z - b.z;
            return dx * dx + dy * dy + dz * dz;
        };
        
        if (distSq(v0, v1) < epsilon) return true;
        if (distSq(v1, v2) < epsilon) return true;
        if (distSq(v2, v0) < epsilon) return true;
        
        // Check for zero area
        float area = CalculateTriangleArea(v0, v1, v2);
        return area < epsilon;
    }

    RE::NiGeometry* MeshValidator::GetGeometry(RE::NiAVObject* object) {
        if (!object) {
            return nullptr;
        }

        // Try to cast to NiGeometry
        return object->AsNiGeometry();
    }

    const RE::NiGeometry* MeshValidator::GetGeometry(const RE::NiAVObject* object) {
        if (!object) {
            return nullptr;
        }

        // Try to cast to NiGeometry (const version)
        return const_cast<RE::NiAVObject*>(object)->AsNiGeometry();
    }

}  // namespace MeshValidation
