#include "ai/LLMFactory.h"
#include "config/Dotenv.h"
#include "service/SettingService.h"

#include <cstdlib>
#include <iostream>
#include <string>

static std::string getenvStr(const char* k) {
    const char* v = std::getenv(k);
    if (!v || !*v) return "";
    return std::string(v);
}

static std::string firstNonEmpty(std::initializer_list<std::string> xs) {
    for (const auto& s : xs) {
        if (!s.empty()) return s;
    }
    return "";
}

static std::string preview(const std::string& s, size_t n) {
    if (s.size() <= n) return s;
    return s.substr(0, n);
}

int main() {
    if (!LLMFactory::loadProfilesFromFile("/root/my_ai_app/config/llm_profiles.json")) {
        std::cout << "profiles_file_missing=true\n";
        return 0;
    }

    const std::string custom_model = firstNonEmpty({
        getenvStr("CUSTOM_MODEL_TYPE"),
        getenvStr("DEEPSEEK_MODEL_TYPE"),
        getenvStr("deepseek_model_type")
    });
    const std::string custom_key = firstNonEmpty({
        getenvStr("CUSTOM_API_KEY"),
        getenvStr("DEEPSEEK_API_KEY"),
        getenvStr("deepseek_api_key")
    });
    const std::string custom_url = firstNonEmpty({
        getenvStr("CUSTOM_BASE_URL"),
        getenvStr("DEEPSEEK_BASE_URL"),
        getenvStr("deepseek_base_url")
    });

    auto before = LLMFactory::get();
    if (!before) {
        std::cerr << "default llm init failed\n";
        return 1;
    }

    const std::string q = "请用一句话回答：你是谁？";
    hv::Json q_messages = hv::Json::array();
    q_messages.push_back({{"role", "user"}, {"content", q}});
    const std::string before_out = before->chat_messages(q_messages);
    std::cout << "before_preview=" << preview(before_out, 180) << "\n";

    if (custom_model.empty() || custom_key.empty() || custom_url.empty()) {
        std::cout << "custom_env_missing=true\n";
        std::cout << "need_env_keys=CUSTOM_MODEL_TYPE,CUSTOM_API_KEY,CUSTOM_BASE_URL (or DEEPSEEK_*/deepseek_*)\n";
        return 0;
    }

    SettingService svc;
    hv::Json body;
    body["modelType"] = custom_model;
    body["apiKey"] = custom_key;
    body["baseUrl"] = custom_url;
    body["persist"] = false;
    body["setActive"] = true;
    hv::Json out;
    int code = 0;
    svc.applyLlmConfig(body, out, code);
    std::cout << "apply_code=" << code << "\n";
    std::cout << "apply_success=" << (out.contains("success") ? out["success"].dump() : "false") << "\n";
    const std::string new_key = (out.contains("key") && out["key"].is_string()) ? out["key"].get<std::string>() : "";
    std::cout << "apply_key=" << (new_key.empty() ? "(empty)" : new_key) << "\n";

    auto after = LLMFactory::get("active");
    if (!after) {
        std::cerr << "active llm init failed\n";
        return 1;
    }

    const std::string after_out = after->chat_messages(q_messages);
    std::cout << "after_preview=" << preview(after_out, 180) << "\n";

    const auto profiles = LLMFactory::listProfiles();
    for (const auto& p : profiles) {
        if (!new_key.empty() && p.key == new_key) {
            std::cout << "custom_modelType=" << p.default_model << "\n";
            std::cout << "custom_baseUrl=" << p.base_url << "\n";
        }
    }

    return 0;
}
