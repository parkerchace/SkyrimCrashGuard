#pragma once
#include <cstdint>
#include <atomic>
#include <RE/Skyrim.h>

namespace CrashGuard {

/// Minimal stub for ActorBudgetManager - provides basic actor counting
/// This is a placeholder until a proper NPC management system is designed
class ActorBudgetManager {
public:
    static ActorBudgetManager& GetSingleton() {
        static ActorBudgetManager instance;
        return instance;
    }

    void Initialize() {
        // Stub - no-op
    }

    void Update(float deltaSeconds) {
        // Stub - just update the count periodically
        m_updateTimer += deltaSeconds;
        if (m_updateTimer >= 2.0f) {
            m_updateTimer = 0.0f;
            UpdateActorCount();
        }
    }

    void ForceAudit() {
        UpdateActorCount();
    }

    uint32_t GetActiveActorCount() const {
        return m_activeActorCount.load();
    }

private:
    ActorBudgetManager() = default;
    ~ActorBudgetManager() = default;
    ActorBudgetManager(const ActorBudgetManager&) = delete;
    ActorBudgetManager& operator=(const ActorBudgetManager&) = delete;

    void UpdateActorCount() {
        uint32_t count = 0;
        
        // Count actors from ProcessLists (high + middle-high priority)
        auto processLists = RE::ProcessLists::GetSingleton();
        if (processLists) {
            // Count high priority actors
            count += static_cast<uint32_t>(processLists->highActorHandles.size());
            
            // Count middle-high priority actors
            for (const auto& handle : processLists->middleHighActorHandles) {
                if (handle && handle.get()) {
                    count++;
                }
            }
        }
        
        m_activeActorCount.store(count);
    }

    std::atomic<uint32_t> m_activeActorCount{0};
    float m_updateTimer{0.0f};
};

} // namespace CrashGuard
