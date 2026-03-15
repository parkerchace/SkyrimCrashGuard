// Simple benchmark manager to automate timed metric snapshots
#pragma once
#include "PerformanceMetrics.h"
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <mutex>

namespace CrashGuard {

    struct BenchmarkSnapshot {
        std::chrono::system_clock::time_point timestamp;
        PerformanceMetrics metrics;
        uint32_t currentActors = 0;
        uint32_t currentParticles = 0;
        // limiterStats and lodStats removed (old systems)
        std::string note;
    };

    class BenchmarkManager {
    public:
        static BenchmarkManager& GetSingleton() {
            static BenchmarkManager instance;
            return instance;
        }

        void Initialize() {}

        // Register a named action that can be invoked during a sequence
        void RegisterAction(const std::string& name, std::function<void()> fn);

        // Register safe built-in actions (Hide/Restore nearby NPCs, etc.)
        void RegisterBuiltinActions();
        // Execute a registered action immediately on the calling thread (main thread)
        void ExecuteActionNow(const std::string& name);

        // Start an automated sequence: baseline -> wait -> action -> wait -> finish
        void StartSequence(float baselineSeconds, float actionDelaySeconds, float postActionSeconds, const std::string& actionName);
        void CancelSequence();

        // Start an interactive profiling session that runs for durationSeconds and
        // collects per-second snapshots. Provides progress and export.
        void StartInteractiveProfile(float durationSeconds);
        float GetInteractiveProgress() const; // 0.0-1.0
        bool IsInteractiveRunning() const { return m_running && m_phase == 10; }
        bool ExportCSV(const std::filesystem::path& outPath) const;

        // Progress state machine; must be called regularly from the main thread (e.g., ImGui render)
        void Update();

        // Access results
        const std::vector<BenchmarkSnapshot>& GetSnapshots() const { return m_snapshots; }
        bool IsRunning() const { return m_running; }
        std::string GetStatus() const;

    private:
        BenchmarkManager() = default;

        void TakeSnapshot(const std::string& note = "");

        mutable std::mutex m_mutex;
        std::vector<BenchmarkSnapshot> m_snapshots;
        std::unordered_map<std::string, std::function<void()>> m_actions;

        // Sequence state
        bool m_running = false;
        float m_baselineSeconds = 5.0f;
        float m_actionDelaySeconds = 1.0f;
        float m_postActionSeconds = 10.0f;
        std::string m_actionName;
        std::chrono::steady_clock::time_point m_phaseStart;
        int m_phase = 0; // 0=idle,1=baseline,2=actionDelay,3=postAction,4=done
    };

} // namespace CrashGuard
