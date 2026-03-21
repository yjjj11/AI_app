#ifndef AGENTIC_EMBEDDING_FACTORY_H
#define AGENTIC_EMBEDDING_FACTORY_H

#include "agentic/EmbeddingClient.h"
#include "config/AgentConfig.h"

#include <memory>

std::unique_ptr<EmbeddingClient> createEmbeddingClientFromConfig(const AgentConfig& cfg);

#endif
