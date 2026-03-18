#include "ai/BailianLLM.h"

#include <cstdlib>
#include <iostream>
#include <utility>

#include "httplib.h"

#include <hv/axios.h>
#include <hv/HttpClient.h>

struct ParsedUrl {
    bool https{false};
    std::string host;
    int port{0};
    std::string path;
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
    out.path = (slash == std::string::npos) ? "/" : s.substr(slash);
    if (out.path.empty()) out.path = "/";

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

BailianLLM::BailianLLM(std::string api_key, std::string base_url, std::string default_model, int timeout_seconds)
    : api_key_(std::move(api_key)),
      base_url_(std::move(base_url)),
      default_model_(std::move(default_model)),
      timeout_seconds_(timeout_seconds > 0 ? timeout_seconds : 30) {}

std::string BailianLLM::chat_messages(const hv::Json& messages) {
    if (api_key_.empty() || base_url_.empty()) return "";

    if (default_model_.empty()) return "";

    // {
    //     std::string preview = prompt.substr(0, 120);
    //     for (auto& ch : preview) {
    //         if (ch == '\n' || ch == '\r' || ch == '\t') ch = ' ';
    //     }
    //     std::cerr << "[BailianLLM] request\n";
    //     std::cerr << "  url=" << base_url_ << "\n";
    //     std::cerr << "  model=" << model << "\n";
    //     std::cerr << "  timeout=" << timeout_seconds_ << "s\n";
    //     std::cerr << "  api_key_set=" << (api_key_.empty() ? "false" : "true") << " len=" << api_key_.size() << "\n";
    //     std::cerr << "  prompt_len=" << prompt.size() << " preview=\"" << preview << "\"\n";
    // }

    hv::Json req;
    req["timeout"] = timeout_seconds_;
    req["headers"] = {
        {"Authorization", std::string("Bearer ") + api_key_},
        {"Content-Type", "application/json"}
    };
    req["body"] = {
        {"model", default_model_},
        {"messages", messages}
    };

    auto httpReq = axios::newRequestFromJson(req);
    httpReq->url = base_url_;
    auto resp = std::make_shared<HttpResponse>();
    const int ret = http_client_send(httpReq.get(), resp.get());
    if (ret != 0) {
        std::cerr << "[BailianLLM] http_client_send failed: code=" << ret << " msg=" << http_client_strerror(ret) << "\n";
        return "";
    }

    // std::cerr << "[BailianLLM] response\n";
    // std::cerr << "  status=" << resp->status_code << " " << resp->status_message() << "\n";
    // std::cerr << "  body_len=" << resp->body.size() << "\n";

    if (resp->status_code < 200 || resp->status_code >= 300) {
        std::string body_preview = resp->body.substr(0, 1200);
        std::cerr << "  body_preview=" << body_preview << "\n";
        return "";
    }

    auto j = hv::Json::parse(resp->body, nullptr, false);
    if (j.is_discarded()) {
        std::cerr << "[BailianLLM] json parse failed\n";
        std::string body_preview = resp->body.substr(0, 1200);
        std::cerr << "  body_preview=" << body_preview << "\n";
        return "";
    }

    if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
        const auto& c0 = j["choices"][0];
        if (c0.contains("message") && c0["message"].is_object()) {
            const auto& msg = c0["message"];
            if (msg.contains("content") && msg["content"].is_string()) {
                return msg["content"].get<std::string>();
            }
        }
        if (c0.contains("text") && c0["text"].is_string()) {
            return c0["text"].get<std::string>();
        }
    }

    if (j.contains("output") && j["output"].is_object()) {
        const auto& out = j["output"];
        if (out.contains("text") && out["text"].is_string()) {
            return out["text"].get<std::string>();
        }
    }

    // std::cerr << "[BailianLLM] json parsed but no content field matched\n";
    // std::string body_preview = resp->body.substr(0, 1200);
    // std::cerr << "  body_preview=" << body_preview << "\n";
    return "";
}

std::string BailianLLM::stream_chat_messages(const hv::Json& messages,
                                             const std::function<void(std::string_view)>& on_delta) {
    if (api_key_.empty() || base_url_.empty()) return "";

    if (default_model_.empty()) return "";

    const ParsedUrl u = parseUrl(base_url_);
    httplib::Client cli(originFromParsed(u));

    cli.set_follow_location(true);
    cli.set_connection_timeout(timeout_seconds_, 0);
    cli.set_read_timeout(timeout_seconds_, 0);
    cli.set_write_timeout(timeout_seconds_, 0);

    httplib::Headers headers = {
        {"Authorization", std::string("Bearer ") + api_key_},
        {"Content-Type", "application/json"},
        {"Accept", "text/event-stream"}
    };

    hv::Json body;
    body["model"] = default_model_;
    body["stream"] = true;
    body["messages"] = messages;

    const std::string body_str = body.dump();
    std::string aggregated;
    std::string sse_buf;
    bool done = false;

    const auto drain_events = [&]() {
        while (true) {
            size_t pos = sse_buf.find("\n\n");
            size_t dlen = 2;
            const size_t pos2 = sse_buf.find("\r\n\r\n");
            if (pos == std::string::npos || (pos2 != std::string::npos && pos2 < pos)) {
                pos = pos2;
                dlen = 4;
            }
            if (pos == std::string::npos) break;

            std::string event = sse_buf.substr(0, pos);
            sse_buf.erase(0, pos + dlen);

            size_t start = 0;
            while (start < event.size()) {
                size_t end = event.find('\n', start);
                if (end == std::string::npos) end = event.size();
                std::string line = event.substr(start, end - start);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                start = end + 1;

                if (line.rfind("data:", 0) != 0) continue;
                std::string payload = line.substr(5);
                while (!payload.empty() && (payload[0] == ' ' || payload[0] == '\t')) payload.erase(0, 1);
                if (payload == "[DONE]") {
                    done = true;
                    break;
                }

                auto j = hv::Json::parse(payload, nullptr, false);
                if (j.is_discarded()) continue;
                std::string delta;
                if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
                    const auto& c0 = j["choices"][0];
                    if (c0.contains("delta") && c0["delta"].is_object()) {
                        const auto& d = c0["delta"];
                        if (d.contains("content") && d["content"].is_string()) {
                            delta = d["content"].get<std::string>();
                        }
                    }
                }
                if (delta.empty()) continue;
                aggregated += delta;
                if (on_delta) on_delta(delta);
            }

            if (done) break;
        }
    };

    auto res = cli.Post(
        u.path.c_str(),
        headers,
        body_str,
        "application/json",
        [&](const char* data, uint64_t data_len) {
            if (done) return false;
            if (data && data_len) {
                sse_buf.append(data, (size_t)data_len);
                drain_events();
            }
            return !done;
        }
    );

    drain_events();

    if (!res) {
        if (!aggregated.empty()) return aggregated;
        std::cerr << "Request failed, status: -1\n";
        return "";
    }
    if (res->status != 200) {
        if (!aggregated.empty()) return aggregated;
        std::cerr << "Request failed, status: " << res->status << "\n";
        if (!res->body.empty()) {
            std::cerr << res->body.substr(0, 1200) << "\n";
        }
        return "";
    }
    return aggregated;
}
