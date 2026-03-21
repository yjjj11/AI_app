#ifndef MEMORY_INDEX_SERVICE_H
#define MEMORY_INDEX_SERVICE_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

class MemoryIndexService {
public:
    static MemoryIndexService& instance();

    void init();
    void stop();
    void maybeEnqueue(const std::string& username, const std::string& session_id);

private:
    MemoryIndexService() = default;
    ~MemoryIndexService();

    void workerLoop();

    std::atomic<bool> inited_{false};
    std::atomic<bool> stop_{false};
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<std::pair<std::string, std::string>> q_;
    std::thread worker_;
};

#endif
