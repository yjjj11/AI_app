#include "router/ChatRouter.h"
#include "service/ChatMySqlStore.h"

#include <chrono>
#include <fstream>
#include <sstream>

static int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

static std::string readTextFile(const std::string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in.is_open()) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void ChatRouter::registerRoutes(hv::HttpService& service) {
    service.GET("/health", [](HttpRequest*, HttpResponse* resp) {
        resp->json = {{"ok", true}};
        return 200;
    });

    service.GET("/ai.html", [](HttpRequest*, HttpResponse* resp) {
        const std::string html = readTextFile("/root/my_ai_app/src/resource/ai.html");
        resp->content_type = TEXT_HTML;
        resp->body = html.empty() ? "<html><body>ai.html not found</body></html>" : html;
        return 200;
    });

    service.POST("/chat/session/new", [this](HttpRequest* req, HttpResponse* resp) {
        req->ParseBody();
        const std::string username = req->GetHeader("X-Auth-User");
        const std::string title = req->GetString("title");
        const std::string sessionId = chat_service_.createSession(username, title);
        resp->json = {{"success", true}, {"sessionId", sessionId}};
        return 200;
    });

    service.POST("/chat/session/delete", [this](HttpRequest* req, HttpResponse* resp) {
        req->ParseBody();
        const std::string username = req->GetHeader("X-Auth-User");
        const std::string sessionId = req->GetString("sessionId");
        const bool ok = chat_service_.deleteSession(username, sessionId);
        resp->json = {{"success", ok}};
        return ok ? 200 : 500;
    });

    service.GET("/chat/sessions", [this](HttpRequest* req, HttpResponse* resp) {
        req->ParseUrl();
        const std::string username = req->GetHeader("X-Auth-User");
        const int limit = (int)req->GetInt("limit", 50);
        const auto sessions = chat_service_.listSessions(username, limit);
        hv::Json arr = hv::Json::array();
        for (const auto& s : sessions) {
            arr.push_back({{"sessionId", s.session_id}, {"title", s.title}, {"createdAtMs", s.created_at_ms}});
        }
        resp->json = {{"success", true}, {"sessions", arr}};
        return 200;
    });

    service.POST("/chat/history", [this](HttpRequest* req, HttpResponse* resp) {
        req->ParseBody();
        const std::string username = req->GetHeader("X-Auth-User");
        const std::string sessionId = req->GetString("sessionId");
        const int limit = (int)req->GetInt("limit", 50);
        const int offset = (int)req->GetInt("offset", 0);
        const auto msgs = chat_service_.getHistory(username, sessionId, limit, offset);
        hv::Json arr = hv::Json::array();
        for (const auto& m : msgs) {
            arr.push_back({{"role", m.role}, {"model", m.model}, {"content", m.content}, {"createdAtMs", m.created_at_ms}});
        }
        resp->json = {{"success", true}, {"messages", arr}};
        return 200;
    });

    service.POST("/chat/send", [this](HttpRequest* req, HttpResponse* resp) {
        req->ParseBody();
        std::string username = req->GetHeader("X-Auth-User");
        std::string sessionId = req->GetString("sessionId");
        std::string question = req->GetString("question");
        std::string modelType = req->GetString("modelType");

        // std::cout<<"[ChatRouter] 收到消息："<<question<<std::endl;
        std::string ai_response = chat_service_.sendMessage(question, modelType);

        const int64_t ts = nowMs();
        ChatMySqlStore::instance().enqueue(chat_message_row{
            username, 
            "user", 
            modelType, 
            question,  
            ts,
            sessionId
        });
        ChatMySqlStore::instance().enqueue(chat_message_row{
            username, 
            "assistant", 
            modelType, 
            ai_response,  
            ts,
            sessionId
        });

        resp->json = {
            {"success", true},
            {"Information", ai_response}
        };
        return 200;
    });
}
