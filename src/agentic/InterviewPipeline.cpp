#include "agentic/InterviewPipeline.h"

DefaultInterviewPipeline::DefaultInterviewPipeline(IPlanner& planner,
                                                   IRetriever& retriever,
                                                   IComposer& composer,
                                                   IGenerator& generator,
                                                   IReflector& reflector)
    : planner_(planner), retriever_(retriever), composer_(composer), generator_(generator), reflector_(reflector) {}

std::string DefaultInterviewPipeline::run(const InterviewTurnInput& in,
                                         const std::function<void(std::string_view)>& on_delta,
                                         hv::Json* out_plan,
                                         hv::Json* out_reflection) {
    if (out_plan) *out_plan = planner_.plan(in);
    InterviewTurnState st;
    st.retrieved_chunks = retriever_.retrieve(in);
    hv::Json messages = composer_.composeMessages(in, st);
    const std::string out = generator_.generateStream(messages, on_delta);
    if (out_reflection) *out_reflection = reflector_.reflect(in, out);
    return out;
}

