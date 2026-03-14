#include "ai/LLMFactory.h"
#include "config/Dotenv.h"

#include <cstdlib>
#include <iostream>

int main() {
    Dotenv::loadFromFile("/root/my_ai_app/config/ai.env");

    auto llm = LLMFactory::create();
    if (!llm) {
        std::cerr << "LLMFactory::create failed\n";
        return 2;
    }

    const std::string model = []() {
        const char* v = std::getenv("BAILIAN_DEFAULT_MODEL");
        return (v && *v) ? std::string(v) : std::string("qwen-plus");
    }();

    const std::string out = llm->chat("ping", model);
    if (out.empty()) {
        std::cerr << "LLM chat failed (empty response)\n";
        return 1;
    }

    std::cout << out << "\n";
    return 0;
}

