#include "router/SettingRouter.h"

#include <fstream>
#include <sstream>
#include <string>

static std::string readTextFile(const std::string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in.is_open()) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void SettingRouter::registerRoutes(hv::HttpService& service) {
    service.GET("/settings.html", [](HttpRequest*, HttpResponse* resp) {
        const std::string html = readTextFile("/root/my_ai_app/src/resource/settings.html");
        resp->content_type = TEXT_HTML;
        resp->body = html.empty() ? "<html><body>settings.html not found</body></html>" : html;
        return 200;
    });

    service.GET("/llm/profiles", [this](HttpRequest*, HttpResponse* resp) {
        resp->json = setting_service_.listLlmProfiles();
        return 200;
    });

    service.POST("/llm/config", [this](HttpRequest* req, HttpResponse* resp) {
        req->ParseBody();
        hv::Json body;
        if (!req->body.empty()) {
            body = hv::Json::parse(req->body, nullptr, false);
        }
        if (body.is_discarded() || !body.is_object()) {
            resp->json = {{"success", false}, {"message", "invalid json"}};
            return 200;
        }

        hv::Json out;
        int code = 200;
        setting_service_.applyLlmConfig(body, out, code);
        resp->json = std::move(out);
        return 200;
    });

    service.POST("/llm/active", [this](HttpRequest* req, HttpResponse* resp) {
        req->ParseBody();
        hv::Json body;
        if (!req->body.empty()) {
            body = hv::Json::parse(req->body, nullptr, false);
        }
        if (body.is_discarded() || !body.is_object()) {
            resp->json = {{"success", false}, {"message", "invalid json"}};
            return 200;
        }
        resp->json = setting_service_.setActiveProfile(body);
        return 200;
    });
}
