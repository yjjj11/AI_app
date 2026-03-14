#ifndef CHAT_DB_MODELS_H
#define CHAT_DB_MODELS_H

#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <string_view>

#include "ormpp/dbng.hpp"
#include "ormpp/mysql.hpp"
#include "ylt/reflection/user_reflect_macro.hpp"

using Db = ormpp::dbng<ormpp::mysql>;

struct chat_message_row {
    std::string username;
    std::string role;
    std::string model;
    std::string content;
    int64_t created_at_ms;
    std::string session_id;
    int64_t id;
    static constexpr std::string_view get_alias_struct_name(chat_message_row*) { return "chat_messages"; }
};
REGISTER_AUTO_KEY(chat_message_row, id)
YLT_REFL(chat_message_row, id, username, role, model, content, created_at_ms, session_id)

struct chat_session_row {
    std::string username;
    std::string session_id;
    std::string title;
    int64_t created_at_ms;
    int64_t id;
    static constexpr std::string_view get_alias_struct_name(chat_session_row*) { return "chat_sessions"; }
};
REGISTER_AUTO_KEY(chat_session_row, id)
YLT_REFL(chat_session_row, id, username, session_id, title, created_at_ms)

inline int64_t chat_now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

inline std::string generate_session_id() {
    std::array<unsigned char, 16> bytes{};
    std::random_device rd;
    for (auto& b : bytes) {
        b = static_cast<unsigned char>(rd());
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto b : bytes) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

#endif
