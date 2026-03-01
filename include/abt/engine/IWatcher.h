#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace abt {

enum class WatchEvent { MODIFIED, CREATED, DELETED };

struct FileEvent {
    WatchEvent  type;
    std::string path;
};

using WatchCallback = std::function<void(const FileEvent&)>;

// ── Platform-independent watcher interface ───────────────────────────────────
class IWatcher {
public:
    virtual ~IWatcher() = default;

    // Watch a directory recursively
    virtual bool addWatch(const std::string& dir) = 0;

    // Start the event loop in a background thread.
    // Calls cb on the watcher thread — cb must be thread-safe.
    virtual void start(WatchCallback cb) = 0;

    // Stop and join the background thread
    virtual void stop() = 0;
};

// Factory — returns platform-specific implementation
std::unique_ptr<IWatcher> createWatcher();

} // namespace abt
