#include "PCH.h"
#include "BenchmarkManager.h"
#include "Config.h"
#include <spdlog/spdlog.h>
#include <filesystem>

namespace CrashGuard {

    void BenchmarkManager::RegisterAction(const std::string& name, std::function<void()> fn) {
        std::lock_guard l(m_mutex);
        m_actions[name] = fn;
    }

    void BenchmarkManager::RegisterBuiltinActions()
    {
        // NPC management actions (HideNearbyNPCs / RestoreNearbyNPCs) were
        // removed in v2.3.6.  The ActorLOD subsystem they depended on was
        // incomplete and has been fully deleted from the codebase.
        // No built-in actions are registered at this time.
    }

    void BenchmarkManager::ExecuteActionNow(const std::string& name)
    {
        std::lock_guard l(m_mutex);
        auto it = m_actions.find(name);
        if (it != m_actions.end()) {
            try {
                it->second();
                spdlog::info("[Benchmark] Executed action '{}'", name);
            } catch (const std::exception& e) {
                spdlog::error("[Benchmark] Action '{}' threw: {}", name, e.what());
            } catch (...) {
                spdlog::error("[Benchmark] Action '{}' threw (unknown)", name);
            }
        } else {
            spdlog::warn("[Benchmark] Action '{}' not found", name);
        }
    }

    void BenchmarkManager::StartSequence(float baselineSeconds, float actionDelaySeconds, float postActionSeconds, const std::string& actionName) {
        std::lock_guard l(m_mutex);
        m_snapshots.clear();
        m_baselineSeconds = baselineSeconds;
        m_actionDelaySeconds = actionDelaySeconds;
        m_postActionSeconds = postActionSeconds;
        m_actionName = actionName;
        m_phase = 1;
        m_phaseStart = std::chrono::steady_clock::now();
        m_running = true;
        spdlog::info("[Benchmark] Starting sequence: baseline={}s, actionDelay={}s, postAction={}s, action='{}'", baselineSeconds, actionDelaySeconds, postActionSeconds, actionName);
        TakeSnapshot("baseline-start");
    }

    void BenchmarkManager::CancelSequence() {
        std::lock_guard l(m_mutex);
        m_running = false;
        m_phase = 0;
        spdlog::info("[Benchmark] Sequence cancelled");
    }

    void BenchmarkManager::Update() {
        std::lock_guard l(m_mutex);
        if (!m_running) return;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - m_phaseStart).count();

        if (m_phase == 1) {
            // baseline running
            if (elapsed >= m_baselineSeconds) {
                TakeSnapshot("baseline-end");
                // move to actionDelay
                m_phase = 2;
                m_phaseStart = now;
            }
        } else if (m_phase == 2) {
            if (elapsed >= m_actionDelaySeconds) {
                // execute action if registered
                if (!m_actionName.empty()) {
                    auto it = m_actions.find(m_actionName);
                    if (it != m_actions.end()) {
                        spdlog::info("[Benchmark] Executing action '{}'", m_actionName);
                        try { it->second(); } catch (const std::exception& e) { spdlog::error("[Benchmark] Action threw: {}", e.what()); }
                        TakeSnapshot("action-executed");
                    } else {
                        spdlog::warn("[Benchmark] Action '{}' not found", m_actionName);
                        TakeSnapshot("action-missing");
                    }
                }
                m_phase = 3;
                m_phaseStart = now;
            }
        } else if (m_phase == 3) {
            if (elapsed >= m_postActionSeconds) {
                TakeSnapshot("post-action-end");
                m_phase = 4;
                m_running = false;
                spdlog::info("[Benchmark] Sequence complete; {} snapshots taken", m_snapshots.size());
            }
        }
    }

    void BenchmarkManager::StartInteractiveProfile(float durationSeconds) {
        std::lock_guard l(m_mutex);
        m_snapshots.clear();
        m_baselineSeconds = durationSeconds; // repurpose field to hold duration
        m_phase = 10; // interactive mode
        m_phaseStart = std::chrono::steady_clock::now();
        m_running = true;
        spdlog::info("[Benchmark] Interactive profiling started ({}s)", durationSeconds);
        TakeSnapshot("interactive-start");
    }

    float BenchmarkManager::GetInteractiveProgress() const {
        std::lock_guard l(m_mutex);
        if (!m_running || m_phase != 10) return 0.0f;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - m_phaseStart).count();
        return std::clamp(elapsed / m_baselineSeconds, 0.0f, 1.0f);
    }

    bool BenchmarkManager::ExportCSV(const std::filesystem::path& outPath) const {
        std::lock_guard l(m_mutex);
        try {
            std::filesystem::create_directories(outPath.parent_path());
            std::ofstream csv(outPath);
            if (!csv) return false;
            csv << "timestamp,fps,frame_ms,note\n";
            for (const auto& s : m_snapshots) {
                auto tt = std::chrono::system_clock::to_time_t(s.timestamp);
                csv << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M:%S") << ",";
                csv << s.metrics.currentFPS << "," << s.metrics.frameTimeMs << ",";
                csv << '"' << s.note << '"' << "\n";
            }
            csv.close();
            spdlog::info("[Benchmark] Exported {} snapshots to {}", m_snapshots.size(), outPath.string());
            return true;
        } catch (const std::exception& e) {
            spdlog::error("[Benchmark] ExportCSV failed: {}", e.what());
            return false;
        }
    }

    void BenchmarkManager::TakeSnapshot(const std::string& note) {
        BenchmarkSnapshot snap;
        snap.timestamp = std::chrono::system_clock::now();
        snap.metrics = PerformanceMonitor::GetSingleton().GetMetrics();
        snap.currentActors = 0;
        snap.currentParticles = 0;
        // snap.limiterStats and snap.lodStats removed (old systems)
        snap.note = note;
        m_snapshots.push_back(std::move(snap));
        spdlog::info("[Benchmark] Snapshot '{}' taken: FPS={:.1f}", note, m_snapshots.back().metrics.currentFPS);
    }

    std::string BenchmarkManager::GetStatus() const {
        if (m_running) {
            switch (m_phase) {
                case 1: return "Baseline running";
                case 2: return "Waiting to execute action";
                case 3: return "Post-action collection";
                default: return "Running";
            }
        }
        return "Idle";
    }

} // namespace CrashGuard
