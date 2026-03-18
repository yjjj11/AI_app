#ifndef AI_LLM_H
#define AI_LLM_H

#include <functional>
#include <string>

#include <hv/HttpService.h>

class LLM {
public:
    virtual ~LLM() = default;
    virtual std::string chat_messages(const hv::Json& messages) = 0;
    virtual std::string stream_chat_messages(const hv::Json& messages,
                                             const std::function<void(std::string_view)>& on_delta) = 0;
};

#endif
