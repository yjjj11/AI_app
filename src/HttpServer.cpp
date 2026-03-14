#include "HttpServer.h"

#include <cstdio>
#include <memory>
#include <string>
#include <cstdlib>

#include "router/ChatRouter.h"
#include "router/LoginRouter.h"
#include "service/ChatMySqlStore.h"

#include <jwt-cpp/jwt.h>

static std::string getJwtSecret() {
    const char* v = std::getenv("JWT_SECRET");
    if (v && *v) return std::string(v);
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

    server_.registerHttpService(&service_);
}

void MyHttpServer::setupMiddleware() {
    service_.Use([](HttpRequest* req, HttpResponse* resp) {
        resp->SetHeader("Access-Control-Allow-Origin", "*");
        resp->SetHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        resp->SetHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
        resp->SetHeader("Access-Control-Max-Age", "3600");

        if (req->method == HTTP_OPTIONS) {
            return 204;
        }
        return HTTP_STATUS_NEXT;
    });

    service_.Use([](HttpRequest* req, HttpResponse* resp) {
        if (req->path == "/" || req->path == "/login" || req->path == "/health") {
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
            if (req->path == "/" || req->path == "/ai.html") {
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
            if (req->path == "/" || req->path == "/ai.html") {
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

    for (auto& router : routers_) {
        router->registerRoutes(service_);
    }
}

void MyHttpServer::run() {
    std::printf("HTTP 服务器启动：http://127.0.0.1:%d\n", port_);
    server_.run();
}
