#include "router/LoginRouter.h"

#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>

static bool getenvTruthy2(const char* key) {
    const char* v = std::getenv(key);
    if (!v || !*v) return false;
    if (std::string(v) == "0") return false;
    if (std::string(v) == "false") return false;
    return true;
}

static std::string readTextFile(const std::string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in.is_open()) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void LoginRouter::registerRoutes(hv::HttpService& service) {
    service.GET("/assets/theme.css", [](HttpRequest*, HttpResponse* resp) {
        const std::string css = readTextFile("/root/my_ai_app/src/resource/assets/theme.css");
        resp->content_type = TEXT_PLAIN;
        resp->SetHeader("Content-Type", "text/css; charset=utf-8");
        resp->body = css.empty() ? "" : css;
        return css.empty() ? 404 : 200;
    });

    service.GET("/assets/theme.js", [](HttpRequest*, HttpResponse* resp) {
        const std::string js = readTextFile("/root/my_ai_app/src/resource/assets/theme.js");
        resp->content_type = TEXT_PLAIN;
        resp->SetHeader("Content-Type", "application/javascript; charset=utf-8");
        resp->body = js.empty() ? "" : js;
        return js.empty() ? 404 : 200;
    });

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

        std::string cookie = "token=" + token + "; Path=/; HttpOnly; SameSite=Lax";
        if (getenvTruthy2("COOKIE_SECURE")) cookie += "; Secure";
        resp->SetHeader("Set-Cookie", cookie);
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
