#include "agentic/EmbeddingClient.h"

#include <cstdlib>
#include <iostream>
#include <utility>

#include <hv/axios.h>
#include <hv/HttpClient.h>

EmbeddingClient::EmbeddingClient(std::string api_key,
                                 std::string embeddings_url,
                                 std::string model,
                                 int timeout_seconds,
                                 int dimensions)
    : api_key_(std::move(api_key)),
      embeddings_url_(std::move(embeddings_url)),
      model_(std::move(model)),
      timeout_seconds_(timeout_seconds > 0 ? timeout_seconds : 30),
      dimensions_(dimensions) {}

EmbeddingResult EmbeddingClient::embedBatch(const std::vector<std::string>& inputs) const {
    EmbeddingResult r;
    if (inputs.empty()) {
        r.ok = true;
        r.dimensions = dimensions_;
        return r;
    }
    if (embeddings_url_.empty() || model_.empty()) {
        r.ok = false;
        r.error = "embedding url/model not configured";
        return r;
    }
    if (api_key_.empty()) {
        r.ok = false;
        r.error = "embedding api key missing";
        return r;
    }

    hv::Json body;
    body["model"] = model_;
    if (dimensions_ > 0) body["dimensions"] = dimensions_;
    hv::Json arr = hv::Json::array();
    for (const auto& s : inputs) {
        arr.push_back(s);
    }
    body["input"] = arr;

    hv::Json req;
    req["timeout"] = timeout_seconds_;
    req["headers"] = {
        {"Authorization", std::string("Bearer ") + api_key_},
        {"Content-Type", "application/json"}
    };
    req["body"] = body;

    auto httpReq = axios::newRequestFromJson(req);
    httpReq->url = embeddings_url_;
    auto resp = std::make_shared<HttpResponse>();
    const int ret = http_client_send(httpReq.get(), resp.get());
    if (ret != 0) {
        r.ok = false;
        r.error = std::string("http_client_send failed: ") + http_client_strerror(ret);
        return r;
    }
    if (resp->status_code < 200 || resp->status_code >= 300) {
        r.ok = false;
        r.error = "http status=" + std::to_string(resp->status_code) + " body=" + resp->body.substr(0, 600);
        return r;
    }

    auto j = hv::Json::parse(resp->body, nullptr, false);
    if (j.is_discarded()) {
        r.ok = false;
        r.error = "json parse failed";
        return r;
    }
    if (!j.contains("data") || !j["data"].is_array()) {
        r.ok = false;
        r.error = "invalid response: data missing";
        return r;
    }
    const auto& data = j["data"];
    r.vectors.reserve(data.size());
    int dim = 0;
    for (const auto& item : data) {
        if (!item.is_object()) continue;
        if (!item.contains("embedding") || !item["embedding"].is_array()) continue;
        const auto& e = item["embedding"];
        std::vector<float> v;
        v.reserve(e.size());
        for (const auto& x : e) {
            if (x.is_number_float()) v.push_back((float)x.get<double>());
            else if (x.is_number_integer()) v.push_back((float)x.get<long long>());
        }
        if (dim == 0) dim = (int)v.size();
        r.vectors.push_back(std::move(v));
    }
    r.ok = !r.vectors.empty();
    r.dimensions = dim;
    if (!r.ok) r.error = "no embeddings parsed";
    return r;
}
