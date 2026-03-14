#include "service/LoginService.h"

#include <jwt-cpp/jwt.h>

#include <chrono>
#include <cstdlib>
#include <string>

static std::string getJwtSecret() {
    const char* v = std::getenv("JWT_SECRET");
    if (v && *v) return std::string(v);
    return "dev_secret";
}

std::string LoginService::login(const std::string& username, const std::string& password) {
    if (username != "admin") return "";
    if (password != "123456") return "";
    const auto now = std::chrono::system_clock::now();
    const auto exp = now + std::chrono::hours(24 * 7);
    return jwt::create()
        .set_type("JWT")
        .set_subject(username)
        .set_issued_at(now)
        .set_expires_at(exp)
        .sign(jwt::algorithm::hs256{getJwtSecret()});
}
