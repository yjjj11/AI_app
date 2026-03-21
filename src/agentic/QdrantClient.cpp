#include "agentic/QdrantClient.h"

#include <hv/axios.h>
#include <hv/HttpClient.h>

#include <utility>

static http_method toMethod(const std::string& m) {
    if (m == "GET") return HTTP_GET;
    if (m == "POST") return HTTP_POST;
    if (m == "PUT") return HTTP_PUT;
    if (m == "DELETE") return HTTP_DELETE;
    return HTTP_POST;
}

static std::string normalizeDistance(const std::string& d) {
    std::string s = d;
    for (auto& ch : s) ch = (char)std::tolower((unsigned char)ch);
    if (s == "cosine") return "Cosine";
    if (s == "dot") return "Dot";
    if (s == "euclid" || s == "euclidean") return "Euclid";
    return d;
}

static std::string toLower(const std::string& d) {
    std::string s = d;
    for (auto& ch : s) ch = (char)std::tolower((unsigned char)ch);
    return s;
}

static QdrantOpResult sendJson(const std::string& method,
                              const std::string& url,
                              int timeout_seconds,
                              const std::string& api_key,
                              const hv::Json* body) {
    QdrantOpResult r;
    hv::Json req;
    req["timeout"] = timeout_seconds > 0 ? timeout_seconds : 30;
    hv::Json headers = hv::Json::object();
    headers["Content-Type"] = "application/json";
    if (!api_key.empty()) headers["api-key"] = api_key;
    req["headers"] = headers;
    if (body) req["body"] = *body;

    auto httpReq = axios::newRequestFromJson(req);
    httpReq->method = toMethod(method);
    httpReq->url = url;
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
    r.ok = true;
    r.data = std::move(j);
    return r;
}

static QdrantOpResult sendRaw(const std::string& method,
                             const std::string& url,
                             int timeout_seconds,
                             const std::string& api_key) {
    QdrantOpResult r;
    hv::Json req;
    req["timeout"] = timeout_seconds > 0 ? timeout_seconds : 30;
    hv::Json headers = hv::Json::object();
    if (!api_key.empty()) headers["api-key"] = api_key;
    req["headers"] = headers;
    auto httpReq = axios::newRequestFromJson(req);
    httpReq->method = toMethod(method);
    httpReq->url = url;
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
    r.ok = true;
    r.data = {{"status", resp->status_code}, {"body", resp->body}};
    return r;
}

QdrantClient::QdrantClient(std::string base_url, std::string api_key, int timeout_seconds)
    : base_url_(std::move(base_url)), api_key_(std::move(api_key)), timeout_seconds_(timeout_seconds > 0 ? timeout_seconds : 30) {}

QdrantOpResult QdrantClient::health() const {
    return sendRaw("GET", base_url_ + "/healthz", timeout_seconds_, api_key_);
}

QdrantOpResult QdrantClient::getCollection(const std::string& collection) const {
    return sendJson("GET", base_url_ + "/collections/" + collection, timeout_seconds_, api_key_, nullptr);
}

QdrantOpResult QdrantClient::createCollection(const std::string& collection, int vector_size, const std::string& distance) const {
    hv::Json body;
    body["vectors"] = {{"size", vector_size}, {"distance", normalizeDistance(distance)}};
    return sendJson("PUT", base_url_ + "/collections/" + collection, timeout_seconds_, api_key_, &body);
}

QdrantOpResult QdrantClient::ensureCollection(const std::string& collection, int vector_size, const std::string& distance) const {
    auto cur = getCollection(collection);
    if (!cur.ok) {
        auto created = createCollection(collection, vector_size, distance);
        if (!created.ok) return created;
        cur = getCollection(collection);
    }
    if (!cur.data.contains("result") || !cur.data["result"].is_object()) return cur;
    const auto& result = cur.data["result"];
    if (!result.contains("config") || !result["config"].is_object()) return cur;
    const auto& cfg = result["config"];
    if (!cfg.contains("params") || !cfg["params"].is_object()) return cur;
    const auto& params = cfg["params"];
    if (!params.contains("vectors") || !params["vectors"].is_object()) return cur;
    const auto& v = params["vectors"];
    const int cur_size = v.value("size", 0);
    const std::string cur_dist = v.value("distance", "");
    if (cur_size != vector_size) {
        QdrantOpResult r;
        r.ok = false;
        r.error = "collection vector_size mismatch: expected=" + std::to_string(vector_size) + " got=" + std::to_string(cur_size);
        return r;
    }
    if (!cur_dist.empty() && !distance.empty() && toLower(cur_dist) != toLower(distance)) {
        QdrantOpResult r;
        r.ok = false;
        r.error = "collection distance mismatch: expected=" + distance + " got=" + cur_dist;
        return r;
    }
    const auto idx = ensurePayloadIndexes(collection, {{"session_id", "keyword"}, {"memory_type", "keyword"}, {"note_id", "keyword"}});
    if (!idx.ok) return idx;
    return cur;
}

QdrantOpResult QdrantClient::createPayloadIndex(const std::string& collection, const std::string& field_name, const std::string& field_schema) const {
    hv::Json body;
    body["field_name"] = field_name;
    body["field_schema"] = field_schema;
    const std::string url = base_url_ + "/collections/" + collection + "/index";
    return sendJson("PUT", url, timeout_seconds_, api_key_, &body);
}

QdrantOpResult QdrantClient::ensurePayloadIndexes(const std::string& collection,
                                                 const std::vector<std::pair<std::string, std::string>>& indexes) const {
    for (const auto& it : indexes) {
        const auto r = createPayloadIndex(collection, it.first, it.second);
        if (!r.ok) {
            if (r.error.find("already exists") != std::string::npos) continue;
            if (r.error.find("409") != std::string::npos) continue;
            if (r.error.find("Conflict") != std::string::npos) continue;
            return r;
        }
    }
    QdrantOpResult ok;
    ok.ok = true;
    return ok;
}

QdrantOpResult QdrantClient::upsertPoints(const std::string& collection, const hv::Json& points, bool wait) const {
    hv::Json body;
    body["points"] = points;
    const std::string url = base_url_ + "/collections/" + collection + "/points?wait=" + std::string(wait ? "true" : "false");
    return sendJson("PUT", url, timeout_seconds_, api_key_, &body);
}

QdrantOpResult QdrantClient::searchPoints(const std::string& collection,
                                         const std::vector<float>& query,
                                         int limit,
                                         const hv::Json& filter,
                                         bool with_payload) const {
    hv::Json body;
    hv::Json v = hv::Json::array();
    for (float x : query) v.push_back(x);
    body["vector"] = v;
    body["limit"] = limit;
    body["with_payload"] = with_payload;
    if (!filter.is_null() && !filter.is_discarded() && filter.is_object()) {
        body["filter"] = filter;
    }
    const std::string url = base_url_ + "/collections/" + collection + "/points/search";
    return sendJson("POST", url, timeout_seconds_, api_key_, &body);
}
