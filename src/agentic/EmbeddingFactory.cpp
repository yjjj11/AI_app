#include "agentic/EmbeddingFactory.h"

#include "ai/LLMFactory.h"

#include <algorithm>

static std::string deriveEmbeddingsUrlFromChatUrl(std::string chat_url) {
    const std::string needle = "/chat/completions";
    const size_t pos = chat_url.rfind(needle);
    if (pos != std::string::npos) {
        chat_url.replace(pos, needle.size(), "/embeddings");
        return chat_url;
    }
    if (!chat_url.empty() && chat_url.back() != '/') chat_url.push_back('/');
    chat_url += "embeddings";
    return chat_url;
}

static bool isLocalUrl(const std::string& u) {
    return u.find("127.0.0.1") != std::string::npos || u.find("localhost") != std::string::npos;
}

static bool selectProfileForEmbedding(const std::vector<LLMFactory::Profile>& profiles,
                                      const std::string& active_key,
                                      LLMFactory::Profile& out) {
    for (const auto& p : profiles) {
        if (p.key == active_key && !isLocalUrl(p.base_url)) {
            out = p;
            return true;
        }
    }
    for (const auto& p : profiles) {
        if (p.base_url.find("dashscope.aliyuncs.com") != std::string::npos) {
            out = p;
            return true;
        }
    }
    for (const auto& p : profiles) {
        if (!isLocalUrl(p.base_url)) {
            out = p;
            return true;
        }
    }
    for (const auto& p : profiles) {
        if (p.key == "default") {
            out = p;
            return true;
        }
    }
    if (!profiles.empty()) {
        out = profiles.front();
        return true;
    }
    return false;
}

std::unique_ptr<EmbeddingClient> createEmbeddingClientFromConfig(const AgentConfig& cfg) {
    std::string api_key = cfg.embedding_api_key;
    std::string url = cfg.embedding_base_url;
    std::string model = cfg.embedding_model;
    int timeout = cfg.embedding_timeout_seconds;
    int dim = cfg.embedding_dimensions;

    const auto profiles = LLMFactory::listProfiles();
    LLMFactory::Profile p;
    if (selectProfileForEmbedding(profiles, LLMFactory::getActiveKey(), p)) {
        if (api_key.empty()) api_key = p.api_key;
        if (url.empty()) url = deriveEmbeddingsUrlFromChatUrl(p.base_url);
        if (model.empty()) model = "text-embedding-v4";
        if (timeout <= 0) timeout = p.timeout_seconds > 0 ? p.timeout_seconds : 30;
    }

    if (dim <= 0) dim = 1024;
    if (timeout <= 0) timeout = 30;

    if (api_key.empty() || url.empty() || model.empty()) {
        return nullptr;
    }
    return std::make_unique<EmbeddingClient>(api_key, url, model, timeout, dim);
}
