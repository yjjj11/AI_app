#ifndef CHAT_SERVICE_H
#define CHAT_SERVICE_H

#include "service/ChatDbModels.h"

#include <string>
#include <vector>

class ChatService {
public:
    std::string createSession(const std::string& username, const std::string& title);
    bool deleteSession(const std::string& username, const std::string& sessionId);
    std::vector<chat_session_row> listSessions(const std::string& username, int limit);
    std::vector<chat_message_row> getHistory(const std::string& username, const std::string& sessionId, int limit, int offset);

    std::string sendMessage(const std::string& username,
                            const std::string& sessionId,
                            const std::string& question,
                            const std::string& modelType);
};

#endif
