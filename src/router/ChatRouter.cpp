#include "router/ChatRouter.h"
#include "service/ChatMySqlStore.h"
#include "service/MemoryIndexService.h"
#include "ai/LLMFactory.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <string_view>

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

    service.POST("/chat/send", [this](const ::HttpRequestPtr& req, const ::HttpResponseWriterPtr& writer) {
        req->ParseBody();
        std::string username = req->GetHeader("X-Auth-User");
        std::string sessionId;
        std::string question;
        std::vector<std::string> images;

        hv::Json body;
        if (!req->body.empty()) {
            body = hv::Json::parse(req->body, nullptr, false);
        }
        if (body.is_object()) {
            if (body.contains("sessionId") && body["sessionId"].is_string()) sessionId = body["sessionId"].get<std::string>();
            if (body.contains("question") && body["question"].is_string()) question = body["question"].get<std::string>();
            if (body.contains("images") && body["images"].is_array()) {
                const auto& arr = body["images"];
                const size_t n = std::min<size_t>(arr.size(), 4);
                images.reserve(n);
                for (size_t i = 0; i < n; i++) {
                    if (!arr[i].is_string()) continue;
                    std::string s = arr[i].get<std::string>();
                    if (s.size() > 2 * 1024 * 1024) continue;
                    images.push_back(std::move(s));
                }
            }
        } else {
            sessionId = req->GetString("sessionId");
            question = req->GetString("question");
        }

        if (username.empty()) username = "unknown";
        if (sessionId.empty()) sessionId = "default";
        const std::string modelKey = LLMFactory::getActiveKey().empty() ? "default" : LLMFactory::getActiveKey();

        writer->Begin();
        writer->WriteStatus(HTTP_STATUS_OK);
        writer->WriteHeader("Content-Type", "text/event-stream");
        writer->WriteHeader("Cache-Control", "no-cache");
        writer->WriteHeader("Connection", "keep-alive");
        writer->EndHeaders();

        if (!images.empty()) {
            bool supports_vision = false;
            for (const auto& p : LLMFactory::listProfiles()) {
                if (p.key == modelKey) {
                    supports_vision = p.capabilities.vision;
                    break;
                }
            }
            if (!supports_vision) {
                hv::Json payload;
                payload["error"] = "当前模型不支持图片输入，请切换到支持多模态的模型或移除图片后再发送。";
                writer->SSEvent(payload.dump(), "error");
                writer->End();
                return;
            }
        }

        const int64_t ts_user = nowMs();
        ChatMySqlStore::instance().enqueue(chat_message_row{
            username,
            "user",
            modelKey,
            question,
            ts_user,
            sessionId
        });
        std::string aggregated;
        const std::string out = chat_service_.streamMessage(
            question,
            images,
            [&](std::string_view delta) {
                hv::Json payload;
                payload["delta"] = std::string(delta);
                writer->SSEvent(payload.dump(), "delta");
                aggregated.append(delta.data(), delta.size());
            }
        );

        if (aggregated.empty() && out.empty()) {
            hv::Json payload;
            payload["error"] = "LLM request failed";
            writer->SSEvent(payload.dump(), "error");
            writer->End();
            return;
        }

        const std::string final_text = !aggregated.empty() ? aggregated : out;
        const int64_t ts_ai = nowMs();
        ChatMySqlStore::instance().enqueue(chat_message_row{
            username,
            "assistant",
            modelKey,
            final_text,
            ts_ai,
            sessionId
        });
        MemoryIndexService::instance().maybeEnqueue(username, sessionId);

        hv::Json payload;
        payload["done"] = true;
        payload["len"] = (int)final_text.size();
        writer->SSEvent(payload.dump(), "done");
        writer->End();
    });
}
