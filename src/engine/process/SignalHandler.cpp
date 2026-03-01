#include <abt/common/Logger.h>

#include <signal.h>
#include <atomic>
#include <functional>

namespace abt {

static std::atomic<bool> g_interrupted{false};
static std::function<void()> g_cleanupCb;

void signalHandler(int sig) {
    g_interrupted.store(true);
    ABT_WARN("\nInterrupted (signal %d) — cleaning up...", sig);
    if (g_cleanupCb) g_cleanupCb();
}

void installSignalHandlers(std::function<void()> cleanupCb) {
    g_cleanupCb = std::move(cleanupCb);

    struct sigaction sa{};
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND; // restore default after first signal

    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP,  &sa, nullptr);

    // Ignore SIGPIPE — common when a child closes its pipe early
    signal(SIGPIPE, SIG_IGN);
}

bool wasInterrupted() {
    return g_interrupted.load();
}

} // namespace abt
