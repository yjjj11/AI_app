#include "agentic/RagRetriever.h"

#include "agentic/EmbeddingFactory.h"
#include "agentic/QdrantClient.h"

RagRetriever::RagRetriever(AgentConfig cfg) : cfg_(std::move(cfg)) {}

std::vector<std::string> RagRetriever::retrieve(const std::string& session_id,
                                               const std::string& query,
                                               const std::string& memory_type,
                                               int top_k) const {
    if (session_id.empty() || query.empty()) return {};
    if (cfg_.qdrant_url.empty() || cfg_.qdrant_api_key.empty() || cfg_.qdrant_collection.empty()) return {};
    if (top_k <= 0) top_k = cfg_.rag_top_k;
    if (top_k <= 0) top_k = 5;

    auto emb = createEmbeddingClientFromConfig(cfg_);
    if (!emb) return {};
    EmbeddingResult er = emb->embedBatch({query});
    if (!er.ok || er.vectors.empty()) return {};

    AgentConfig cfg = cfg_;
    if (cfg.qdrant_vector_size <= 0) cfg.qdrant_vector_size = er.dimensions;
    QdrantClient qc(cfg.qdrant_url, cfg.qdrant_api_key, cfg.qdrant_timeout_seconds);
    const auto col = qc.ensureCollection(cfg.qdrant_collection, cfg.qdrant_vector_size, cfg.qdrant_distance);
    if (!col.ok) return {};

    hv::Json filter;
    hv::Json must = hv::Json::array();
    must.push_back({{"key", "session_id"}, {"match", {{"value", session_id}}}});
    if (!memory_type.empty()) {
        must.push_back({{"key", "memory_type"}, {"match", {{"value", memory_type}}}});
    }
    filter["must"] = must;

    auto sr = qc.searchPoints(cfg.qdrant_collection, er.vectors[0], top_k, filter, true);
    if (!sr.ok) return {};
    if (!sr.data.contains("result") || !sr.data["result"].is_array()) return {};

    std::vector<std::string> out;
    for (const auto& item : sr.data["result"]) {
        if (!item.is_object()) continue;
        if (!item.contains("payload") || !item["payload"].is_object()) continue;
        const auto& p = item["payload"];
        if (p.contains("text") && p["text"].is_string()) {
            out.push_back(p["text"].get<std::string>());
        }
    }
    return out;
}

