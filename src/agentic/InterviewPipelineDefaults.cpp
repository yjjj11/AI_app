#include "agentic/InterviewPipelineDefaults.h"

#include "agentic/RagRetriever.h"
#include "ai/LLMFactory.h"

#include <sstream>

hv::Json DefaultPlanner::plan(const InterviewTurnInput& in) {
    hv::Json j;
    j["mode"] = "interview";
    j["direction"] = in.direction;
    j["has_answer"] = !in.user_answer.empty();
    return j;
}

DefaultRetriever::DefaultRetriever(AgentConfig cfg) : cfg_(std::move(cfg)) {}

std::vector<std::string> DefaultRetriever::retrieve(const InterviewTurnInput& in) {
    RagRetriever r(cfg_);
    const std::string q = in.user_answer.empty() ? in.jd_text : in.user_answer;
    return r.retrieve(in.session_id, q, "short", cfg_.rag_top_k);
}

DefaultComposer::DefaultComposer(AgentConfig cfg) : cfg_(std::move(cfg)) {}

hv::Json DefaultComposer::composeMessages(const InterviewTurnInput& in, const InterviewTurnState& st) {
    auto buildMemoryContext = [&](int budget) -> std::string {
        if (st.retrieved_chunks.empty()) return "";
        if (budget <= 0) budget = 2500;
        std::string out;
        out += "MEMORY (retrieved notes chunks):\n";
        for (const auto& c : st.retrieved_chunks) {
            if ((int)out.size() >= budget) break;
            std::string x = c;
            if ((int)x.size() > 900) x = x.substr(0, 900);
            if ((int)(out.size() + x.size() + 6) > budget) break;
            out += "- " + x + "\n";
        }
        return out;
    };

    hv::Json messages = hv::Json::array();
    std::ostringstream sys;
    sys << "You are an interviewer agent for " << in.direction << ".\n";
    sys << "Goal: ask one good follow-up question based on the candidate answer, then give brief feedback and a score (0-10).\n";
    sys << "Output plain text only.\n";
    const std::string mem = buildMemoryContext(cfg_.rag_context_char_budget);
    if (!mem.empty()) sys << "\n" << mem << "\n";
    sys << "\nJD:\n" << in.jd_text << "\n";
    if (!in.resume_text.empty()) sys << "\nResume:\n" << in.resume_text << "\n";
    messages.push_back({{"role", "system"}, {"content", sys.str()}});

    hv::Json user;
    user["role"] = "user";
    user["content"] = in.user_answer.empty() ? "开始模拟面试，请给出第一个问题。" : ("我的回答：\n" + in.user_answer);
    messages.push_back(user);
    return messages;
}

std::string DefaultGenerator::generateStream(const hv::Json& messages, const std::function<void(std::string_view)>& on_delta) {
    auto llm = LLMFactory::get("active");
    if (!llm) llm = LLMFactory::get();
    if (!llm) return "";
    return llm->stream_chat_messages(messages, on_delta);
}

hv::Json DefaultReflector::reflect(const InterviewTurnInput&, const std::string&) {
    return hv::Json::object();
}

