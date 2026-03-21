#include "router/InterviewRouter.h"

#include "ai/LLMFactory.h"
#include "service/ChatMySqlStore.h"
#include "service/MemoryIndexService.h"

#include <algorithm>
#include <chrono>

static int64_t nowMs2() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

static void sseSend2(const ::HttpResponseWriterPtr& writer, const char* event, const hv::Json& payload) {
    writer->SSEvent(payload.dump(), event);
}

void InterviewRouter::registerRoutes(hv::HttpService& service) {
    service.POST("/interview/jd/parse", [this](HttpRequest* req, HttpResponse* resp) {
        req->ParseBody();
        hv::Json body;
        if (!req->body.empty()) body = hv::Json::parse(req->body, nullptr, false);
        if (body.is_discarded() || !body.is_object()) {
            resp->json = {{"ok", false}, {"error", "invalid json"}};
            return 200;
        }
        const std::string jd = body.value("jd", "");
        const std::string direction = body.value("direction", "");
        resp->json = interview_service_.parseJd(jd, direction);
        return 200;
    });

    service.POST("/interview/plan", [this](HttpRequest* req, HttpResponse* resp) {
        req->ParseBody();
        hv::Json body;
        if (!req->body.empty()) body = hv::Json::parse(req->body, nullptr, false);
        if (body.is_discarded() || !body.is_object()) {
            resp->json = {{"ok", false}, {"error", "invalid json"}};
            return 200;
        }
        const std::string jd = body.value("jd", "");
        const std::string resume = body.value("resume", "");
        const std::string direction = body.value("direction", "");
        resp->json = interview_service_.generatePlan(jd, resume, direction);
        return 200;
    });

    service.POST("/interview/review", [this](HttpRequest* req, HttpResponse* resp) {
        req->ParseBody();
        hv::Json body;
        if (!req->body.empty()) body = hv::Json::parse(req->body, nullptr, false);
        if (body.is_discarded() || !body.is_object()) {
            resp->json = {{"ok", false}, {"error", "invalid json"}};
            return 200;
        }
        std::string username = req->GetHeader("X-Auth-User");
        if (username.empty()) username = "unknown";
        const std::string sessionId = body.value("sessionId", "default");
        const std::string direction = body.value("direction", "C++后端");
        resp->json = interview_service_.reviewSession(username, sessionId, direction);
        MemoryIndexService::instance().maybeEnqueue(username, sessionId);
        return 200;
    });

    service.POST("/interview/turn", [this](const ::HttpRequestPtr& req, const ::HttpResponseWriterPtr& writer) {
        req->ParseBody();
        hv::Json body;
        if (!req->body.empty()) body = hv::Json::parse(req->body, nullptr, false);
        if (body.is_discarded() || !body.is_object()) {
            writer->Begin();
            writer->WriteStatus(HTTP_STATUS_OK);
            writer->WriteHeader("Content-Type", "text/event-stream");
            writer->WriteHeader("Cache-Control", "no-cache");
            writer->WriteHeader("Connection", "keep-alive");
            writer->EndHeaders();
            sseSend2(writer, "error", {{"error", "invalid json"}});
            writer->End();
            return;
        }

        std::string username = req->GetHeader("X-Auth-User");
        if (username.empty()) username = "unknown";
        std::string sessionId = body.value("sessionId", "default");
        if (sessionId.empty()) sessionId = "default";
        const std::string jd = body.value("jd", "");
        const std::string resume = body.value("resume", "");
        const std::string direction = body.value("direction", "C++后端");
        const std::string answer = body.value("answer", "");

        const std::string modelKey = LLMFactory::getActiveKey().empty() ? "default" : LLMFactory::getActiveKey();
        const int64_t ts_user = nowMs2();
        ChatMySqlStore::instance().enqueue(chat_message_row{
            username,
            "user",
            modelKey,
            answer.empty() ? "[interview_start]" : answer,
            ts_user,
            sessionId
        });

        writer->Begin();
        writer->WriteStatus(HTTP_STATUS_OK);
        writer->WriteHeader("Content-Type", "text/event-stream");
        writer->WriteHeader("Cache-Control", "no-cache");
        writer->WriteHeader("Connection", "keep-alive");
        writer->EndHeaders();

        std::string aggregated;
        const std::string out = interview_service_.streamTurn(
            sessionId,
            jd,
            resume,
            direction,
            answer,
            [&](std::string_view delta) {
                hv::Json payload;
                payload["delta"] = std::string(delta);
                sseSend2(writer, "delta", payload);
                aggregated.append(delta.data(), delta.size());
            }
        );

        const std::string final_text = aggregated.empty() ? out : aggregated;
        if (final_text.empty()) {
            sseSend2(writer, "error", {{"error", "LLM request failed"}});
            writer->End();
            return;
        }

        const int64_t ts_ai = nowMs2();
        ChatMySqlStore::instance().enqueue(chat_message_row{
            username,
            "assistant",
            modelKey,
            final_text,
            ts_ai,
            sessionId
        });
        MemoryIndexService::instance().maybeEnqueue(username, sessionId);

        sseSend2(writer, "done", {{"done", true}, {"len", (int)final_text.size()}});
        writer->End();
    });
}
