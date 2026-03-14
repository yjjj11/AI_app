#include "service/ChatMySqlStore.h"

#include <cstdlib>
#include <optional>
#include <stdexcept>

#include "ormpp/connection_pool.hpp"
#include "service/ChatDbModels.h"

ChatMySqlStore& ChatMySqlStore::instance() {
    static ChatMySqlStore inst;
    return inst;
}

ChatMySqlStore::ChatMySqlStore() {}

ChatMySqlStore::~ChatMySqlStore() {
    stopWorker();
}

std::optional<std::string> ChatMySqlStore::getenvStr(const char* key) {
    const char* v = std::getenv(key);
    if (!v || !*v) return std::nullopt;
    return std::string(v);
}

std::optional<int> ChatMySqlStore::getenvInt(const char* key) {
    const auto v = getenvStr(key);
    if (!v.has_value()) return std::nullopt;
    return std::atoi(v->c_str());
}

void ChatMySqlStore::startWorker() {
    if (worker_.joinable()) return;
    stop_.store(false);
    worker_ = std::thread([this]() { workerLoop(); });
}

void ChatMySqlStore::stopWorker() {
    stop_.store(true);
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void ChatMySqlStore::init() {
    bool expected = false;
    if (!inited_.compare_exchange_strong(expected, true)) return;

    const std::string host = getenvStr("MYSQL_HOST").value_or("127.0.0.1");
    const std::string user = getenvStr("MYSQL_USER").value_or("root");
    const std::string passwd = getenvStr("MYSQL_PASSWORD").value_or("");
    const std::string db = getenvStr("MYSQL_DB").value_or("my_aiapp");
    const std::optional<int> timeout = getenvInt("MYSQL_TIMEOUT").value_or(5);
    const std::optional<int> port = getenvInt("MYSQL_PORT").value_or(3306);
    const int pool_size = getenvInt("MYSQL_POOL_SIZE").value_or(4);

    try {
        auto& pool = ormpp::connection_pool<Db>::instance();
        pool.init(pool_size, host, user, passwd, db, timeout, port);

        {
            auto conn = pool.get();
            if (!conn) throw std::runtime_error("mysql connect failed");
            conn->create_datatable<chat_session_row>(ormpp_auto_key{"id"});
            conn->create_datatable<chat_message_row>(ormpp_auto_key{"id"});
        }

        startWorker();
    } catch (...) {
        inited_.store(false);
    }
}

void ChatMySqlStore::enqueue(chat_message_row task) {
    if (task.session_id.empty()) task.session_id = "default";
    if (task.created_at_ms <= 0) task.created_at_ms = chat_now_ms();
    if (task.username.empty()) task.username = "unknown";

    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(task));
    }
    cv_.notify_one();
}

void ChatMySqlStore::workerLoop() {
    init();
    auto& pool = ormpp::connection_pool<Db>::instance();
    while (!stop_.load()) {
        chat_message_row task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return stop_.load() || !queue_.empty(); });
            if (stop_.load()) break;
            task = std::move(queue_.front());
            queue_.pop_front();
        }

        auto conn = pool.get();
        if (!conn) continue;
        conn->insert(task);
    }
}
