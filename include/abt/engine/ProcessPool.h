#pragma once

#include <abt/common/Types.h>
#include <string>
#include <vector>

namespace abt {

struct ProcessResult {
    int         exitCode = 0;
    std::string stdoutStr;
    std::string stderrStr;

    bool ok() const { return exitCode == 0; }
};

// ── Spawn a subprocess via fork+execve ──────────────────────────────────────
// argv[0] must be the full path to the executable.
// env is overlaid on top of the current environment.
ProcessResult spawnProcess(
    const std::vector<std::string>& argv,
    const std::string& workingDir = "",
    const std::vector<std::string>& extraEnv = {}
);

// Convenience: build argv from a shell-style command line (splits on spaces,
// does NOT handle quotes — use spawnProcess for complex args)
ProcessResult runCommand(const std::string& cmd, const std::string& cwd = "");

// ── Process Pool ─────────────────────────────────────────────────────────────
// Limits concurrent child processes. Used by the TaskScheduler.
class ProcessPool {
public:
    explicit ProcessPool(int maxProcs);
    ~ProcessPool();

    // Acquire a slot (blocks if at capacity)
    void acquire();
    // Release a slot
    void release();

    int capacity() const { return maxProcs_; }

private:
    int                  maxProcs_;
    // POSIX semaphore or condition variable
    struct Impl;
    Impl* impl_;
};

} // namespace abt
