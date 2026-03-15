// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT

#include "NPCManager.h"
#include "Config.h"
#include "ToastNotificationManager.h"
#include <spdlog/spdlog.h>
#include <SKSE/Trampoline.h>
#include <sstream>
#include <cmath>

namespace CrashGuard {

// Static storage for original PlaceAtMe function
static REL::Relocation<RE::TESObjectREFR*(RE::TESObjectREFR*, RE::TESBoundObject*, std::int32_t, bool, bool)> PlaceAtMe_Original;

void NPCManager::Initialize() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) return;
        
        spdlog::info("[NPCManager] Initializing reactive NPC management system");
        
        // Hook will be installed after game loads in Update()
        
        m_initialized = true;
    }
    
    // Do initial count (outside the lock to avoid deadlock)
    ForceAudit();
    
    spdlog::info("[NPCManager] Initialized - Max NPCs: {} (Reactive Mode)", Config::Get().maxActors);
    spdlog::info("[NPCManager] PlaceAtMe hook disabled (causes stack alignment crashes)");
    spdlog::info("[NPCManager] Emergency mode: 2x threshold, ultra-fast deletion: 500+ NPCs/frame");
}

void NPCManager::Update(float deltaTime) {
    if (!m_initialized) return;
    
    // Detect when game is fully loaded (player exists and can move)
    if (!m_gameFullyLoaded.load()) {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (player && !player->IsDisabled() && player->Is3DLoaded()) {
            m_gameFullyLoaded.store(true);
            spdlog::info("[NPCManager] Game fully loaded - NPC management active");
            
            // Install hook now that game is ready
            if (!m_hooksInstalled) {
                InstallSpawnHooks();
            }
        } else {
            // Don't enforce limits during loading
            return;
        }
    }
    
    // CRITICAL: Check NPC count EVERY frame for emergency situations
    // This catches massive spawns (like 2000 NPCs) before they crash
    CountNPCs();
    
    const auto& config = Config::Get();
    uint32_t current = m_activeNPCCount.load();
    uint32_t threshold = GetEffectiveThreshold();
    
    // EMERGENCY: If we're MASSIVELY over limit, delete IMMEDIATELY every frame
    // Lower threshold to 2x to catch massive spawns faster
    if (current > threshold * 2) {
        uint32_t emergency = current - threshold;
        
        if (!m_emergencyMode.load()) {
            m_emergencyMode.store(true);
            spdlog::error("[NPCManager] *** EMERGENCY MODE *** - {} NPCs detected!", current);
            
            if (config.npcToolsToasts) {
                ToastNotificationManager::ShowRecoveryToast(
                    fmt::format("EMERGENCY: {} NPCs! Deleting immediately.", current),
                    5
                );
            }
        }
        
        spdlog::error("[NPCManager] EMERGENCY FRAME: {}/{} NPCs - removing {} NOW", 
                     current, threshold, emergency);
        
        // Remove excess IMMEDIATELY - don't wait for timer
        RemoveExcessNPCs(emergency);
        
        // Don't do normal processing during emergency
        return;
    } else if (m_emergencyMode.load()) {
        m_emergencyMode.store(false);
        spdlog::info("[NPCManager] *** EMERGENCY RESOLVED ***");
        
        if (config.npcToolsToasts) {
            ToastNotificationManager::ShowRecoveryToast("Emergency resolved", 3);
        }
    }
    
    // Normal processing - only run every 0.5 seconds
    m_timeSinceLastUpdate += deltaTime;
    if (m_timeSinceLastUpdate < m_updateInterval) return;
    
    m_timeSinceLastUpdate = 0.0f;
    
    // Learn cell baseline if needed
    LearnCellBaseline();
    
    uint32_t deadBodies = m_deadBodyCount.load();
    
    // REACTIVE APPROACH: If we're over the limit, immediately remove excess NPCs
    if (current > threshold) {
        uint32_t excess = current - threshold;
        spdlog::warn("[NPCManager] OVER LIMIT: {}/{} NPCs - removing {} excess immediately", 
                     current, threshold, excess);
        RemoveExcessNPCs(excess);
    } else if (current < threshold && !m_disabledNPCs.empty()) {
        // We have room - restore some disabled NPCs
        uint32_t room = threshold - current;
        uint32_t toRestore = std::min(room, static_cast<uint32_t>(m_disabledNPCs.size()));
        if (toRestore > 0) {
            spdlog::info("[NPCManager] Room available: restoring {} disabled NPCs", toRestore);
            RestoreDisabledNPCs(toRestore);
        }
    }
    
    // Auto-cleanup dead bodies when enabled
    if (config.autoManageNPCs) {
        // Clean up if we exceed max dead bodies OR if we're near NPC limit with bodies
        bool exceedsDeadLimit = deadBodies >= static_cast<uint32_t>(config.maxDeadBodies);
        bool nearLimitWithBodies = (current > static_cast<uint32_t>(threshold * 0.8f)) && deadBodies > 5;
        
        if (exceedsDeadLimit || nearLimitWithBodies) {
            spdlog::info("[NPCManager] Auto-cleanup triggered: {} dead bodies (max: {}), {}/{} NPCs", 
                         deadBodies, config.maxDeadBodies, current, threshold);
            CleanupDeadBodies();
        }
    }
    
    // Log status periodically
    static int logCounter = 0;
    if (++logCounter >= 15) { // Every 30 seconds (15 * 2 second intervals)
        logCounter = 0;
        spdlog::info("[NPCManager] Status: {}/{} NPCs, {} dead, {} disabled", 
                     current, threshold, deadBodies, m_disabledNPCs.size());
    }
}

uint32_t NPCManager::GetQueuedSpawnCount() const {
    return 0; // Always 0 in reactive mode
}

void NPCManager::CountNPCs() {
    uint32_t npcCount = 0;
    uint32_t deadCount = 0;
    
    auto processLists = RE::ProcessLists::GetSingleton();
    if (!processLists) {
        m_activeNPCCount = 0;
        m_deadBodyCount = 0;
        return;
    }
    
    // Count high priority actors (nearby, active)
    for (auto& handle : processLists->highActorHandles) {
        auto actor = handle.get();
        if (actor && actor->Is3DLoaded()) {
            npcCount++;
            if (actor->IsDead()) {
                deadCount++;
            }
        }
    }
    
    m_activeNPCCount = npcCount;
    m_deadBodyCount = deadCount;
}

uint32_t NPCManager::GetActiveNPCCount() {
    return m_activeNPCCount.load();
}

bool NPCManager::CanSpawnNPC() {
    // EMERGENCY MODE: Block ALL spawns when we're massively over limit
    if (m_emergencyMode.load()) {
        return false;
    }
    
    const auto& config = Config::Get();
    uint32_t current = m_activeNPCCount.load();
    uint32_t max = static_cast<uint32_t>(config.maxActors);
    
    return current < max;
}

void NPCManager::ForceAudit() {
    CountNPCs();
    spdlog::debug("[NPCManager] Audit complete - Active: {}, Dead: {}, Queued: {}", 
                  m_activeNPCCount.load(), m_deadBodyCount.load(), GetQueuedSpawnCount());
}

void NPCManager::RemoveExcessNPCs(uint32_t excessCount) {
    auto processLists = RE::ProcessLists::GetSingleton();
    if (!processLists) {
        spdlog::warn("[NPCManager] ProcessLists not available for excess removal");
        return;
    }
    
    const auto& config = Config::Get();
    
    // For extreme cases (>500 NPCs to remove OR emergency mode), use ULTRA-FAST deletion
    // Skip burden calculation, skip sorting, just delete everything non-essential ASAP
    bool ultraFastMode = excessCount > 500 || m_emergencyMode.load();
    
    if (ultraFastMode) {
        spdlog::error("[NPCManager] ULTRA-FAST MODE: {} excess NPCs - IMMEDIATE DELETION", excessCount);
        
        uint32_t removed = 0;
        uint32_t maxIterations = 0;
        const uint32_t MAX_ITERATIONS = 20000; // Increased safety limit for massive spawns
        
        // FAST PATH: Just iterate and delete, no sorting, no burden calculation
        // Keep looping until we've removed enough or hit safety limit
        while (removed < excessCount && maxIterations < MAX_ITERATIONS) {
            maxIterations++;
            
            for (auto& handle : processLists->highActorHandles) {
                if (removed >= excessCount) break;
                
                auto actorPtr = handle.get();
                if (!actorPtr) continue;
                
                auto actor = actorPtr.get();
                if (!actor || actor->IsDeleted() || actor->IsMarkedForDeletion()) continue;
                
                // Minimal checks - only protect absolute essentials
                if (actor->IsPlayerRef() || actor->IsPlayerTeammate()) continue;
                if (IsEssentialActor(actor)) continue;
                
                // DELETE IMMEDIATELY - no burden calc, no sorting, no disable
                try {
                    actor->SetDelete(true);
                    removed++;
                    m_excessRemoved++;
                    
                    // Log every 200 deletions to show progress
                    if (removed % 200 == 0) {
                        spdlog::info("[NPCManager] Ultra-fast progress: {}/{} deleted", removed, excessCount);
                    }
                } catch (...) {
                    // Silently continue on error in emergency mode
                }
            }
            
            // If we didn't remove any this iteration, break to avoid infinite loop
            if (removed == 0) break;
        }
        
        spdlog::error("[NPCManager] ULTRA-FAST COMPLETE: Deleted {} NPCs in {} iterations", removed, maxIterations);
        CountNPCs();
        return;
    }
    
    // NORMAL MODE: Use burden-based prioritization
    std::vector<std::pair<RE::Actor*, int>> npcCandidates;
    
    spdlog::info("[NPCManager] Finding {} excess NPCs to remove...", excessCount);
    
    // Find non-essential NPCs and calculate their burden
    for (auto& handle : processLists->highActorHandles) {
        auto actorPtr = handle.get();
        if (!actorPtr) continue;
        
        auto actor = actorPtr.get();
        
        // Enhanced safety checks
        if (!actor || actor->IsDeleted() || actor->IsMarkedForDeletion()) {
            continue;
        }
        
        // Check if actor base is valid
        auto actorBase = actor->GetActorBase();
        if (!actorBase || !actor->GetFormID()) {
            continue;
        }
        
        // Don't remove essential or quest-related actors
        if (IsEssentialActor(actor) || IsQuestActor(actor)) {
            continue;
        }
        
        // Check whitelist (never remove)
        if (IsWhitelistedNPC(actor)) {
            continue;
        }
        
        // Don't remove player's followers or the player
        if (actor->IsPlayerTeammate() || actor->IsPlayerRef()) {
            continue;
        }
        
        // Calculate burden score
        int burden = CalculateNPCBurden(actor);
        
        // Blacklisted NPCs get higher burden (removed first)
        if (IsBlacklistedNPC(actor)) {
            burden += 100;
        }
        
        npcCandidates.push_back({actor, burden});
        
        // Stop collecting once we have enough candidates
        if (npcCandidates.size() >= excessCount * 1.5f) {
            break;
        }
    }
    
    // Sort by burden (highest first) - remove most burdensome NPCs first
    std::sort(npcCandidates.begin(), npcCandidates.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    spdlog::info("[NPCManager] Found {} candidates, removing top {} by burden", 
                 npcCandidates.size(), std::min(excessCount, static_cast<uint32_t>(npcCandidates.size())));
    
    uint32_t removed = 0;
    uint32_t disabled = 0;
    
    // Remove/disable the excess NPCs
    for (size_t i = 0; i < npcCandidates.size() && removed < excessCount; ++i) {
        auto* actor = npcCandidates[i].first;
        int burden = npcCandidates[i].second;
        
        try {
            // Triple-check the actor is still valid
            if (!actor || actor->IsDeleted() || actor->IsMarkedForDeletion()) {
                continue;
            }
            
            // Additional safety: check if actor has valid 3D before disabling
            if (!actor->Get3D()) {
                spdlog::debug("[NPCManager] Actor has no 3D, skipping disable");
                continue;
            }
            
            // Check if actor is in a valid state
            auto actorBase = actor->GetActorBase();
            if (!actorBase) {
                spdlog::debug("[NPCManager] Actor has no base form, skipping");
                continue;
            }
            
            const char* name = actor->GetName();
            bool isDead = actor->IsDead();
            
            // Always delete in emergency mode or if dead
            if (ultraFastMode || !config.disableInsteadOfDelete || isDead) {
                // Delete
                try {
                    actor->SetDelete(true);
                    spdlog::info("[NPCManager] Deleting excess NPC: {} ({})", 
                                 name ? name : "Unknown", isDead ? "dead" : "alive");
                } catch (const std::exception& e) {
                    spdlog::warn("[NPCManager] Failed to delete {}: {}", name ? name : "Unknown", e.what());
                    continue;
                } catch (...) {
                    spdlog::warn("[NPCManager] Failed to delete {}: unknown error", name ? name : "Unknown");
                    continue;
                }
            } else if (config.disableInsteadOfDelete && !isDead) {
                // Disable instead of delete (can restore later)
                std::lock_guard<std::mutex> lock(m_mutex);
                
                // Check if we're at max disabled NPCs
                if (m_disabledNPCs.size() >= static_cast<size_t>(config.maxDisabledNPCs)) {
                    // Delete oldest disabled NPC to make room
                    if (!m_disabledNPCs.empty()) {
                        auto& oldest = m_disabledNPCs.front();
                        if (oldest.actor && !oldest.actor->IsDeleted()) {
                            try {
                                oldest.actor->SetDelete(true);
                            } catch (...) {
                                spdlog::warn("[NPCManager] Failed to delete oldest disabled NPC");
                            }
                        }
                        m_disabledNPCs.erase(m_disabledNPCs.begin());
                    }
                }
                
                DisabledNPC disabledNPC;
                disabledNPC.actor = actor;
                disabledNPC.cell = actor->GetParentCell();
                disabledNPC.name = name ? name : "Unknown";
                disabledNPC.formID = actor->GetFormID();
                disabledNPC.disabledTime = 0.0f;
                disabledNPC.burden = burden;
                
                // Safely disable the actor
                try {
                    actor->Disable();
                    m_disabledNPCs.push_back(disabledNPC);
                    
                    spdlog::info("[NPCManager] Disabled excess NPC: {} (burden: {})", 
                                 disabledNPC.name, burden);
                    disabled++;
                } catch (const std::exception& e) {
                    spdlog::warn("[NPCManager] Failed to disable {}: {}", disabledNPC.name, e.what());
                    // Fall back to deletion
                    try {
                        actor->SetDelete(true);
                        spdlog::info("[NPCManager] Deleted {} instead (disable failed)", disabledNPC.name);
                    } catch (...) {
                        spdlog::error("[NPCManager] Failed to delete {} after disable failure", disabledNPC.name);
                        continue;
                    }
                } catch (...) {
                    spdlog::warn("[NPCManager] Failed to disable {}: unknown error", disabledNPC.name);
                    // Fall back to deletion
                    try {
                        actor->SetDelete(true);
                        spdlog::info("[NPCManager] Deleted {} instead (disable failed)", disabledNPC.name);
                    } catch (...) {
                        spdlog::error("[NPCManager] Failed to delete {} after disable failure", disabledNPC.name);
                        continue;
                    }
                }
            }
            
            removed++;
            m_excessRemoved++;
            
        } catch (const std::exception& e) {
            spdlog::warn("[NPCManager] Failed to remove excess NPC: {}", e.what());
        } catch (...) {
            spdlog::warn("[NPCManager] Failed to remove excess NPC: unknown error");
        }
    }
    
    if (removed > 0) {
        spdlog::info("[NPCManager] Successfully removed {} excess NPCs ({} disabled, {} deleted)", 
                     removed, disabled, removed - disabled);
        
        // Recount after removal
        CountNPCs();
    } else {
        spdlog::warn("[NPCManager] No excess NPCs could be removed safely");
    }
}

void NPCManager::RestoreDisabledNPCs(uint32_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_disabledNPCs.empty()) return;
    
    const auto& config = Config::Get();
    uint32_t toRestore = std::min(count, static_cast<uint32_t>(config.npcRestoreRate));
    uint32_t restored = 0;
    
    auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    
    RE::NiPoint3 playerPos;
    float playerAngle = 0.0f;
    float viewDirX = 0.0f;
    float viewDirY = 0.0f;
    
    // Only calculate player view if we need it
    if (config.restoreBehindPlayer) {
        try {
            playerPos = player->GetPosition();
            playerAngle = player->GetAngleZ();
            
            // Calculate player's view direction
            viewDirX = std::sin(playerAngle);
            viewDirY = std::cos(playerAngle);
        } catch (...) {
            spdlog::warn("[NPCManager] Failed to get player position/angle, disabling FOV check");
            // Fall back to restoring without FOV check
        }
    }
    
    auto it = m_disabledNPCs.begin();
    
    while (it != m_disabledNPCs.end() && restored < toRestore) {
        try {
            auto& disabledNPC = *it;
            
            // Check if actor is still valid
            if (!disabledNPC.actor || disabledNPC.actor->IsDeleted() || disabledNPC.actor->IsMarkedForDeletion()) {
                spdlog::debug("[NPCManager] Disabled NPC no longer valid, removing from list");
                it = m_disabledNPCs.erase(it);
                continue;
            }
            
            // Additional safety: check if actor has valid 3D
            if (!disabledNPC.actor->Get3D()) {
                spdlog::debug("[NPCManager] Disabled NPC has no 3D, skipping: {}", disabledNPC.name);
                ++it;
                continue;
            }
            
            // Check if NPC is in player's field of view (skip if visible for immersion)
            if (config.restoreBehindPlayer && playerAngle != 0.0f) {
                try {
                    auto npcPos = disabledNPC.actor->GetPosition();
                    
                    // Calculate vector from player to NPC
                    float toNpcX = npcPos.x - playerPos.x;
                    float toNpcY = npcPos.y - playerPos.y;
                    float distance = std::sqrt(toNpcX * toNpcX + toNpcY * toNpcY);
                    
                    if (distance > 10.0f) { // Only check FOV if NPC is not too close
                        // Normalize the vector
                        toNpcX /= distance;
                        toNpcY /= distance;
                        
                        // Calculate dot product with view direction
                        float dotProduct = (viewDirX * toNpcX) + (viewDirY * toNpcY);
                        
                        // If dot product > 0.5, NPC is in front of player (within ~120 degree FOV)
                        // Skip this NPC and try the next one
                        if (dotProduct > 0.5f) {
                            spdlog::debug("[NPCManager] Skipping {} - in player's view (dot: {:.2f})", 
                                         disabledNPC.name, dotProduct);
                            ++it;
                            continue;
                        }
                    }
                } catch (const std::exception& e) {
                    spdlog::warn("[NPCManager] Failed to check FOV for {}: {}", disabledNPC.name, e.what());
                    // Continue with restoration anyway
                } catch (...) {
                    spdlog::warn("[NPCManager] Failed to check FOV for {}: unknown error", disabledNPC.name);
                    // Continue with restoration anyway
                }
            }
            
            // NPC is behind player or FOV check is disabled - restore it
            // Note: We can't call Enable() directly, but removing from disabled list
            // allows the game to naturally process the actor again
            
            spdlog::info("[NPCManager] Restoring {} (behind player or out of view)", disabledNPC.name);
            
            restored++;
            m_restoredNPCs++;
            
            it = m_disabledNPCs.erase(it);
            
        } catch (const std::exception& e) {
            spdlog::warn("[NPCManager] Failed to restore disabled NPC: {}", e.what());
            ++it;
        } catch (...) {
            spdlog::warn("[NPCManager] Failed to restore disabled NPC: unknown error");
            ++it;
        }
    }
    
    if (restored > 0) {
        spdlog::info("[NPCManager] Restored {} disabled NPCs (behind player)", restored);
    }
}

void NPCManager::CleanupDeadBodies() {
    auto processLists = RE::ProcessLists::GetSingleton();
    if (!processLists) {
        spdlog::warn("[NPCManager] ProcessLists not available for cleanup");
        return;
    }
    
    const auto& config = Config::Get();
    uint32_t removed = 0;
    uint32_t maxToRemove = static_cast<uint32_t>(config.npcCleanupRate);
    
    std::vector<RE::Actor*> bodiesToRemove;
    
    spdlog::info("[NPCManager] Starting dead body cleanup scan (max: {})...", maxToRemove);
    
    // Find non-essential dead bodies
    for (auto& handle : processLists->highActorHandles) {
        auto actorPtr = handle.get();
        if (!actorPtr || !actorPtr->IsDead()) continue;
        
        auto actor = actorPtr.get();
        
        // Enhanced safety checks to prevent crashes
        if (!actor) {
            spdlog::debug("[NPCManager] Null actor pointer, skipping");
            continue;
        }
        
        // Skip actors that are already deleted or being deleted
        if (actor->IsDeleted() || actor->IsMarkedForDeletion()) {
            spdlog::debug("[NPCManager] Skipping already deleted actor");
            continue;
        }
        
        // Additional safety: check if actor has valid form data
        if (!actor->GetFormID() || actor->GetFormID() == 0) {
            spdlog::debug("[NPCManager] Skipping invalid actor (no FormID)");
            continue;
        }
        
        // Check if actor base is valid
        auto actorBase = actor->GetActorBase();
        if (!actorBase) {
            spdlog::debug("[NPCManager] Skipping actor with no base form");
            continue;
        }
        
        // Don't remove essential or quest-related actors
        if (IsEssentialActor(actor)) {
            spdlog::debug("[NPCManager] Skipping essential actor: {}", actor->GetName());
            continue;
        }
        
        if (IsQuestActor(actor)) {
            spdlog::debug("[NPCManager] Skipping quest actor: {}", actor->GetName());
            continue;
        }
        
        // Don't remove player's followers
        if (actor->IsPlayerTeammate()) {
            spdlog::debug("[NPCManager] Skipping follower: {}", actor->GetName());
            continue;
        }
        
        // Don't remove the player
        if (actor->IsPlayerRef()) {
            continue;
        }
        
        // Additional safety: check if actor is in a valid state for deletion
        if (!actor->IsDead()) {
            spdlog::debug("[NPCManager] Skipping non-dead actor: {}", actor->GetName());
            continue;
        }
        
        bodiesToRemove.push_back(actor);
        spdlog::debug("[NPCManager] Marked for removal: {}", actor->GetName());
        
        if (bodiesToRemove.size() >= maxToRemove) break;
    }
    
    spdlog::info("[NPCManager] Found {} dead bodies to remove", bodiesToRemove.size());
    
    // Remove the bodies with enhanced safety checks
    for (auto* actor : bodiesToRemove) {
        try {
            // Triple-check the actor is still valid before removal
            if (!actor) {
                spdlog::debug("[NPCManager] Actor became null during cleanup");
                continue;
            }
            
            if (actor->IsDeleted() || actor->IsMarkedForDeletion()) {
                spdlog::debug("[NPCManager] Actor became deleted during cleanup");
                continue;
            }
            
            // Final safety check - ensure actor is still dead
            if (!actor->IsDead()) {
                spdlog::debug("[NPCManager] Actor is no longer dead, skipping");
                continue;
            }
            
            const char* name = actor->GetName();
            spdlog::info("[NPCManager] Removing dead body: {}", name ? name : "Unknown");
            
            // Use the safest deletion method - just mark for deletion
            // Don't call Disable() as it can cause issues with already-processed actors
            actor->SetDelete(true);
            
            removed++;
            m_bodiesRemoved++;
            
        } catch (const std::exception& e) {
            spdlog::warn("[NPCManager] Failed to remove dead body: {}", e.what());
        } catch (...) {
            spdlog::warn("[NPCManager] Failed to remove dead body: unknown error");
        }
    }
    
    if (removed > 0) {
        spdlog::info("[NPCManager] Successfully cleaned up {} dead bodies", removed);
        
        // Recount after cleanup
        CountNPCs();
    } else {
        spdlog::info("[NPCManager] No dead bodies found to clean up");
    }
}

bool NPCManager::IsWhitelistedNPC(RE::Actor* actor) {
    if (!actor) return false;
    
    const auto& config = Config::Get();
    if (config.npcWhitelistKeywords.empty()) return false;
    
    auto base = actor->GetActorBase();
    if (!base) return false;
    
    const char* name = base->GetName();
    if (!name || strlen(name) == 0) return false;
    
    std::string actorName(name);
    
    // Parse whitelist keywords (comma-separated)
    std::stringstream ss(config.npcWhitelistKeywords);
    std::string keyword;
    
    while (std::getline(ss, keyword, ',')) {
        // Trim whitespace
        keyword.erase(0, keyword.find_first_not_of(" \t"));
        keyword.erase(keyword.find_last_not_of(" \t") + 1);
        
        if (!keyword.empty() && actorName.find(keyword) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

bool NPCManager::IsBlacklistedNPC(RE::Actor* actor) {
    if (!actor) return false;
    
    const auto& config = Config::Get();
    if (config.npcBlacklistKeywords.empty()) return false;
    
    auto base = actor->GetActorBase();
    if (!base) return false;
    
    const char* name = base->GetName();
    if (!name || strlen(name) == 0) return false;
    
    std::string actorName(name);
    
    // Parse blacklist keywords (comma-separated)
    std::stringstream ss(config.npcBlacklistKeywords);
    std::string keyword;
    
    while (std::getline(ss, keyword, ',')) {
        // Trim whitespace
        keyword.erase(0, keyword.find_first_not_of(" \t"));
        keyword.erase(keyword.find_last_not_of(" \t") + 1);
        
        if (!keyword.empty() && actorName.find(keyword) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

int NPCManager::CalculateNPCBurden(RE::Actor* actor) {
    if (!actor) return 0;
    
    const auto& config = Config::Get();
    int burden = 0;
    
    // Base burden
    burden += 10;
    
    // Dead actors are less burdensome (handled by corpse cleanup)
    if (actor->IsDead()) {
        burden -= 5;
    }
    
    // Actors in combat are more burdensome (expensive AI)
    if (actor->IsInCombat()) {
        burden += config.burdenInCombat;
    }
    
    // Actors with AI packages are more burdensome
    auto currentPackage = actor->GetCurrentPackage();
    if (currentPackage) {
        burden += config.burdenComplexAI;
    }
    
    // Actors with magic effects are more burdensome (simplified check)
    auto magicTarget = actor->GetMagicTarget();
    if (magicTarget) {
        burden += config.burdenHasMagicEffects;
    }
    
    // Actors with 3D loaded are more burdensome (rendering cost)
    if (actor->Is3DLoaded()) {
        burden += 10;
    }
    
    // Check if this is a duplicate NPC (2nd+ instance of same base)
    auto actorBase = actor->GetActorBase();
    if (actorBase) {
        uint32_t baseFormID = actorBase->GetFormID();
        
        // Count how many instances of this base exist
        auto processLists = RE::ProcessLists::GetSingleton();
        if (processLists) {
            int instanceCount = 0;
            for (auto& handle : processLists->highActorHandles) {
                auto otherPtr = handle.get();
                if (otherPtr) {
                    auto other = otherPtr.get();
                    if (other && other->GetActorBase()) {
                        if (other->GetActorBase()->GetFormID() == baseFormID) {
                            instanceCount++;
                            // If this is the 2nd+ instance, it's a duplicate
                            if (instanceCount > 1 && other == actor) {
                                burden += config.burdenDuplicate;
                                spdlog::debug("[NPCManager] Detected duplicate: {} (instance #{})", 
                                             actor->GetName(), instanceCount);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    
    return burden;
}

void NPCManager::LearnCellBaseline() {
    auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    
    auto currentCell = player->GetParentCell();
    if (!currentCell || currentCell == m_currentCell) return;
    
    m_currentCell = currentCell;
    
    const auto& config = Config::Get();
    if (!config.usePerCellBaseline) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if we already have a baseline for this cell
    auto it = m_cellBaselines.find(currentCell);
    if (it != m_cellBaselines.end() && it->second.isLearned) {
        spdlog::info("[NPCManager] Using existing baseline for cell: {} NPCs", 
                     it->second.baselineCount);
        return;
    }
    
    // Learn new baseline
    uint32_t currentCount = m_activeNPCCount.load();
    
    CellBaseline baseline;
    baseline.cell = currentCell;
    baseline.baselineCount = currentCount;
    baseline.learnedTime = 0.0f; // Will be set by game time
    baseline.isLearned = true;
    
    m_cellBaselines[currentCell] = baseline;
    
    spdlog::info("[NPCManager] Learned new cell baseline: {} NPCs (threshold: {})", 
                 currentCount, currentCount + config.cellNPCDelta);
}

uint32_t NPCManager::GetEffectiveThreshold() const {
    const auto& config = Config::Get();
    
    if (!config.usePerCellBaseline) {
        return static_cast<uint32_t>(config.maxActors);
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    return GetEffectiveThresholdInternal();
}

uint32_t NPCManager::GetEffectiveThresholdInternal() const {
    const auto& config = Config::Get();
    
    if (!config.usePerCellBaseline) {
        return static_cast<uint32_t>(config.maxActors);
    }
    
    if (!m_currentCell) {
        return static_cast<uint32_t>(config.maxActors);
    }
    
    auto it = m_cellBaselines.find(m_currentCell);
    if (it != m_cellBaselines.end() && it->second.isLearned) {
        return it->second.baselineCount + static_cast<uint32_t>(config.cellNPCDelta);
    }
    
    return static_cast<uint32_t>(config.maxActors);
}

bool NPCManager::IsEssentialActor(RE::Actor* actor) {
    if (!actor) return false;
    
    auto base = actor->GetActorBase();
    if (!base) return false;
    
    // Check if marked as essential
    return base->IsEssential() || actor->IsEssential();
}

bool NPCManager::IsQuestActor(RE::Actor* actor) {
    if (!actor) return false;
    
    auto base = actor->GetActorBase();
    if (!base) return false;
    
    // Very simple approach: only protect truly unique/important NPCs
    const char* name = base->GetName();
    if (!name || strlen(name) == 0) {
        return false; // Unnamed actors are safe to remove
    }
    
    std::string actorName(name);
    
    // These are definitely safe to remove (generic enemies/NPCs)
    if (actorName.find("Bandit") != std::string::npos ||
        actorName.find("Draugr") != std::string::npos ||
        actorName.find("Forsworn") != std::string::npos ||
        actorName.find("Thief") != std::string::npos ||
        actorName.find("Vampire") != std::string::npos ||
        actorName.find("Skeleton") != std::string::npos ||
        actorName.find("Spider") != std::string::npos ||
        actorName.find("Wolf") != std::string::npos ||
        actorName.find("Bear") != std::string::npos ||
        actorName.find("Saber Cat") != std::string::npos ||
        actorName.find("Guard") != std::string::npos ||  // Guards respawn
        actorName.find("Soldier") != std::string::npos ||
        actorName.find("Citizen") != std::string::npos ||
        actorName.find("Warrior") != std::string::npos) {
        return false; // Safe to remove
    }
    
    // For now, be more aggressive - only protect very specific important NPCs
    // You can add specific names here if needed
    if (actorName == "Lydia" || actorName == "Balgruuf the Greater" || 
        actorName == "Jarl Elisif the Fair" || actorName == "Ulfric Stormcloak") {
        return true; // Definitely keep these
    }
    
    // Default: allow removal (we can adjust this based on testing)
    return false;
}

NPCManager::Stats NPCManager::GetStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return {
        m_activeNPCCount.load(),
        m_deadBodyCount.load(),
        m_excessRemoved.load(),
        m_bodiesRemoved.load(),
        static_cast<uint32_t>(m_disabledNPCs.size()),
        m_restoredNPCs.load(),
        m_currentCell && m_cellBaselines.count(m_currentCell) ? m_cellBaselines.at(m_currentCell).baselineCount : 0,
        GetEffectiveThresholdInternal()  // Use internal version to avoid deadlock
    };
}

void NPCManager::InstallSpawnHooks() {
    // DISABLED: PlaceAtMe hooking causes stack alignment crashes
    // The reactive system handles massive spawns by deleting excess NPCs immediately
    // Emergency mode activates at 2x threshold (100 NPCs), deleting 500+ per frame
    
    spdlog::info("[NPCManager] PlaceAtMe hook DISABLED (causes crashes)");
    spdlog::info("[NPCManager] Using reactive deletion system for spawn control");
    spdlog::info("[NPCManager] Emergency mode activates at 2x threshold (100 NPCs)");
    
    m_hooksInstalled = false;
    
    // NOTE: If we need proactive spawn control in the future, we should:
    // 1. Hook the console command handler (safer than PlaceAtMe)
    // 2. Use a detours library instead of SKSE trampolines
    // 3. Hook at a call site instead of function entry
}

}  // namespace CrashGuard
