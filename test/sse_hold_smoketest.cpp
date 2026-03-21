#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <string>

#include "httplib.h"

struct ParsedUrl {
    bool https{false};
    std::string host;
    int port{0};
};

static ParsedUrl parseUrl(const std::string& url) {
    ParsedUrl out;
    std::string s = url;
    if (s.rfind("https://", 0) == 0) {
        out.https = true;
        s.erase(0, 8);
    } else if (s.rfind("http://", 0) == 0) {
        out.https = false;
        s.erase(0, 7);
    }
    const size_t slash = s.find('/');
    std::string hostport = (slash == std::string::npos) ? s : s.substr(0, slash);
    const size_t colon = hostport.rfind(':');
    if (colon != std::string::npos) {
        out.host = hostport.substr(0, colon);
        out.port = std::atoi(hostport.substr(colon + 1).c_str());
    } else {
        out.host = hostport;
        out.port = out.https ? 443 : 80;
    }
    return out;
}

static std::string originFromParsed(const ParsedUrl& u) {
    std::string origin = (u.https ? "https://" : "http://") + u.host;
    if ((u.https && u.port != 443) || (!u.https && u.port != 80)) {
        origin += ":" + std::to_string(u.port);
    }
    return origin;
}

static std::string getenvOr(const char* key, const std::string& defval) {
    const char* v = std::getenv(key);
    if (!v || !*v) return defval;
    return std::string(v);
}

static int getenvIntOr(const char* key, int defval) {
    const char* v = std::getenv(key);
    if (!v || !*v) return defval;
    return std::atoi(v);
}

static std::string trim(const std::string& s) {
    size_t l = 0;
    while (l < s.size() && (s[l] == ' ' || s[l] == '\t' || s[l] == '\r' || s[l] == '\n')) l++;
    size_t r = s.size();
    while (r > l && (s[r - 1] == ' ' || s[r - 1] == '\t' || s[r - 1] == '\r' || s[r - 1] == '\n')) r--;
    return s.substr(l, r - l);
}

static std::string extractTokenFromSetCookie(const std::string& set_cookie) {
    if (set_cookie.empty()) return "";
    const size_t p = set_cookie.find("token=");
    if (p == std::string::npos) return "";
    size_t i = p + 6;
    size_t j = i;
    while (j < set_cookie.size() && set_cookie[j] != ';' && set_cookie[j] != ',' && set_cookie[j] != ' ') j++;
    if (j <= i) return "";
    return set_cookie.substr(i, j - i);
}

struct SseStats {
    int delta_count = 0;
    bool done = false;
};

static void parseSseFrames(std::string& buf, SseStats& stats) {
    while (true) {
        size_t pos = buf.find("\n\n");
        size_t dlen = 2;
        const size_t pos2 = buf.find("\r\n\r\n");
        if (pos == std::string::npos || (pos2 != std::string::npos && pos2 < pos)) {
            pos = pos2;
            dlen = 4;
        }
        if (pos == std::string::npos) break;

        std::string frame = buf.substr(0, pos);
        buf.erase(0, pos + dlen);
        frame.erase(std::remove(frame.begin(), frame.end(), '\r'), frame.end());

        std::string event;
        size_t start = 0;
        while (start < frame.size()) {
            size_t end = frame.find('\n', start);
            if (end == std::string::npos) end = frame.size();
            std::string line = frame.substr(start, end - start);
            start = end + 1;
            if (line.rfind("event:", 0) == 0) {
                event = trim(line.substr(6));
            }
        }

        if (event == "delta") stats.delta_count++;
        if (event == "done") stats.done = true;
    }
}

int main(int argc, char** argv) {
    const std::string base_url = getenvOr("HTTP_BASE_URL", "http://127.0.0.1:8888");
    int seconds = getenvIntOr("SSE_SECONDS", 10);
    if (argc >= 2) seconds = std::atoi(argv[1]);
    seconds = std::max(1, std::min(3600, seconds));

    const ParsedUrl u = parseUrl(base_url);
    httplib::Client cli(originFromParsed(u));
    cli.set_follow_location(true);
    cli.set_connection_timeout(10, 0);
    cli.set_read_timeout(seconds + 20, 0);
    cli.set_write_timeout(10, 0);

    const auto t0 = std::chrono::steady_clock::now();

    SseStats stats;
    std::string sse_buf;
    httplib::Headers headers = {
        {"Accept", "text/event-stream"}
    };
    const std::string path = "/debug/sse/hold?seconds=" + std::to_string(seconds);
    auto res = cli.Get(
        path.c_str(),
        headers,
        [&](const char* data, size_t data_len) {
            if (data && data_len) {
                sse_buf.append(data, data_len);
                parseSseFrames(sse_buf, stats);
            }
            return !stats.done;
        }
    );

    parseSseFrames(sse_buf, stats);
    if (!res) {
        const auto t1 = std::chrono::steady_clock::now();
        const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        if (stats.done && stats.delta_count >= seconds) {
            std::cout << "sse_ok=true\n";
            std::cout << "seconds=" << seconds << "\n";
            std::cout << "delta_count=" << stats.delta_count << "\n";
            std::cout << "done=true\n";
            std::cout << "duration_ms=" << duration_ms << "\n";
            return 0;
        }
        std::cout << "sse_ok=false\n";
        std::cout << "error=sse request failed\n";
        std::cout << "delta_count=" << stats.delta_count << "\n";
        std::cout << "done=" << (stats.done ? "true" : "false") << "\n";
        std::cout << "duration_ms=" << duration_ms << "\n";
        return 2;
    }
    const auto t1 = std::chrono::steady_clock::now();
    const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    if (res->status != 200) {
        std::cout << "sse_ok=false\n";
        std::cout << "error=sse http status=" << res->status << " body=" << res->body.substr(0, 200) << "\n";
        std::cout << "delta_count=" << stats.delta_count << "\n";
        std::cout << "done=" << (stats.done ? "true" : "false") << "\n";
        std::cout << "duration_ms=" << duration_ms << "\n";
        return 2;
    }
    const std::string ct = res->get_header_value("Content-Type");
    if (ct.find("text/event-stream") == std::string::npos) {
        std::cout << "sse_ok=false\n";
        std::cout << "error=content-type not text/event-stream\n";
        std::cout << "delta_count=" << stats.delta_count << "\n";
        std::cout << "done=" << (stats.done ? "true" : "false") << "\n";
        std::cout << "duration_ms=" << duration_ms << "\n";
        return 2;
    }
    if (!stats.done) {
        std::cout << "sse_ok=false\n";
        std::cout << "error=missing done event\n";
        std::cout << "delta_count=" << stats.delta_count << "\n";
        std::cout << "done=false\n";
        std::cout << "duration_ms=" << duration_ms << "\n";
        return 2;
    }

    std::cout << "sse_ok=true\n";
    std::cout << "seconds=" << seconds << "\n";
    std::cout << "delta_count=" << stats.delta_count << "\n";
    std::cout << "done=true\n";
    std::cout << "duration_ms=" << duration_ms << "\n";
    return 0;
}
