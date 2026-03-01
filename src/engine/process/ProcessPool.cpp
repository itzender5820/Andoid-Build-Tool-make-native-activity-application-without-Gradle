#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <abt/engine/ProcessPool.h>
#include <abt/common/Logger.h>

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>

#include <sstream>
#include <stdexcept>
#include <cstring>
#include <array>
#include <mutex>
#include <condition_variable>
#include <thread>

// Declare the POSIX global environ at file scope (outside any namespace)
extern "C" { extern char** environ; }

namespace abt {

// ── spawnProcess ─────────────────────────────────────────────────────────────
ProcessResult spawnProcess(
    const std::vector<std::string>& argv,
    const std::string& workingDir,
    const std::vector<std::string>& extraEnv)
{
    if (argv.empty()) return {-1, "", "Empty argv"};

    // Pipes for stdout and stderr
    int pipeOut[2], pipeErr[2];
    if (pipe(pipeOut) != 0 || pipe(pipeErr) != 0) {
        return {-1, "", std::string("pipe() failed: ") + strerror(errno)};
    }

    pid_t pid = fork();
    if (pid < 0) {
        return {-1, "", std::string("fork() failed: ") + strerror(errno)};
    }

    if (pid == 0) {
        // ── Child ──
        close(pipeOut[0]); close(pipeErr[0]);
        dup2(pipeOut[1], STDOUT_FILENO);
        dup2(pipeErr[1], STDERR_FILENO);
        close(pipeOut[1]); close(pipeErr[1]);

        if (!workingDir.empty()) {
            if (chdir(workingDir.c_str()) != 0) {
                perror("chdir");
                _exit(127);
            }
        }

        // Build argv[]
        std::vector<char*> cargv;
        cargv.reserve(argv.size() + 1);
        for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
        cargv.push_back(nullptr);

        // Build envp[] — copy existing env and append extras
        std::vector<std::string> envStore;
        std::vector<char*> envp;
        for (char** e = ::environ; *e; ++e) envp.push_back(*e);
        for (const auto& ev : extraEnv) {
            envStore.push_back(ev);
            envp.push_back(const_cast<char*>(envStore.back().c_str()));
        }
        envp.push_back(nullptr);

        // execvpe searches PATH (unlike execve which needs an absolute path).
        // Lets callers use bare names like "zip", "aapt2", "clang++".
        execvpe(cargv[0], cargv.data(), envp.data());
        // execve only returns on error
        perror("execve");
        _exit(127);
    }

    // ── Parent ──
    close(pipeOut[1]); close(pipeErr[1]);

    // Read stdout and stderr
    auto readAll = [](int fd) -> std::string {
        std::string result;
        char buf[4096];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0)
            result.append(buf, n);
        close(fd);
        return result;
    };

    // Read both in threads to avoid deadlock
    std::string outStr, errStr;
    std::thread tOut([&]{ outStr = readAll(pipeOut[0]); });
    std::thread tErr([&]{ errStr = readAll(pipeErr[0]); });
    tOut.join();
    tErr.join();

    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}

    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return {exitCode, std::move(outStr), std::move(errStr)};
}

ProcessResult runCommand(const std::string& cmd, const std::string& cwd) {
    // Simple split on spaces — good enough for internal use
    std::vector<std::string> argv;
    std::istringstream ss(cmd);
    std::string tok;
    while (ss >> tok) argv.push_back(tok);
    if (argv.empty()) return {-1, "", "Empty command"};
    return spawnProcess(argv, cwd);
}

// ── ProcessPool ───────────────────────────────────────────────────────────────
struct ProcessPool::Impl {
    int                     maxProcs;
    int                     current{0};
    std::mutex              mtx;
    std::condition_variable cv;
};

ProcessPool::ProcessPool(int maxProcs)
    : maxProcs_(maxProcs), impl_(new Impl{maxProcs, 0, {}, {}}) {}

ProcessPool::~ProcessPool() { delete impl_; }

void ProcessPool::acquire() {
    std::unique_lock<std::mutex> lk(impl_->mtx);
    impl_->cv.wait(lk, [&]{ return impl_->current < impl_->maxProcs; });
    impl_->current++;
}

void ProcessPool::release() {
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        impl_->current--;
    }
    impl_->cv.notify_one();
}

} // namespace abt
