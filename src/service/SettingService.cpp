#include "service/SettingService.h"

#include "ai/BailianLLM.h"
#include "ai/LLMFactory.h"

#include <string>

static std::string jsonStringOrEmpty(const hv::Json& j, const char* key) {
    if (!j.contains(key)) return "";
    const auto& v = j[key];
    if (v.is_string()) return v.get<std::string>();
    return "";
}

static bool jsonBool(const hv::Json& j, const char* key) {
    if (!j.contains(key)) return false;
    const auto& v = j[key];
    if (v.is_boolean()) return v.get<bool>();
    if (v.is_number_integer()) return v.get<int>() != 0;
    return false;
}

static std::string normalizeEndpoint(std::string url) {
    if (url.empty()) return "";
    if (url.find("/chat/completions") != std::string::npos) return url;
    if (url.find("/compatible-mode/") != std::string::npos) return url;

    while (!url.empty() && url.back() == '/') url.pop_back();
    if (url.size() >= 3 && url.compare(url.size() - 3, 3, "/v1") == 0) {
        return url + "/chat/completions";
    }
    if (url.find("dashscope.aliyuncs.com") != std::string::npos) {
        return url + "/compatible-mode/v1/chat/completions";
    }
    return url + "/v1/chat/completions";
}

static uint64_t fnv1a64(std::string_view s) {
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) {
        h ^= (uint64_t)c;
        h *= 1099511628211ULL;
    }
    return h;
}

static std::string hex64(uint64_t v) {
    static const char* k = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[(size_t)i] = k[v & 0xF];
        v >>= 4;
    }
    return out;
}

static std::string deriveProfileKey(const std::string& base_url, const std::string& model_type) {
    const std::string seed = base_url + "|" + model_type;
    const uint64_t h = fnv1a64(seed);
    return "p_" + hex64(h).substr(0, 12);
}

static std::string pingChatPreview(const LLMFactory::Profile& p) {
    const int timeout = (p.timeout_seconds > 0 && p.timeout_seconds < 10) ? p.timeout_seconds : 10;
    BailianLLM llm(p.api_key, p.base_url, p.default_model, timeout);
    hv::Json messages = hv::Json::array();
    messages.push_back({{"role", "user"}, {"content", "请用一句话回答：你是谁？"}});
    const std::string out = llm.chat_messages(messages);
    if (out.empty()) return "";
    if (out.size() <= 180) return out;
    return out.substr(0, 180);
}

hv::Json SettingService::listLlmProfiles() {
    const auto profiles = LLMFactory::listProfiles();
    hv::Json arr = hv::Json::array();
    for (const auto& p : profiles) {
        arr.push_back({
            {"key", p.key},
            {"provider", p.provider},
            {"baseUrl", p.base_url},
            {"modelType", p.default_model},
            {"timeout", p.timeout_seconds},
            {"capabilities", {
                {"vision", p.capabilities.vision},
                {"audio", p.capabilities.audio},
                {"files", p.capabilities.files}
            }}
        });
    }
    hv::Json out;
    out["success"] = true;
    out["activeKey"] = LLMFactory::getActiveKey();
    out["profiles"] = std::move(arr);
    return out;
}

hv::Json SettingService::setActiveProfile(const hv::Json& body) {
    hv::Json out;
    std::string key = jsonStringOrEmpty(body, "key");
    const bool persist = !body.contains("persist") ? true : jsonBool(body, "persist");

    if (key.empty()) key = "default";

    const auto profiles = LLMFactory::listProfiles();
    bool exists = false;
    for (const auto& p : profiles) {
        if (p.key == key) {
            exists = true;
            break;
        }
    }
    if (!exists) {
        out["success"] = false;
        out["message"] = "profile not found";
        return out;
    }

    LLMFactory::setActiveKey(key);
    LLMFactory::reset("active");

    bool persisted = false;
    if (persist) {
        persisted = LLMFactory::saveProfilesToFile("/root/my_ai_app/config/llm_profiles.json");
    }

    out["success"] = true;
    out["activeKey"] = LLMFactory::getActiveKey();
    out["persisted"] = persisted;
    return out;
}

bool SettingService::applyLlmConfig(const hv::Json& body, hv::Json& out, int& status_code) {
    LLMFactory::Profile p;
    p.key = jsonStringOrEmpty(body, "profileKey");
    p.provider = "";
    p.api_key = jsonStringOrEmpty(body, "apiKey");
    p.base_url = normalizeEndpoint(jsonStringOrEmpty(body, "baseUrl"));
    p.default_model = jsonStringOrEmpty(body, "modelType");
    if (body.contains("timeout") && body["timeout"].is_number_integer()) {
        p.timeout_seconds = body["timeout"].get<int>();
    }
    if (body.contains("capabilities") && body["capabilities"].is_object()) {
        const auto& c = body["capabilities"];
        if (c.contains("vision") && c["vision"].is_boolean()) p.capabilities.vision = c["vision"].get<bool>();
        if (c.contains("audio") && c["audio"].is_boolean()) p.capabilities.audio = c["audio"].get<bool>();
        if (c.contains("files") && c["files"].is_boolean()) p.capabilities.files = c["files"].get<bool>();
    }

    if (p.api_key.empty()) {
        out = {{"success", false}, {"message", "apiKey required"}};
        status_code = 400;
        return false;
    }
    if (p.default_model.empty()) {
        out = {{"success", false}, {"message", "modelType required"}};
        status_code = 400;
        return false;
    }
    if (p.base_url.empty()) {
        out = {{"success", false}, {"message", "baseUrl required"}};
        status_code = 400;
        return false;
    }

    if (p.key.empty()) {
        p.key = deriveProfileKey(p.base_url, p.default_model);
    }
    if (!body.contains("capabilities")) {
        p.capabilities = LLMFactory::inferCapabilitiesFromModel(p.default_model);
    }

    const std::string preview = pingChatPreview(p);
    if (preview.empty()) {
        out = {{"success", false}, {"message", "ping failed"}};
        status_code = 200;
        return false;
    }

    if (!LLMFactory::upsertProfile(p)) {
        out = {{"success", false}, {"message", "upsert failed"}};
        status_code = 500;
        return false;
    }

    const bool set_active = jsonBool(body, "setActive");
    if (set_active) {
        LLMFactory::setActiveKey(p.key);
    }

    const bool persist = !body.contains("persist") ? true : jsonBool(body, "persist");
    bool persisted = false;
    if (persist) {
        persisted = LLMFactory::saveProfilesToFile("/root/my_ai_app/config/llm_profiles.json");
    }

    out = {
        {"success", true},
        {"key", p.key},
        {"activeKey", LLMFactory::getActiveKey()},
        {"persisted", persisted},
        {"pingPreview", preview}
    };
    status_code = 200;
    return true;
}
