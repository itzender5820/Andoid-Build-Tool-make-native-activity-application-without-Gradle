#include <abt/engine/BuildGraph.h>
#include <abt/common/Logger.h>
#include <stdexcept>

namespace abt {

TaskId BuildGraph::addTask(TaskType type, const std::string& label) {
    TaskId id = nextId_++;
    tasks_[id] = std::make_unique<Task>(id, type, label);
    return id;
}

Task* BuildGraph::getTask(TaskId id) {
    auto it = tasks_.find(id);
    return (it != tasks_.end()) ? it->second.get() : nullptr;
}

const Task* BuildGraph::getTask(TaskId id) const {
    auto it = tasks_.find(id);
    return (it != tasks_.end()) ? it->second.get() : nullptr;
}

Result BuildGraph::addEdge(TaskId dependent, TaskId dependency) {
    if (!getTask(dependent))
        return Result::fail(AbtError::UNKNOWN,
            "addEdge: unknown dependent task " + std::to_string(dependent));
    if (!getTask(dependency))
        return Result::fail(AbtError::UNKNOWN,
            "addEdge: unknown dependency task " + std::to_string(dependency));

    getTask(dependent)->dependencies.push_back(dependency);
    return Result::success();
}

bool BuildGraph::dfsVisit(TaskId id,
                          std::unordered_set<TaskId>& visited,
                          std::unordered_set<TaskId>& inStack,
                          std::vector<TaskId>& order)
{
    if (inStack.count(id)) return false; // cycle
    if (visited.count(id)) return true;  // already processed

    inStack.insert(id);
    visited.insert(id);

    const Task* t = getTask(id);
    for (TaskId dep : t->dependencies) {
        if (!dfsVisit(dep, visited, inStack, order))
            return false;
    }

    inStack.erase(id);
    order.push_back(id);
    return true;
}

Result BuildGraph::topologicalSort() {
    sorted_.clear();
    sorted_.reserve(tasks_.size());

    std::unordered_set<TaskId> visited;
    std::unordered_set<TaskId> inStack;
    std::vector<TaskId> order;

    for (auto& [id, task] : tasks_) {
        if (!visited.count(id)) {
            if (!dfsVisit(id, visited, inStack, order)) {
                return Result::fail(AbtError::CYCLE_DETECTED,
                    "Cycle detected in build graph involving task: " + task->label);
            }
        }
    }

    sorted_ = std::move(order);
    ABT_INFO("BuildGraph: %zu tasks sorted", sorted_.size());
    return Result::success();
}

void BuildGraph::reset() {
    tasks_.clear();
    sorted_.clear();
    nextId_ = 1;
}

} // namespace abt
