#include "HttpServer.h"
#include "ai/LLMFactory.h"
#include "config/Dotenv.h"

#include <cstdlib>
#include <iostream>

int main() {
    Dotenv::loadFromFile("/root/my_ai_app/config/mysql.env");
    Dotenv::loadFromFile("/root/my_ai_app/config/ai.env");
    LLMFactory::loadProfilesFromFile("/root/my_ai_app/config/llm_profiles.json");
    auto llm = LLMFactory::get();
    if (!llm) {
        std::cerr << "LLM init failed\n";
        return 1;
    }
    int port = 8888;
    if (const char* v = std::getenv("HTTP_PORT"); v && *v) {
        const int x = std::atoi(v);
        if (x > 0 && x < 65536) port = x;
    }
    MyHttpServer server(port, 4);
    server.run();
    return 0;
}
