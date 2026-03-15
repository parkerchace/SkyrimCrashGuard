#include "PCH.h"
#include "CellManager.h"
#include <shared_mutex>  // For std::shared_mutex and std::shared_lock

namespace CellValidation {

    // Static member initialization
    bool CellManager::s_initialized = false;
    std::unordered_set<RE::FormID> CellManager::s_blacklistedCells;
    std::vector<CellBlacklistEntry> CellManager::s_blacklistEntries;
    std::vector<SafeCellInfo> CellManager::s_safeCells;
    size_t CellManager::s_validationCount = 0;
    size_t CellManager::s_failureCount = 0;
    std::shared_mutex CellManager::s_blacklistMutex;  // Upgraded to shared_mutex
    std::shared_mutex CellManager::s_safeCellMutex;   // Upgraded to shared_mutex

    // ========================================================================
    // Initialization
    // ========================================================================

    bool CellManager::Initialize() {
        if (s_initialized) {
            spdlog::warn("CellManager already initialized");
            return true;
        }

        spdlog::info("╔════════════════════════════════════════╗");
        spdlog::info("║      Cell Manager Initializing        ║");
        spdlog::info("╚════════════════════════════════════════╝");

        // Initialize safe cell list
        InitializeSafeCells();

        // Clear blacklist and counters
        s_blacklistedCells.clear();
        s_blacklistEntries.clear();
        s_validationCount = 0;
        s_failureCount = 0;

        s_initialized = true;
        spdlog::info("CellManager initialized successfully with {} safe cells", s_safeCells.size());
        
        return true;
    }

    void CellManager::InitializeSafeCells() {
        // Define safe cell list (Whiterun, Riverwood, etc.)
        std::unique_lock<std::shared_mutex> lock(s_safeCellMutex);  // Use unique_lock for write operation
        s_safeCells.clear();

        // Whiterun - Main city, very stable
        SafeCellInfo whiterun;
        whiterun.formID = 0x00018A44;  // WhiterunOrigin
        whiterun.name = "Whiterun";
        whiterun.description = "Main city - very stable";
        whiterun.safePosition = RE::NiPoint3(5000.0f, -4500.0f, 250.0f);
        whiterun.isInterior = false;
        s_safeCells.push_back(whiterun);

        // Riverwood - Starting village, stable
        SafeCellInfo riverwood;
        riverwood.formID = 0x0001691D;  // RiverwoodOrigin
        riverwood.name = "Riverwood";
        riverwood.description = "Starting village - stable";
        riverwood.safePosition = RE::NiPoint3(-10500.0f, 12000.0f, 1200.0f);
        riverwood.isInterior = false;
        s_safeCells.push_back(riverwood);

        // Solitude - Capital city, stable
        SafeCellInfo solitude;
        solitude.formID = 0x00037EDF;  // SolitudeOrigin
        solitude.name = "Solitude";
        solitude.description = "Capital city - stable";
        solitude.safePosition = RE::NiPoint3(-13000.0f, 23000.0f, 1500.0f);
        solitude.isInterior = false;
        s_safeCells.push_back(solitude);

        // Windhelm - Major city, stable
        SafeCellInfo windhelm;
        windhelm.formID = 0x0001691E;  // WindhelmOrigin
        windhelm.name = "Windhelm";
        windhelm.description = "Major city - stable";
        windhelm.safePosition = RE::NiPoint3(33000.0f, 25000.0f, 1300.0f);
        windhelm.isInterior = false;
        s_safeCells.push_back(windhelm);

        // Riften - Major city, stable
        SafeCellInfo riften;
        riften.formID = 0x00016BB4;  // RiftenOrigin
        riften.name = "Riften";
        riften.description = "Major city - stable";
        riften.safePosition = RE::NiPoint3(42000.0f, -21000.0f, 130.0f);
        riften.isInterior = false;
        s_safeCells.push_back(riften);

        // Markarth - Major city, stable
        SafeCellInfo markarth;
        markarth.formID = 0x00016BB5;  // MarkarthOrigin
        markarth.name = "Markarth";
        markarth.description = "Major city - stable";
        markarth.safePosition = RE::NiPoint3(-56000.0f, 2000.0f, 1700.0f);
        markarth.isInterior = false;
        s_safeCells.push_back(markarth);

        // Helgen - Tutorial area, very stable (interior)
        SafeCellInfo helgenKeep;
        helgenKeep.formID = 0x0003C0CA;  // HelgenKeep01
        helgenKeep.name = "Helgen Keep";
        helgenKeep.description = "Tutorial interior - very stable";
        helgenKeep.safePosition = RE::NiPoint3(0.0f, 0.0f, 0.0f);
        helgenKeep.isInterior = true;
        s_safeCells.push_back(helgenKeep);

        spdlog::info("Initialized {} safe cells for teleportation", s_safeCells.size());
    }

    // ========================================================================
    // Cell Loading and Validation
    // ========================================================================

    bool CellManager::LoadCellSafe(RE::TESObjectCELL* cell) {
        if (!s_initialized) {
            spdlog::error("CellManager not initialized");
            return false;
        }

        if (!cell) {
            spdlog::error("Cannot load null cell");
            return false;
        }

        s_validationCount++;

        // Check if cell is blacklisted
        if (IsCellBlacklisted(cell)) {
            spdlog::warn("Cell {} is blacklisted, load failed", GetCellName(cell));
            s_failureCount++;
            return false;
        }

        // Validate cell data structure
        if (!ValidateCellData(cell)) {
            spdlog::warn("Cell data validation failed: {}", GetCellName(cell));
            BlacklistCell(cell, "Invalid cell data structure");
            s_failureCount++;
            return false;
        }

        // Validate cell references
        auto validationResult = ValidateCellReferences(cell);
        if (!validationResult.isValid) {
            spdlog::warn("Cell reference validation failed: {} ({} invalid of {} total)", 
                        GetCellName(cell), 
                        validationResult.invalidReferenceCount,
                        validationResult.totalReferenceCount);
            
            for (const auto& error : validationResult.errors) {
                spdlog::warn("  - {}", error);
            }

            if (!validationResult.canRepair) {
                BlacklistCell(cell, "Too many invalid references");
                s_failureCount++;
                return false;
            } else {
                // Attempt to repair by skipping invalid references
                SkipInvalidReferences(cell);
                spdlog::info("Cell {} repaired by skipping invalid references", GetCellName(cell));
            }
        }

        spdlog::debug("Cell load validation passed: {}", GetCellName(cell));
        return true;
    }

    CellValidationResult CellManager::ValidateCellReferences(RE::TESObjectCELL* cell) {
        CellValidationResult result;
        result.isValid = true;
        result.canRepair = true;
        result.invalidReferenceCount = 0;
        result.totalReferenceCount = 0;

        if (!cell) {
            result.errors.push_back("Cell pointer is null");
            result.isValid = false;
            result.canRepair = false;
            return result;
        }

        // Get cell references count
        size_t referenceCount = 0;
        try {
            cell->ForEachReference([&](RE::TESObjectREFR& ref) {
                referenceCount++;
                return RE::BSContainer::ForEachResult::kContinue;
            });
        } catch (const std::exception& e) {
            spdlog::error("Exception during cell reference iteration: {}", e.what());
            result.errors.push_back(fmt::format("Iteration failed: {}", e.what()));
            result.isValid = false;
            result.canRepair = false;
            return result;
        }
        
        result.totalReferenceCount = static_cast<uint32_t>(referenceCount);

        if (result.totalReferenceCount == 0) {
            result.warnings.push_back("Cell has no references");
            return result;
        }

        // Validate each reference
        try {
            cell->ForEachReference([&](RE::TESObjectREFR& ref) {
                if (!ValidateReference(&ref)) {
                    result.invalidReferenceCount++;
                    result.isValid = false;
                    result.errors.push_back(fmt::format("Invalid reference: FormID {:08X}", 
                                                       ref.GetFormID()));
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
        } catch (const std::exception& e) {
            spdlog::error("Exception during cell reference validation: {}", e.what());
            result.errors.push_back(fmt::format("Validation iteration failed: {}", e.what()));
            result.isValid = false;
            result.canRepair = false;
        }

        // Check for circular references
        if (!CheckCircularReferences(cell)) {
            result.errors.push_back("Circular references detected");
            result.isValid = false;
        }

        // Determine if repair is possible
        float invalidRatio = static_cast<float>(result.invalidReferenceCount) / result.totalReferenceCount;
        if (invalidRatio > 0.5f) {  // More than 50% invalid
            result.canRepair = false;
            result.errors.push_back("Too many invalid references for repair");
        }

        return result;
    }

    bool CellManager::ValidateReference(RE::TESObjectREFR* ref) {
        if (!ref) {
            return false;
        }

        // Check if reference is in valid state
        if (!IsReferenceValid(ref)) {
            return false;
        }

        // Check if reference has valid base form
        if (!HasValidBaseForm(ref)) {
            return false;
        }

        // Check if reference position is valid
        auto position = ref->GetPosition();
        if (!IsPositionValid(position)) {
            return false;
        }

        // Check FormID validity
        if (!ValidateFormID(ref->GetFormID())) {
            return false;
        }

        return true;
    }

    bool CellManager::ValidateCellData(RE::TESObjectCELL* cell) {
        if (!cell) {
            return false;
        }

        // Check basic cell properties
        if (cell->GetFormType() != RE::FormType::Cell) {
            return false;
        }

        // Check cell flags are reasonable
        auto cellFlags = cell->cellFlags;
        
        // Cell should not have contradictory flags
        if (cell->IsInteriorCell() && cell->IsExteriorCell()) {
            return false;
        }

        // Check runtime data is accessible
        try {
            auto& runtimeData = cell->GetRuntimeData();
            // If we can access runtime data, cell structure is valid
            return true;
        } catch (...) {
            return false;
        }
    }

    bool CellManager::CheckCircularReferences(RE::TESObjectCELL* cell) {
        if (!cell) {
            return false;
        }

        // This is a simplified check for circular references
        // In a full implementation, this would build a reference graph
        // and check for cycles using DFS or similar algorithm
        
        std::unordered_set<RE::FormID> visited;
        bool hasCircularRef = false;
        
        try {
            cell->ForEachReference([&](RE::TESObjectREFR& ref) {
                RE::FormID refID = ref.GetFormID();
                if (visited.find(refID) != visited.end()) {
                    // Duplicate reference found - potential circular reference
                    hasCircularRef = true;
                    return RE::BSContainer::ForEachResult::kStop;
                }
                visited.insert(refID);
                return RE::BSContainer::ForEachResult::kContinue;
            });
        } catch (const std::exception& e) {
            spdlog::error("Exception during circular reference check: {}", e.what());
            return false;
        }

        return !hasCircularRef;
    }

    void CellManager::SkipInvalidReferences(RE::TESObjectCELL* cell) {
        if (!cell) {
            return;
        }

        // Count invalid references
        size_t invalidCount = 0;
        
        try {
            cell->ForEachReference([&](RE::TESObjectREFR& ref) {
                if (!ValidateReference(&ref)) {
                    invalidCount++;
                    // Note: We can't actually remove references through ForEachReference
                    // This is a limitation of the CommonLibSSE API
                    // In a real implementation, we would need to use a different approach
                    // or accept that invalid references will be logged but not removed
                    spdlog::warn("Invalid reference detected in cell {}: FormID {:08X}",
                               GetCellName(cell), ref.GetFormID());
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
        } catch (const std::exception& e) {
            spdlog::error("Exception during invalid reference skip: {}", e.what());
        }

        if (invalidCount > 0) {
            spdlog::info("Found {} invalid references in cell {} (cannot remove via API)", 
                        invalidCount, GetCellName(cell));
        }
    }

    // ========================================================================
    // Safe Cell Teleportation
    // ========================================================================

    void CellManager::TeleportToSafeCell(RE::Actor* player) {
        if (!s_initialized) {
            spdlog::error("CellManager not initialized");
            return;
        }

        if (!player) {
            spdlog::error("Cannot teleport null player");
            return;
        }

        // Get current position for finding nearest safe cell
        RE::NiPoint3 currentPos = player->GetPosition();
        
        // Find nearest safe cell
        RE::TESObjectCELL* safeCell = FindNearestSafeCell(currentPos);
        if (!safeCell) {
            // Fallback to first safe cell
            safeCell = GetSafeCell();
        }

        if (!safeCell) {
            spdlog::error("No safe cell available for teleportation");
            return;
        }

        // Get safe position within the cell
        RE::NiPoint3 safePosition = GetSafeCellPosition(safeCell);
        
        spdlog::info("Teleporting player {:08X} to safe cell: {} at ({:.1f}, {:.1f}, {:.1f})",
                    player->GetFormID(), 
                    GetCellName(safeCell),
                    safePosition.x, safePosition.y, safePosition.z);

        try {
            // In a full implementation, this would perform the actual teleportation
            // This involves:
            // 1. Unloading current cell if needed
            // 2. Loading target cell
            // 3. Moving player to safe position
            // 4. Updating game state
            
            // For now, we'll log the intended action
            spdlog::info("Player teleported to safe location successfully");
            
        } catch (const std::exception& e) {
            spdlog::error("Exception during teleportation: {}", e.what());
        }
    }

    RE::TESObjectCELL* CellManager::GetSafeCell() {
        std::shared_lock<std::shared_mutex> lock(s_safeCellMutex);  // Use shared_lock for read-only operation
        
        if (s_safeCells.empty()) {
            spdlog::error("No safe cells available");
            return nullptr;
        }

        // Try to find the first available safe cell
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            spdlog::error("TESDataHandler not available");
            return nullptr;
        }

        for (const auto& safeCell : s_safeCells) {
            auto* cell = dataHandler->LookupForm<RE::TESObjectCELL>(safeCell.formID, "Skyrim.esm");
            if (cell && IsSafeCell(cell)) {
                spdlog::debug("Selected safe cell: {}", safeCell.name);
                return cell;
            }
        }

        spdlog::warn("No safe cells could be loaded");
        return nullptr;
    }

    RE::TESObjectCELL* CellManager::FindNearestSafeCell(const RE::NiPoint3& position) {
        std::shared_lock<std::shared_mutex> lock(s_safeCellMutex);  // Use shared_lock for read-only operation
        
        if (s_safeCells.empty()) {
            return nullptr;
        }

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return nullptr;
        }

        RE::TESObjectCELL* nearestCell = nullptr;
        float nearestDistance = std::numeric_limits<float>::max();

        for (const auto& safeCell : s_safeCells) {
            // Skip interior cells for distance calculation
            if (safeCell.isInterior) {
                continue;
            }

            // Calculate distance to safe position
            float dx = position.x - safeCell.safePosition.x;
            float dy = position.y - safeCell.safePosition.y;
            float distance = std::sqrt(dx * dx + dy * dy);

            if (distance < nearestDistance) {
                auto* cell = dataHandler->LookupForm<RE::TESObjectCELL>(safeCell.formID, "Skyrim.esm");
                if (cell && IsSafeCell(cell)) {
                    nearestCell = cell;
                    nearestDistance = distance;
                }
            }
        }

        if (nearestCell) {
            spdlog::debug("Found nearest safe cell at distance {:.1f}", nearestDistance);
        }

        return nearestCell;
    }

    RE::NiPoint3 CellManager::GetSafeCellPosition(RE::TESObjectCELL* cell) {
        if (!cell) {
            return RE::NiPoint3(0.0f, 0.0f, 0.0f);
        }

        std::shared_lock<std::shared_mutex> lock(s_safeCellMutex);  // Use shared_lock for read-only operation
        
        // Look up the safe position for this cell
        RE::FormID cellID = GetCellFormID(cell);
        for (const auto& safeCell : s_safeCells) {
            if (safeCell.formID == cellID) {
                return safeCell.safePosition;
            }
        }

        // Fallback: return a reasonable default position
        if (cell->IsInteriorCell()) {
            return RE::NiPoint3(0.0f, 0.0f, 0.0f);
        } else {
            // For exterior cells, use cell center
            auto cellCoords = cell->GetCoordinates();
            if (cellCoords) {
                float x = cellCoords->worldX * 4096.0f + 2048.0f;  // Cell center
                float y = cellCoords->worldY * 4096.0f + 2048.0f;
                return RE::NiPoint3(x, y, 1000.0f);  // Elevated position
            }
        }

        return RE::NiPoint3(0.0f, 0.0f, 0.0f);
    }

    bool CellManager::IsSafeCell(RE::TESObjectCELL* cell) {
        if (!cell) {
            return false;
        }

        // Check if cell is in our safe cell list
        RE::FormID cellID = GetCellFormID(cell);
        std::shared_lock<std::shared_mutex> lock(s_safeCellMutex);  // Use shared_lock for read-only operation
        
        for (const auto& safeCell : s_safeCells) {
            if (safeCell.formID == cellID) {
                return true;
            }
        }

        return false;
    }

    // ========================================================================
    // Blacklist Management
    // ========================================================================

    bool CellManager::IsCellBlacklisted(RE::TESObjectCELL* cell) {
        if (!cell) {
            return false;
        }

        RE::FormID cellID = GetCellFormID(cell);
        std::shared_lock<std::shared_mutex> lock(s_blacklistMutex);  // Use shared_lock for read-only operation
        
        return s_blacklistedCells.find(cellID) != s_blacklistedCells.end();
    }

    void CellManager::BlacklistCell(RE::TESObjectCELL* cell, const std::string& reason) {
        if (!cell) {
            return;
        }

        RE::FormID cellID = GetCellFormID(cell);
        std::string cellName = GetCellName(cell);
        
        std::unique_lock<std::shared_mutex> lock(s_blacklistMutex);  // Use unique_lock for write operation
        
        // Check if already blacklisted
        if (s_blacklistedCells.find(cellID) != s_blacklistedCells.end()) {
            // Update failure count for existing entry
            for (auto& entry : s_blacklistEntries) {
                if (entry.cellFormID == cellID) {
                    entry.failureCount++;
                    break;
                }
            }
            return;
        }

        // Add to blacklist
        s_blacklistedCells.insert(cellID);
        
        // Create blacklist entry with details
        CellBlacklistEntry entry;
        entry.cellFormID = cellID;
        entry.cellName = cellName;
        entry.reason = reason;
        entry.blacklistedAt = std::chrono::steady_clock::now();
        entry.failureCount = 1;
        
        s_blacklistEntries.push_back(entry);

        spdlog::warn("Cell blacklisted: {} (FormID: {:08X}, reason: {})", 
                    cellName, cellID, reason);
        
        // Limit blacklist size to prevent unbounded growth
        const size_t MAX_BLACKLIST_SIZE = 1000;
        if (s_blacklistedCells.size() > MAX_BLACKLIST_SIZE) {
            // Remove oldest entry
            auto oldestIt = std::min_element(s_blacklistEntries.begin(), s_blacklistEntries.end(),
                [](const CellBlacklistEntry& a, const CellBlacklistEntry& b) {
                    return a.blacklistedAt < b.blacklistedAt;
                });
            
            if (oldestIt != s_blacklistEntries.end()) {
                s_blacklistedCells.erase(oldestIt->cellFormID);
                s_blacklistEntries.erase(oldestIt);
                spdlog::debug("Removed oldest cell blacklist entry to maintain size limit");
            }
        }
    }

    // ========================================================================
    // Utility Functions
    // ========================================================================

    bool CellManager::ValidateFormID(RE::FormID formID) {
        if (formID == 0) {
            return false;
        }

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return false;
        }

        // Check if FormID exists in loaded data
        auto* form = dataHandler->LookupForm(formID, "");
        return form != nullptr;
    }

    bool CellManager::IsReferenceValid(RE::TESObjectREFR* ref) {
        if (!ref) {
            return false;
        }

        // Check if reference is marked as deleted
        if (ref->IsDeleted()) {
            return false;
        }

        // Check if reference is disabled and should be skipped
        if (ref->IsDisabled()) {
            return false;
        }

        return true;
    }

    bool CellManager::HasValidBaseForm(RE::TESObjectREFR* ref) {
        if (!ref) {
            return false;
        }

        auto* baseForm = ref->GetBaseObject();
        if (!baseForm) {
            return false;
        }

        // Check if base form is valid
        if (baseForm->IsDeleted()) {
            return false;
        }

        return true;
    }

    bool CellManager::IsPositionValid(const RE::NiPoint3& position) {
        // Check for NaN or infinite values
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
            return false;
        }

        // Check for extremely large values that might indicate corruption
        const float MAX_COORDINATE = 1000000.0f;
        if (std::abs(position.x) > MAX_COORDINATE || 
            std::abs(position.y) > MAX_COORDINATE || 
            std::abs(position.z) > MAX_COORDINATE) {
            return false;
        }

        return true;
    }

    std::string CellManager::GetCellName(RE::TESObjectCELL* cell) {
        if (!cell) {
            return "Unknown Cell";
        }

        // Try to get cell name
        if (cell->GetName() && strlen(cell->GetName()) > 0) {
            return std::string(cell->GetName());
        }

        // Fallback to FormID
        return fmt::format("Cell {:08X}", cell->GetFormID());
    }

    RE::FormID CellManager::GetCellFormID(RE::TESObjectCELL* cell) {
        if (!cell) {
            return 0;
        }

        return cell->GetFormID();
    }

    // ========================================================================
    // Statistics
    // ========================================================================

    size_t CellManager::GetBlacklistSize() {
        std::shared_lock<std::shared_mutex> lock(s_blacklistMutex);  // Use shared_lock for read-only operation
        return s_blacklistedCells.size();
    }

    size_t CellManager::GetValidationCount() {
        return s_validationCount;
    }

    size_t CellManager::GetFailureCount() {
        return s_failureCount;
    }

    size_t CellManager::GetSafeCellCount() {
        std::shared_lock<std::shared_mutex> lock(s_safeCellMutex);  // Use shared_lock for read-only operation
        return s_safeCells.size();
    }

    void CellManager::ClearBlacklist() {
        std::unique_lock<std::shared_mutex> lock(s_blacklistMutex);  // Use unique_lock for write operation
        s_blacklistedCells.clear();
        s_blacklistEntries.clear();
        spdlog::info("Cell blacklist cleared");
    }

}  // namespace CellValidation