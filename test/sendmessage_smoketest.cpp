#include "config/Dotenv.h"
#include "service/ChatService.h"

#include <iostream>

int main() {
    Dotenv::loadFromFile("/root/my_ai_app/config/ai.env");

    ChatService svc;
    const std::string out = svc.sendMessage("ping", "qwen-plus");
    if (out.empty()) {
        std::cerr << "empty response\n";
        return 1;
    }
    std::cout << out << "\n";
    return 0;
}

