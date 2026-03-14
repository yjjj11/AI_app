#include "router/LoginRouter.h"

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

void LoginRouter::registerRoutes(hv::HttpService& service) {
    service.GET("/", [](HttpRequest*, HttpResponse* resp) {
        const std::string html = readTextFile("/root/my_ai_app/src/resource/login.html");
        resp->content_type = TEXT_HTML;
        resp->body = html.empty() ? "<html><body>login.html not found</body></html>" : html;
        return 200;
    });

    service.POST("/login", [this](HttpRequest* req, HttpResponse* resp) {
        req->ParseBody();
        const std::string username = req->GetString("username");
        const std::string password = req->GetString("password");

        const std::string token = login_service_.login(username, password);
        if (token.empty()) {
            resp->json = {{"success", false}, {"msg", "invalid username or password"}};
            return 400;
        }

        resp->SetHeader("Set-Cookie", "token=" + token + "; Path=/; HttpOnly; SameSite=Lax");
        const std::string ct = req->GetHeader("Content-Type");
        if (ct.find("application/json") != std::string::npos) {
            resp->json = {{"success", true}, {"redirect", "/ai.html"}};
            return 200;
        }

        resp->status_code = HTTP_STATUS_FOUND;
        resp->SetHeader("Location", "/ai.html");
        return 302;
    });
}
