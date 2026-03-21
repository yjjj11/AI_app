#ifndef CHAT_SERVICE_H
#define CHAT_SERVICE_H

#include "service/ChatDbModels.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

class ChatService {
public:
    std::string createSession(const std::string& username, const std::string& title);
    bool deleteSession(const std::string& username, const std::string& sessionId);
    std::vector<chat_session_row> listSessions(const std::string& username, int limit);
    std::vector<chat_message_row> getHistory(const std::string& username, const std::string& sessionId, int limit, int offset);
    int countMessages(const std::string& username, const std::string& sessionId);
    int64_t maxMessageId(const std::string& username, const std::string& sessionId);
    std::vector<chat_message_row> getTailHistory(const std::string& username, const std::string& sessionId, int limit);

    std::string streamMessage(const std::string& question,
                              const std::vector<std::string>& imageDataUrls,
                              const std::function<void(std::string_view)>& on_delta);
};

#endif
