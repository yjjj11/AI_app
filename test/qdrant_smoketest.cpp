#include "agentic/EmbeddingFactory.h"
#include "agentic/QdrantClient.h"
#include "ai/LLMFactory.h"
#include "config/AgentConfig.h"
#include "config/Dotenv.h"

#include <iostream>

int main() {
    Dotenv::loadFromFile("/root/my_ai_app/config/ai.env");
    if (!Dotenv::loadFromFile("/root/my_ai_app/config/agent.config")) {
        Dotenv::loadFromFile("/root/my_ai_app/config/agent.config.template");
    }
    if (!LLMFactory::loadProfilesFromFile("/root/my_ai_app/config/llm_profiles.json")) {
        LLMFactory::loadProfilesFromFile("/root/my_ai_app/config/llm_profiles.json.template");
    }

    AgentConfig cfg = AgentConfig::loadFromEnv();
    auto emb = createEmbeddingClientFromConfig(cfg);
    if (!emb) {
        std::cout << "embedding_client_ok=false\n";
        std::cout << "skipped=true\n";
        std::cout << "reason=embedding not configured\n";
        return 0;
    }
    EmbeddingResult er = emb->embedBatch({"qdrant ping"});
    if (!er.ok || er.vectors.empty()) {
        std::cerr << "embedding_ok=false\n";
        std::cerr << "error=" << er.error << "\n";
        return 2;
    }
    const int dim = er.dimensions;
    if (cfg.qdrant_vector_size <= 0) cfg.qdrant_vector_size = dim;
    if (cfg.qdrant_url.empty()) {
        std::cout << "qdrant_url_missing\n";
        std::cout << "skipped=true\n";
        std::cout << "reason=qdrant not configured\n";
        return 0;
    }
    QdrantClient qc(cfg.qdrant_url, cfg.qdrant_api_key, cfg.qdrant_timeout_seconds);
    auto h = qc.health();
    std::cout << "health_ok=" << (h.ok ? "true" : "false") << "\n";
    if (!h.ok) {
        std::cout << "health_error=" << h.error << "\n";
        return 4;
    }
    auto col = qc.ensureCollection(cfg.qdrant_collection, cfg.qdrant_vector_size, cfg.qdrant_distance);
    std::cout << "collection_ok=" << (col.ok ? "true" : "false") << "\n";
    if (!col.ok) {
        std::cout << "collection_error=" << col.error << "\n";
        return 5;
    }

    hv::Json points = hv::Json::array();
    hv::Json payload = {{"text", "qdrant ping"}, {"session_id", "smoke"}, {"memory_type", "short"}};
    hv::Json vec = hv::Json::array();
    for (float x : er.vectors[0]) vec.push_back(x);
    points.push_back({{"id", 1}, {"vector", vec}, {"payload", payload}});
    auto up = qc.upsertPoints(cfg.qdrant_collection, points, true);
    std::cout << "upsert_ok=" << (up.ok ? "true" : "false") << "\n";
    if (!up.ok) {
        std::cout << "upsert_error=" << up.error << "\n";
        return 6;
    }

    hv::Json filter;
    filter["must"] = hv::Json::array({{{"key", "session_id"}, {"match", {{"value", "smoke"}}}}});
    auto sr = qc.searchPoints(cfg.qdrant_collection, er.vectors[0], 3, filter, true);
    std::cout << "search_ok=" << (sr.ok ? "true" : "false") << "\n";
    if (!sr.ok) {
        std::cout << "search_error=" << sr.error << "\n";
        return 7;
    }
    int n = 0;
    if (sr.data.contains("result") && sr.data["result"].is_array()) n = (int)sr.data["result"].size();
    std::cout << "search_results=" << n << "\n";
    return 0;
}
