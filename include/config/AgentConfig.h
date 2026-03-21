#ifndef CONFIG_AGENT_CONFIG_H
#define CONFIG_AGENT_CONFIG_H

#include <string>

struct AgentConfig {
    std::string qdrant_url;
    std::string qdrant_api_key;
    std::string qdrant_collection{"ai_app"};
    int qdrant_vector_size{0};
    std::string qdrant_distance{"cosine"};
    int qdrant_timeout_seconds{30};

    std::string embedding_base_url;
    std::string embedding_api_key;
    std::string embedding_model{"text-embedding-v4"};
    int embedding_dimensions{1024};
    int embedding_timeout_seconds{30};

    int memory_trigger_messages{100};
    int rag_top_k{5};
    int rag_context_char_budget{2500};

    static AgentConfig loadFromEnv();
    void logStartupHints() const;
};

#endif
