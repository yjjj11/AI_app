#include "ai/BailianLLM.h"

#include <cstdlib>
#include <utility>

#include <hv/axios.h>

static std::string pickModel(const std::string& req_model, const std::string& default_model) {
    return req_model.empty() ? default_model : req_model;
}

BailianLLM::BailianLLM(std::string api_key, std::string base_url, std::string default_model, int timeout_seconds)
    : api_key_(std::move(api_key)),
      base_url_(std::move(base_url)),
      default_model_(std::move(default_model)),
      timeout_seconds_(timeout_seconds > 0 ? timeout_seconds : 30) {}

std::string BailianLLM::chat(const std::string& prompt, const std::string& model_type) {
    if (api_key_.empty() || base_url_.empty()) return "";

    const std::string model = pickModel(model_type, default_model_);
    if (model.empty()) return "";

    nlohmann::json req;
    req["timeout"] = timeout_seconds_;
    req["headers"] = {
        {"Authorization", std::string("Bearer ") + api_key_},
        {"Content-Type", "application/json"}
    };
    req["body"] = {
        {"model", model},
        {"messages", nlohmann::json::array({{
            {"role", "user"},
            {"content", prompt}
        }})}
    };

    auto resp = axios::post(base_url_.c_str(), req);
    if (!resp) return "";
    if (resp->status_code < 200 || resp->status_code >= 300) return "";

    auto j = nlohmann::json::parse(resp->body, nullptr, false);
    if (j.is_discarded()) return "";

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

    return "";
}

