#include "ai/LLMFactory.h"

#include "ai/BailianLLM.h"

#include <cstdlib>

static std::string getenvStr(const char* k) {
    const char* v = std::getenv(k);
    if (!v || !*v) return "";
    return std::string(v);
}

static int getenvInt(const char* k, int def) {
    const char* v = std::getenv(k);
    if (!v || !*v) return def;
    const int x = std::atoi(v);
    return x > 0 ? x : def;
}

std::unique_ptr<LLM> LLMFactory::create() {
    const std::string provider = getenvStr("LLM_PROVIDER");
    const std::string p = provider.empty() ? "bailian" : provider;
    if (p == "bailian") {
        const std::string api_key = getenvStr("BAILIAN_API_KEY");
        const std::string base_url = getenvStr("BAILIAN_BASE_URL");
        const std::string model = getenvStr("BAILIAN_DEFAULT_MODEL");
        const int timeout = getenvInt("BAILIAN_TIMEOUT", 30);
        return std::make_unique<BailianLLM>(api_key, base_url, model, timeout);
    }
    return nullptr;
}

