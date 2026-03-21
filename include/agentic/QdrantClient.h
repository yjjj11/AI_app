#ifndef AGENTIC_QDRANT_CLIENT_H
#define AGENTIC_QDRANT_CLIENT_H

#include <string>
#include <vector>

#include <hv/HttpService.h>

struct QdrantOpResult {
    bool ok{false};
    std::string error;
    hv::Json data;
};

class QdrantClient {
public:
    QdrantClient(std::string base_url, std::string api_key, int timeout_seconds);

    QdrantOpResult health() const;
    QdrantOpResult getCollection(const std::string& collection) const;
    QdrantOpResult createCollection(const std::string& collection, int vector_size, const std::string& distance) const;
    QdrantOpResult ensureCollection(const std::string& collection, int vector_size, const std::string& distance) const;
    QdrantOpResult createPayloadIndex(const std::string& collection, const std::string& field_name, const std::string& field_schema) const;
    QdrantOpResult ensurePayloadIndexes(const std::string& collection, const std::vector<std::pair<std::string, std::string>>& indexes) const;

    QdrantOpResult upsertPoints(const std::string& collection, const hv::Json& points, bool wait) const;
    QdrantOpResult searchPoints(const std::string& collection,
                                const std::vector<float>& query,
                                int limit,
                                const hv::Json& filter,
                                bool with_payload) const;

private:
    std::string base_url_;
    std::string api_key_;
    int timeout_seconds_{30};
};

#endif
