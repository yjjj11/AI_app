#include "service/ChatService.h"

#include "ai/LLMFactory.h"

#include "ormpp/connection_pool.hpp"

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

std::string ChatService::sendMessage( const std::string& question,const std::string& modelType) {
    static std::unique_ptr<LLM> llm = LLMFactory::create();
    if (!llm) {
        return "LLM not configured";
    }
    
    const std::string out = llm->chat(question, "qwen-plus");
    if (out.empty()) return "LLM request failed";
    return out;
}
