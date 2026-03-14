#ifndef CHAT_MYSQL_STORE_H
#define CHAT_MYSQL_STORE_H

#include "service/ChatDbModels.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>

class ChatMySqlStore {
public:
    static ChatMySqlStore& instance();

    void init();
    void enqueue(chat_message_row task);

private:
    ChatMySqlStore();
    ~ChatMySqlStore();
    ChatMySqlStore(const ChatMySqlStore&) = delete;
    ChatMySqlStore& operator=(const ChatMySqlStore&) = delete;

    void startWorker();
    void stopWorker();
    void workerLoop();

    std::optional<std::string> getenvStr(const char* key);
    std::optional<int> getenvInt(const char* key);

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<chat_message_row> queue_;

    std::atomic<bool> inited_{false};
    std::atomic<bool> stop_{false};
    std::thread worker_;
};

#endif
