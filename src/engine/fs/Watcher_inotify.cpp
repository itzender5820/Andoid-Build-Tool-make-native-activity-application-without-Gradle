#include <abt/engine/IWatcher.h>
#include <abt/common/Logger.h>

#include <sys/inotify.h>
#include <sys/select.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <stdexcept>
#include <cstring>

namespace abt {

class InotifyWatcher : public IWatcher {
public:
    InotifyWatcher() {
        fd_ = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
        if (fd_ < 0)
            throw std::runtime_error(std::string("inotify_init1: ") + strerror(errno));

        // Self-pipe for stopping the event loop
        if (pipe(stopPipe_) != 0)
            throw std::runtime_error("pipe() failed");
    }

    ~InotifyWatcher() override {
        stop();
        close(fd_);
        close(stopPipe_[0]);
        close(stopPipe_[1]);
    }

    bool addWatch(const std::string& dir) override {
        // Watch the directory itself
        int wd = inotify_add_watch(fd_, dir.c_str(),
            IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
        if (wd < 0) {
            ABT_WARN("inotify_add_watch(%s): %s", dir.c_str(), strerror(errno));
            return false;
        }
        {
            std::lock_guard<std::mutex> lk(mapMtx_);
            wdToDir_[wd] = dir;
        }

        // Recurse into subdirectories
        DIR* d = opendir(dir.c_str());
        if (!d) return true;
        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr) {
            if (ent->d_name[0] == '.') continue;
            if (ent->d_type == DT_DIR) {
                addWatch(dir + "/" + ent->d_name);
            }
        }
        closedir(d);
        return true;
    }

    void start(WatchCallback cb) override {
        if (running_.load()) return;
        running_.store(true);
        cb_ = std::move(cb);

        thread_ = std::thread([this]() { eventLoop(); });
    }

    void stop() override {
        if (!running_.load()) return;
        running_.store(false);
        // Unblock the select() via the self-pipe
        char b = 1;
        write(stopPipe_[1], &b, 1);
        if (thread_.joinable()) thread_.join();
    }

private:
    int                              fd_;
    int                              stopPipe_[2];
    std::atomic<bool>                running_{false};
    WatchCallback                    cb_;
    std::thread                      thread_;
    std::unordered_map<int, std::string> wdToDir_;
    std::mutex                       mapMtx_;

    void eventLoop() {
        constexpr size_t BUF_SIZE = sizeof(inotify_event) + NAME_MAX + 1;
        alignas(inotify_event) char buf[BUF_SIZE * 16];

        while (running_.load()) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(fd_, &rfds);
            FD_SET(stopPipe_[0], &rfds);
            int maxFd = std::max(fd_, stopPipe_[0]) + 1;

            struct timeval tv{1, 0}; // 1s timeout
            int ret = select(maxFd, &rfds, nullptr, nullptr, &tv);
            if (ret <= 0) continue;

            if (FD_ISSET(stopPipe_[0], &rfds)) break;
            if (!FD_ISSET(fd_, &rfds)) continue;

            ssize_t len = read(fd_, buf, sizeof(buf));
            if (len < 0) continue;

            const char* ptr = buf;
            while (ptr < buf + len) {
                const auto* event = reinterpret_cast<const inotify_event*>(ptr);
                ptr += sizeof(inotify_event) + event->len;

                std::string dirPath;
                {
                    std::lock_guard<std::mutex> lk(mapMtx_);
                    auto it = wdToDir_.find(event->wd);
                    if (it == wdToDir_.end()) continue;
                    dirPath = it->second;
                }

                std::string fileName = (event->len > 0) ? event->name : "";
                std::string fullPath = dirPath + "/" + fileName;

                WatchEvent wev;
                if (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO))
                    wev = WatchEvent::MODIFIED;
                else if (event->mask & IN_CREATE)
                    wev = WatchEvent::CREATED;
                else if (event->mask & (IN_DELETE | IN_MOVED_FROM))
                    wev = WatchEvent::DELETED;
                else
                    continue;

                // If a new directory was created, watch it too
                if ((event->mask & IN_CREATE) && (event->mask & IN_ISDIR)) {
                    addWatch(fullPath);
                }

                ABT_TRACE("inotify: %s [%s]",
                    fullPath.c_str(),
                    wev == WatchEvent::MODIFIED ? "MODIFIED" :
                    wev == WatchEvent::CREATED  ? "CREATED"  : "DELETED");

                if (cb_) cb_(FileEvent{wev, fullPath});
            }
        }
    }
};

// ── Factory ───────────────────────────────────────────────────────────────────
std::unique_ptr<IWatcher> createWatcher() {
    return std::make_unique<InotifyWatcher>();
}

} // namespace abt
