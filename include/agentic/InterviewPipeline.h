#ifndef AGENTIC_INTERVIEW_PIPELINE_H
#define AGENTIC_INTERVIEW_PIPELINE_H

#include <functional>
#include <string>
#include <vector>

#include <hv/HttpService.h>

struct InterviewTurnInput {
    std::string session_id;
    std::string direction;
    std::string jd_text;
    std::string resume_text;
    std::string user_answer;
};

struct InterviewTurnState {
    std::vector<std::string> retrieved_chunks;
    std::string composed_system_prompt;
};

class IPlanner {
public:
    virtual ~IPlanner() = default;
    virtual hv::Json plan(const InterviewTurnInput& in) = 0;
};

class IRetriever {
public:
    virtual ~IRetriever() = default;
    virtual std::vector<std::string> retrieve(const InterviewTurnInput& in) = 0;
};

class IComposer {
public:
    virtual ~IComposer() = default;
    virtual hv::Json composeMessages(const InterviewTurnInput& in, const InterviewTurnState& st) = 0;
};

class IGenerator {
public:
    virtual ~IGenerator() = default;
    virtual std::string generateStream(const hv::Json& messages, const std::function<void(std::string_view)>& on_delta) = 0;
};

class IReflector {
public:
    virtual ~IReflector() = default;
    virtual hv::Json reflect(const InterviewTurnInput& in, const std::string& assistant_text) = 0;
};

class DefaultInterviewPipeline {
public:
    DefaultInterviewPipeline(IPlanner& planner, IRetriever& retriever, IComposer& composer, IGenerator& generator, IReflector& reflector);
    std::string run(const InterviewTurnInput& in,
                    const std::function<void(std::string_view)>& on_delta,
                    hv::Json* out_plan,
                    hv::Json* out_reflection);

private:
    IPlanner& planner_;
    IRetriever& retriever_;
    IComposer& composer_;
    IGenerator& generator_;
    IReflector& reflector_;
};

#endif

