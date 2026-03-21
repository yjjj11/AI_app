#include "agentic/EmbeddingFactory.h"
#include "agentic/QdrantClient.h"
#include "ai/LLMFactory.h"
#include "config/AgentConfig.h"
#include "config/Dotenv.h"
#include "ormpp/connection_pool.hpp"
#include "service/ChatDbModels.h"
#include "service/ChatMySqlStore.h"
#include "service/MemoryIndexService.h"

#include <iostream>
#include <thread>

int main() {
    Dotenv::loadFromFile("/root/my_ai_app/config/mysql.env");
    Dotenv::loadFromFile("/root/my_ai_app/config/ai.env");
    if (!Dotenv::loadFromFile("/root/my_ai_app/config/agent.config")) {
        Dotenv::loadFromFile("/root/my_ai_app/config/agent.config.template");
    }
    if (!LLMFactory::loadProfilesFromFile("/root/my_ai_app/config/llm_profiles.json")) {
        LLMFactory::loadProfilesFromFile("/root/my_ai_app/config/llm_profiles.json.template");
    }

    ChatMySqlStore::instance().init();
    MemoryIndexService::instance().init();

    AgentConfig cfg = AgentConfig::loadFromEnv();
    auto emb0 = createEmbeddingClientFromConfig(cfg);
    if (!emb0 || cfg.qdrant_url.empty()) {
        std::cout << "skipped=true\n";
        std::cout << "reason=embedding/qdrant not configured\n";
        return 0;
    }
    auto& pool = ormpp::connection_pool<Db>::instance();
    auto conn = pool.get();
    if (!conn) {
        std::cerr << "mysql_connect_failed\n";
        return 1;
    }

    const std::string user = "smoke_user";
    const std::string sid = "smoke_mem_" + std::to_string(chat_now_ms());
    chat_session_row s;
    s.username = user;
    s.session_id = sid;
    s.title = "smoke";
    s.created_at_ms = chat_now_ms();
    conn->insert(s);

    for (int i = 0; i < 105; i++) {
        chat_message_row m;
        m.username = user;
        m.role = (i % 2 == 0) ? "user" : "assistant";
        m.model = "smoke";
        m.content = "msg_" + std::to_string(i);
        m.created_at_ms = chat_now_ms();
        m.session_id = sid;
        conn->insert(m);
    }

    MemoryIndexService::instance().maybeEnqueue(user, sid);
    std::this_thread::sleep_for(std::chrono::seconds(6));

    auto notes = conn->query_s<note_row>("username=? and session_id=? and memory_type=? order by id desc limit 1", user, sid, "short");
    std::cout << "note_rows=" << notes.size() << "\n";
    if (notes.empty()) return 2;

    auto emb = createEmbeddingClientFromConfig(cfg);
    if (!emb) return 3;
    EmbeddingResult er = emb->embedBatch({"msg"});
    if (!er.ok || er.vectors.empty()) return 4;

    if (cfg.qdrant_vector_size <= 0) cfg.qdrant_vector_size = er.dimensions;
    QdrantClient qc(cfg.qdrant_url, cfg.qdrant_api_key, cfg.qdrant_timeout_seconds);
    hv::Json filter;
    filter["must"] = hv::Json::array({{{"key", "session_id"}, {"match", {{"value", sid}}}}});
    auto sr = qc.searchPoints(cfg.qdrant_collection, er.vectors[0], 3, filter, true);
    std::cout << "qdrant_search_ok=" << (sr.ok ? "true" : "false") << "\n";
    if (!sr.ok) {
        std::cout << "qdrant_error=" << sr.error << "\n";
        return 5;
    }
    int n = 0;
    if (sr.data.contains("result") && sr.data["result"].is_array()) n = (int)sr.data["result"].size();
    std::cout << "qdrant_results=" << n << "\n";
    return n > 0 ? 0 : 6;
}
