#include "ai/LLMFactory.h"
#include "config/Dotenv.h"

#include <iostream>
#include <cstdlib>

int main() {
    if (!LLMFactory::loadProfilesFromFile("/root/my_ai_app/config/llm_profiles.json")) {
        LLMFactory::loadProfilesFromFile("/root/my_ai_app/config/llm_profiles.json.template");
    }

    auto a1 = LLMFactory::get();
    auto a2 = LLMFactory::get();
    if (!a1 || !a2) {
        std::cerr << "LLMFactory::get failed\n";
        return 1;
    }

    std::cout << "default_same_instance=" << (a1.get() == a2.get() ? "true" : "false") << "\n";

    LLMFactory::reset();
    auto a3 = LLMFactory::get();
    if (!a3) {
        std::cerr << "LLMFactory::get after reset failed\n";
        return 1;
    }
    std::cout << "default_new_after_reset=" << (a1.get() != a3.get() ? "true" : "false") << "\n";

    const char* api_key = std::getenv("BAILIAN_API_KEY");
    const char* base_url = std::getenv("BAILIAN_BASE_URL");
    if (api_key && *api_key && base_url && *base_url) {
        LLMFactory::Profile p;
        p.key = "demo";
        p.provider = "bailian";
        p.api_key = api_key;
        p.base_url = base_url;
        p.default_model = "qwen-plus";
        p.timeout_seconds = 30;
        if (!LLMFactory::upsertProfile(p)) {
            std::cerr << "LLMFactory::upsertProfile failed\n";
            return 1;
        }
        LLMFactory::setActiveKey("demo");
        auto k = LLMFactory::get("demo");
        if (!k) {
            std::cerr << "LLMFactory::get(demo) failed\n";
            return 1;
        }
        std::cout << "demo_profile_ok=true\n";
    } else {
        std::cout << "demo_profile_ok=false\n";
    }

    return 0;
}
