#ifndef AGENTIC_RAG_RETRIEVER_H
#define AGENTIC_RAG_RETRIEVER_H

#include "config/AgentConfig.h"

#include <string>
#include <vector>

class RagRetriever {
public:
    explicit RagRetriever(AgentConfig cfg);

    std::vector<std::string> retrieve(const std::string& session_id,
                                      const std::string& query,
                                      const std::string& memory_type,
                                      int top_k) const;

private:
    AgentConfig cfg_;
};

#endif
