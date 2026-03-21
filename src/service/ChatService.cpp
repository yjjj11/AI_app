#include "service/ChatService.h"

#include "ai/LLMFactory.h"

#include "ormpp/connection_pool.hpp"

#include <algorithm>
#include <hv/HttpService.h>

static hv::Json buildUserMessages(const std::string& question, const std::vector<std::string>& imageDataUrls) {
    hv::Json messages = hv::Json::array();
    if (imageDataUrls.empty()) {
        messages.push_back({{"role", "user"}, {"content", question}});
        return messages;
    }

    hv::Json content = hv::Json::array();
    content.push_back({{"type", "text"}, {"text", question}});
    for (const auto& url : imageDataUrls) {
        if (url.empty()) continue;
        content.push_back({{"type", "image_url"}, {"image_url", {{"url", url}}}});
    }
    messages.push_back({{"role", "user"}, {"content", content}});
    return messages;
}

std::string ChatService::createSession(const std::string& username, const std::string& title) {
    auto& pool = ormpp::connection_pool<Db>::instance();
    auto conn = pool.get();
    if (!conn) return "";

    chat_session_row row;
    row.username = username.empty() ? "unknown" : username;
    row.session_id = generate_session_id();
    row.title = title.empty() ? "新对话" : title;
    row.created_at_ms = chat_now_ms();
    conn->insert(row);
    return row.session_id;
}

bool ChatService::deleteSession(const std::string& username, const std::string& sessionId) {
    if (sessionId.empty()) return false;
    auto& pool = ormpp::connection_pool<Db>::instance();
    auto conn = pool.get();
    if (!conn) return false;

    const std::string user = username.empty() ? "unknown" : username;
    const auto n1 = conn->delete_records_s<chat_message_row>("username=? and session_id=?", user, sessionId);
    const auto n2 = conn->delete_records_s<chat_session_row>("username=? and session_id=?", user, sessionId);
    return (n1 + n2) > 0;
}

std::vector<chat_session_row> ChatService::listSessions(const std::string& username, int limit) {
    if (limit <= 0) limit = 50;
    auto& pool = ormpp::connection_pool<Db>::instance();
    auto conn = pool.get();
    if (!conn) return {};

    const std::string user = username.empty() ? "unknown" : username;
    return conn->query_s<chat_session_row>("username=? order by id desc limit ?", user, limit);
}

std::vector<chat_message_row> ChatService::getHistory(const std::string& username, const std::string& sessionId, int limit, int offset) {
    if (limit <= 0) limit = 50;
    if (offset < 0) offset = 0;
    auto& pool = ormpp::connection_pool<Db>::instance();
    auto conn = pool.get();
    if (!conn) return {};

    const std::string user = username.empty() ? "unknown" : username;
    const std::string sid = sessionId.empty() ? "default" : sessionId;
    return conn->query_s<chat_message_row>("username=? and session_id=? order by id asc limit ? offset ?", user, sid, limit, offset);
}

int ChatService::countMessages(const std::string& username, const std::string& sessionId) {
    auto& pool = ormpp::connection_pool<Db>::instance();
    auto conn = pool.get();
    if (!conn) return 0;
    const std::string user = username.empty() ? "unknown" : username;
    const std::string sid = sessionId.empty() ? "default" : sessionId;
    try {
        auto rows = conn->query<std::tuple<int64_t>>("select count(*) from chat_messages where username=? and session_id=?", user, sid);
        if (rows.empty()) return 0;
        return (int)std::get<0>(rows[0]);
    } catch (...) {
        return 0;
    }
}

int64_t ChatService::maxMessageId(const std::string& username, const std::string& sessionId) {
    auto& pool = ormpp::connection_pool<Db>::instance();
    auto conn = pool.get();
    if (!conn) return 0;
    const std::string user = username.empty() ? "unknown" : username;
    const std::string sid = sessionId.empty() ? "default" : sessionId;
    try {
        auto rows = conn->query<std::tuple<int64_t>>("select max(id) from chat_messages where username=? and session_id=?", user, sid);
        if (rows.empty()) return 0;
        return (int64_t)std::get<0>(rows[0]);
    } catch (...) {
        return 0;
    }
}

std::vector<chat_message_row> ChatService::getTailHistory(const std::string& username, const std::string& sessionId, int limit) {
    if (limit <= 0) limit = 100;
    auto& pool = ormpp::connection_pool<Db>::instance();
    auto conn = pool.get();
    if (!conn) return {};
    const std::string user = username.empty() ? "unknown" : username;
    const std::string sid = sessionId.empty() ? "default" : sessionId;
    auto rows = conn->query_s<chat_message_row>("username=? and session_id=? order by id desc limit ?", user, sid, limit);
    std::reverse(rows.begin(), rows.end());
    return rows;
}

std::string ChatService::streamMessage(const std::string& question,
                                       const std::vector<std::string>& imageDataUrls,
                                       const std::function<void(std::string_view)>& on_delta) {
    auto llm = LLMFactory::get();
    if (!llm) {
        return "";
    }
    const auto messages = buildUserMessages(question, imageDataUrls);
    const std::string out = llm->stream_chat_messages(messages, on_delta);
    return out;
}
