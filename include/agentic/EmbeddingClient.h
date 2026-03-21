#ifndef AGENTIC_EMBEDDING_CLIENT_H
#define AGENTIC_EMBEDDING_CLIENT_H

#include <string>
#include <vector>

struct EmbeddingResult {
    bool ok{false};
    std::string error;
    int dimensions{0};
    std::vector<std::vector<float>> vectors;
};

class EmbeddingClient {
public:
    EmbeddingClient(std::string api_key,
                    std::string embeddings_url,
                    std::string model,
                    int timeout_seconds,
                    int dimensions);

    EmbeddingResult embedBatch(const std::vector<std::string>& inputs) const;
    const std::string& url() const { return embeddings_url_; }
    const std::string& model() const { return model_; }
    int configuredDimensions() const { return dimensions_; }

private:
    std::string api_key_;
    std::string embeddings_url_;
    std::string model_;
    int timeout_seconds_{30};
    int dimensions_{0};
};

#endif
