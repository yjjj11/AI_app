#include "ai/LLMFactory.h"
#include "service/ChatService.h"

#include <iostream>

int main() {
    if (!LLMFactory::loadProfilesFromFile("/root/my_ai_app/config/llm_profiles.json")) {
        std::cout << "profiles_file_missing=true\n";
        return 0;
    }

    ChatService svc;
    std::string aggregated;
    const std::string out = svc.streamMessage(
        "ping",
        {},
        [&](std::string_view delta) {
            aggregated.append(delta.data(), delta.size());
        }
    );
    const std::string final_text = !aggregated.empty() ? aggregated : out;
    if (final_text.empty()) {
        std::cerr << "empty response\n";
        return 1;
    }
    std::cout << final_text << "\n";
    return 0;
}
