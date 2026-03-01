#pragma once

#include <abt/engine/BuildGraph.h>
#include <abt/common/Types.h>
#include <functional>
#include <atomic>

namespace abt {

struct SchedulerStats {
    int total   = 0;
    int skipped = 0;
    int ran     = 0;
    int failed  = 0;
    double wallTimeMs = 0.0;
};

// Progress callback: (label, state)
using ProgressCb = std::function<void(const std::string&, Task::State)>;

// ── Executes the sorted DAG respecting dependency order ──────────────────────
// Uses a thread pool sized to std::thread::hardware_concurrency().
// Tasks with all dependencies DONE are dispatched immediately.
class TaskScheduler {
public:
    explicit TaskScheduler(BuildGraph& graph, int parallelism = 0);
    ~TaskScheduler();

    // Run all tasks. Stops on first failure unless continueOnError is true.
    Result run(bool continueOnError = false);

    void setProgressCallback(ProgressCb cb) { progressCb_ = std::move(cb); }

    const SchedulerStats& stats() const { return stats_; }

private:
    BuildGraph& graph_;
    int         parallelism_;
    ProgressCb  progressCb_;
    SchedulerStats stats_;

    void workerLoop(
        std::atomic<int>& pendingCount,
        std::atomic<bool>& hasFailed,
        bool continueOnError
    );
};

} // namespace abt
