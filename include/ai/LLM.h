#ifndef AI_LLM_H
#define AI_LLM_H

#include <string>

class LLM {
public:
    virtual ~LLM() = default;
    virtual std::string chat(const std::string& prompt, const std::string& model_type) = 0;
};

#endif
