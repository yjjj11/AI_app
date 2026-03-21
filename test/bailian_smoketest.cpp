#include "ai/LLMFactory.h"
#include "config/Dotenv.h"

#include <cstdlib>
#include <iostream>

int main() {
    if (!LLMFactory::loadProfilesFromFile("/root/my_ai_app/config/llm_profiles.json")) {
        LLMFactory::loadProfilesFromFile("/root/my_ai_app/config/llm_profiles.json.template");
    }

    auto llm = LLMFactory::create();
    if (!llm) {
        std::cerr << "LLMFactory::create failed\n";
        return 2;
    }

    std::cout << "\n--- stream_chat ---\n";
    hv::Json messages = hv::Json::array();
    messages.push_back({{"role", "user"}, {"content", "请用一句话解释什么是 SSE。"}});
    const std::string streamed = llm->stream_chat_messages(
        messages,
        [](std::string_view chunk) {
            std::cout << chunk;
            std::cout.flush();
        }
    );
    std::cout << "\n\n--- stream_chat done, total_len=" << streamed.size() << " ---\n";
    return 0;
}
