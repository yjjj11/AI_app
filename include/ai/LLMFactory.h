#ifndef AI_LLM_FACTORY_H
#define AI_LLM_FACTORY_H

#include "ai/LLM.h"

#include <memory>

class LLMFactory {
public:
    static std::unique_ptr<LLM> create();
};

#endif
