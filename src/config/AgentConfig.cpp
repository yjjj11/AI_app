#include "config/AgentConfig.h"

#include <cstdlib>
#include <iostream>

static std::string getenvStr(const char* k) {
    const char* v = std::getenv(k);
    if (!v || !*v) return "";
    return std::string(v);
}

static int getenvInt(const char* k, int def) {
    const char* v = std::getenv(k);
    if (!v || !*v) return def;
    const int x = std::atoi(v);
    return x == 0 ? def : x;
}

static std::string maskSecret(const std::string& s) {
    if (s.empty()) return "";
    if (s.size() <= 6) return "***";
    return s.substr(0, 3) + "***" + s.substr(s.size() - 3);
}

AgentConfig AgentConfig::loadFromEnv() {
    AgentConfig c;
    c.qdrant_url = getenvStr("QDRANT_URL");
    c.qdrant_api_key = getenvStr("QDRANT_API_KEY");
    {
        const std::string v = getenvStr("QDRANT_COLLECTION");
        if (!v.empty()) c.qdrant_collection = v;
    }
    c.qdrant_vector_size = getenvInt("QDRANT_VECTOR_SIZE", 0);
    {
        const std::string v = getenvStr("QDRANT_DISTANCE");
        if (!v.empty()) c.qdrant_distance = v;
    }
    c.qdrant_timeout_seconds = getenvInt("QDRANT_TIMEOUT", 30);

    c.embedding_base_url = getenvStr("EMBEDDING_BASE_URL");
    c.embedding_api_key = getenvStr("EMBEDDING_API_KEY");
    {
        const std::string v = getenvStr("EMBEDDING_MODEL");
        if (!v.empty()) c.embedding_model = v;
    }
    c.embedding_dimensions = getenvInt("EMBEDDING_DIMENSIONS", 1024);
    c.embedding_timeout_seconds = getenvInt("EMBEDDING_TIMEOUT", 30);

    c.memory_trigger_messages = getenvInt("MEMORY_TRIGGER_MESSAGES", 100);
    c.rag_top_k = getenvInt("RAG_TOP_K", 5);
    c.rag_context_char_budget = getenvInt("RAG_CONTEXT_CHAR_BUDGET", 2500);
    return c;
}

void AgentConfig::logStartupHints() const {
    if (qdrant_url.empty()) {
        std::cerr << "[AgentConfig] QDRANT_URL is missing. RAG memory will be disabled.\n";
    }
    if (!qdrant_url.empty() && qdrant_api_key.empty()) {
        std::cerr << "[AgentConfig] QDRANT_API_KEY is missing for Qdrant Cloud. RAG memory will be disabled.\n";
    }
    if (!qdrant_url.empty() && qdrant_vector_size <= 0) {
        std::cerr << "[AgentConfig] QDRANT_VECTOR_SIZE is missing. Please set it to match embedding dimension.\n";
    }
    if (embedding_base_url.empty()) {
        std::cerr << "[AgentConfig] EMBEDDING_BASE_URL is missing. Will try to reuse active LLM profile.\n";
    }
    if (!embedding_base_url.empty() && embedding_api_key.empty()) {
        std::cerr << "[AgentConfig] EMBEDDING_API_KEY is missing. Will try to reuse active LLM profile.\n";
    }
    std::cout << "[AgentConfig] qdrant_collection=" << qdrant_collection
              << " distance=" << qdrant_distance
              << " timeout=" << qdrant_timeout_seconds
              << " api_key=" << (qdrant_api_key.empty() ? "(empty)" : maskSecret(qdrant_api_key))
              << "\n";
}

