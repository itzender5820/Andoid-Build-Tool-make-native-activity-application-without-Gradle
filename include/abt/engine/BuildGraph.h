#pragma once

#include <abt/common/Types.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <atomic>

namespace abt {

// ── A single unit of work ────────────────────────────────────────────────────
struct Task {
    TaskId      id;
    TaskType    type;
    std::string label;                      // Human-readable: "compile Renderer.cpp"

    // Inputs/outputs — used for hash-based invalidation
    std::vector<std::string> inputs;        // Source files or task outputs
    std::vector<std::string> outputs;       // Files this task produces

    // The actual work lambda — set by the builder
    std::function<Result()> action;

    // Runtime state
    enum class State { PENDING, SKIPPED, RUNNING, DONE, FAILED };
    std::atomic<State> state{State::PENDING};

    // For DAG edges
    std::vector<TaskId> dependencies;       // Tasks that MUST complete before this

    Task(TaskId id, TaskType type, std::string label)
        : id(id), type(type), label(std::move(label)) {}

    // Non-copyable, non-movable because of atomic
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&&) = delete;
};

// ── Directed Acyclic Graph of Tasks ─────────────────────────────────────────
class BuildGraph {
public:
    // Add a task — returns its assigned ID
    TaskId addTask(TaskType type, const std::string& label);

    // Get task by ID
    Task* getTask(TaskId id);
    const Task* getTask(TaskId id) const;

    // Add a dependency edge: 'dependent' runs after 'dependency'
    Result addEdge(TaskId dependent, TaskId dependency);

    // Topological sort — populates 'sorted_' or returns CYCLE_DETECTED
    Result topologicalSort();

    // Sorted execution order (valid after topologicalSort())
    const std::vector<TaskId>& sorted() const { return sorted_; }

    // All tasks
    const std::unordered_map<TaskId, std::unique_ptr<Task>>& tasks() const { return tasks_; }

    void reset();   // Clear all tasks (for incremental rebuild)

    size_t taskCount() const { return tasks_.size(); }

private:
    std::unordered_map<TaskId, std::unique_ptr<Task>> tasks_;
    std::vector<TaskId> sorted_;
    TaskId nextId_ = 1;

    // DFS helpers for cycle detection
    bool dfsVisit(TaskId id,
                  std::unordered_set<TaskId>& visited,
                  std::unordered_set<TaskId>& inStack,
                  std::vector<TaskId>& order);
};

} // namespace abt
