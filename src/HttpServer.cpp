#include "HttpServer.h"

#include <cstdio>
#include <memory>
#include <string>
#include <cstdlib>

#include "router/ChatRouter.h"
#include "router/DebugRouter.h"
#include "router/InterviewRouter.h"
#include "router/LoginRouter.h"
#include "router/SettingRouter.h"
#include "service/ChatMySqlStore.h"
#include "service/MemoryIndexService.h"

#include <jwt-cpp/jwt.h>

static bool getenvTruthy(const char* key) {
    const char* v = std::getenv(key);
    if (!v || !*v) return false;
    if (std::string(v) == "0") return false;
    if (std::string(v) == "false") return false;
    return true;
}

static std::string getenvOr(const char* key, const char* defval) {
    const char* v = std::getenv(key);
    if (v && *v) return std::string(v);
    return std::string(defval ? defval : "");
}

static size_t getenvSizeOr(const char* key, size_t defval) {
    const char* v = std::getenv(key);
    if (!v || !*v) return defval;
    const long long x = std::atoll(v);
    if (x <= 0) return defval;
    return (size_t)x;
}

static std::string getJwtSecret() {
    const char* v = std::getenv("JWT_SECRET");
    if (v && *v) return std::string(v);
    static bool warned = false;
    if (!warned) {
        warned = true;
        std::fprintf(stderr, "[Security] JWT_SECRET not set, using dev_secret\n");
    }
    return "dev_secret";
}

static std::string verifyJwtAndGetUser(const std::string& token) {
    try {
        const auto decoded = jwt::decode(token);
        jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{getJwtSecret()})
            .verify(decoded);
        if (decoded.has_expires_at()) {
            if (std::chrono::system_clock::now() > decoded.get_expires_at()) return "";
        }
        return decoded.get_subject();
    } catch (...) {
        return "";
    }
}

MyHttpServer::MyHttpServer(int port, int thread_num)
    : port_(port), thread_num_(thread_num) {
    service_.base_url = "";

    server_.setPort(port_);
    server_.setThreadNum(thread_num_);

    setupMiddleware();
    registerRoutes();

    ChatMySqlStore::instance().init();
    MemoryIndexService::instance().init();

    server_.registerHttpService(&service_);
}

void MyHttpServer::setupMiddleware() {
    service_.Use([](HttpRequest* req, HttpResponse* resp) {
        const std::string allow_origin = getenvOr("CORS_ALLOW_ORIGIN", "*");
        resp->SetHeader("Access-Control-Allow-Origin", allow_origin);
        resp->SetHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        resp->SetHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
        resp->SetHeader("Access-Control-Max-Age", "3600");
        if (getenvTruthy("CORS_ALLOW_CREDENTIALS") && allow_origin != "*") {
            resp->SetHeader("Access-Control-Allow-Credentials", "true");
        }

        if (req->method == HTTP_OPTIONS) {
            return 204;
        }
        return HTTP_STATUS_NEXT;
    });

    service_.Use([](HttpRequest* req, HttpResponse* resp) {
        const size_t max_body = getenvSizeOr("MAX_BODY_BYTES", 12 * 1024 * 1024);
        if (req->method != HTTP_GET && req->method != HTTP_OPTIONS) {
            if (req->body.size() > max_body) {
                resp->status_code = HTTP_STATUS_PAYLOAD_TOO_LARGE;
                resp->json = {{"code", 413}, {"msg", "Payload Too Large"}};
                return 413;
            }
        }

        const bool enable_debug = getenvTruthy("ENABLE_DEBUG_ROUTES");
        if (req->path == "/" || req->path == "/login" || req->path == "/health" || req->path.rfind("/assets/", 0) == 0 || (enable_debug && req->path.rfind("/debug/", 0) == 0)) {
            return HTTP_STATUS_NEXT;
        }

        std::string auth_header = req->GetHeader("Authorization");
        std::string token;

        if (!auth_header.empty() && auth_header.find("Bearer ") == 0) {
            token = auth_header.substr(7);
        } else {
            const HttpCookie& cookie_auth = req->GetCookie("token");
            token = cookie_auth.value;
        }

        if (token.empty()) {
            if (req->path == "/" || req->path == "/ai.html" || req->path == "/settings.html") {
                resp->status_code = HTTP_STATUS_FOUND;
                resp->SetHeader("Location", "/");
                return 302;
            }
            resp->status_code = HTTP_STATUS_UNAUTHORIZED;
            resp->json = {{"code", 401}, {"msg", "Unauthorized: No token provided"}};
            return 401;
        }

        std::string username = verifyJwtAndGetUser(token);
        if (username.empty()) {
            if (req->path == "/" || req->path == "/ai.html" || req->path == "/settings.html") {
                resp->status_code = HTTP_STATUS_FOUND;
                resp->SetHeader("Location", "/");
                return 302;
            }
            resp->status_code = HTTP_STATUS_UNAUTHORIZED;
            resp->json = {{"code", 401}, {"msg", "Unauthorized: Invalid or expired token"}};
            return 401;
        }

        req->SetHeader("X-Auth-User", username);
        return HTTP_STATUS_NEXT;
    });
}

void MyHttpServer::registerRoutes() {
    routers_.emplace_back(std::make_unique<LoginRouter>());
    routers_.emplace_back(std::make_unique<ChatRouter>());
    routers_.emplace_back(std::make_unique<InterviewRouter>());
    routers_.emplace_back(std::make_unique<SettingRouter>());
    if (getenvTruthy("ENABLE_DEBUG_ROUTES")) {
        routers_.emplace_back(std::make_unique<DebugRouter>());
    }

    for (auto& router : routers_) {
        router->registerRoutes(service_);
    }
}

void MyHttpServer::run() {
    std::printf("HTTP 服务器启动：http://127.0.0.1:%d\n", port_);
    server_.run();
}
