#ifndef AGENTIC_INTERVIEW_PIPELINE_DEFAULTS_H
#define AGENTIC_INTERVIEW_PIPELINE_DEFAULTS_H

#include "agentic/InterviewPipeline.h"
#include "config/AgentConfig.h"

#include <memory>

class LLM;

class DefaultPlanner : public IPlanner {
public:
    hv::Json plan(const InterviewTurnInput& in) override;
};

class DefaultRetriever : public IRetriever {
public:
    explicit DefaultRetriever(AgentConfig cfg);
    std::vector<std::string> retrieve(const InterviewTurnInput& in) override;

private:
    AgentConfig cfg_;
};

class DefaultComposer : public IComposer {
public:
    explicit DefaultComposer(AgentConfig cfg);
    hv::Json composeMessages(const InterviewTurnInput& in, const InterviewTurnState& st) override;

private:
    AgentConfig cfg_;
};

class DefaultGenerator : public IGenerator {
public:
    std::string generateStream(const hv::Json& messages, const std::function<void(std::string_view)>& on_delta) override;
};

class DefaultReflector : public IReflector {
public:
    hv::Json reflect(const InterviewTurnInput&, const std::string&) override;
};

#endif
