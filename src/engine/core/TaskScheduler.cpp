#include <abt/engine/TaskScheduler.h>
#include <abt/common/Logger.h>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <unordered_set>

namespace abt {

TaskScheduler::TaskScheduler(BuildGraph& graph, int parallelism)
    : graph_(graph), parallelism_(parallelism)
{
    if (parallelism_ <= 0)
        parallelism_ = static_cast<int>(std::thread::hardware_concurrency());
    if (parallelism_ < 1) parallelism_ = 1;
}

TaskScheduler::~TaskScheduler() = default;

Result TaskScheduler::run(bool continueOnError) {
    auto& sorted = graph_.sorted();
    if (sorted.empty()) return Result::success();

    auto t0 = std::chrono::steady_clock::now();

    stats_ = {};
    stats_.total = static_cast<int>(sorted.size());

    std::mutex               mtx;
    std::condition_variable  cv;
    std::atomic<int>         pendingCount{static_cast<int>(sorted.size())};
    std::atomic<bool>        hasFailed{false};

    // Set of completed task IDs (for dependency checking)
    std::unordered_set<TaskId> done;
    // Queue of tasks ready to run
    std::queue<Task*> readyQueue;

    // Find initially ready tasks (no dependencies)
    {
        std::lock_guard<std::mutex> lk(mtx);
        for (TaskId id : sorted) {
            Task* t = graph_.getTask(id);
            if (t->dependencies.empty()) {
                readyQueue.push(t);
            }
        }
    }

    // Worker lambda
    auto dispatchReady = [&]() {
        // After a task finishes, check which tasks are now unblocked
        std::lock_guard<std::mutex> lk(mtx);
        for (TaskId id : sorted) {
            Task* t = graph_.getTask(id);
            if (t->state.load() != Task::State::PENDING) continue;
            bool ready = true;
            for (TaskId dep : t->dependencies) {
                Task* depTask = graph_.getTask(dep);
                auto s = depTask->state.load();
                if (s != Task::State::DONE && s != Task::State::SKIPPED) {
                    ready = false;
                    break;
                }
            }
            if (ready) readyQueue.push(t);
        }
        cv.notify_all();
    };

    std::vector<std::thread> threads;
    threads.reserve(parallelism_);

    for (int i = 0; i < parallelism_; ++i) {
        threads.emplace_back([&]() {
            while (true) {
                Task* task = nullptr;
                {
                    std::unique_lock<std::mutex> lk(mtx);
                    cv.wait(lk, [&]{
                        return !readyQueue.empty()
                            || pendingCount.load() == 0
                            || (hasFailed.load() && !continueOnError);
                    });

                    if (pendingCount.load() == 0) return;
                    if (hasFailed.load() && !continueOnError) return;
                    if (readyQueue.empty()) continue;

                    task = readyQueue.front();
                    readyQueue.pop();
                    // Guard against duplicate dispatch
                    Task::State expected = Task::State::PENDING;
                    if (!task->state.compare_exchange_strong(expected, Task::State::RUNNING))
                        continue;
                }

                if (progressCb_) progressCb_(task->label, Task::State::RUNNING);

                Result r = task->action ? task->action() : Result::success();

                if (r.ok()) {
                    task->state.store(Task::State::DONE);
                    stats_.ran++;
                    ABT_INFO("  ✓  %s", task->label.c_str());
                    if (progressCb_) progressCb_(task->label, Task::State::DONE);
                } else {
                    task->state.store(Task::State::FAILED);
                    stats_.failed++;
                    ABT_ERR("  ✗  %s: %s", task->label.c_str(), r.msg.c_str());
                    hasFailed.store(true);
                    if (progressCb_) progressCb_(task->label, Task::State::FAILED);
                }

                pendingCount.fetch_sub(1);
                if (pendingCount.load() == 0) {
                    cv.notify_all();
                    return;
                }
                // Unlock and unblock newly ready tasks
                dispatchReady();
            }
        });
    }

    for (auto& th : threads) th.join();

    auto t1 = std::chrono::steady_clock::now();
    stats_.wallTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    if (hasFailed.load() && !continueOnError) {
        return Result::fail(AbtError::COMPILE_FAILED,
            std::to_string(stats_.failed) + " task(s) failed");
    }
    return Result::success();
}

} // namespace abt
