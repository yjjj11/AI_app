#ifndef AI_BAILIAN_LLM_H
#define AI_BAILIAN_LLM_H

#include "ai/LLM.h"

#include <string>

class BailianLLM final : public LLM {
public:
    BailianLLM(std::string api_key, std::string base_url, std::string default_model, int timeout_seconds);
    std::string chat_messages(const hv::Json& messages) override;
    std::string stream_chat_messages(const hv::Json& messages,
                                     const std::function<void(std::string_view)>& on_delta) override;

private:
    std::string api_key_;
    std::string base_url_;
    std::string default_model_;
    int timeout_seconds_{30};

    std::vector<std::string> short_memory_;
    std::vector<std::string> long_memory_;
};

#endif
