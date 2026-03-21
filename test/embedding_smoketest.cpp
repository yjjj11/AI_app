#include "agentic/EmbeddingFactory.h"
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

    const AgentConfig cfg = AgentConfig::loadFromEnv();
    auto client = createEmbeddingClientFromConfig(cfg);
    if (!client) {
        std::cout << "embedding_client_ok=false\n";
        std::cout << "skipped=true\n";
        std::cout << "reason=embedding not configured\n";
        return 0;
    }
    std::cout << "url=" << client->url() << "\n";
    std::cout << "model=" << client->model() << "\n";
    std::cout << "cfg_dimensions=" << client->configuredDimensions() << "\n";
    EmbeddingResult r = client->embedBatch({"ping"});
    std::cout << "embedding_client_ok=true\n";
    std::cout << "ok=" << (r.ok ? "true" : "false") << "\n";
    std::cout << "dimensions=" << r.dimensions << "\n";
    std::cout << "vectors=" << r.vectors.size() << "\n";
    if (!r.ok) {
        std::cout << "error=" << r.error << "\n";
        return 2;
    }
    return 0;
}
