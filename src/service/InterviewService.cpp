#include "service/InterviewService.h"

#include "agentic/InterviewPipeline.h"
#include "agentic/InterviewPipelineDefaults.h"
#include "ai/LLMFactory.h"
#include "config/AgentConfig.h"
#include "service/ChatService.h"

#include <sstream>

static hv::Json safeParseJson(const std::string& s) {
    hv::Json j = hv::Json::parse(s, nullptr, false);
    if (j.is_object()) return j;
    size_t l = std::string::npos;
    size_t r = std::string::npos;
    const size_t fence_end = s.rfind("```");
    if (fence_end != std::string::npos) {
        r = s.rfind('}', fence_end);
    }
    if (r == std::string::npos) r = s.rfind('}');
    const size_t fence_begin = s.find("```");
    if (fence_begin != std::string::npos) {
        l = s.find('{', fence_begin);
    }
    if (l == std::string::npos) l = s.find('{');
    if (l != std::string::npos && r != std::string::npos && r > l) {
        hv::Json jj = hv::Json::parse(s.substr(l, r - l + 1), nullptr, false);
        if (jj.is_object()) return jj;
    }
    return hv::Json();
}

static std::string buildMemoryContext(const std::vector<std::string>& chunks, int char_budget) {
    if (chunks.empty()) return "";
    if (char_budget <= 0) char_budget = 2500;
    std::string out;
    out += "MEMORY (retrieved notes chunks):\n";
    for (const auto& c : chunks) {
        if ((int)out.size() >= char_budget) break;
        std::string x = c;
        if ((int)x.size() > 900) x = x.substr(0, 900);
        if ((int)(out.size() + x.size() + 6) > char_budget) break;
        out += "- ";
        out += x;
        out += "\n";
    }
    return out;
}

hv::Json InterviewService::parseJd(const std::string& jd_text, const std::string& direction) {
    hv::Json out;
    auto llm = LLMFactory::get("active");
    if (!llm) llm = LLMFactory::get();
    if (!llm) {
        out["ok"] = false;
        out["error"] = "llm not configured";
        return out;
    }
    hv::Json messages = hv::Json::array();
    messages.push_back({{"role", "system"},
                        {"content",
                         "You are an interview assistant. Extract JD into STRICT JSON only.\n"
                         "Schema:\n"
                         "{\n"
                         "  \"direction\": \"C++后端|Go后端|Agent开发|其他\",\n"
                         "  \"keywords\": [\"...\"],\n"
                         "  \"requirements\": [\"...\"],\n"
                         "  \"question_topics\": [\"...\"],\n"
                         "  \"difficulty\": \"junior|mid|senior\"\n"
                         "}\n"
                         "No markdown. No extra text."}});
    std::string user = "direction: " + direction + "\nJD:\n" + jd_text;
    messages.push_back({{"role", "user"}, {"content", user}});
    const std::string raw = llm->chat_messages(messages);
    hv::Json j = safeParseJson(raw);
    if (j.is_object()) {
        j["ok"] = true;
        return j;
    }
    out["ok"] = false;
    out["error"] = "parse failed";
    out["raw"] = raw;
    return out;
}

hv::Json InterviewService::generatePlan(const std::string& jd_text, const std::string& resume_text, const std::string& direction) {
    hv::Json out;
    auto llm = LLMFactory::get("active");
    if (!llm) llm = LLMFactory::get();
    if (!llm) {
        out["ok"] = false;
        out["error"] = "llm not configured";
        return out;
    }
    hv::Json messages = hv::Json::array();
    messages.push_back({{"role", "system"},
                        {"content",
                         "You are a strict interviewer. Produce an interview plan in STRICT JSON only.\n"
                         "Schema:\n"
                         "{\n"
                         "  \"direction\": \"...\",\n"
                         "  \"questions\": [\n"
                         "    {\"type\":\"基础|项目|系统设计|并发|网络|数据库\",\"question\":\"...\",\"followups\":[\"...\"],\"rubric\":[\"...\"]}\n"
                         "  ]\n"
                         "}\n"
                         "No markdown. No extra text."}});
    std::string user = "direction: " + direction + "\nJD:\n" + jd_text + "\n\nResume:\n" + resume_text;
    messages.push_back({{"role", "user"}, {"content", user}});
    const std::string raw = llm->chat_messages(messages);
    hv::Json j = safeParseJson(raw);
    if (j.is_object()) {
        j["ok"] = true;
        return j;
    }
    out["ok"] = false;
    out["error"] = "parse failed";
    out["raw"] = raw;
    return out;
}

hv::Json InterviewService::reviewSession(const std::string& username, const std::string& session_id, const std::string& direction) {
    hv::Json out;
    auto llm = LLMFactory::get("active");
    if (!llm) llm = LLMFactory::get();
    if (!llm) {
        out["ok"] = false;
        out["error"] = "llm not configured";
        return out;
    }
    ChatService chat;
    const auto tail = chat.getTailHistory(username, session_id, 120);
    std::string transcript;
    for (const auto& m : tail) {
        std::string c = m.content;
        if (c.size() > 900) c = c.substr(0, 900);
        transcript += m.role + ": " + c + "\n";
    }
    hv::Json messages = hv::Json::array();
    messages.push_back({{"role", "system"},
                        {"content",
                         "You are an interview coach. Review the session and output STRICT JSON only.\n"
                         "Schema:\n"
                         "{\n"
                         "  \"overall_score\": 0,\n"
                         "  \"strengths\": [\"...\"],\n"
                         "  \"weaknesses\": [\"...\"],\n"
                         "  \"must_fix\": [\"...\"],\n"
                         "  \"next_questions\": [\"...\"],\n"
                         "  \"direction\": \"...\"\n"
                         "}\n"
                         "No markdown. No extra text."}});
    messages.push_back({{"role", "user"}, {"content", "direction: " + direction + "\n" + transcript}});
    const std::string raw = llm->chat_messages(messages);
    hv::Json j = safeParseJson(raw);
    if (j.is_object()) {
        j["ok"] = true;
        return j;
    }
    out["ok"] = false;
    out["error"] = "parse failed";
    out["raw"] = raw;
    return out;
}

std::string InterviewService::streamTurn(const std::string& session_id,
                                         const std::string& jd_text,
                                         const std::string& resume_text,
                                         const std::string& direction,
                                         const std::string& user_answer,
                                         const std::function<void(std::string_view)>& on_delta) {
    AgentConfig cfg = AgentConfig::loadFromEnv();
    DefaultPlanner planner;
    DefaultRetriever retriever(cfg);
    DefaultComposer composer(cfg);
    DefaultGenerator generator;
    DefaultReflector reflector;
    DefaultInterviewPipeline pipe(planner, retriever, composer, generator, reflector);

    InterviewTurnInput in;
    in.session_id = session_id;
    in.direction = direction;
    in.jd_text = jd_text;
    in.resume_text = resume_text;
    in.user_answer = user_answer;
    return pipe.run(in, on_delta, nullptr, nullptr);
}
